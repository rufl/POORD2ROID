// 每批独立命令/查询/fence；GPU 完成后按 FIFO 公布客体内存，再释放资源。
// Independent commands/queries/fences per batch; publish guest memory in FIFO order after GPU completion, then release resources.
// 异步环的槽位数决定 CPU 能领先 GPU 多少批次：槽位不够时 async_begin_locked 会阻塞等 GPU 腾出槽位。
// 槽位数 = CPU 可领先的绘制数上限（槽位 × 每批绘制阈值），太浅会让逐绘制录制与 GPU 执行退化成串行。
// The ring's slot count bounds how many batches the CPU may run ahead of the GPU; when slots run out,
// async_begin_locked blocks until the GPU retires one. Slots × per-batch draw threshold = the draw lookahead.
#ifndef DM_VK_BATCH_SLOTS
#define DM_VK_BATCH_SLOTS 8
#endif

enum {ASYNC_FREE,ASYNC_RECORDING,ASYNC_PENDING};
// The full FIFO chain remains authoritative. A page Bloom filter rejects impossible overlaps only; ranges too
// large to represent still force the original scan, and every possible hit retains the exact chain walk.
// 每段至少 32 draw 且录制 2 ms，再在自然通道边界提交；每批最多八段。
// Submit at natural pass boundaries after at least 32 draws and 2 ms of recording; cap each batch at eight chunks.
enum {ASYNC_CHUNK_CAPACITY=DM_VK_MAX_CHUNKS};
static unsigned async_chunk_min_draws(void) {
    static int value=-1;
    if(value<0){
        const char *env=getenv("DM_VK_CHUNK_DRAWS");
        int parsed=env&&*env?(int)strtol(env,NULL,10):perf_config_int("chunk_draws",32);
        if(parsed<16)parsed=16;if(parsed>256)parsed=256;
        value=parsed;
    }
    return (unsigned)value;
}
static uint64_t async_chunk_record_ns(void) {
    static int64_t value=-1;
    if(value<0){
        const char *env=getenv("DM_VK_CHUNK_MS");
        int parsed=env&&*env?(int)strtol(env,NULL,10):perf_config_int("chunk_ms",2);
        if(parsed<0)parsed=0;if(parsed>16)parsed=16;
        value=(int64_t)parsed*1000000ll;
    }
    return (uint64_t)value;
}
enum { ASYNC_BLOOM_WORDS=128, ASYNC_BLOOM_BITS=ASYNC_BLOOM_WORDS*64, ASYNC_BLOOM_MAX_SIZE=8u<<20 };
enum {ASYNC_QUERY_CAPACITY=1024,ASYNC_TIMESTAMP_BYTES=2*ASYNC_CHUNK_CAPACITY*sizeof(uint64_t),
      ASYNC_QUERY_READBACK_BYTES=ASYNC_TIMESTAMP_BYTES+ASYNC_QUERY_CAPACITY*sizeof(uint64_t)};
typedef struct async_batch {
    VkCommandBuffer command;
    VkCommandBuffer chunks[ASYNC_CHUNK_CAPACITY];
    unsigned chunk_count,chunk_index,chunk_draws,chunk_timestamps;
    uint64_t chunk_started;
    VkFence fence;
    VkQueryPool queries,times,counter_queries;
    uint32_t counter_used;
    vk_buffer query_readback;
    post_op *posts,*copies;
    dm_pending_ranges pending_ranges;
    uint64_t guest_frame;
    screen_submit_kind screen_kind;
    uint64_t ticket,temporary_base;
    uintptr_t known_copy_low,known_copy_high;
    uintptr_t copy_low,copy_high;
    uintptr_t pending_low,pending_high;   // 区间索引登记范围的包围盒（保守超集）
    uint64_t known_copy_bloom[ASYNC_BLOOM_WORDS],copy_bloom[ASYNC_BLOOM_WORDS],pending_bloom[ASYNC_BLOOM_WORDS];
    uint32_t query_count;
    int host_sync,has_known_cpu_copy,known_copy_unbounded,known_copy_bloom_unbounded;
    int has_any_copy,copy_unbounded,copy_bloom_unbounded;
    int status,pending_any,pending_bloom_unbounded;
    struct async_batch *next;
} async_batch;
static async_batch async_batches[DM_VK_BATCH_SLOTS];
static async_batch *async_first,*async_last,*async_current;
static pthread_cond_t async_condition=PTHREAD_COND_INITIALIZER;
static pthread_t async_thread;
static unsigned async_next_slot;
static int async_initialized,async_stopping;
static uint64_t async_ring_stalls;
static uint64_t async_early_submits,async_logical_submits,async_early_submit_ns;
static void async_chunk_report(void) {
    static uint64_t frame,early,logical,submit_ns;
    if(!presents||presents%60||presents==frame)return;
    uint64_t span=presents-frame;
    LOG("[vulkan-chunks] frame=%llu span=%llu early=%.2f/frame batches=%.2f/frame early_submit_wall=%.4f ms/frame max_chunks=%u min_draws=%u record_budget_ms=%.1f gpu_timestamp=per_chunk_sum",
        (unsigned long long)presents,(unsigned long long)span,(async_early_submits-early)/(double)span,
        (async_logical_submits-logical)/(double)span,(async_early_submit_ns-submit_ns)/(span*1e6),
        ASYNC_CHUNK_CAPACITY,async_chunk_min_draws(),async_chunk_record_ns()/1e6);
    frame=presents;early=async_early_submits;logical=async_logical_submits;submit_ns=async_early_submit_ns;
}
// 仅绘制前的自然通道边界调用；不回收、不发布，也不重置批次资源或查询。
// Called only before a draw at a natural pass boundary; never retire, publish, or reset batch resources/queries.
static void async_chunk_boundary_locked(void) {
    async_batch *batch=async_current;
    uint64_t record_ns=async_chunk_record_ns();
    if(!recording||!batch||batch->status!=ASYNC_RECORDING)die("Vulkan chunk without recording batch");
    if(!record_ns||screen_submission||batch->chunk_index+1>=ASYNC_CHUNK_CAPACITY||batch_draws-batch->chunk_draws<async_chunk_min_draws())return;
    uint64_t now=dm_android_clock_ns(CLOCK_MONOTONIC);
    if(!now||!batch->chunk_started||now<batch->chunk_started||now-batch->chunk_started<record_ns)return;
    if(batch->times)vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,batch->times,2*batch->chunk_index+1);
    VK_CHECK(vkEndCommandBuffer(command));
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&command};
    // 同队列最终 fence 覆盖此前各段；客体 ticket 和 WSI 信号仍只在最终提交公布。
    // The final same-queue fence covers prior chunks; guest tickets and WSI signals remain exclusive to final submission.
    pthread_mutex_lock(&vk_queue_lock);VK_CHECK(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE));pthread_mutex_unlock(&vk_queue_lock);
    uint64_t submitted_at=dm_android_clock_ns(CLOCK_MONOTONIC);
    if(submitted_at>=now)async_early_submit_ns+=submitted_at-now;
    async_early_submits++;batch->chunk_index++;
    if(batch->chunk_index==batch->chunk_count){
        VkCommandBufferAllocateInfo allocation={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=commands,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
        VK_CHECK(vkAllocateCommandBuffers(device,&allocation,&batch->chunks[batch->chunk_count]));batch->chunk_count++;
    }
    command=batch->command=batch->chunks[batch->chunk_index];
    VK_CHECK(vkResetCommandBuffer(command,0));
    VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VK_CHECK(vkBeginCommandBuffer(command,&begin));graphics_command_reset();
    if(batch->times){vkCmdResetQueryPool(command,batch->times,2*batch->chunk_index,2);vkCmdWriteTimestamp(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,batch->times,2*batch->chunk_index);}
    batch->chunk_draws=batch_draws;batch->chunk_started=dm_android_clock_ns(CLOCK_MONOTONIC);
}
#pragma weak nvn_dirty_publish_copy
#pragma weak nvn_dirty_publish_copy_get_stats
static uint64_t post0_calls,post0_bytes,post0_fallback_calls,post0_fallback_bytes;
static uint64_t known_copy_filter_calls,known_copy_filter_negative;
static void post0_publish_report(void) {
    static uint64_t frame,calls,bytes,fallback_calls,fallback_bytes;static nvn_dirty_publish_copy_stats previous;
    static uint64_t filter_calls,filter_negative;
    if(!presents||presents%60||presents==frame)return;
    int saved_errno=errno;uint64_t span=presents-frame;nvn_dirty_publish_copy_stats stats={0};
    if(nvn_dirty_publish_copy_get_stats)nvn_dirty_publish_copy_get_stats(&stats);
    // 批量 RW 是原 mprotect 总量的子集；拒绝前已完成的权限操作也如实保留，不能相加。
    // Batched RW is a subset of total mprotect work; include permission work preceding rejection and never add the two totals.
    LOG("[vulkan-post0-publish] frame=%llu span=%llu api=%u stats_api=%u calls=%.2f/frame bytes=%.4f MiB/frame helper_calls=%.2f/frame helper_bytes=%.4f MiB/frame protected_pages=%.2f/frame rw_calls=%.2f/frame rw_wall=%.4f ms/frame rw_wall_status=%s rw_clock_unavailable=%llu helper_fallbacks=%llu fallback_calls=%.2f/frame fallback_bytes=%.4f MiB/frame mprotect_total_subset=1",
        (unsigned long long)presents,(unsigned long long)span,(unsigned)(nvn_dirty_publish_copy!=NULL),(unsigned)(nvn_dirty_publish_copy_get_stats!=NULL),
        (post0_calls-calls)/(double)span,(post0_bytes-bytes)/(span*1048576.),(stats.calls-previous.calls)/(double)span,(stats.bytes-previous.bytes)/(span*1048576.),
        (stats.protected_pages-previous.protected_pages)/(double)span,(stats.rw_calls-previous.rw_calls)/(double)span,(stats.rw_ns-previous.rw_ns)/(span*1e6),
        stats.rw_clock_unavailable==previous.rw_clock_unavailable?"complete":"partial",(unsigned long long)(stats.rw_clock_unavailable-previous.rw_clock_unavailable),
        (unsigned long long)(stats.fallback_calls-previous.fallback_calls),(post0_fallback_calls-fallback_calls)/(double)span,(post0_fallback_bytes-fallback_bytes)/(span*1048576.));
    uint64_t checked=known_copy_filter_calls-filter_calls,negative=known_copy_filter_negative-filter_negative;
    LOG("[vulkan-known-copy-filter] frame=%llu span=%llu calls=%.2f/frame negative=%.2f/frame original_scan=%.2f/frame negative_pct=%.2f (conservative bounds/page rejection only)",
        (unsigned long long)presents,(unsigned long long)span,checked/(double)span,negative/(double)span,(checked-negative)/(double)span,checked?negative*100.0/checked:0);
    filter_calls=known_copy_filter_calls;filter_negative=known_copy_filter_negative;
    frame=presents;calls=post0_calls;bytes=post0_bytes;fallback_calls=post0_fallback_calls;fallback_bytes=post0_fallback_bytes;previous=stats;errno=saved_errno;
}
static uint64_t snapshot_known_copy_hits,snapshot_known_copy_read_saved,snapshot_known_copy_overlay_saved,snapshot_known_copy_final_bytes;
static uint64_t pending_index_queries[2],pending_index_nodes[2],pending_index_empty[2],pending_index_registered[2],pending_index_peak_records;
#ifdef DM_VK_REFERENCE_GATES
static uint64_t pending_index_oracle_queries,pending_index_oracle_nodes;
#endif

// 字段完成才登记；未知输出在 encoded 之前已可见，已知 CPU copy 仍保留原发布链。
// Register only completed fields; unknown outputs are visible before encoding, while known CPU copies retain the original publication chain.
static inline unsigned async_page_bit1(uint64_t page) {
    uint64_t h = page * 0x9e3779b97f4a7c15ull; h ^= h >> 30; return (unsigned)(h & (ASYNC_BLOOM_BITS-1));
}
static inline unsigned async_page_bit2(uint64_t page) {
    uint64_t h = page * 0xbf58476d1ce4e5b9ull; h ^= h >> 27; return (unsigned)(h & (ASYNC_BLOOM_BITS-1));
}
static void async_bloom_mark(uint64_t bloom[ASYNC_BLOOM_WORDS],int *unbounded,uintptr_t start,uint64_t bytes) {
    if(*unbounded||!bytes)return;
    if(bytes>ASYNC_BLOOM_MAX_SIZE||bytes>UINTPTR_MAX-start){*unbounded=1;return;}
    uint64_t first=start>>12,last=(start+bytes-1)>>12;
    for(uint64_t page=first;page<=last;page++){
        unsigned b1=async_page_bit1(page),b2=async_page_bit2(page);
        bloom[b1>>6]|=1ull<<(b1&63u);
        bloom[b2>>6]|=1ull<<(b2&63u);
    }
}
static int async_bloom_may_hit(const uint64_t bloom[ASYNC_BLOOM_WORDS],int unbounded,uintptr_t address,uint64_t size) {
    if(unbounded||size>ASYNC_BLOOM_MAX_SIZE)return 1;
    uint64_t first=address>>12,last=(address+size-1)>>12;
    for(uint64_t page=first;page<=last;page++){
        unsigned b1=async_page_bit1(page),b2=async_page_bit2(page);
        if(((bloom[b1>>6]>>(b1&63u))&1u)&&((bloom[b2>>6]>>(b2&63u))&1u))return 1;
    }
    return 0;
}
static void post_register_range(post_op *p) {
    if(!recording||!async_current||!p||!p->order||p->range_registered)die("Vulkan pending range registration state");
    dm_pending_kind kind;uintptr_t address;uint64_t bytes;
    if(p->kind==0&&!p->source.buffer&&p->destination&&p->size){kind=DM_PENDING_UNKNOWN;address=(uintptr_t)p->destination;bytes=p->size;}
    else if(p->kind==1){kind=DM_PENDING_HOST_EFFECT;address=(uintptr_t)p->event;bytes=4;}
    else if(p->kind==2){kind=DM_PENDING_HOST_EFFECT;address=(uintptr_t)p->destination;bytes=16;}
    else{if(p->kind!=0)die("Vulkan pending range unexpected post kind");
        if(p->source.buffer&&p->destination&&p->size){
            async_batch *batch=async_current;uintptr_t start=(uintptr_t)p->destination;
            // 无法表示的范围只允许原扫描；摘要永远不能批准一次复制。
            // Unrepresentable ranges force the original scan; summaries never authorize a copy.
            if(p->size>UINTPTR_MAX-start){batch->known_copy_unbounded=1;batch->copy_unbounded=1;}
            else{
                uintptr_t end=start+p->size;
                if(!batch->has_any_copy){batch->copy_low=start;batch->copy_high=end;}
                else{if(start<batch->copy_low)batch->copy_low=start;if(end>batch->copy_high)batch->copy_high=end;}
                async_bloom_mark(batch->copy_bloom,&batch->copy_bloom_unbounded,start,p->size);
                if(!batch->known_copy_unbounded){
                    if(!batch->has_known_cpu_copy){batch->known_copy_low=start;batch->known_copy_high=end;}
                    else{if(start<batch->known_copy_low)batch->known_copy_low=start;if(end>batch->known_copy_high)batch->known_copy_high=end;}
                }
                async_bloom_mark(batch->known_copy_bloom,&batch->known_copy_bloom_unbounded,start,p->size);
            }
            batch->has_any_copy=1;batch->has_known_cpu_copy=1;
        }
        p->range_registered=1;return;}
    if(dm_pending_ranges_insert(&async_current->pending_ranges,kind,address,bytes,p->order)!=1)die("Vulkan pending range insert: %s",strerror(errno));
    // 登记范围的包围盒与页过滤器：不相交时跳过每绘制树查询，不漏判任何重叠。
    // Bounding box + page filter of registered ranges: disjoint queries skip per-draw tree lookups with zero false negatives.
    {async_batch *b=async_current;
     if(p->kind==0&&p->destination&&p->size){
         if(p->size>UINTPTR_MAX-(uintptr_t)p->destination)b->copy_unbounded=1;
         else{
             uintptr_t end=(uintptr_t)p->destination+p->size;
             if(!b->has_any_copy){b->copy_low=(uintptr_t)p->destination;b->copy_high=end;}
             else{if((uintptr_t)p->destination<b->copy_low)b->copy_low=(uintptr_t)p->destination;if(end>b->copy_high)b->copy_high=end;}
             async_bloom_mark(b->copy_bloom,&b->copy_bloom_unbounded,(uintptr_t)p->destination,p->size);
         }
         b->has_any_copy=1;
     }
     if(!b->pending_any){b->pending_any=1;b->pending_low=address;b->pending_high=address+bytes;}
     else{if(address<b->pending_low)b->pending_low=address;if(address+bytes>b->pending_high)b->pending_high=address+bytes;}
     async_bloom_mark(b->pending_bloom,&b->pending_bloom_unbounded,address,bytes);}
    p->range_registered=1;pending_index_registered[kind]++;
    uint64_t records=async_current->pending_ranges.collections[0].count+async_current->pending_ranges.collections[1].count;
    if(records>pending_index_peak_records)pending_index_peak_records=records;
}
static int pending_range_overlap(const async_batch *batch,dm_pending_kind kind,uintptr_t address,uint64_t size,uint64_t before) {
    if(!batch)return 0;size_t visited=0;
    int result=dm_pending_ranges_overlap(&batch->pending_ranges,kind,address,size,before,&visited);
    if(result<0)die("Vulkan pending range query: %s",strerror(errno));
    pending_index_queries[kind]++;pending_index_nodes[kind]+=visited;pending_index_empty[kind]+=!result;return result;
}
static void pending_index_report(void) {
    static uint64_t frame,queries[2],nodes[2],empty[2],registered[2];
    if(!presents||presents%60||presents==frame)return;uint64_t frames=presents-frame,retained=0;
    for(unsigned i=0;i<DM_VK_BATCH_SLOTS;i++)retained+=async_batches[i].pending_ranges.retained_bytes;
    double q0=pending_index_queries[0]-queries[0],q1=pending_index_queries[1]-queries[1];
    LOG("[vulkan-pending-index] frame=%llu unknown/host_queries=%.1f/%.1f per_frame tree_nodes/query=%.2f/%.2f no_overlap=%.1f/%.1f%% inserted=%.1f/%.1f per_frame peak_batch_records=%llu retained_heap=%.1f KiB",(unsigned long long)presents,
        q0/frames,q1/frames,q0?(pending_index_nodes[0]-nodes[0])/q0:0,q1?(pending_index_nodes[1]-nodes[1])/q1:0,
        q0?(pending_index_empty[0]-empty[0])*100/q0:0,q1?(pending_index_empty[1]-empty[1])*100/q1:0,
        (pending_index_registered[0]-registered[0])/(double)frames,(pending_index_registered[1]-registered[1])/(double)frames,(unsigned long long)pending_index_peak_records,retained/1024.);
    frame=presents;for(unsigned i=0;i<2;i++){queries[i]=pending_index_queries[i];nodes[i]=pending_index_nodes[i];empty[i]=pending_index_empty[i];registered[i]=pending_index_registered[i];}
}

static int async_overlap(uintptr_t address,uint64_t size,uintptr_t start,uint64_t bytes) {
    if(!size||!bytes)return 0;
    if(size>UINTPTR_MAX-address||bytes>UINTPTR_MAX-start)die("Vulkan asynchronous visibility range overflow");
    return address<start+bytes&&start<address+size;
}

// 所有 metadata 访问与完成回收均持 queue_lock；录制中不能释放在途复制快照。
// Metadata access and retirement hold queue_lock; in-flight copy snapshots cannot be released during recording.
static void async_overlay_chain(const post_op *copies,uintptr_t address,void *destination,uint64_t size) {
    for(const post_op *p=copies;p;p=p->copy_next)if(p->destination&&p->source.buffer&&async_overlap(address,size,(uintptr_t)p->destination,p->size)){
        uintptr_t start=(uintptr_t)p->destination,low=address>start?address:start;
        uintptr_t high=address+size<start+p->size?address+size:start+p->size;
        memcpy((uint8_t*)destination+low-address,(const uint8_t*)p->source.mapped+low-start,high-low);
    }
}

static int async_any_copy_may_overlap(const async_batch *batch,uintptr_t address,uint64_t size) {
    if(!batch||!batch->has_any_copy)return 0;
    if(!batch->copy_unbounded&&(address>=batch->copy_high||batch->copy_low>=address+size))return 0;
    return async_bloom_may_hit(batch->copy_bloom,batch->copy_bloom_unbounded,address,size);
}
static int async_pending_may_overlap(const async_batch *batch,uintptr_t address,uint64_t size) {
    if(!batch||!batch->pending_any)return 0;
    if(address+size<=batch->pending_low||address>=batch->pending_high)return 0;
    return async_bloom_may_hit(batch->pending_bloom,batch->pending_bloom_unbounded,address,size);
}
static int async_known_copy_may_overlap(const async_batch *batch,uintptr_t address,uint64_t size);
static void async_overlay_copies(uintptr_t address,void *destination,uint64_t size) {
    if(!size||size>UINTPTR_MAX-address)return;
    for(const async_batch *b=async_first;b;b=b->next)if(async_known_copy_may_overlap(b,address,size))async_overlay_chain(b->copies,address,destination,size);
    if(async_known_copy_may_overlap(async_current,address,size))async_overlay_chain(post_copy_first,address,destination,size);
}

static const post_op *async_version_chain(const post_op *copies,uintptr_t address,uint64_t size,const post_op *version) {
    for(const post_op *p=copies;p;p=p->copy_next)
        if(p->destination&&p->source.buffer&&async_overlap(address,size,(uintptr_t)p->destination,p->size))version=p;
    return version;
}

static const post_op *async_copy_version(uintptr_t address,uint64_t size) {
    if(!size||size>UINTPTR_MAX-address)return NULL;
    const post_op *version=NULL;
    for(const async_batch *b=async_first;b;b=b->next)if(async_known_copy_may_overlap(b,address,size))version=async_version_chain(b->copies,address,size,version);
    if(async_known_copy_may_overlap(async_current,address,size))version=async_version_chain(post_copy_first,address,size,version);
    return version;
}

static int async_copy_pending(uintptr_t address,uint64_t size) {
    return async_copy_version(address,size)!=NULL;
}

#ifdef DM_VK_REFERENCE_GATES
// 专用真实 GPU 回归同时保留原链 oracle；游戏不承担额外扫描或关闭同步的开关。
// Real-GPU regressions retain the original-chain oracle; games have neither extra scans nor a switch that disables synchronization.
static int async_unknown_chain(const post_op *copies,const post_op *posts,int host_sync,uintptr_t address,uint64_t size) {
    for(const post_op *p=copies;p;p=p->copy_next){pending_index_oracle_nodes++;
        if(!p->source.buffer&&p->destination&&async_overlap(address,size,(uintptr_t)p->destination,p->size))return 1;
    }
    if(host_sync)for(const post_op *p=posts;p;p=p->next){
        pending_index_oracle_nodes++;
        if(p->kind==1&&async_overlap(address,size,(uintptr_t)p->event,4))return 1;
        if(p->kind==2&&async_overlap(address,size,(uintptr_t)p->destination,16))return 1;
    }
    return 0;
}
#endif
static int pending_batch_unknown(const async_batch *batch,uintptr_t address,uint64_t size) {
    if(!batch)return 0;
    // 包围盒快路：查询区间不与登记范围的盒子重叠，就一定不与任何登记重叠。
    // 盒子是保守超集，所以跳过树查询不会漏判；没有登记的批次直接答否。
    // Bounding-box fast path: a query disjoint from the box of the registered ranges is disjoint from
    // every one of them. The box is a conservative superset, so skipping the tree cannot miss an overlap.
    int may_overlap=async_pending_may_overlap(batch,address,size);
    int result=may_overlap&&(pending_range_overlap(batch,DM_PENDING_UNKNOWN,address,size,DM_PENDING_ORDER_ALL)||pending_range_overlap(batch,DM_PENDING_HOST_EFFECT,address,size,DM_PENDING_ORDER_ALL));
#ifdef DM_VK_REFERENCE_GATES
    const post_op *copies=batch==async_current?post_copy_first:batch->copies,*posts=batch==async_current?post_first:batch->posts;
    int host_sync=batch==async_current?pending_host_sync:batch->host_sync;
    int reference=async_unknown_chain(copies,posts,host_sync,address,size);pending_index_oracle_queries++;
    if(reference!=result)die("Vulkan pending index/unknown chain mismatch range=%#llx+%llu ticket=%llu",(unsigned long long)address,(unsigned long long)size,(unsigned long long)batch->ticket);
#endif
    return result;
}

static int async_unknown(uintptr_t address,uint64_t size) {
    if(size>UINTPTR_MAX-address)die("Vulkan asynchronous read range overflow");
    for(const async_batch *b=async_first;b;b=b->next)
        if(pending_batch_unknown(b,address,size))return 1;
    return pending_batch_unknown(async_current,address,size);
}

typedef struct known_copy_read {
    const post_op *latest;
    uint64_t overlay_bytes;
    uint64_t source_offset;
} known_copy_read;
static int async_known_copy_chain(const post_op *copies,const post_op *posts,int host_sync,uintptr_t address,uint64_t size,known_copy_read *read) {
    for(const post_op *p=copies;p;p=p->copy_next){
        if(!p->destination||!p->size)continue;uintptr_t start=(uintptr_t)p->destination;
        if(p->size>UINTPTR_MAX-start)return 0;
        if(address>=start+p->size||start>=address+size)continue;
        // 任一重叠 GPU/未知写入都回原路径；更晚的完整 CPU 复制也不取消真实依赖预检。
        // Any overlapping GPU/unknown write retains the original path; a later full CPU copy never replaces real dependency preflight.
        if(!p->source.buffer)return 0;
        uintptr_t low=address>start?address:start,high=address+size<start+p->size?address+size:start+p->size;
        if(high-low>UINT64_MAX-read->overlay_bytes)return 0;
        read->overlay_bytes+=high-low;read->latest=p;
    }
    if(host_sync)for(const post_op *p=posts;p;p=p->next){
        uintptr_t start;uint64_t bytes;
        if(p->kind==1){start=(uintptr_t)p->event;bytes=4;}
        else if(p->kind==2){start=(uintptr_t)p->destination;bytes=16;}
        else continue;
        if(bytes>UINTPTR_MAX-start||(address<start+bytes&&start<address+size))return 0;
    }
    return 1;
}
// 请求范围已检查无溢出；此处只作负筛选，不回答覆盖、版本或 FIFO 可见性。
// The request is already overflow-checked; only reject impossible overlap, never infer coverage, version or FIFO visibility.
static int async_known_copy_may_overlap(const async_batch *batch,uintptr_t address,uint64_t size) {
    if(!batch||!batch->has_known_cpu_copy)return 0;
    if(!batch->known_copy_unbounded&&(address>=batch->known_copy_high||batch->known_copy_low>=address+size))return 0;
    return async_bloom_may_hit(batch->known_copy_bloom,batch->known_copy_bloom_unbounded,address,size);
}
static int async_known_full_copy(uintptr_t address,uint64_t size,known_copy_read *out) {
    if(!out||!address||!size||size>UINTPTR_MAX-address)return 0;
    *out=(known_copy_read){0};
    known_copy_filter_calls++;
    int may_overlap=async_known_copy_may_overlap(async_current,address,size);
    for(const async_batch *b=async_first;!may_overlap&&b;b=b->next)may_overlap=async_known_copy_may_overlap(b,address,size);
    if(!may_overlap){known_copy_filter_negative++;return 0;}
    known_copy_read read={0};
    for(const async_batch *b=async_first;b;b=b->next)
        if(!async_known_copy_chain(b->copies,b->posts,b->host_sync,address,size,&read))return 0;
    if(!async_known_copy_chain(post_copy_first,post_first,pending_host_sync,address,size,&read)||!read.latest)return 0;
    const post_op *p=read.latest;uintptr_t start=(uintptr_t)p->destination;
    // 最新部分覆盖不能退回旧 full；合法非零偏移仅在实际 source 与 post 两个范围都包含时使用。
    // A latest partial write must never fall back to an old full one; validate nonzero offsets against both post and actual source bounds.
    if(address<start||size>p->size||address-start>p->size-size||!p->source.mapped||!p->source.owner||
        !p->source.owner->activation||size>p->source.size||address-start>p->source.size-size||read.overlay_bytes<size)return 0;
    read.source_offset=address-start;*out=read;return 1;
}
static void async_account_known_full_copy(const known_copy_read *read,uint64_t size) {
    snapshot_known_copy_hits++;snapshot_known_copy_read_saved+=size;
    snapshot_known_copy_overlay_saved+=read->overlay_bytes-size;snapshot_known_copy_final_bytes+=size;
}
static int async_read_known_full_copy(uintptr_t address,void *destination,uint64_t size) {
    // queue_lock 保证复制源仍不可变且未退役；只省客体整段读取和旧覆盖，不改变发布顺序。
    // queue_lock keeps the immutable copy source alive; avoid only the base read/old overlays, without changing publication order.
    if(!destination)return 0;
    known_copy_read read;
    if(!async_known_full_copy(address,size,&read))return 0;
    memcpy(destination,(const uint8_t*)read.latest->source.mapped+read.source_offset,size);
    async_account_known_full_copy(&read,size);return 1;
}

#ifdef DM_VK_REFERENCE_GATES
static int async_chain_has_unknown(const post_op *copies,int host_sync) {
    if(host_sync)return 1;
    for(const post_op *p=copies;p;p=p->copy_next)if(!p->source.buffer&&p->destination&&p->size)return 1;
    return 0;
}
#endif
static int pending_batch_has_unknown(const async_batch *batch) {
    if(!batch)return 0;int result=batch->pending_ranges.collections[0].count||batch->pending_ranges.collections[1].count;
#ifdef DM_VK_REFERENCE_GATES
    const post_op *copies=batch==async_current?post_copy_first:batch->copies;
    int host_sync=batch==async_current?pending_host_sync:batch->host_sync;
    if(result!=async_chain_has_unknown(copies,host_sync))die("Vulkan pending index/has-unknown chain mismatch");
#endif
    return result;
}

static int async_have_unknown(void) {
    for(const async_batch *b=async_first;b;b=b->next)if(pending_batch_has_unknown(b))return 1;
    return pending_batch_has_unknown(async_current);
}

static uint64_t async_batch_temporary_bytes(void) {
    uint64_t temporary=active_buffer_bytes-read_cache_bytes-immutable_buffer_bytes;
    return async_current&&temporary>async_current->temporary_base?temporary-async_current->temporary_base:0;
}

static void async_publish_locked(async_batch *batch) {
    uint64_t started=dm_android_clock_ns(CLOCK_MONOTONIC);
    if(batch->times&&batch->chunk_timestamps){
        uint64_t times[2*ASYNC_CHUNK_CAPACITY],batch_gpu_ns=0;
        memcpy(times,batch->query_readback.mapped,2*batch->chunk_timestamps*sizeof(uint64_t));
        for(unsigned c=0;c<batch->chunk_timestamps;c++)
            batch_gpu_ns+=(uint64_t)(timestamp_delta(times[2*c],times[2*c+1],gpu_timestamp_bits)*gpu_timestamp_period);
        gpu_total_ns+=batch_gpu_ns;
        gpu_submit_spans[batch->screen_kind].ns += batch_gpu_ns;
        gpu_submit_spans[batch->screen_kind].count++;
        gpu_frame_entry *gfe=&gpu_frame_ring[batch->guest_frame&15u];
        if(!gfe->valid||gfe->frame!=batch->guest_frame){gfe->valid=1;gfe->frame=batch->guest_frame;gfe->ns=batch_gpu_ns;}
        else gfe->ns+=batch_gpu_ns;
    }
    if(batch->query_count)memcpy(query_counts,(const uint8_t*)batch->query_readback.mapped+ASYNC_TIMESTAMP_BYTES,batch->query_count*sizeof *query_counts);
    graphics_probe_query_batch(batch->ticket,query_counts,batch->query_count);
    for(post_op *p=batch->posts,*next;p;p=next){
        next=p->next;
        if(p->kind==0){
            // 真 fence 后仅借用完整 CPU-ready owned 源；helper=0 尚未复制，严格保留原发布顺序。
            // After the real fence, borrow only a complete CPU-ready owned source; helper=0 copied nothing, preserving original publication order.
            post0_calls++;post0_bytes+=p->size;const vk_buffer *output=&p->output;int copied=0;
            if(nvn_dirty_publish_copy&&output->buffer&&output->memory&&output->mapped&&output->owner&&output->owner->activation&&atomic_load(&output->owner->references)&&
                p->size<=output->size&&output->size<=output->capacity&&p->size<=SIZE_MAX&&p->size<=UINTPTR_MAX-(uintptr_t)output->mapped)
                copied=nvn_dirty_publish_copy(p->destination,output->mapped,(size_t)p->size);
            if(copied!=0&&copied!=1)die("Vulkan post0 publish-copy contract: %d",copied);
            if(!copied){post0_fallback_calls++;post0_fallback_bytes+=p->size;write_trace_add((uintptr_t)p->destination,p->size,0);dm_guest_write_prepare(p->destination,p->size);memcpy(p->destination,output->mapped,p->size);}
            if(p->source.buffer==p->output.buffer)memset(&p->output,0,sizeof p->output);
            buffer_destroy(&p->source);buffer_destroy(&p->output);
        }else if(p->kind==1){
            dm_guest_write_prepare((void*)p->event,4);if(p->mode)*p->event+=p->value;else *p->event=p->value;
        }else if(p->kind==2){
            uint64_t record[2];
            // 所有支持类型共享真实设备时间域；samples 只修改发布副本，绝不提前供 GPU 读取。
            // Supported types share the device clock domain; samples modify only the publication copy and never become GPU-readable early.
            if(p->counter_timestamp_encoded)memcpy(record,p->output.mapped,sizeof record);
            else{uint64_t t=dm_android_clock_ns(CLOCK_MONOTONIC)/625*12;record[0]=record[1]=t;}
            if(p->value==NVN_COUNTER_TYPE_SAMPLES_PASSED)record[0]=samples_passed;
            else if(p->value!=NVN_COUNTER_TYPE_TIMESTAMP&&p->value!=NVN_COUNTER_TYPE_TIMESTAMP_TOP)die("Vulkan counter type %u is not implemented",p->value);
            dm_guest_write_prepare(p->destination,sizeof record);write_trace_add((uintptr_t)p->destination,sizeof record,2);memcpy(p->destination,record,sizeof record);
            static unsigned report_diagnostics;
            if(p->counter_timestamp_encoded&&report_diagnostics<6){report_diagnostics++;
                LOG("[vulkan-counter-published] sample=%u type=%#x count=%llu timestamp=%llu domain=device GPU_forward=%u",report_diagnostics,p->value,
                    (unsigned long long)record[0],(unsigned long long)record[1],(unsigned)p->counter_gpu_encoded);}
            buffer_destroy(&p->output);
        }else if(p->kind==4)samples_passed=0;
        else if(p->kind==5){if(p->value>=batch->query_count)die("Vulkan asynchronous query index");samples_passed+=query_counts[p->value];}
        else{
            if(p->buffer_view)vkDestroyBufferView(device,p->buffer_view,NULL);
            buffer_destroy(&p->source);buffer_destroy(&p->output);
            if(p->frame)vkDestroyFramebuffer(device,p->frame,NULL);
            if(p->pipeline)vkDestroyPipeline(device,p->pipeline,NULL);
            if(p->descriptors)vkDestroyDescriptorPool(device,p->descriptors,NULL);
            if(p->pipeline_layout)vkDestroyPipelineLayout(device,p->pipeline_layout,NULL);
            if(p->pass)vkDestroyRenderPass(device,p->pass,NULL);
            if(p->image_view)vkDestroyImageView(device,p->image_view,NULL);
        }
        post_pool_put(p);
    }
    batch->posts=batch->copies=NULL;batch->query_count=0;batch->host_sync=0;
    // 先真实发布/释放，再退役该批区间；其它在途批次的根与消费者引用保持不变。
    // Publish/retire resources first, then retire only this batch's ranges; other in-flight roots and consumer references remain intact.
    dm_pending_ranges_reset(&batch->pending_ranges);
    uint64_t elapsed=dm_android_clock_ns(CLOCK_MONOTONIC)-started;
    stability_worker_publish_ns+=elapsed;
}

static void *async_completion_main(void *unused) {
    (void)unused;
    pthread_setname_np(pthread_self(),"D2R GPU done");
    pthread_mutex_lock(&queue_lock);
    for(;;){
        while(!async_first&&!async_stopping){int result=pthread_cond_wait(&async_condition,&queue_lock);if(result)die("Vulkan asynchronous condition %d",result);}
        if(!async_first&&async_stopping)break;
        async_batch *batch=async_first;uint64_t ticket=batch->ticket;
        // worker 唯一等待/回收者；FREE 之前不会 reset 这个 fence。
        // The worker alone waits/retires; this fence cannot be reset before the slot becomes FREE.
        pthread_mutex_unlock(&queue_lock);
        VK_CHECK(vkWaitForFences(device,1,&batch->fence,VK_TRUE,UINT64_MAX));
        pthread_mutex_lock(&queue_lock);
        if(async_first!=batch||batch->status!=ASYNC_PENDING||batch->ticket!=ticket)die("Vulkan asynchronous FIFO corruption");
        async_publish_locked(batch);
        async_first=batch->next;if(!async_first)async_last=NULL;batch->next=NULL;batch->status=ASYNC_FREE;
        // CPU 写回与资源退役完成后才公布 completed，保持 SyncWait/Event 可见性。
        // Publish completed only after CPU writeback and retirement, preserving SyncWait/Event visibility.
        pthread_mutex_lock(&completion_lock);
        if(ticket!=atomic_load(&completed)+1)die("Vulkan asynchronous completion order");
        atomic_store(&completed,ticket);pthread_cond_broadcast(&completion_condition);
        pthread_mutex_unlock(&completion_lock);
        pthread_cond_broadcast(&async_condition);
    }
    pthread_mutex_unlock(&queue_lock);return NULL;
}

static void async_initialize_locked(void) {
    if(async_initialized)return;
    for(unsigned i=0;i<DM_VK_BATCH_SLOTS;i++){
        async_batch *batch=&async_batches[i];
        if(!i&&command&&gpu_fence&&draw_queries){batch->command=command;batch->fence=gpu_fence;batch->queries=draw_queries;batch->times=batch_times;}
        else{
            VkCommandBufferAllocateInfo allocation={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=commands,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};
            VK_CHECK(vkAllocateCommandBuffers(device,&allocation,&batch->command));
            VkFenceCreateInfo fence={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VK_CHECK(vkCreateFence(device,&fence,NULL,&batch->fence));
            VkQueryPoolCreateInfo query={.sType=VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,.queryType=VK_QUERY_TYPE_OCCLUSION,.queryCount=ASYNC_QUERY_CAPACITY};
            VK_CHECK(vkCreateQueryPool(device,&query,NULL,&batch->queries));
            if(gpu_timestamp_bits&&gpu_timestamp_period>0){query.queryType=VK_QUERY_TYPE_TIMESTAMP;query.queryCount=2*ASYNC_CHUNK_CAPACITY;VK_CHECK(vkCreateQueryPool(device,&query,NULL,&batch->times));}
        }
        // 每个 slot 的查询结果到真实 fence 完成后才读，长期容量不算临时快照。
        // Read each slot's results only after its real fence; exclude persistent capacity from temporary snapshots.
        batch->query_readback=buffer_create(ASYNC_QUERY_READBACK_BYTES);immutable_buffer_bytes+=batch->query_readback.capacity;
        batch->chunks[0]=batch->command;batch->chunk_count=1;
    }
    async_stopping=0;async_initialized=1;
    int result=pthread_create(&async_thread,NULL,async_completion_main,NULL);if(result)die("Vulkan completion worker %d",result);
    LOG("[vulkan] asynchronous submission contexts=%u; completion publishes guest effects in FIFO order",DM_VK_BATCH_SLOTS);
}

// host GetQueryPoolResults 在 LVP 中会隐含 DeviceWaitIdle，即使不用 WAIT；
// 在本批末 GPU 复制查询后加 HOST 可见性，完成 worker 不会等更晚提交。
// Host GetQueryPoolResults implicitly waits for device idle on LVP, even without WAIT.
// Copy queries at this batch's tail and make them host-visible so publication never waits for later batches.
static void async_record_query_readback_locked(void) {
    async_batch *batch=async_current;
    if(!recording||!batch||batch->status!=ASYNC_RECORDING||used_queries>ASYNC_QUERY_CAPACITY)die("Vulkan asynchronous query readback state");
    batch->chunk_timestamps=batch->chunk_index+1;
    VkQueryResultFlags flags=VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT;
    if(batch->times)vkCmdCopyQueryPoolResults(command,batch->times,0,2*batch->chunk_timestamps,batch->query_readback.buffer,0,sizeof(uint64_t),flags);
    if(used_queries)vkCmdCopyQueryPoolResults(command,batch->queries,0,used_queries,batch->query_readback.buffer,ASYNC_TIMESTAMP_BYTES,sizeof(uint64_t),flags);
    if(!batch->times&&!used_queries)return;
    VkBufferMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=batch->query_readback.buffer,.offset=0,.size=ASYNC_QUERY_READBACK_BYTES};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&barrier,0,NULL);
}

static unsigned async_begin_locked(void) {
    async_initialize_locked();
    if(vulkan_diag_enabled()){post0_publish_report();async_chunk_report();}
    if(async_current||async_stopping)die("Vulkan asynchronous nested/stopping recording");
    for(;;){
        for(unsigned i=0;i<DM_VK_BATCH_SLOTS;i++){
            unsigned slot=(async_next_slot+i)%DM_VK_BATCH_SLOTS;async_batch *batch=&async_batches[slot];
            if(batch->status!=ASYNC_FREE)continue;
            if(batch->pending_ranges.collections[0].count||batch->pending_ranges.collections[1].count||batch->pending_ranges.live_nodes)die("Vulkan pending index reused before retirement");
            batch->status=ASYNC_RECORDING;batch->has_known_cpu_copy=batch->known_copy_unbounded=batch->known_copy_bloom_unbounded=0;
            batch->has_any_copy=batch->copy_unbounded=batch->copy_bloom_unbounded=0;batch->pending_any=batch->pending_bloom_unbounded=0;
            batch->known_copy_low=batch->known_copy_high=batch->copy_low=batch->copy_high=batch->pending_low=batch->pending_high=0;
            memset(batch->known_copy_bloom,0,sizeof batch->known_copy_bloom);
            memset(batch->copy_bloom,0,sizeof batch->copy_bloom);
            memset(batch->pending_bloom,0,sizeof batch->pending_bloom);
            batch->temporary_base=active_buffer_bytes-read_cache_bytes-immutable_buffer_bytes;
            async_current=batch;async_next_slot=(slot+1)%DM_VK_BATCH_SLOTS;
            batch->chunk_index=batch->chunk_draws=batch->chunk_timestamps=0;batch->command=batch->chunks[0];batch->chunk_started=0;
            batch->counter_used=0;
            command=batch->command;gpu_fence=batch->fence;draw_queries=batch->queries;batch_times=batch->times;
            return slot;
        }
        if(!async_first)die("Vulkan asynchronous slot corruption");
        uint64_t wait_start=dm_android_clock_ns(CLOCK_MONOTONIC);async_ring_stalls++;
        // 条件等待释放 queue_lock，让已完成 GPU 的 worker 能公布结果并腾出 slot。
        // Waiting releases queue_lock so the completed GPU's worker can publish results and free a slot.
        int result=pthread_cond_wait(&async_condition,&queue_lock);if(result)die("Vulkan asynchronous slot wait %d",result);
        uint64_t elapsed=dm_android_clock_ns(CLOCK_MONOTONIC)-wait_start;
        fence_wait_total_ns+=elapsed;stability_ring_wait_ns+=elapsed;
    }
}

// 调用前已结束 render pass、写尾时间戳并 EndCommandBuffer；返回时仍持 queue_lock。
// The caller has ended the render pass, written the end timestamp and ended the command buffer; queue_lock remains held on return.
static uint64_t async_commit_locked(void) {
    async_batch *batch=async_current;
    if(!batch||!recording||batch->status!=ASYNC_RECORDING)die("Vulkan asynchronous commit without begin");
#ifdef DM_VK_REFERENCE_GATES
    // 回归每次提交审计全部真实 post，防新分支漏登记或计数与链不符。
    // Audit every real post at regression commit, catching unregistered branches or counts that disagree with the chain.
    size_t unknown=0,host=0;uint64_t previous=0;
    for(const post_op *p=post_first;p;p=p->next){
        if(!p->order||p->order<=previous||(p->kind<=2&&!p->range_registered))die("Vulkan pending post registration/order audit");previous=p->order;
        if(p->kind==0&&!p->source.buffer&&p->destination&&p->size)unknown++;
        if(p->kind==1||p->kind==2)host++;
    }
    if(unknown!=batch->pending_ranges.collections[0].count||host!=batch->pending_ranges.collections[1].count)die("Vulkan pending index registration count audit");
#endif
    VK_CHECK(vkResetFences(device,1,&batch->fence));
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&batch->command};
    VkPipelineStageFlags present_stage = screen_submission == SCREEN_SUBMIT_SHARPEN
        ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
    if(screen_submission){
        if(!screen_acquired||screen_acquire_slot>=screen_acquire_slots||!screen_acquired[screen_acquire_slot])die("Vulkan present acquire semaphore state");
        submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&screen_acquired[screen_acquire_slot];submit.pWaitDstStageMask=&present_stage;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&screen_rendered;
    }
    #ifdef DM_VK_REFERENCE_GATES
    reference_gate_submit(&submit);
#endif
    batch->guest_frame=atomic_load(&guest_frame_index);
    batch->screen_kind = screen_submission;
    uint64_t ticket=atomic_fetch_add(&submitted,1)+1;
    pthread_mutex_lock(&vk_queue_lock);VK_CHECK(vkQueueSubmit(queue,1,&submit,batch->fence));pthread_mutex_unlock(&vk_queue_lock);
    async_logical_submits++;
    batch->posts=post_first;batch->copies=post_copy_first;batch->host_sync=pending_host_sync;batch->query_count=used_queries;batch->ticket=ticket;batch->status=ASYNC_PENDING;batch->next=NULL;
    if(async_last)async_last->next=batch;else async_first=batch;async_last=batch;
    post_first=post_copy_first=NULL;post_tail=&post_first;post_copy_tail=&post_copy_first;pending_host_sync=0;
    recording=0;screen_submission=SCREEN_SUBMIT_GAME;async_current=NULL;
    pthread_cond_signal(&async_condition);return ticket;
}

// 仅在停止新的图形调用后使用；普通 suspend/drain 不需要销毁 context 或 worker。
// Use only after stopping new graphics calls; ordinary suspend/drain need not destroy contexts or the worker.
static void async_shutdown(void) {
    mtl_finish();pthread_mutex_lock(&queue_lock);
    if(!async_initialized){pthread_mutex_unlock(&queue_lock);return;}
    if(async_current)die("Vulkan asynchronous shutdown while recording");
    async_stopping=1;pthread_cond_broadcast(&async_condition);pthread_mutex_unlock(&queue_lock);
    int result=pthread_join(async_thread,NULL);if(result)die("Vulkan completion worker join %d",result);
    pthread_mutex_lock(&queue_lock);
    for(unsigned i=0;i<DM_VK_BATCH_SLOTS;i++){
        async_batch *batch=&async_batches[i];if(batch->status!=ASYNC_FREE)die("Vulkan asynchronous shutdown with live slot");
        if(batch->times)vkDestroyQueryPool(device,batch->times,NULL);
        if(batch->counter_queries)vkDestroyQueryPool(device,batch->counter_queries,NULL);
        vkDestroyQueryPool(device,batch->queries,NULL);vkDestroyFence(device,batch->fence,NULL);vkFreeCommandBuffers(device,commands,batch->chunk_count,batch->chunks);
        immutable_buffer_bytes-=batch->query_readback.capacity;buffer_destroy(&batch->query_readback);
        dm_pending_ranges_destroy(&batch->pending_ranges);
    }
    post_pool_destroy();
    memset(async_batches,0,sizeof async_batches);command=VK_NULL_HANDLE;gpu_fence=VK_NULL_HANDLE;draw_queries=batch_times=VK_NULL_HANDLE;
    async_initialized=async_stopping=0;async_next_slot=0;pthread_mutex_unlock(&queue_lock);
}
