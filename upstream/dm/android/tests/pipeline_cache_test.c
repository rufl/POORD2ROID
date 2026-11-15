#define _GNU_SOURCE
// 回归：Vulkan pipeline-cache 持久化信封（pipeline_cache_disk.inc）。
//
// 覆盖：
//   1. write→load 往返：同一身份读写一致，字节级不变。
//   2. 身份不匹配：不同命名空间（驱动/设备/策略）的文件被拒载（cross-driver 隔离）。
//   3. 摘要不匹配：blob 被篡改后拒载（digest_mismatch）。
//   4. 长度不一致：截断/伪造长度后拒载（invalid_length）。
//   5. 尺寸上限：超过 64MB 拒载。
//   6. 空/缺失文件：返回 missing 而不是崩溃。
//
// Regression: Vulkan pipeline-cache persistence envelope.
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
/* MinGW / Windows POSIX emulation stubs for host test compilation */
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
static inline int openat(int dirfd, const char *pathname, int flags, ...) {
    (void)dirfd; return open(pathname, flags);
}
static inline int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
    (void)olddirfd; (void)newdirfd; return rename(oldpath, newpath);
}
static inline int unlinkat(int dirfd, const char *pathname, int flags) {
    (void)dirfd; (void)flags; return unlink(pathname);
}
#define pread(fd, buf, count, offset) (_lseeki64(fd, offset, SEEK_SET), read(fd, buf, count))
#define pwrite(fd, buf, count, offset) (_lseeki64(fd, offset, SEEK_SET), write(fd, buf, count))
#define fsync(fd) _commit(fd)
#define HAVE_MKDTEMP 1
static inline char *custom_mkdtemp(char *tmpl) {
    _mktemp(tmpl);
    if (mkdir(tmpl) != 0) return NULL;
    return tmpl;
}
#endif

/* legacy_digest 提供 dm_android_sha256_*；本测试只用到摘要与信封编解码。 */
#include "pipeline_cache_disk.inc"
static void *make_blob(uint32_t seed, size_t size) {
    uint8_t *b = malloc(size);
    assert(b);
    for (size_t i = 0; i < size; i++) b[i] = (uint8_t)(seed + i * 131u);
    return b;
}

static int reject_blob(const uint8_t *data, size_t size) { (void)data; (void)size; return 0; }

int main(void) {
    char root[] = "./d2r-pipeline-cache-XXXXXX";
    assert(mkdtemp(root));
    int dir = open(root, O_RDONLY | O_CLOEXEC);
    assert(dir >= 0);

    uint8_t id_a[32], id_b[32];
    for (int i = 0; i < 32; i++) { id_a[i] = (uint8_t)i; id_b[i] = (uint8_t)(0x80 + i); }

    /* 1. 往返 */
    size_t blob_size = 4096;
    uint8_t *blob = make_blob(7, blob_size);
    assert(pipeline_disk_envelope_write(dir, "a.bin", id_a, blob, blob_size, NULL, NULL));

    void *loaded = NULL;
    size_t loaded_size = 0;
    const char *reason = NULL;
    pipeline_disk_envelope_load(dir, "a.bin", id_a, NULL, &loaded, &loaded_size, &reason);
    assert(loaded != NULL);
    assert(reason && reason[0] == 0);
    assert(loaded_size == blob_size);
    assert(memcmp(loaded, blob, blob_size) == 0);
    free(loaded);

    /* 2. 身份不匹配：不同驱动/命名空间不能复用同一 blob。 */
    loaded = NULL; loaded_size = 0;
    pipeline_disk_envelope_load(dir, "a.bin", id_b, NULL, &loaded, &loaded_size, &reason);
    assert(loaded == NULL);
    assert(strcmp(reason, "identity_mismatch") == 0);

    /* 3. 摘要不匹配：篡改 blob，保持头不动。 */
    {
        int fd = openat(dir, "a.bin", O_RDWR | O_CLOEXEC);
        assert(fd >= 0);
        uint8_t flipped;
        assert(pread(fd, &flipped, 1, (off_t)PIPELINE_DISK_HEADER) == 1);
        flipped ^= 0xFF;
        assert(pwrite(fd, &flipped, 1, (off_t)PIPELINE_DISK_HEADER) == 1);
        close(fd);
    }
    loaded = NULL; loaded_size = 0;
    pipeline_disk_envelope_load(dir, "a.bin", id_a, NULL, &loaded, &loaded_size, &reason);
    assert(loaded == NULL);
    assert(strcmp(reason, "digest_mismatch") == 0);

    /* 4. 长度不一致：伪造头里的长度，越过文件实际大小。 */
    {
        int fd = openat(dir, "a.bin", O_RDWR | O_CLOEXEC);
        assert(fd >= 0);
        uint8_t len_field[8];
        assert(pread(fd, len_field, 8, 40) == 8);
        pipeline_disk_put64(len_field, pipeline_disk_u64(len_field) + 1);
        assert(pwrite(fd, len_field, 8, 40) == 8);
        close(fd);
    }
    loaded = NULL; loaded_size = 0;
    pipeline_disk_envelope_load(dir, "a.bin", id_a, NULL, &loaded, &loaded_size, &reason);
    assert(loaded == NULL);
    assert(strcmp(reason, "invalid_length") == 0);

    /* 5. 尺寸上限：头声明一个 64MB+1 的 blob。 */
    {
        unlinkat(dir, "a.bin", 0);
        int fd = openat(dir, "a.bin", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
        uint8_t header[80];
        (void)header;
        memcpy(header, "DMVKPC01", 8);
        memcpy(header + 8, id_a, 32);
        pipeline_disk_put64(header + 40, (uint64_t)(64u * 1024u * 1024u) + 1u);
        assert(write(fd, header, sizeof header) == (ssize_t)sizeof header);
        close(fd);
    }
    loaded = NULL; loaded_size = 0;
    pipeline_disk_envelope_load(dir, "a.bin", id_a, NULL, &loaded, &loaded_size, &reason);
    assert(loaded == NULL);
    assert(strcmp(reason, "invalid_size") == 0);

    /* 6. 缺失文件。 */
    loaded = NULL; loaded_size = 0;
    pipeline_disk_envelope_load(dir, "does-not-exist.bin", id_a, NULL, &loaded, &loaded_size, &reason);
    assert(loaded == NULL);
    assert(strcmp(reason, "missing") == 0);

    /* 7. 蓝图检测回调拒绝：信封合法但 blob 检测失败也要拒载。 */
    assert(pipeline_disk_envelope_write(dir, "a.bin", id_a, blob, blob_size, NULL, NULL));
    loaded = NULL; loaded_size = 0;
    pipeline_disk_envelope_load(dir, "a.bin", id_a, reject_blob, &loaded, &loaded_size, &reason);
    assert(loaded == NULL);
    assert(strcmp(reason, "invalid_blob") == 0);

    unlinkat(dir, "a.bin", 0);
    close(dir);
    rmdir(root);
    free(blob);
    puts("pipeline-cache disk envelope: PASS (roundtrip, identity, digest, length, limit, missing, blob-check)");
    return 0;
}
