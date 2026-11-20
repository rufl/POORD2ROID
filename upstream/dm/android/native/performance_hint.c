#include "performance_hint.h"

#ifdef __ANDROID__
#include <android/log.h>
#include <android/performance_hint.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// NDK 接口从 API 33 开始提供；动态解析令 API 29–32 仍能加载应用。
// NDK interfaces start at API 33; dynamic resolution keeps the application loadable on API 29–32.
typedef struct {
    APerformanceHintManager *(*manager)(void);
    APerformanceHintSession *(*create)(APerformanceHintManager *,const int32_t *,size_t,int64_t);
    int (*report)(APerformanceHintSession *,int64_t);
    void (*close)(APerformanceHintSession *);
    int64_t (*preferred_rate)(APerformanceHintManager *);
    int (*update_target)(APerformanceHintSession *,int64_t);
    int (*set_threads)(APerformanceHintSession *,const int32_t *,size_t);
} hint_api;

static hint_api api;
static pthread_once_t load_once=PTHREAD_ONCE_INIT;
static pthread_mutex_t hint_lock=PTHREAD_MUTEX_INITIALIZER;
static APerformanceHintSession *session;
static int available,attempted,reported,hal_unsupported;
static unsigned retry_delay_cycles=60;
static uint64_t retry_after_cycle;

#define HINT_THREAD_CAPACITY 2
static int32_t bound_tids[HINT_THREAD_CAPACITY];
static size_t bound_tid_count;
static int32_t cycle_begin_tid;
static int64_t target_ns=16666667;
static int paused,cycle_pending,cycle_invalid;
static const void *cycle_window;
static uint64_t cycle_start,cycle_wsi,accepted,skipped,cycles,work_total,wsi_total;


static uint64_t monotonic_ns(void) {
    struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;
    return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;
}
static void cycle_clear(void) {
    cycle_pending=cycle_invalid=0;cycle_window=NULL;cycle_start=cycle_wsi=0;cycle_begin_tid=0;
}

static void load_api(void) {
    const char *enabled=getenv("DM_ANDROID_PERF_HINT");
    if(enabled&&!strcmp(enabled,"0")){fprintf(stderr,"[dm] [adpf] disabled by launch environment\n");return;}
    void *library=dlopen("libandroid.so",RTLD_NOW|RTLD_LOCAL);
    if(!library){fprintf(stderr,"[dm] [adpf] libandroid load failed\n");return;}
    // Android 的函数指针与 dlsym 地址同大小；保持库句柄存活至进程退出。
    // Android function pointers have the size of dlsym addresses; retain the library handle until process exit.
    _Static_assert(sizeof api.manager==sizeof(void *),"Android function pointer size");
#define LOAD_HINT(field,name) do {void *symbol=dlsym(library,name);memcpy(&api.field,&symbol,sizeof api.field);} while(0)
    LOAD_HINT(manager,"APerformanceHint_getManager");
    LOAD_HINT(create,"APerformanceHint_createSession");
    LOAD_HINT(report,"APerformanceHint_reportActualWorkDuration");
    LOAD_HINT(close,"APerformanceHint_closeSession");
    LOAD_HINT(preferred_rate,"APerformanceHint_getPreferredUpdateRateNanos");
    LOAD_HINT(update_target,"APerformanceHint_updateTargetWorkDuration");
    LOAD_HINT(set_threads,"APerformanceHint_setThreads");
#undef LOAD_HINT
    available=api.manager&&api.create&&api.report&&api.close;
    // 冷路径写入已有游戏日志，避免设备过滤 logcat 后误判会话状态。
    // Mirror cold-path evidence to the existing game log when device logcat filters hide session state.
    fprintf(stderr,"[dm] [adpf] API available=%d\n",available);
    if(!available)__android_log_print(ANDROID_LOG_INFO,"D2R","[adpf] native performance hints unavailable; continue with normal scheduling");
}

void dm_android_performance_frame_begin(const void *window,int index) {
    (void)index;
    pthread_once(&load_once,load_api);
    if(!available||hal_unsupported)return;
    pthread_mutex_lock(&hint_lock);
    // 一帧内客体可能多次取图；重复 begin 只保留最早起点，不作废周期。
    // A frame may acquire several textures; a repeated begin keeps the earliest start and never invalidates the cycle.
    if(!paused&&!hal_unsupported){
        if(!cycle_pending){cycle_pending=1;cycle_window=window;cycle_start=monotonic_ns();cycle_wsi=0;cycle_begin_tid=gettid();cycle_invalid=!window||!cycle_start;}
        else if(window)cycle_window=window;
    }
    pthread_mutex_unlock(&hint_lock);
}

void dm_android_performance_wsi_calls(uint64_t duration_ns) {
    pthread_mutex_lock(&hint_lock);
    // WSI 可能在与 acquire 不同的线程上执行（本端口：acquire 在客体线程，present/WSI 在录制线程）。
    // 按线程号作废会把每个周期都判死，ADPF 永远收不到一次上报。这里只保留算术溢出保护。
    // WSI may run on a different thread than acquire (this port: acquire on the guest thread, present/WSI on
    // the recording thread). Invalidating on thread identity killed every cycle and ADPF never received a
    // single report; only the arithmetic overflow guard is kept here.
    if(cycle_pending&&!cycle_invalid){
        if(duration_ns>UINT64_MAX-cycle_wsi)cycle_invalid=1;
        else cycle_wsi+=duration_ns;
    }
    pthread_mutex_unlock(&hint_lock);
}

static size_t thread_group_make(int32_t begin_tid,int32_t end_tid,int32_t tids[HINT_THREAD_CAPACITY]) {
    size_t count=0;
    if(begin_tid>0)tids[count++]=begin_tid;
    if(end_tid>0&&(count==0||tids[0]!=end_tid))tids[count++]=end_tid;
    return count;
}
static int thread_group_matches(const int32_t *tids,size_t count) {
    return count==bound_tid_count&&(!count||!memcmp(tids,bound_tids,count*sizeof *tids));
}
static void session_close_locked(void) {
    if(session&&api.close)api.close(session);
    session=NULL;bound_tid_count=0;memset(bound_tids,0,sizeof bound_tids);attempted=0;reported=0;
}
static int session_bind_threads_locked(const int32_t *tids,size_t count) {
    if(!session||!count)return 0;
    if(thread_group_matches(tids,count))return 1;
    if(api.set_threads){
        int result=api.set_threads(session,tids,count);
        if(!result){
            memcpy(bound_tids,tids,count*sizeof *tids);bound_tid_count=count;
            fprintf(stderr,"[dm] [adpf] session thread group updated tids=%d,%d count=%zu\n",tids[0],count>1?tids[1]:0,count);
            return 1;
        }
        fprintf(stderr,"[dm] [adpf] thread group update failed result=%d; recreating session\n",result);
    }else fprintf(stderr,"[dm] [adpf] thread group changed without setThreads API; recreating session\n");
    session_close_locked();
    return 0;
}
static void session_create_locked(const int32_t *tids,size_t count) {
    if(attempted||hal_unsupported||cycles<retry_after_cycle||!count)return;
    APerformanceHintManager *manager=api.manager();
    int64_t preferred=manager&&api.preferred_rate?api.preferred_rate(manager):-1;
    if(!manager||(api.preferred_rate&&preferred<=0)){
        hal_unsupported=1;
        fprintf(stderr,"[dm] [adpf] unavailable on device HAL (manager=%d tids=%d,%d count=%zu preferred_ns=%lld); disabling ADPF hints\n",
                manager!=NULL,tids[0],count>1?tids[1]:0,count,(long long)preferred);
        return;
    }
    session=api.create(manager,tids,count,target_ns);
    if(session){
        attempted=1;retry_after_cycle=0;retry_delay_cycles=60;
        memcpy(bound_tids,tids,count*sizeof *tids);bound_tid_count=count;
        fprintf(stderr,"[dm] [adpf] session created tids=%d,%d count=%zu target_ns=%lld preferred_ns=%lld\n",tids[0],count>1?tids[1]:0,count,(long long)target_ns,(long long)preferred);
        __android_log_print(ANDROID_LOG_INFO,"D2R","[adpf] session tids=%d,%d count=%zu target=%.3f ms preferred_report=%.3f ms",tids[0],count>1?tids[1]:0,count,target_ns/1e6,preferred<0?-1:preferred/1e6);
    }else{
        uint32_t delay=retry_delay_cycles;
        retry_after_cycle=cycles+delay;
        retry_delay_cycles=delay<1800?delay*2:3600;
        fprintf(stderr,"[dm] [adpf] session creation failed manager=%d tids=%d,%d count=%zu preferred_ns=%lld retry_after_cycle=%llu\n",
                manager!=NULL,tids[0],count>1?tids[1]:0,count,(long long)preferred,(unsigned long long)retry_after_cycle);
        __android_log_print(ANDROID_LOG_INFO,"D2R","[adpf] session creation failed; retry in %u valid frame cycles",delay);
    }
}
void dm_android_performance_set_target(int64_t target_duration_ns) {
    if(target_duration_ns<1000000||target_duration_ns>1000000000)return;
    pthread_once(&load_once,load_api);
    pthread_mutex_lock(&hint_lock);
    if(target_ns!=target_duration_ns){
        target_ns=target_duration_ns;cycle_clear();
        if(session){
            int result=api.update_target?api.update_target(session,target_ns):-1;
            if(result){
                fprintf(stderr,"[dm] [adpf] target update unavailable/failed result=%d; recreate session target_ns=%lld\n",result,(long long)target_ns);
                session_close_locked();
            }
        }
        fprintf(stderr,"[dm] [adpf] target updated target_ns=%lld update_api=%d\n",(long long)target_ns,api.update_target!=NULL);
    }
    pthread_mutex_unlock(&hint_lock);
}


void dm_android_performance_frame_end(const void *window,int index) {
    (void)index;
    uint64_t ended=monotonic_ns();int32_t tid=gettid();
    pthread_once(&load_once,load_api);
    pthread_mutex_lock(&hint_lock);
    if(paused||!available||hal_unsupported){cycle_clear();pthread_mutex_unlock(&hint_lock);return;}
    uint64_t elapsed=ended>=cycle_start?ended-cycle_start:0;
    // acquire 与 present/WSI 在本端口分属两个线程，取图索引也未必是出图那张；周期有效性只看窗口与墙钟。
    // Acquire and present/WSI run on two different threads in this port, and the acquired index is not
    // necessarily the presented one; cycle validity depends only on the window and the wall clock.
    int valid=cycle_pending&&!cycle_invalid&&cycle_window==window&&elapsed>cycle_wsi;
    uint64_t work_ns=valid?elapsed-cycle_wsi:0,wsi_ns=cycle_wsi;
    int32_t begin_tid=cycle_begin_tid,tids[HINT_THREAD_CAPACITY]={0};
    size_t tid_count=thread_group_make(begin_tid,tid,tids);
    // 周期墙钟保留真实依赖等待；仅扣 WSI 两个完整调用，含少量驱动CPU，不称为纯等待。
    // Cycle wall time retains dependency waits; subtract only both full WSI calls, including some driver CPU, not pure idle.
    cycle_clear();cycles++;
    if(valid&&work_ns&&work_ns<=INT64_MAX&&tid_count){
        if(session)session_bind_threads_locked(tids,tid_count);
        if(!session)session_create_locked(tids,tid_count);
        valid=session&&thread_group_matches(tids,tid_count);
    }else valid=0;
    if(valid){
        int result=api.report(session,(int64_t)work_ns);
        if(!reported||result){fprintf(stderr,"[dm] [adpf] report tids=%d,%d count=%zu caller_tid=%d work_ns=%llu result=%d\n",bound_tids[0],bound_tid_count>1?bound_tids[1]:0,bound_tid_count,gettid(),(unsigned long long)work_ns,result);reported=1;}
        if(!result){accepted++;work_total+=work_ns;wsi_total+=wsi_ns;}else {
            skipped++;
            __android_log_print(ANDROID_LOG_INFO,"D2R","[adpf] report failed (%d) tids=%d,%d; close hint session",result,bound_tids[0],bound_tid_count>1?bound_tids[1]:0);
            // 上报失败即关闭会话，但必须允许重建：否则 attempted 恒为 1，ADPF 一次失败后永久失效。
            // A failed report closes the session, but recreation must stay possible; otherwise attempted stays 1
            // and ADPF is dead for good after a single failure. Back off like the create-failure path.
            session_close_locked();
            retry_after_cycle=cycles+retry_delay_cycles;
            retry_delay_cycles=retry_delay_cycles<1800?retry_delay_cycles*2:3600;
        }
    }else skipped++;
    static int diag_env=-1;
    if(diag_env<0){const char *e=getenv("DM_VK_DIAG");diag_env=e&&*e&&strcmp(e,"0")!=0;}
    if(diag_env&&(cycles==1||!(cycles%120)))fprintf(stderr,"[dm] [adpf-cycle] cycles=%llu accepted=%llu skipped=%llu caller_tid=%d begin_tid=%d bound_tids=%d,%d bound_count=%zu last_work_ns=%llu last_wsi_calls_ns=%llu accepted_mean_ms=%.3f excluded_wsi_mean_ms=%.3f target_ns=%lld clock=monotonic dependency_wait_included=1 worker_not_added=0 set_threads_api=%d\n",(unsigned long long)cycles,(unsigned long long)accepted,(unsigned long long)skipped,tid,begin_tid,bound_tids[0],bound_tid_count>1?bound_tids[1]:0,bound_tid_count,(unsigned long long)(valid?work_ns:0),(unsigned long long)wsi_ns,accepted?work_total/(accepted*1e6):0,accepted?wsi_total/(accepted*1e6):0,(long long)target_ns,api.set_threads!=NULL);
    pthread_mutex_unlock(&hint_lock);
}

void dm_android_performance_close(void) {
    pthread_mutex_lock(&hint_lock);
    session_close_locked();retry_after_cycle=0;retry_delay_cycles=60;
    paused=1;cycle_clear();accepted=skipped=cycles=work_total=wsi_total=0;
    pthread_mutex_unlock(&hint_lock);
}
void dm_android_performance_resume(void) {
    pthread_mutex_lock(&hint_lock);cycle_clear();paused=0;pthread_mutex_unlock(&hint_lock);
}

#else
void dm_android_performance_frame_begin(const void *window,int index) {(void)window;(void)index;}
void dm_android_performance_frame_end(const void *window,int index) {(void)window;(void)index;}
void dm_android_performance_wsi_calls(uint64_t duration_ns) {(void)duration_ns;}
void dm_android_performance_close(void) {}
void dm_android_performance_resume(void) {}
void dm_android_performance_set_target(int64_t target_duration_ns) {(void)target_duration_ns;}
#endif
