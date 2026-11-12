// nn::oe（应用运行环境）：真机上经 am/oe 服务查询主机状态；宿主直接给固定状态。
// oe::Initialize 在 hle_sdkinit.c，不在这里。
// 环境开关：DM_OPMODE=0 掌机 / 1 主机（默认 1，对应主机模式的渲染档）；DM_LANG=语言码（默认 zh-Hans）。
// nn::oe (application operating environment): on the real device the console state is queried through the am/oe services; the host gives fixed state directly.
// oe::Initialize is in hle_sdkinit.c, not here.
// Environment switches: DM_OPMODE=0 handheld / 1 console (default 1, corresponding to the console-mode render profile); DM_LANG=language code (default zh-Hans).
#include "hle.h"
#ifdef __ANDROID__
#include "../android/native/host_tls.h"
#define pthread_setspecific dm_android_host_setspecific
#endif
#include "dm_uiscreen.h"

// 运行模式是运行时状态（前端可改：iOS 过热降级切掌机）：唯一来源在此，启动时从 DM_OPMODE 取一次，之后不再读环境变量
// The operation mode is runtime state (the front end can change it: iOS overheat downgrade switches to handheld): the single source is here, taken once from DM_OPMODE at startup, the environment variable is not read again afterwards
#include <stdatomic.h>
static _Atomic int g_opmode = -1;
int dm_opmode(void) {
    int m = g_opmode;
    if (m < 0) { const char *e = getenv("DM_OPMODE"); int v = e ? atoi(e) : 1; int exp = -1; atomic_compare_exchange_strong(&g_opmode, &exp, v); m = g_opmode; }
    return m;
}
// ---------- 系统通知队列（AppletMessage）：前端状态变化 → 游戏每帧 TryPopNotificationMessage 取 ----------
// 消息号（游戏 main 0xe10d30 实际处理的）：15 焦点变化、30 运行模式变化、31 性能模式变化
// ---------- System notification queue (AppletMessage): front end state changes → fetched by the game every frame with TryPopNotificationMessage ----------
// Message numbers (those actually handled by the game at main 0xe10d30): 15 focus change, 30 operation mode change, 31 performance mode change
enum { MSG_FOCUS = 15, MSG_OPMODE = 30, MSG_PERFMODE = 31 };
#define MQ 32
static pthread_mutex_t g_mq_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_mq[MQ]; static int g_mqh, g_mqn;
static _Atomic int g_ntf_opmode, g_ntf_perfmode;          // 游戏用 Set*NotificationEnabled 打开后才推 | only pushed after the game turns it on with Set*NotificationEnabled
static void mq_push(uint32_t m) {
    pthread_mutex_lock(&g_mq_lock);
    // 15/30/31 是「查询当前状态」的失效通知，不是携带历史值的事件。
    // 同类型尚未取走时保留首次排位：D2R 消费循环只读最新状态，重复通知的写回相同，尺寸相同不再 resize。
    // 只合并这三种状态通知；未来的逐次事件（如 Resume/Exit）不能套这个规则。
    // 合法生产者只发这三种，因此待取最多三项，任意多次失焦/恢复不再撞 32 槽上限；弹出后新变化照常入队。
    // 15/30/31 are invalidation notifications meaning "query the current state", not events carrying historical values.
    // While one of the same type has not been taken yet, the first queue position is kept: the D2R consumer loop only reads the latest state, repeated notifications write back the same thing, and with the same size there is no further resize.
    // Only these three state notifications are merged; future per-occurrence events (such as Resume/Exit) must not use this rule.
    // Legitimate producers only send these three, so at most three items are pending, and any number of focus losses/restores no longer hits the 32-slot limit; after a pop, new changes are enqueued as usual.
    if (m == MSG_FOCUS || m == MSG_OPMODE || m == MSG_PERFMODE)
        for (int i = 0; i < g_mqn; i++)
            if (g_mq[(g_mqh + i) % MQ] == m) { pthread_mutex_unlock(&g_mq_lock); return; }
    if (g_mqn == MQ) die("oe 通知队列溢出（游戏没在取？）");
    g_mq[(g_mqh + g_mqn++) % MQ] = m;
    pthread_mutex_unlock(&g_mq_lock);
}
void dm_oe_res_changed(void) { if (getenv("DM_TEST_OE_FOCUS_FIRST")) mq_push(MSG_FOCUS);   /* 测试：让 31 排在一条焦点通知后面（iOS 包启动时的顺序） | Test: queue 31 behind a focus notification (the order at iOS app startup) */
    if (g_ntf_perfmode) mq_push(MSG_PERFMODE); else LOG("【红】分辨率要切，但游戏没打开性能模式变化通知"); }   // 出图分辨率该变了（dm_uiscreen.c）：让游戏重读参数位 | the render resolution should change (dm_uiscreen.c): make the game re-read the parameter bits
// 只对 GetOperationMode 报的运行模式（手柄 / 触屏布局切换让游戏重载布局用，dm_padmode.c）：m ≥ 0 报 m 并推一次运行模式通知，m < 0 报回真值再推一次。
// 手柄样式（hle_hid.c connected_id）与性能模式照真值，所以不弹「重新连接控制器」、出图分辨率不变
// Operation mode reported to GetOperationMode only (used to make the game reload layouts when switching between controller / touch layouts, dm_padmode.c):
// m ≥ 0 reports m and pushes one operation-mode notification; m < 0 reports the true value again and pushes one more.
// The controller style (hle_hid.c connected_id) and performance mode keep their true values, so no "Reconnect controller" prompt appears and the render
// resolution does not change
static _Atomic int g_ui_opmode = -1;
static _Atomic int g_ui_armed; static _Atomic uint32_t g_ui_reads;   // 推过通知后游戏第一次来读运行模式记一次：切换事务拿它回读「游戏收到了」 | Counts once when the game first comes to read the operation mode after a notification has been pushed: the switch transaction uses it to read back "the game received it"
uint32_t dm_oe_ui_opmode_reads(void) { return atomic_load(&g_ui_reads); }
void dm_oe_ui_opmode(int m) { g_ui_opmode = m < 0 ? -1 : m; atomic_store(&g_ui_armed, 1); if (g_ntf_opmode) mq_push(MSG_OPMODE); }
void dm_set_opmode(int m) {   // 前端调（过热降级等）：改状态并按游戏打开的开关推送 30 / 31 | called by the front end (overheat downgrade etc.): change the state and push 30 / 31 according to the switches the game has turned on
    int old = dm_opmode(); if (old == m) return;
    g_opmode = m;
    LOG("oe 运行模式 %d → %d", old, m);
    if (g_ntf_opmode) mq_push(MSG_OPMODE);
    if (g_ntf_perfmode) mq_push(MSG_PERFMODE);
}
// FocusState：1 InFocus、2 OutOfFocus、3 Background。前端调 dm_focus_set（iOS 生命周期 / Mac 窗口失焦）
// FocusState: 1 InFocus, 2 OutOfFocus, 3 Background. The front end calls dm_focus_set (iOS lifecycle / Mac window losing focus)
static _Atomic int g_focus = 1; static _Atomic uint64_t g_focus_in_ns;   // 最近一次进入 InFocus 的时刻 | time of the most recent entry into InFocus
int dm_focus_now(void) { return atomic_load(&g_focus); }
uint64_t dm_focus_in_since(void) { return atomic_load(&g_focus_in_ns); }   // 图形层显示节拍看门狗：只在激活时算停顿，从激活起算（nvn_mtl.m mtl_present_wait） | graphics layer display link watchdog: stalls are only computed while active, counted from activation (nvn_mtl.m mtl_present_wait)
void dm_focus_set(int st) {
    if (st < 1 || st > 3) die("dm_focus_set 非法状态 %d", st);
    if (st == 1) atomic_store(&g_focus_in_ns, now_ns());
    if (atomic_exchange(&g_focus, st) == st) return;
    LOG("oe 焦点状态 → %d", st);
    mq_push(MSG_FOCUS);
}
static int opmode(void) { return dm_opmode(); }

// 截图 / 录像开关：宿主无系统截图服务，空操作
// Screenshot / video recording switches: the host has no system screenshot service, no-op
HLE_FORCE("_ZN2nn2oe15EnableRecordingEv", "录像开关；宿主无系统录像", void, h_oe_EnableRec, void) {}
HLE_FORCE("_ZN2nn2oe16DisableRecordingEv", "录像开关；宿主无系统录像", void, h_oe_DisableRec, void) {}
HLE_FORCE("_ZN2nn2oe16EnableScreenShotEv", "截图开关；宿主无系统截图", void, h_oe_EnableShot, void) {}
HLE_FORCE("_ZN2nn2oe17DisableScreenShotEv", "截图开关；宿主无系统截图", void, h_oe_DisableShot, void) {}

// OperationMode：0 Handheld、1 Console；PerformanceMode：0 Normal、1 Boost（与运行模式一一对应）
// OperationMode: 0 Handheld, 1 Console; PerformanceMode: 0 Normal, 1 Boost (one-to-one with the operation mode)
HLE_FORCE("_ZN2nn2oe16GetOperationModeEv", "运行模式查询；按 DM_OPMODE 给固定值", int, h_oe_GetOpMode, void) { int u = g_ui_opmode; if (atomic_exchange(&g_ui_armed, 0)) atomic_fetch_add(&g_ui_reads, 1); return u >= 0 ? u : opmode(); }
HLE_FORCE("_ZN2nn2oe18GetPerformanceModeEv", "性能模式查询；与运行模式一致", int, h_oe_GetPerfMode, void) { return opmode(); }
HLE_FORCE("_ZN2nn2oe27SetPerformanceConfigurationENS0_15PerformanceModeEi", "设 CPU/GPU 频率档；宿主不调频", void, h_oe_SetPerfCfg, int mode, int cfg) {
    LOG("oe::SetPerformanceConfiguration mode=%d cfg=%#x（忽略）", mode, cfg);
}

// 脚本化状态变化（无窗口验证生命周期 / 过热降级）：DM_OE_SCRIPT="focus=3@10,focus=1@20,opmode=0@30"（第 N 秒）
// 时刻从游戏第一次取通知算起；「@ingame+N」= 读取画面第 K 次撤下（K = 前面 @ingame 项数 + 1，首个即进局）后 N 秒（按事件，不受负载影响）。与前端调用并存。
// Scripted state changes (verify lifecycle / overheat downgrade without a window): DM_OE_SCRIPT="focus=3@10,focus=1@20,opmode=0@30" (second N)
// Time counts from the game's first notification fetch; "@ingame+N" = N seconds after the loading screen is taken down for the K-th time (K = number of preceding @ingame items + 1, the first one is run entry) (event-based, not affected by load). Coexists with front end calls.
#include <unistd.h>
static _Atomic int g_ls_unloads;   // 读取画面撤下次数（diag.c 的 UIMSG 处理调 dm_note_loadscreen_unload） | number of times the loading screen was taken down (the UIMSG handler in diag.c calls dm_note_loadscreen_unload)
void dm_note_loadscreen_unload(void) { atomic_fetch_add(&g_ls_unloads, 1); }
static void *oe_script(void *arg) {
    char *buf = arg; uint64_t t0 = now_ns();
    for (char *save, *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        char key[16]; int v; double at;
        if (sscanf(tok, "%15[a-z]=%d@ingame+%lf", key, &v, &at) == 3) {
            static int need;
            need++; while (atomic_load(&g_ls_unloads) < need) usleep(20000);
            usleep((useconds_t)(at * 1e6));
        } else {
            if (sscanf(tok, "%15[a-z]=%d@%lf", key, &v, &at) != 3) die("DM_OE_SCRIPT 项「%s」格式应为 键=值@秒 或 键=值@ingame+秒", tok);
            uint64_t due = t0 + (uint64_t)(at * 1e9), now = now_ns();
            if (due > now) usleep((useconds_t)((due - now) / 1000));
        }
        if (!strcmp(key, "focus")) dm_focus_set(v);
        else if (!strcmp(key, "opmode")) dm_set_opmode(v);
        else die("DM_OE_SCRIPT 未知键「%s」（focus / opmode）", key);
    }
    return NULL;
}
static void oe_script_start(void) {
    const char *e = getenv("DM_OE_SCRIPT"); if (!e) return;
    pthread_t th; if (pthread_create(&th, NULL, oe_script, strdup(e))) die("DM_OE_SCRIPT 线程");
    pthread_detach(th);
}
static pthread_once_t g_oe_once = PTHREAD_ONCE_INIT;

HLE_FORCE("_ZN2nn2oe20GetCurrentFocusStateEv", "前后台状态；由前端 dm_focus_set 驱动", int, h_oe_GetFocus, void) { return g_focus; }
HLE_FORCE("_ZN2nn2oe20SetFocusHandlingModeENS0_17FocusHandlingModeE", "前后台切换策略；游戏设 1（收焦点通知），宿主按通知语义实现", void, h_oe_SetFocusMode, int m) { LOG("oe::SetFocusHandlingMode %d", m); }

// 通知：返回 bool（w0），宿主没有系统通知
// ---------- 3D 内部渲染比例 ----------
// 游戏自带一个静态的 3D 渲染比例：渲染器对象（全局指针 [main+0x31e0848]）成员 +0x66a4（float，原版恒 1.0）。
// 改分辨率函数 main+0x8582f0(输出宽, 输出高) → main+0x1627ef0 按「比例 × 输出宽高」重建 3D 目标（+0xee78 / +0xee7c），
// 界面层与 Final Result 仍按输出尺寸（+0xee88 / +0xee8c）建，末端由游戏自己的放大着色器 ps_upsample_p 放回输出尺寸。
// 启动流程不调 0x8582f0，所以要主动调一次；调用点必须是游戏主线程取系统通知处——游戏自己处理模式切换的同一上下文
// （在出图 HLE 里调会自锁：渲染锁已被出图路径持有）。
// 缺省：iOS 0.75，Mac 宿主 1.0（画面闸门对 Switch 真值比）；DM_3D_SCALE=0.5–1.0 覆盖。
// 视觉尺子真值阶段（DM_NVN_RULER=1 的 A / B）强制 1.0，切到被测模式后才生效。
// 动态比例按上屏帧率与 Android Vulkan GPU 时间共同判瓶颈：配置值 → 0.72 → 0.70 → 0.65 → 0.60 → 0.50；
// GPU 时间不可用时退回帧率规则。每级只缩小内部 3D 目标，UI 与输出尺寸不变。规则见 scale3d_ctl_step。
// Dynamic scaling uses present rate and Android Vulkan GPU time to identify the bottleneck: configured → 0.72 → 0.70 → 0.65 → 0.60 → 0.50;
// if GPU timing is unavailable, it falls back to frame-rate-only control. Each level only shrinks the internal 3D target; UI/output size stay unchanged.
// Notification: returns bool (w0), the host has no system notifications
// ---------- 3D internal render scale ----------
// The game has a static 3D render scale built in: renderer object (global pointer [main+0x31e0848]) member +0x66a4 (float, always 1.0 in the original game).
// The resolution change function main+0x8582f0(output width, output height) → main+0x1627ef0 rebuilds the 3D target as "scale × output width/height" (+0xee78 / +0xee7c),
// the UI layer and Final Result are still built at the output size (+0xee88 / +0xee8c), and at the end the game's own upscale shader ps_upsample_p brings it back to the output size.
// The boot flow does not call 0x8582f0, so it has to be called once actively; the call site must be where the game main thread fetches system notifications -- the same context in which the game itself handles mode switches
// (calling it in the present HLE self-deadlocks: the render lock is already held by the present path).
// Default: iOS 0.75, Mac host 1.0 (the screenshot gate compares against Switch ground truth); DM_3D_SCALE=0.5–1.0 overrides.
// The visual ruler ground truth phase (A / B of DM_NVN_RULER=1) forces 1.0, it only takes effect after switching to the mode under test.
// Dynamic scale (system throttling looks at utilization, thermal state is only a coarse signal): combine present intervals
// with Android Vulkan GPU batch timing. If GPU timing is unavailable, use the frame-rate-only fallback.
// Switches: DM_3D_SCALE_DYN=0 off; DM_THERMAL_NOFALLBACK=1 (power / frame time measurement) also turns it off -- the scale must be pinned during measurement.
// DM_3D_SCALE_DYN_INJECT=start frame-end frame: diagnostic, simulates GPU-bound 25 ms frames and 60 fps / GPU-headroom frames outside that range.
#include <TargetConditionals.h>
#define SCALE3D_PERFORMANCE 0.70f
#define SCALE3D_MID 0.72f
#define SCALE3D_LOW 0.65f
#define SCALE3D_LOWER 0.60f
// 纯函数控制器（单测 t_oe.c 直接喂序列）：level 0 = 配置值，越大越低。
// 窗：每累计 2 秒上屏间隔一个窗；GPU 样本有效时同时统计均值。
// 降：连续两个窗帧率 < 目标 × 0.92，且 GPU 时间 ≥ 目标周期 × 0.80 → 降一级；无 GPU 样本时仅按帧率降级。
// 升：连续 10 个窗帧率 ≥ 目标 × 0.97，且 GPU 时间 ≤ 目标周期 × 0.70 → 升一级；无 GPU 样本时仅按帧率升级。
// 每次换级停判 2 秒；短期反复降级后升档等待 60 秒。
// Pure-function controller (unit test t_oe.c feeds sequences directly): level 0 = configured value; higher levels lower resolution.
// Window: one window per 2 seconds of presented intervals; average valid GPU samples in the same window.
// Lower: two consecutive windows below target × 0.92 and GPU time ≥ 0.80 × target period; frame-rate-only fallback if GPU timing is unavailable.
// Raise: 10 consecutive windows at or above target × 0.97 and GPU time ≤ 0.70 × target period; frame-rate-only fallback if GPU timing is unavailable.
// Suspend judging for 2 seconds after each change; after a quick knock-back, wait 60 seconds before raising again.
typedef struct { int level, max_level; double period_ms, win_gpu_ms; uint64_t hold_until, last_up; double win_ms, last_fps; unsigned win_n, win_gpu_n; int bad_run, good_run, need_good; } scale3d_ctl;
#define S3_SEC 1000000000ull
static void scale3d_ctl_hold(scale3d_ctl *c, uint64_t now, uint64_t ns) {   // 停判一段时间（进局 / 回前台 / 换级之后）：窗与连续计数清零 | suspend judging for a while (after entering a run / returning to foreground / changing level): the window and the consecutive counts are cleared
    if (now + ns > c->hold_until) c->hold_until = now + ns;
    c->win_ms = 0; c->win_n = 0; c->win_gpu_ms = 0; c->win_gpu_n = 0; c->bad_run = c->good_run = 0;
}
static int scale3d_ctl_step(scale3d_ctl *c, double iv_ms, double gpu_ms, uint64_t now) {   // 返回当前级 | returns the current level
    if (now < c->hold_until || iv_ms <= 0) return c->level;   // 停判中 / 无读数（0）/ 被丢弃（−1）| judging suspended / no reading (0) / dropped (−1)
    c->win_ms += iv_ms; c->win_n++;
    if (gpu_ms > 0) { c->win_gpu_ms += gpu_ms; c->win_gpu_n++; }
    if (c->win_ms < 2000) return c->level;
    double period = c->period_ms > 0 ? c->period_ms : 1000.0 / 60;
    double tgt = 1000.0 / period, fps = c->win_n * 1000.0 / c->win_ms;
    int have_gpu = c->win_gpu_n != 0;
    double gpu = have_gpu ? c->win_gpu_ms / c->win_gpu_n : 0;
    c->last_fps = fps; c->win_ms = 0; c->win_n = 0; c->win_gpu_ms = 0; c->win_gpu_n = 0;
    if (fps < tgt * 0.92 && (!have_gpu || gpu >= period * 0.80)) {
        c->good_run = 0;
        if (++c->bad_run >= 2 && c->level < c->max_level) {
            if (c->last_up && now - c->last_up < 10 * S3_SEC) c->need_good = 30;
            c->level++; scale3d_ctl_hold(c, now, 2 * S3_SEC);
        }
    } else {
        c->bad_run = 0;
        if (fps >= tgt * 0.97 && (!have_gpu || gpu <= period * 0.70)) {
            if (++c->good_run >= (c->need_good ? c->need_good : 10) && c->level > 0) {
                c->level--; c->last_up = now; c->need_good = 0; scale3d_ctl_hold(c, now, 2 * S3_SEC);
            }
        } else c->good_run = 0;
    }
    return c->level;
}
// 游戏内菜单固定档位：-1 = 自动，0 高质量 / 1 平衡 / 2 性能 / 3 超性能。定档期间动态控制器不插手，
// 否则它会把玩家选的档又按帧率降下去；切回自动它立刻恢复工作。
// Menu-pinned level: -1 = automatic, 0 high / 1 balanced / 2 performance / 3 ultra performance.
// While pinned, the dynamic controller stays out of the way; selecting automatic resumes it immediately.
#define SCALE3D_BALANCED 0.85f
#define SCALE3D_ULTRA 0.50f
static float scale3d_preset_target(int preset, float configured) {
    float target;
    switch (preset) {
        case 0: target = configured; break;
        case 1: target = SCALE3D_BALANCED; break;
        case 2: target = SCALE3D_PERFORMANCE; break;
        case 3: target = SCALE3D_ULTRA; break;
        default: return configured;
    }
    return target > configured ? configured : target;
}
static float scale3d_auto_target(int level, float configured) {
    float target = level <= 0 ? configured : level == 1 ? SCALE3D_MID : level == 2 ? SCALE3D_PERFORMANCE :
                   level == 3 ? SCALE3D_LOW : level == 4 ? SCALE3D_LOWER : SCALE3D_ULTRA;
    return target > configured ? configured : target;
}
static _Atomic int g_scale3d_forced = -1;
static _Atomic int g_scale3d_auto_min = 50;
static _Atomic int g_scale3d_auto_max = 100;
static _Atomic int g_scale3d_thermal_cap = 100;
void dm_3d_scale_set_preset(int preset) {
    if (preset < -1 || preset > 3) return;
    atomic_store(&g_scale3d_forced, preset);
    LOG("3D 比例档位切到「%s」，下一拍生效", preset < 0 ? "自动" : preset == 0 ? "高质量" : preset == 1 ? "平衡" : preset == 2 ? "性能" : "超性能");
}
void dm_3d_scale_set_auto_bounds(int min_pct, int max_pct) {
    if (min_pct < 50) min_pct = 50;
    if (max_pct > 100) max_pct = 100;
    if (min_pct > max_pct) min_pct = max_pct;
    atomic_store(&g_scale3d_auto_min, min_pct);
    atomic_store(&g_scale3d_auto_max, max_pct);
    LOG("3D 自动范围设为 %d%%–%d%%", min_pct, max_pct);
}
static float scale3d_clamp_auto(float target) {
    int min_pct = atomic_load(&g_scale3d_auto_min);
    int max_pct = atomic_load(&g_scale3d_auto_max);
    int thermal_pct = atomic_load(&g_scale3d_thermal_cap);
    float lo = min_pct / 100.0f;
    float hi = max_pct / 100.0f;
    float tcap = thermal_pct / 100.0f;
    if (tcap < lo) lo = tcap;
    if (tcap < hi) hi = tcap;
    if (target < lo) target = lo;
    if (target > hi) target = hi;
    return target;
}
// 前端可改的出图节拍（帧率目标）：>0 固定帧率，<0 跟随屏幕最高刷新率。
// 运行中可改，下一次出图生效：出图周期、窗口的刷新率请求、3D 比例控制的目标周期一起跟着变。
// The present rate the front end can set: >0 a fixed rate, <0 follow the screen's maximum refresh rate.
// It takes effect on the next present: the present period, the window's refresh-rate request and the dynamic 3D
// controller's target period follow it.
void dm_present_hz_set(int hz) {
    void nvn_present_hz_set(int); int nvn_present_hz(void);
    nvn_present_hz_set(hz);
    if (hz < 0) LOG("出图节拍目标 → 跟随屏幕刷新率（窗口起来后按屏幕的实际刷新率走）");
    else LOG("出图节拍目标 → %d Hz", hz);
}
static _Atomic int g_scale3d_level;
static _Atomic int g_scale3d_resumed;
void dm_3d_scale_resumed(void) { atomic_store(&g_scale3d_resumed, 1); }   // 前端：刚回前台。回来头几秒的慢帧是恢复（表面重建、缓存变冷），不是 GPU 吃紧：停判 3 秒 | front end: just returned to foreground. Slow frames in the first few seconds back are recovery (surface rebuild, caches gone cold), not GPU pressure: suspend judging for 3 seconds
static int scale3d_cpu_aware_gate(int was, int lv, double fps, double gpu_ms, double cpu_ms, double period_ms,
                                  int *last_down_level, double *last_down_fps, double *last_down_gpu) {
    if (lv < was) {
        *last_down_level = -1;
        return lv;
    }
    if (lv > was) {
        // CPU 瓶颈时降低 3D 分辨率无益（只会模糊画面）；仅 GPU 受限才继续降档。
        // Do not lower 3D resolution when CPU-limited (it only blurs without gaining FPS).
        if (cpu_ms > 0 && gpu_ms > 0 && cpu_ms > gpu_ms * 1.15 && gpu_ms < period_ms * 0.85) return was;
        // 上次降档既没带来帧率提升也没明显缩短 GPU，说明降分辨率无益，避免一路糊到最低档。
        // If the previous downstep neither improved FPS nor reduced GPU time, stop blurring further.
        if (was > 0 && *last_down_level == was) {
            int fps_improved = *last_down_fps > 0 && fps > *last_down_fps * 1.03;
            int gpu_improved = gpu_ms > 0 && *last_down_gpu > 0 && gpu_ms < *last_down_gpu * 0.95;
            if (!fps_improved && !gpu_improved) return was;
        }
        *last_down_level = lv;
        *last_down_fps = fps;
        *last_down_gpu = gpu_ms;
    }
    return lv;
}
void dm_3d_scale_feed_load(double iv_ms, double gpu_ms, double cpu_ms) {
    static scale3d_ctl c = {.max_level = 5}; static int on = -1; static long inj0 = -1, inj1 = -1;
    static double win_cpu_ms, win_gpu_latest; static unsigned win_cpu_n;
    static int last_down_level = -1; static double last_down_fps, last_down_gpu;
    static uint64_t last_thermal_ns; static int last_thermal = -1;
    if (on < 0) { const char *e = getenv("DM_3D_SCALE_DYN"), *j = getenv("DM_3D_SCALE_DYN_INJECT"); on = !(e && !strcmp(e, "0")) && !getenv("DM_THERMAL_NOFALLBACK");
        if (j) { inj0 = atol(j); inj1 = strchr(j, '-') ? atol(strchr(j, '-') + 1) : inj0; } }
    if (!on) return;
    if (atomic_load(&g_scale3d_forced) >= 0) return;   // 菜单定档期间动态控制器不插手 | while the menu has a level pinned, the dynamic controller stays out
    if (inj0 >= 0) { long f = (long)dm_nvn_frame_count(); int injected = f >= inj0 && f <= inj1; iv_ms = injected ? 25.0 : 16.7; gpu_ms = injected ? 25.0 : 10.0; }
    uint64_t now = now_ns();
    if (now >= last_thermal_ns + 2 * S3_SEC) {
        extern int dm_thermal_now(void) __attribute__((weak));
        int th = dm_thermal_now ? dm_thermal_now() : -1;
        last_thermal_ns = now;
        if (th >= 0 && th != last_thermal) {
            last_thermal = th;
            int cap = th >= 3 ? 70 : th == 2 ? 85 : 100;
            atomic_store(&g_scale3d_thermal_cap, cap);
            LOG("[thermal] status=%d -> Auto DRS cap=%d%%", th, cap);
        }
    }
    if (atomic_exchange(&g_scale3d_resumed, 0)) { scale3d_ctl_hold(&c, now, 3 * S3_SEC); win_cpu_ms = 0; win_cpu_n = 0; last_down_level = -1; }
    { static int was; int in = dm_ingame_now();   // 不在局内（菜单 / 读取）不判；刚进局的 20 秒也不判（进局头十几秒的慢帧是装载，不是 GPU 吃紧） | not judged outside a run (menu / loading); not judged in the first 20 seconds of a run either (slow frames in the first ten-odd seconds of a run are loading, not GPU pressure)
      if (!in) { was = 0; scale3d_ctl_hold(&c, now, S3_SEC); win_cpu_ms = 0; win_cpu_n = 0; return; }
      if (!was) { was = 1; if (inj0 < 0) scale3d_ctl_hold(&c, now, 20 * S3_SEC); win_cpu_ms = 0; win_cpu_n = 0; last_down_level = -1; } }
    { int mtl_fps_target(void); int nvn_present_hz(void);
      double p = mtl_fps_target() == 30 ? 1000.0 / 30 : 0;
      int hz = nvn_present_hz(); if (hz > 0 && hz != 60) p = 1000.0 / hz;
      if (p != c.period_ms) { c.period_ms = p; scale3d_ctl_hold(&c, now, 2 * S3_SEC); win_cpu_ms = 0; win_cpu_n = 0; last_down_level = -1; } }
    if (now >= c.hold_until && iv_ms > 0) {
        if (cpu_ms > 0) { win_cpu_ms += cpu_ms; win_cpu_n++; }
        if (gpu_ms > 0) win_gpu_latest = gpu_ms;
    }
    int was = c.level, lv = scale3d_ctl_step(&c, iv_ms, gpu_ms, now);
    if (lv != was) {
        double avg_cpu = win_cpu_n ? win_cpu_ms / win_cpu_n : -1;
        double period = c.period_ms > 0 ? c.period_ms : 1000.0 / 60;
        int gated = scale3d_cpu_aware_gate(was, lv, c.last_fps, win_gpu_latest, avg_cpu, period, &last_down_level, &last_down_fps, &last_down_gpu);
        if (gated != lv) {
            c.level = gated;
            LOG("3D 比例控制：抑制无效降档（CPU=%.1fms GPU=%.1fms FPS=%.1f）", avg_cpu, win_gpu_latest, c.last_fps);
            lv = gated;
        } else {
            LOG("3D 比例控制：窗口帧率 %.1f（目标 %.0f）→ %s到第 %d 级%s", c.last_fps, c.period_ms > 0 ? 1000.0 / c.period_ms : 60.0, lv > was ? "降" : "升", lv, c.need_good ? "（刚升又掉：下次升档要 60 秒）" : "");
        }
        win_cpu_ms = 0; win_cpu_n = 0;
    } else if (c.win_n == 0) {
        win_cpu_ms = 0; win_cpu_n = 0;
    }
    atomic_store(&g_scale3d_level, lv);
}
void dm_3d_scale_feed(double iv_ms, double gpu_ms) { dm_3d_scale_feed_load(iv_ms, gpu_ms, -1); }
void dm_3d_scale_hot(int hot) { (void)hot; }   // 过热不再直接降档（改按实际掉帧与温控上限）；保留符号给前端 | retained for frontend compatibility
static _Atomic float g_scale3d_cur = 1.0f;
static _Atomic uint32_t g_scale3d_w3d, g_scale3d_h3d, g_scale3d_wout, g_scale3d_hout;
float dm_3d_scale_cur(void) { return atomic_load(&g_scale3d_cur); }
void dm_3d_render_dims(uint32_t *w3d, uint32_t *h3d, uint32_t *wout, uint32_t *hout) {
    if (w3d) *w3d = atomic_load(&g_scale3d_w3d);
    if (h3d) *h3d = atomic_load(&g_scale3d_h3d);
    if (wout) *wout = atomic_load(&g_scale3d_wout);
    if (hout) *hout = atomic_load(&g_scale3d_hout);
}
static void scale3d_tick(void) {
    static float want_cfg = -1, cur = 1.0f; static module_t *main;
    if (want_cfg < 0) {
        const char *e = getenv("DM_3D_SCALE"); want_cfg = TARGET_OS_IPHONE ? 0.75f : 1.0f;
        if (e) { char *end; float v = strtof(e, &end); if (end == e || *end || !(v >= 0.5f && v <= 1.0f)) die("DM_3D_SCALE 非法：%s（要求 0.5–1.0）", e); want_cfg = v; }
        for (int i = 0; i < g_nmod; i++) if (!strcmp(g_mod[i].name, "main")) main = &g_mod[i];
    }
    if (main && dm_nvn_frame_count() >= 30) {
        uintptr_t renderer = *(uintptr_t *)(main->base + 0x31e0848);
        if (renderer) {
            atomic_store(&g_scale3d_w3d, *(uint32_t *)(renderer + 0xee78));
            atomic_store(&g_scale3d_h3d, *(uint32_t *)(renderer + 0xee7c));
            atomic_store(&g_scale3d_wout, *(uint32_t *)(renderer + 0xee88));
            atomic_store(&g_scale3d_hout, *(uint32_t *)(renderer + 0xee8c));
        }
    }
    int lv = atomic_load(&g_scale3d_level);   // 动态级：0 = 配置值，自动级逐步降至 0.50 | dynamic level: 0 = configured value; automatic levels descend to 0.50
    int forced = atomic_load(&g_scale3d_forced);   // 菜单定档时以定档值为准，动态级不参与 | a menu-pinned level wins over the dynamic one
    float dyn = forced >= 0 ? scale3d_preset_target(forced, want_cfg) : scale3d_clamp_auto(scale3d_auto_target(lv, want_cfg));
    float want = dm_nvn_ruler_truth() ? 1.0f : dyn;
    if (want == cur || !main || dm_nvn_frame_count() < 30) return;
    uintptr_t renderer = *(uintptr_t *)(main->base + 0x31e0848); if (!renderer) return;
    uint32_t ow = *(uint32_t *)(renderer + 0xee88), oh = *(uint32_t *)(renderer + 0xee8c); if (!ow || !oh) return;
    { extern void mtl_rt_force_full(int) __attribute__((weak)); if (mtl_rt_force_full) mtl_rt_force_full(4); }   // 运行时外推：换比例前后几帧一律全画（主通道换尺寸那帧不能外推） | runtime extrapolation: the few frames before and after a scale change are all drawn in full (the frame where the main pass changes size cannot be extrapolated)
    float *scale = (float *)(renderer + 0x66a4);
    dm_guest_host_write_begin(scale, sizeof *scale); *scale = want; dm_guest_host_write_end();
    ((void (*)(uint32_t, uint32_t))(main->base + 0x8582f0))(ow, oh);
    cur = want; atomic_store(&g_scale3d_cur, want);
    atomic_store(&g_scale3d_w3d, *(uint32_t *)(renderer + 0xee78));
    atomic_store(&g_scale3d_h3d, *(uint32_t *)(renderer + 0xee7c));
    atomic_store(&g_scale3d_wout, *(uint32_t *)(renderer + 0xee88));
    atomic_store(&g_scale3d_hout, *(uint32_t *)(renderer + 0xee8c));
    LOG("3D 渲染比例 %.2f：3D 目标 %ux%u，输出 %ux%u（帧 %llu）", want, *(uint32_t *)(renderer + 0xee78), *(uint32_t *)(renderer + 0xee7c),
        *(uint32_t *)(renderer + 0xee88), *(uint32_t *)(renderer + 0xee8c), (unsigned long long)dm_nvn_frame_count());
}
HLE_FORCE("_ZN2nn2oe25TryPopNotificationMessageEPj", "系统通知队列；前端状态变化入队", unsigned, h_oe_TryPopMsg, uint32_t *msg) {
    scale3d_tick();
    pthread_once(&g_oe_once, oe_script_start);
    pthread_mutex_lock(&g_mq_lock);
    int ok = g_mqn > 0;
    if (ok) { *msg = g_mq[g_mqh]; g_mqh = (g_mqh + 1) % MQ; g_mqn--; }
    pthread_mutex_unlock(&g_mq_lock);
    // 分辨率参数位在线程槽里，游戏在取通知的线程上读它们来改出图尺寸（收到 31「性能模式变化」时，main+0xe10ddc 起；30 只更新一个标志）。
    // 读的时机：处理函数每次进来、第一条通知取到之后读一次（0xe10d5c / 0xe10d60），后面循环里再取到的通知用的还是这次读的值。所以每取走一条都把此刻该给的值写进当前线程的槽——
    // 只在取走 31 时写不够：31 排在别的通知（如焦点 15）后面时，参数位在取 15 之后就读过了（iOS 包：前端没切成 16:9，Mac 宿主那局 31 正好排第一）。
    // The resolution parameter bits live in the thread slot, and the game reads them on the thread that fetches notifications to change the render size (on receiving 31 "performance mode changed", from main+0xe10ddc; 30 only updates a flag).
    // When they are read: each time the handler is entered, once after the first notification is fetched (0xe10d5c / 0xe10d60), and notifications fetched later in the loop still use the values from that read. So every time one is taken, the value due at this moment is written into the current thread's slot --
    // writing only when 31 is taken is not enough: when 31 is queued behind another notification (e.g. focus 15), the parameter bits have already been read after fetching 15 (iOS app: the front end did not switch to 16:9, in the Mac host run 31 happened to be queued first).
    if (ok) for (int i = 0; i < 2; i++) if (g_keyres[i]) pthread_setspecific(g_keyres[i], (void *)dm_game_res(i));
    if (ok && *msg == MSG_PERFMODE) dm_uiscreen_res_reread();
    if (ok) LOG("oe 通知被游戏取走：%u", *msg);   // 状态变化很少，逐条打不刷屏 | state changes are rare, printing each one does not flood the log
    return ok;
}
// bool 参数：Switch 调用方不扩展，声明成 unsigned 只看低 8 位
// bool arguments: Switch callers do not extend them, declared as unsigned and only the low 8 bits are looked at
HLE_FORCE("_ZN2nn2oe28SetResumeNotificationEnabledEb", "恢复通知开关；宿主的恢复以焦点通知 15 表达", void, h_oe_SetResumeNtf, unsigned on) { (void)on; }
HLE_FORCE("_ZN2nn2oe42SetOperationModeChangedNotificationEnabledEb", "运行模式变化通知开关", void, h_oe_SetOpModeNtf, unsigned on) { g_ntf_opmode = on & 0xff; }
HLE_FORCE("_ZN2nn2oe44SetPerformanceModeChangedNotificationEnabledEb", "性能模式变化通知开关", void, h_oe_SetPerfModeNtf, unsigned on) { g_ntf_perfmode = on & 0xff; }

// nn::settings::LanguageCode = struct { char string[8]; }，8 字节按值返回在 x0
// nn::settings::LanguageCode = struct { char string[8]; }, 8 bytes returned by value in x0
HLE_FORCE("_ZN2nn2oe18GetDesiredLanguageEv", "系统语言查询；按 DM_LANG 给值", uint64_t, h_oe_GetLang, void) {
    const char *e = getenv("DM_LANG");
    char code[8] = {0};
    strncpy(code, e ? e : "zh-Hans", sizeof code - 1);
    uint64_t v; memcpy(&v, code, 8);
    return v;
}

// nn::util::Uuid 16 字节：本机固定值（只用于本地区分设备，联网已剪）
// nn::util::Uuid 16 bytes: a fixed local value (only used to tell devices apart locally, networking is cut)
HLE_FORCE("_ZN2nn2oe17GetPseudoDeviceIdEPNS_4util4UuidE", "伪设备号；宿主固定值", void, h_oe_GetDevId, uint8_t *uuid) {
    static const uint8_t k[16] = {0x78, 0x62, 0x6c, 0x00, 0x53, 0x57, 0x4e, 0x41, 0x54, 0x49, 0x56, 0x45, 0x31, 0x30, 0x30, 0x31};
    memcpy(uuid, k, 16);
}
