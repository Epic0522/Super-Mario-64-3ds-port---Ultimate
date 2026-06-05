/*
 * This file is a stub, and selects from one of the files
 * in the mixer_implementations directory.
 * 
 *               ----- Important -----
 * These files must not be included in the build path!
 * If they are present, compilation errors will abound.
 * 
 *            ----- Developer Note -----
 * If you need to view ASM, you can add specific files
 * to the build path. This may increase code size with
 * lesser compilers, so it should be disabled for
 * release. See Makefile line 242.
 * 
 * This file is ignored completely on N64.
 */


// If set to use reference RSPA, force it.
#if defined RSPA_USE_REFERENCE_IMPLEMENTATION
#include "src/pc/mixer_implementations/mixer_reference.c"

// Empty to save on file size
#elif defined DISABLE_AUDIO
#include "src/pc/mixer_implementations/mixer_null.c"

// x86 SSE4.1 support
#elif defined __SSE4_1__
#include "src/pc/mixer_implementations/mixer_sse41.c"

// ARM Neon support
#elif defined __ARM_NEON
#include "src/pc/mixer_implementations/mixer_neon.c"

// Optimized for N3DS, supports ENHANCED_RSPA_EMULATION.
#elif defined TARGET_N3DS

// SIMD32 enhancements allow for much faster operations
// on audio code. Unfortunately, it seems like the 3DS
// SIMD32 instructions aren't actually that useful.
#if __ARM_FEATURE_SIMD32 == 1 && __ARM_FEATURE_SAT == 1
#include "src/pc/mixer_implementations/mixer_3ds_simd32.c"

// If ARM SIMD32 is disabled, use the standard implementation.
#else
#include "src/pc/mixer_implementations/mixer_3ds.c"
#endif

// Fall back to reference RSPA if no special versions are available.
#else
#include "src/pc/mixer_implementations/mixer_reference.c"
#endif
