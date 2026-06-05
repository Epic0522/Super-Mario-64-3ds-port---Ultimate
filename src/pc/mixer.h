#ifndef MIXER_H
#define MIXER_H

#include <stdint.h>
#include <ultra64.h>

// This file is ignored completely on N64.

#undef aSegment
#undef aClearBuffer
#undef aSetBuffer
#undef aLoadBuffer
#undef aSaveBuffer
#undef aDMEMMove
#undef aMix
#undef aEnvMixer
#undef aResample
#undef aInterleave
#undef aSetVolume
#undef aSetVolume32
#undef aSetLoop
#undef aLoadADPCM
#undef aADPCMdec

void aClearBufferImpl(uint16_t addr, int nbytes);
void aLoadBufferImpl(const void *source_addr);
void aSaveBufferImpl(int16_t *dest_addr);
void aLoadADPCMImpl(int num_entries_times_16, const int16_t *book_source_addr);
void aSetBufferImpl(uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes);
void aSetVolumeImpl(uint8_t flags, int16_t v, int16_t t, int16_t r);
void aInterleaveImpl(uint16_t left, uint16_t right);
void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes);
void aSetLoopImpl(ADPCM_STATE *adpcm_loop_state);
void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state);
void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state);
void aEnvMixerImpl(uint8_t flags, ENVMIX_STATE state);
void aMixImpl(int16_t gain, uint16_t in_addr, uint16_t out_addr);

// Redirects to the native versions of these functions.
// The command increment is completely removed.

#define aSegment(pkt, s, b) do { } while(0)
#define aClearBuffer(pkt, d, c) aClearBufferImpl(d, c)
#define aLoadBuffer(pkt, s) aLoadBufferImpl(s) // Loads data from the given source into rspa.in
#define aSaveBuffer(pkt, s) aSaveBufferImpl(s)
#define aLoadADPCM(pkt, c, d) aLoadADPCMImpl(c, d)
#define aSetBuffer(pkt, f, i, o, c) aSetBufferImpl(f, i, o, c)
#define aSetVolume(pkt, f, v, t, r) aSetVolumeImpl(f, v, t, r)
#define aSetVolume32(pkt, f, v, tr) aSetVolume(pkt, f, v, (int16_t)((tr) >> 16), (int16_t)(tr))
#define aInterleave(pkt, l, r) aInterleaveImpl(l, r)
#define aDMEMMove(pkt, i, o, c) aDMEMMoveImpl(i, o, c)
#define aSetLoop(pkt, a) aSetLoopImpl(a)
#define aADPCMdec(pkt, f, s) aADPCMdecImpl(f, s)
#define aResample(pkt, f, p, s) aResampleImpl(f, p, s)
#define aEnvMixer(pkt, f, s) aEnvMixerImpl(f, s)
#define aMix(pkt, f, g, i, o) aMixImpl(g, i, o)

// Enhanced RSPA emulation allows us to break the rules of
// RSPA emulation a little bit for better performance.
// Should be disabled when using reference implementation.
// This file is ignored completely on N64.
#if defined RSPA_USE_ENHANCEMENTS && !defined RSPA_USE_REFERENCE_IMPLEMENTATION && !defined DISABLE_AUDIO

#define ENHANCED_RSPA_EMULATION

// Snoop contents of the emulated RSPA for debugging.
void aSnoop(volatile int snoopTag);

void aADPCMdecDirectImpl(uint8_t flags, ADPCM_STATE state, uint8_t* source);
void aInterleaveAndCopyImpl(uint16_t left, uint16_t right, int16_t *dest_addr);

#define aADPCMdecDirect(pkt, f, s, src) aADPCMdecDirectImpl(f, s, src) // ADPCM Decode directly from external address
#define aInterleaveAndCopy(pkt, l, r, dest) aInterleaveAndCopyImpl(l, r, dest) // Interleave directly to external address

#endif

#endif // MIXER_H
