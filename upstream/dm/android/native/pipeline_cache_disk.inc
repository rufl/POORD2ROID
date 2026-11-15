#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef PIPELINE_CACHE_DISK_INC
#define PIPELINE_CACHE_DISK_INC

/* 可移植的 Vulkan pipeline-cache 持久化信封（disk-persistence envelope）。
 *
 * 文件布局（80 字节头 + blob）：
 *   [0..7]   魔数 "DMVKPC01"
 *   [8..39]  命名空间身份哈希（32 字节）：设备/驱动/着色器配置/运行时策略
 *   [40..47] blob 长度（little-endian u64）
 *   [48..79] blob 的 SHA-256 摘要
 *   [80..]   blob 本身（Vulkan pipeline-cache 数据，VK_PIPELINE_CACHE_HEADER_VERSION_ONE 起头）
 *
 * 设计点：身份哈希不同的文件绝不跨驱动复用；摘要失效、长度不一致、身份不匹配均拒载；
 * 写入通过临时文件 + fsync + 原子 rename，旧文件从不被不完整快照替换。
 *
 * 本模块仅依赖标准 C 与 legacy_digest（dm_android_sha256），不依赖 Vulkan，
 * 可在宿主机编译并独立回归测试；设备端 nvn_vulkan_pipeline_cache.inc 复用同一份编码。
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#ifndef PIPELINE_DISK_LOG
#include "legacy_digest.h" // dm_android_sha256
#endif

#define PIPELINE_DISK_MAGIC   "DMVKPC01"
#define PIPELINE_DISK_MAGIC_LEN 8
#define PIPELINE_DISK_LIMIT   (64u << 20)   // 64MB 上限
#define PIPELINE_DISK_HEADER  80u

/* 蓝图检测回调：设备端对 Vulkan blob 头做更严格校验；宿主测试可传 NULL。 */
typedef int (*pipeline_disk_blob_check)(const uint8_t *data, size_t size);

static uint32_t pipeline_disk_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t pipeline_disk_u64(const uint8_t *p) {
    return pipeline_disk_u32(p) | ((uint64_t)pipeline_disk_u32(p + 4) << 32);
}
static void pipeline_disk_put32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(n >> (8 * i));
}
static void pipeline_disk_put64(uint8_t *p, uint64_t n) {
    pipeline_disk_put32(p, (uint32_t)n);
    pipeline_disk_put32(p + 4, (uint32_t)(n >> 32));
}

static void pipeline_disk_digest(const void *data, size_t size, uint8_t result[32]) {
    dm_android_sha256_context ctx;
    dm_android_sha256_init(&ctx);
    dm_android_sha256_update(&ctx, data, (uint32_t)size);
    dm_android_sha256_final(result, &ctx);
}

/* 仅做信封级校验：大小界限、蓝图检测回调。身份与摘要在 envelope 层校验。 */
static int pipeline_disk_blob_envelope_valid(const uint8_t *data, size_t size,
                                             pipeline_disk_blob_check check) {
    if (size < 32 || size > PIPELINE_DISK_LIMIT) return 0;
    if (check && !check(data, size)) return 0;
    return 1;
}

/* 全量读写辅助（EINTR 自动重试）。 */
static int pipeline_disk_full(int fd, void *data, size_t size, int writing) {
    uint8_t *p = data;
    while (size) {
        ssize_t n = writing ? write(fd, p, size) : read(fd, p, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        p += n; size -= (size_t)n;
    }
    return 1;
}

/* 载入信封：返回分配的 blob（*out），失败返回 NULL 并在 *reason 写入原因。 */
static void *pipeline_disk_envelope_load(int dir_fd, const char *name,
                                         const uint8_t identity[32],
                                         pipeline_disk_blob_check check,
                                         void **out, size_t *out_size, const char **reason) {
    *out = NULL; *out_size = 0;
    if (reason) *reason = "invalid_file";
    if (dir_fd < 0 || !name || !identity) return NULL;

    int fd = openat(dir_fd, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) { if (reason) *reason = (errno == ENOENT) ? "missing" : "open_failed"; return NULL; }

    struct stat st;
    uint8_t header[PIPELINE_DISK_HEADER], digest[32];
    void *blob = NULL;
    const char *fail = "invalid_file";

    if (fstat(fd, &st) || !S_ISREG(st.st_mode) ||
        st.st_size < (off_t)(PIPELINE_DISK_HEADER + 32) ||
        st.st_size > (off_t)(PIPELINE_DISK_HEADER + PIPELINE_DISK_LIMIT)) {
        fail = "invalid_size"; goto done;
    }
    if (!pipeline_disk_full(fd, header, sizeof header, 0)) { fail = "read_header"; goto done; }
    if (memcmp(header, PIPELINE_DISK_MAGIC, PIPELINE_DISK_MAGIC_LEN) != 0) { fail = "bad_magic"; goto done; }
    if (memcmp(header + PIPELINE_DISK_MAGIC_LEN, identity, 32) != 0) { fail = "identity_mismatch"; goto done; }

    uint64_t length = pipeline_disk_u64(header + 40);
    if (length < 32 || length > PIPELINE_DISK_LIMIT ||
        length != (size_t)st.st_size - PIPELINE_DISK_HEADER) { fail = "invalid_length"; goto done; }

    blob = malloc((size_t)length);
    if (!blob) { fail = "allocation_failed"; goto done; }
    if (!pipeline_disk_full(fd, blob, (size_t)length, 0)) { fail = "read_blob"; goto done; }

    pipeline_disk_digest(blob, (size_t)length, digest);
    if (memcmp(digest, header + 48, 32) != 0) { fail = "digest_mismatch"; goto done; }
    if (!pipeline_disk_blob_envelope_valid(blob, (size_t)length, check)) { fail = "invalid_blob"; goto done; }

    *out = blob; *out_size = (size_t)length; fail = NULL;
done:
    close(fd);
    if (fail) { free(blob); *out = NULL; *out_size = 0; }
    if (reason) *reason = fail ? fail : "";
    return *out;
}

/* 写入信封：临时文件 + fsync + 原子 rename；成功返回 1，失败返回 0。 */
static int pipeline_disk_envelope_write(int dir_fd, const char *name,
                                        const uint8_t identity[32],
                                        const void *blob, size_t size,
                                        uint64_t (*clock_ns)(void), void (*logf)(const char *, ...)) {
    if (dir_fd < 0 || !name || !identity || !blob || size < 32 || size > PIPELINE_DISK_LIMIT) return 0;

    uint8_t header[PIPELINE_DISK_HEADER];
    memset(header, 0, sizeof header);
    memcpy(header, PIPELINE_DISK_MAGIC, PIPELINE_DISK_MAGIC_LEN);
    memcpy(header + PIPELINE_DISK_MAGIC_LEN, identity, 32);
    pipeline_disk_put64(header + 40, size);
    pipeline_disk_digest(blob, size, header + 48);

    char temporary[128];
    int n = snprintf(temporary, sizeof temporary, ".%s.%ld.%llu.tmp", name,
                     (long)getpid(), (unsigned long long)(clock_ns ? clock_ns() : 0));
    if (n < 0 || (size_t)n >= sizeof temporary) return 0;

    int fd = openat(dir_fd, temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return 0;
    int ok = pipeline_disk_full(fd, header, sizeof header, 1) &&
             pipeline_disk_full(fd, (void *)blob, size, 1) &&
             !fsync(fd);
    if (close(fd)) ok = 0;
    if (ok && renameat(dir_fd, temporary, dir_fd, name)) ok = 0;
    if (ok && fsync(dir_fd)) ok = 0;
    if (!ok) unlinkat(dir_fd, temporary, 0);
    if (logf && ok) logf("[pipeline-disk] write=complete bytes=%zu", size);
    return ok;
}

#endif /* PIPELINE_CACHE_DISK_INC */