/* bc_decode_policy_test.c — portable regression test for bc_decode_policy.inc.
 *
 * Exercises the pure decision logic (no Vulkan). Run via ci_checks.py.
 */
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "bc_decode_policy.inc"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } } while (0)

int main(void) {
    /* 1. Below min_bytes → CPU even when GPU ready. */
    bc_decode_policy_set_min_bytes(4096);
    CHECK(bc_decode_policy_should_use_gpu(1000, 1, 0, 0, 4096, 0) == 0, "tiny blob → CPU");
    CHECK(bc_decode_policy_should_use_gpu(4096, 1, 0, 0, 4096, 0) == 1, "exactly min_bytes → GPU (no history)");

    /* 2. No history → GPU (first attempt). */
    CHECK(bc_decode_policy_should_use_gpu(8192, 1, 0, 0, 4096, 0) == 1, "no history → GPU");

    /* 3. GPU clearly faster → GPU. */
    CHECK(bc_decode_policy_should_use_gpu(8192, 1, 100000, 20000, 4096, 0) == 1, "GPU 5x faster → GPU");

    /* 4. CPU faster → CPU (fallback). */
    CHECK(bc_decode_policy_should_use_gpu(8192, 1, 20000, 100000, 4096, 0) == 0, "CPU faster → CPU");

    /* 5. Near-tie (within 20%) → CPU (conservative fallback). */
    CHECK(bc_decode_policy_should_use_gpu(8192, 1, 100000, 90000, 4096, 0) == 0, "near-tie → CPU");

    /* 6. GPU not ready → CPU. */
    CHECK(bc_decode_policy_should_use_gpu(8192, 0, 20000, 100000, 4096, 0) == 0, "GPU not ready → CPU");

    /* 7. Pending pressure: too many jobs in flight → CPU even if GPU faster. */
    bc_decode_policy_set_pending_limit(2);
    CHECK(bc_decode_policy_should_use_gpu(8192, 1, 100000, 20000, 4096, 0) == 1, "pending below limit → GPU");
    CHECK(bc_decode_policy_should_use_gpu(8192, 1, 100000, 20000, 4096, 2) == 0, "pending at limit → CPU");

    /* 8. EWMA record + decide with global state. */
    bc_decode_policy_set_min_bytes(4096);
    bc_decode_policy_record_cpu(100000, 8192);
    bc_decode_policy_record_cpu(100000, 8192);
    bc_decode_policy_record_gpu(20000, 8192);
    bc_decode_policy_record_gpu(20000, 8192);
    /* After EWMA: CPU avg ≈ 60937, GPU avg ≈ 12187 → GPU wins. */
    CHECK(bc_decode_policy_decide(8192, 1, 0) == 1, "EWMA GPU faster → GPU");
    CHECK(bc_decode_policy_cpu_fallbacks() == 0, "no CPU fallbacks recorded");

    /* 9. Sustained CPU pressure flips to CPU (via CPU being slower). */
    for (int i = 0; i < 8; i++) bc_decode_policy_record_cpu(100000, 8192);
    CHECK(bc_decode_policy_decide(8192, 1, 0) == 1, "GPU still faster after CPU pressure → GPU stays");

    /* 10. GPU slower → CPU with fallback counter. */
    bc_decode_policy_record_gpu(500000, 8192);
    bc_decode_policy_record_gpu(500000, 8192);
    CHECK(bc_decode_policy_decide(8192, 1, 0) == 0, "GPU much slower → CPU");
    CHECK(bc_decode_policy_cpu_fallbacks() == 1, "CPU fallback counter incremented");

    CHECK(bc_decode_policy_cpu_jobs() > 0, "cpu jobs recorded");
    CHECK(bc_decode_policy_gpu_jobs() > 0, "gpu jobs recorded");

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("bc_decode_policy_test: ALL PASS\n");
    return 0;
}