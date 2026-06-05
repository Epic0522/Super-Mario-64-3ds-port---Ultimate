#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ultra64.h>
#include "macros.h"

/*
 * Empty mixer.c software implementation, for shrinking file size when audio is disabled.
 */

void aClearBufferImpl(UNUSED uint16_t addr, UNUSED int nbytes) {
}

void aLoadBufferImpl(UNUSED const void *source_addr) {
}

void aSaveBufferImpl(UNUSED int16_t *dest_addr) {
}

void aLoadADPCMImpl(UNUSED int num_entries_times_16, UNUSED const int16_t *book_source_addr) {
}

void aSetBufferImpl(UNUSED uint8_t flags, UNUSED uint16_t in, UNUSED uint16_t out, UNUSED uint16_t nbytes) {
}

void aSetVolumeImpl(UNUSED uint8_t flags, UNUSED int16_t v, UNUSED int16_t t, UNUSED int16_t r) {
}

void aInterleaveImpl(UNUSED uint16_t left, UNUSED uint16_t right) {
}

void aDMEMMoveImpl(UNUSED uint16_t in_addr, UNUSED uint16_t out_addr, UNUSED int nbytes) {
}

void aSetLoopImpl(UNUSED ADPCM_STATE *adpcm_loop_state) {
}

void aADPCMdecImpl(UNUSED uint8_t flags, UNUSED ADPCM_STATE state) {
}

void aResampleImpl(UNUSED uint8_t flags, UNUSED uint16_t pitch, UNUSED RESAMPLE_STATE state) {
}


void aEnvMixerImpl(UNUSED uint8_t flags, UNUSED ENVMIX_STATE state) {
}

void aMixImpl(UNUSED int16_t gain, UNUSED uint16_t in_addr, UNUSED uint16_t out_addr) {
}
