#ifdef TARGET_N3DS

#ifndef GFX_3DS_AUDIO_THREADING_H
#define GFX_3DS_AUDIO_THREADING_H

// I hate this library
// hack for redefinition of types in libctru
// All 3DS includes must be done inside of an equivalent
// #define/undef block to avoid type redefinition issues.
#define u64 __3ds_u64
#define s64 __3ds_s64
#define u32 __3ds_u32
#define vu32 __3ds_vu32
#define vs32 __3ds_vs32
#define s32 __3ds_s32
#define u16 __3ds_u16
#define s16 __3ds_s16
#define u8 __3ds_u8
#define s8 __3ds_s8
#include <3ds/types.h>
#include <3ds.h>
#include <3ds/svc.h>
#undef u64
#undef s64
#undef u32
#undef vu32
#undef vs32
#undef s32
#undef u16
#undef s16
#undef u8
#undef s8

// Set to 1 to enable sleep, or 0 to disable.
#define N3DS_AUDIO_ENABLE_SLEEP_FUNC 1

#define N3DS_AUDIO_SECONDS_TO_NANOS(t) (t * 1000000000)  // Calculate a duration in seconds
#define N3DS_AUDIO_MILLIS_TO_NANOS(t)  (t * 1000000)     // Calculate a duration in milliseconds
#define N3DS_AUDIO_MICROS_TO_NANOS(t)  (t * 1000)        // Calculate a duration in microseconds
#define N3DS_AUDIO_NANOS(t)            (t)               // A duration in nanoseconds

// Audio sleep duration of 10 microseconds (0.01 millis). May sleep for longer.
#define N3DS_AUDIO_SLEEP_DURATION_NANOS N3DS_AUDIO_MILLIS_TO_NANOS(0.01)

#define N3DS_DESIRED_PRIORITY_MAIN_THREAD 0x19
#define N3DS_DESIRED_PRIORITY_AUDIO_THREAD 0x18
#define N3DS_AUDIO_CORE_1_LIMIT_IDLE 10 // Limit when in the home menu, sleeping, etc. Can be [10-80].
#define N3DS_AUDIO_CORE_1_LIMIT 80      // Limit during gameplay. Can be [10-80].

// Allows us to conveniently replace 3DS sleep functions
#if N3DS_AUDIO_ENABLE_SLEEP_FUNC == 1
#define N3DS_AUDIO_SLEEP_FUNC(time) svcSleepThread(time)
#else
#define N3DS_AUDIO_SLEEP_FUNC(time) do {} while (0)
#endif

enum N3dsCpu {
    OLD_CORE_0  = 0,
    OLD_CORE_1  = 1,
    NEW_CORE_2  = 2
};

// Controls when Thread5 is allowed to skip waiting for the audio thread.
extern bool s_thread5_wait_for_audio_to_finish;

// Tells Thread5 whether or not to run audio synchronously
extern bool s_thread5_does_audio;

// Which CPU to run audio on
extern enum N3dsCpu s_audio_cpu;

// Synchronization variables
extern volatile __3ds_s32 s_audio_frames_to_tick;
extern volatile __3ds_s32 s_audio_frames_to_process;

// This is a purely informative value.
// It is set to true when it acknowledges a new frame to process,
// and to false when the audio thread begins spinning.
// Do not use this for bi-directional synchronization!
extern volatile bool s_audio_thread_processing;

#endif
#endif
