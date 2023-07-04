include(CheckCSourceRuns)

check_c_source_runs("
    #include <immintrin.h>

    int main() {
        __m128i l = _mm_set1_epi32(0);
        return _mm_extract_epi32(l, 3);
    }"
    HAS_SSE41
)

check_c_source_runs("
    #include <stdint.h>
    #include <immintrin.h>

    int main() {
        __m256i l = _mm256_set1_epi32(0);
        return _mm256_extract_epi32(l, 7);
    }"
    HAS_AVX2
)

check_c_source_runs("
    #include <stdint.h>
    #include <immintrin.h>

    int main() {
        __m128i i = _mm_set1_epi32(0);
        __m128i j = _mm_set1_epi32(1);
        __m128i k = _mm_set1_epi32(2);
        return _mm_extract_epi32(_mm_sha256rnds2_epu32(i, i, k), 0);
    }"
    HAS_X86_SHANI
)

check_c_source_runs("
    #include <arm_acle.h>
    #include <arm_neon.h>

    int main() {
        uint32x4_t a, b, c;
        vsha256h2q_u32(a, b, c);
        vsha256hq_u32(a, b, c);
        vsha256su0q_u32(a, b);
        vsha256su1q_u32(a, b, c);
        return 0;
    }"
    HAS_ARM_SHANI
)
