#ifndef AUDIO_3DS_H
#define AUDIO_3DS_H

#include "audio_api.h"

extern struct AudioAPI audio_3ds;

// Used in synthesis.c to avoid intermediate copies when possible
extern size_t samples_to_copy;
extern int16_t* copy_buf;
extern int16_t* direct_buf;

// Used in synthesis.c to avoid intermediate copies when possible
extern bool audio_3ds_next_buffer_is_ready();

// Used in audio_3ds when multi-threaded and level_script when single-threaded
extern void audio_3ds_run_one_frame();

// Sets the volume of the NDSP directly.
extern void audio_3ds_set_dsp_volume(float left, float right);

#endif
