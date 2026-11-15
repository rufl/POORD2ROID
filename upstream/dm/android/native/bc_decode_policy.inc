#ifndef BC_DECODE_POLICY_INC
#define BC_DECODE_POLICY_INC

/* Política adaptativa de decodificação BC: GPU vs CPU.
 *
 * Decide, por bloco, se a descompressão vai para a GPU (compute shader) ou CPU.
 * Critérios:
 *   - min_bytes: abaixo disso a sobrecarga de despacho da GPU não compensa → CPU.
 *   - média móvel de custo por job (CPU vs GPU): usa o mais rápido; empates/default → CPU (fallback conservador e mensurável).
 *   - carga (pending): quantos jobs GPU já estão em fila; acima do limiar, a GPU não aguenta mais e o bloco vai para CPU.
 *
 * Toda a lógica de decisão é uma função pura de entrada explícita, portável e
 * testável em host sem Vulkan. Os contadores de custo são atualizados no lado
 * de produção (nvn_vulkan_tex.inc) e lidos aqui.
 */
#include <stdint.h>

#define BC_GPU_MIN_BYTES_DEFAULT 4096u   // abaixo de 4 KB, sempre CPU
#define BC_POLICY_PENDING_LIMIT_DEFAULT 8  // jobs GPU em fila acima disso → CPU
#define BC_POLICY_ALPHA_NUM 3            // EWMA: novo peso 3/8
#define BC_POLICY_ALPHA_DEN 8

static uint64_t bc_policy_min_bytes = BC_GPU_MIN_BYTES_DEFAULT;
static int64_t  bc_policy_cpu_avg_ns = 0;   // 0 = sem amostra ainda
static int64_t  bc_policy_gpu_avg_ns = 0;
static uint32_t bc_policy_cpu_jobs = 0, bc_policy_gpu_jobs = 0;
static uint32_t bc_policy_cpu_fallbacks = 0; // jobs que couberiam na GPU mas foram para CPU
static uint32_t bc_policy_pending_limit = BC_POLICY_PENDING_LIMIT_DEFAULT;

/* Inicializa o limiar mínimo de bytes para usar GPU. Chame uma vez no boot. */
static void bc_decode_policy_set_min_bytes(uint64_t min_bytes) {
    bc_policy_min_bytes = min_bytes;
}
static void bc_decode_policy_set_pending_limit(uint32_t limit) {
    bc_policy_pending_limit = limit;
}

/* Registro de custo (chamado no caminho de produção após cada decodificação). */
static void bc_decode_policy_record_cpu(int64_t ns, uint64_t bytes) {
    (void)bytes;
    bc_policy_cpu_jobs++;
    if (bc_policy_cpu_avg_ns == 0) bc_policy_cpu_avg_ns = ns;
    else bc_policy_cpu_avg_ns = (bc_policy_cpu_avg_ns * (BC_POLICY_ALPHA_DEN - BC_POLICY_ALPHA_NUM) + ns * BC_POLICY_ALPHA_NUM) / BC_POLICY_ALPHA_DEN;
}
static void bc_decode_policy_record_gpu(int64_t ns, uint64_t bytes) {
    (void)bytes;
    bc_policy_gpu_jobs++;
    if (bc_policy_gpu_avg_ns == 0) bc_policy_gpu_avg_ns = ns;
    else bc_policy_gpu_avg_ns = (bc_policy_gpu_avg_ns * (BC_POLICY_ALPHA_DEN - BC_POLICY_ALPHA_NUM) + ns * BC_POLICY_ALPHA_NUM) / BC_POLICY_ALPHA_DEN;
}

/* Decisão pura: deve usar GPU para este bloco?
 *
 * input_bytes: tamanho do bloco comprimido (bytes)
 * gpu_ready:   pipeline de GPU BC inicializado?
 * cpu_avg_ns / gpu_avg_ns: médias móveis de custo por job (0 = sem amostra)
 * min_bytes:   limiar mínimo de bytes para considerar GPU
 * pending:     jobs GPU já em fila (carga)
 *
 * Retorna 1 (GPU) ou 0 (CPU).
 */
static int bc_decode_policy_should_use_gpu(uint64_t input_bytes, int gpu_ready,
                                           int64_t cpu_avg_ns, int64_t gpu_avg_ns,
                                           uint64_t min_bytes, uint32_t pending) {
    if (!gpu_ready) return 0;
    if (input_bytes < min_bytes) return 0;
    if (pending >= bc_policy_pending_limit) return 0;   // carga: limiar de fila
    if (cpu_avg_ns == 0 && gpu_avg_ns == 0) return 1;   // sem histórico: tenta GPU primeiro
    if (cpu_avg_ns == 0) return 0;                       // só temos CPU amostrado → CPU
    if (gpu_avg_ns == 0) return 1;                       // só temos GPU amostrado → GPU
    // GPU só vence se for claramente mais rápida (margem de 20%); empates caem em CPU.
    return gpu_avg_ns * 100 < cpu_avg_ns * 80 ? 1 : 0;
}

/* Decisão com estado global (produção). */
static int bc_decode_policy_decide(uint64_t input_bytes, int gpu_ready, uint32_t pending) {
    int use_gpu = bc_decode_policy_should_use_gpu(input_bytes, gpu_ready,
                                                  bc_policy_cpu_avg_ns, bc_policy_gpu_avg_ns,
                                                  bc_policy_min_bytes, pending);
    if (!use_gpu && gpu_ready && input_bytes >= bc_policy_min_bytes) {
        bc_policy_cpu_fallbacks++;
    }
    return use_gpu;
}

static uint64_t bc_decode_policy_cpu_fallbacks(void) { return bc_policy_cpu_fallbacks; }
static uint64_t bc_decode_policy_cpu_jobs(void) { return bc_policy_cpu_jobs; }
static uint64_t bc_decode_policy_gpu_jobs(void) { return bc_policy_gpu_jobs; }

#endif /* BC_DECODE_POLICY_INC */