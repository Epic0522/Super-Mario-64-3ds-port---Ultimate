#include "profiler_3ds.h"

// If the profiler is disabled, functions and their headers do not exist.
#if PROFILER_3DS_ENABLE == 1

#include <string.h>
#include <stdio.h>

// We want to use the 3DS version of this function
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

#undef osGetTime
#include <3ds/os.h>

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

// Avoid compiler warnings for unused variables
#ifdef __GNUC__
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#define TIMESTAMP_SNOOP_INTERVAL 8
#define TIMESTAMP_ARRAY_COUNT(arr) (int)(sizeof(arr) / sizeof(arr[0]))
#define STR_FREE_SPACE(buf_len, cur) ((buf_len - cur) - 1)
#define STR_HAS_SPACE(buf_len, cur, size) (STR_FREE_SPACE(buf_len, cur) >= size)
#define CIRCULAR_ADJUST_FRAME(index) ((circ_cur_frame + i + 1) % PROFILER_3DS_NUM_CIRCULAR_FRAMES)

// Times are stored in milliseconds

static TickCounter tick_counter_average, tick_counter_linear, tick_counter_circular;

// A long-term average over time.
static volatile double   long_durations_per_id[PROFILER_3DS_NUM_IDS];
static volatile double   long_averages_per_id[PROFILER_3DS_NUM_IDS];
static volatile uint32_t long_counts_per_id[PROFILER_3DS_NUM_IDS];

// A linear log of each time recorded.
// Once the cap is reached, it will not be updated.
static volatile double   lin_all_times[PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH + 1]; // First index is the start time
static volatile double*  lin_elapsed_times = lin_all_times + 1; // Time since startTime for each stamp
static volatile double   lin_durations[PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH]; // Time since the previous stamp for each stamp
static volatile uint32_t lin_ids[PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH]; // ID for each timestamp in elapsed_times
static volatile double   lin_averages_per_id[PROFILER_3DS_NUM_IDS]; // Average duration per-id for the linear log
static volatile double   lin_totals_per_id[PROFILER_3DS_NUM_IDS]; // Total duration per-id for the linear log
static volatile uint32_t lin_counts_per_id[PROFILER_3DS_NUM_IDS]; // Count per-id for the linear log
static volatile uint32_t lin_timestamp_count = 0;

// A circular buffer of the most recent frames.
static volatile double   circ_buffer[PROFILER_3DS_NUM_CIRCULAR_FRAMES][PROFILER_3DS_NUM_IDS]; // Circular buffer of durations
static volatile double   circ_durations_per_id[PROFILER_3DS_NUM_IDS]; // Circular buffer of durations
static volatile double   circ_averages_per_id[PROFILER_3DS_NUM_IDS]; // Average of contents of totals_per_id for each id, computed on-demand
static volatile uint32_t circ_num_frames = 1; // Number of frames encountered in the circular buffer.
static volatile uint32_t circ_cur_frame = 0, circ_next_frame = 0; // Circular buffer indices

// Updated per-snoop-ID each profiler_3ds_snoop_impl() call; used for breakpoints.
static volatile uint8_t snoop_interval = 180;
static volatile uint8_t snoop_counters[PROFILER_3DS_NUM_TRACKED_SNOOP_IDS];

static char log_string[PROFILER_3DS_LOG_STRING_LENGTH];

// libctru's osTickCounterUpdate measures time between updates. We want time since last reset.
static inline void update_tick_counters() {
    const __3ds_u64 system_tick = svcGetSystemTick();

    // For the long average, we want to measure each duration
	tick_counter_average.elapsed = system_tick - tick_counter_average.reference;
	tick_counter_average.reference = system_tick;

    // For the linear log, we want to measure time since the reference
    tick_counter_linear.elapsed = system_tick - tick_counter_linear.reference;

    // For the circular average, we want to measure each duration
	tick_counter_circular.elapsed = system_tick - tick_counter_circular.reference;
	tick_counter_circular.reference = system_tick;
}

static void update_average_log(uint32_t id) {
    const double duration = osTickCounterRead(&tick_counter_average);
    long_counts_per_id[id]++;
    long_durations_per_id[id] += duration;
}

// Update linear log if there is space
static void update_linear_log(uint32_t id) {
    if (lin_timestamp_count < PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH && id < PROFILER_3DS_NUM_IDS) {
        const double curTime = osTickCounterRead(&tick_counter_linear);
        const double lastTime = lin_elapsed_times[lin_timestamp_count - 1];
        const double duration = curTime - lastTime;

        lin_elapsed_times[lin_timestamp_count] = curTime;
        lin_durations[lin_timestamp_count] = duration;
        lin_ids[lin_timestamp_count] = id;
        lin_totals_per_id[id] += duration;
        
        lin_timestamp_count++;
        lin_counts_per_id[id]++;
    }
}

static void update_circular_log(uint32_t id) {
    const double duration = osTickCounterRead(&tick_counter_circular);

    // Update circular log
    circ_buffer[circ_cur_frame][id] += duration;
}


// --------------- Loggers and Calculators ---------------

// Logs a time with the given ID.
void profiler_3ds_log_time_impl(uint32_t id) {
    update_tick_counters();
    update_average_log(id);
    update_linear_log(id);
    update_circular_log(id);
}

void profiler_3ds_average_calculate_average_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        const uint32_t count = long_counts_per_id[id];

        if (count > 0)
            long_averages_per_id[id] = long_durations_per_id[id] / count;
        else
            long_averages_per_id[id] = 0.0;
    }
}

// Calculates the averages over the linear log's history.
void profiler_3ds_linear_calculate_averages_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        const uint32_t count = lin_counts_per_id[id];

        if (count > 0)
            lin_averages_per_id[id] = lin_totals_per_id[id] / count;
        else
            lin_averages_per_id[id] = 0.0;
    }
}

// Advances one frame in the circular log
void profiler_3ds_circular_advance_frame_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        circ_buffer[circ_next_frame][id] = 0.0;
    }

    circ_cur_frame = circ_next_frame;

    if (circ_next_frame == PROFILER_3DS_NUM_CIRCULAR_FRAMES - 1)
        circ_next_frame = 0;
    else
        circ_next_frame++;

    if (circ_num_frames < PROFILER_3DS_NUM_CIRCULAR_FRAMES)
        circ_num_frames++;
    
    tick_counter_circular.reference = svcGetSystemTick();
}

// Calculates the averages for the circular log
void profiler_3ds_circular_calculate_averages_impl() {
    if (circ_num_frames > 0) {
        for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
            circ_durations_per_id[id] = 0.0;

            for (uint32_t frame = 0; frame < circ_num_frames; frame++)
                circ_durations_per_id[id] += circ_buffer[frame][id];

            circ_averages_per_id[id] = circ_durations_per_id[id] / circ_num_frames;
        }
    } else {
        for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++)
            circ_averages_per_id[id] = 0.0;
    }
}


// --------------- Resets and Initializers ---------------


// Resets the long-term average log.
void profiler_3ds_average_reset_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        long_durations_per_id[id] = 0.0;
        long_averages_per_id[id] = 0.0;
        long_counts_per_id[id] = 0;
    }

    osTickCounterStart(&tick_counter_average);
}

// Resets the linear log.
void profiler_3ds_linear_reset_impl() {
    lin_all_times[0] = 0.0;
    lin_timestamp_count = 0;

    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        lin_totals_per_id[id] = 0.0;
        lin_counts_per_id[id] = 0;
    }

    osTickCounterStart(&tick_counter_linear);
}

// Resets the circular log.
void profiler_3ds_circular_reset_impl() {
    for (uint32_t id = 0; id < PROFILER_3DS_NUM_IDS; id++) {
        for (uint32_t frame = 0; frame < PROFILER_3DS_NUM_CIRCULAR_FRAMES; frame++)
            circ_buffer[frame][id] = 0.0;

        circ_averages_per_id[id] = 0.0;
    }

    circ_cur_frame = circ_next_frame = 0;
    circ_num_frames = 1;
    profiler_3ds_circular_advance_frame_impl();
}

// Initializes the snoop counters and resets all logs.
void profiler_3ds_init_impl() {
    for (uint32_t i = 0; i < TIMESTAMP_ARRAY_COUNT(snoop_counters); i++)
        snoop_counters[i] = snoop_interval;

    profiler_3ds_average_reset_impl();
    profiler_3ds_linear_reset_impl();
    profiler_3ds_circular_reset_impl();

    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';
}

// --------------- Getters, Inspectors, and Snoopers ---------------

// Returns the average time from the long-term average log.
double profiler_3ds_average_get_average_impl(uint32_t id) {
    if (id < PROFILER_3DS_NUM_IDS)
        return long_averages_per_id[id];

    return -1.0;
}

// Returns the total elapsed time of the linear log in milliseconds.
double profiler_3ds_linear_get_elapsed_time_impl() {
    return lin_elapsed_times[lin_timestamp_count - 1];
}

// Returns the average time for the given ID from the linear log in milliseconds. Must be calculated first.
double profiler_3ds_linear_get_average_impl(uint32_t id) {
    if (id < PROFILER_3DS_NUM_IDS)
        return lin_averages_per_id[id];
    
    return -1.0;
}

// Returns the duration for a given frame and ID from the circular log.
double profiler_3ds_circular_get_duration_impl(uint32_t frame, uint32_t id) {
    if (frame < circ_cur_frame - 1 && id < PROFILER_3DS_NUM_IDS)
        return circ_buffer[frame][id];
    
    return -1.0;
}

// Returns the average time for an ID from the circular log. Must be calculated first.
double profiler_3ds_circular_get_average_time_impl(uint32_t id) {
    if (id < PROFILER_3DS_NUM_IDS)
        return circ_averages_per_id[id];
    
    return -1.0;
}

// Sets the interval for a snoop counter.
void profiler_3ds_set_snoop_counter_impl(uint32_t snoop_id, uint8_t frames_until_snoop) {
    if (snoop_id < PROFILER_3DS_NUM_TRACKED_SNOOP_IDS)
        snoop_counters[snoop_id] = frames_until_snoop;
}

// Creates a string containing the circular log's data, stored in log_string.
// Returns the size of the log string. It will be positive if it fit within the buffer,
// or negative if it did not fit. If the string does not fit, it will write as much
// as possible.
#define LOG_BUF_SIZE PROFILER_3DS_LOG_STRING_LENGTH
#define WORKER_BUF_LEN 31 // 30 chars + terminator
#define FRAME_SEPARATOR "},\n"
#define VALUE_SEPARATOR ", "
int profiler_3ds_create_log_string_circular_impl(uint32_t min_id_to_print, uint32_t max_id_to_print) {
    log_string[0] = '\0';
    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';
        
    if (min_id_to_print > PROFILER_3DS_NUM_IDS)
        min_id_to_print = PROFILER_3DS_NUM_IDS;

    if (max_id_to_print > PROFILER_3DS_NUM_IDS)
        max_id_to_print = PROFILER_3DS_NUM_IDS;

    if (min_id_to_print > max_id_to_print)
        return 0;

    int log_len = 0;
    char worker[WORKER_BUF_LEN];
    worker[WORKER_BUF_LEN - 1] = '\0';

    const int frame_sep_len = strlen(FRAME_SEPARATOR);
    const int value_sep_len = strlen(VALUE_SEPARATOR);

    // for each frame...
    for (uint32_t i = 0; i < circ_num_frames; i++) {
        int frame_num = CIRCULAR_ADJUST_FRAME(i);
        volatile double *frame = circ_buffer[frame_num];

        if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, 1)) goto too_long;
        strcpy(&log_string[log_len++], "{");

        // print each ID, separated by a comma
        for (uint32_t id = min_id_to_print; id <= max_id_to_print; id++) {
            const int worker_len = snprintf(worker, WORKER_BUF_LEN, "%lf", frame[id]);

            if (worker_len >= WORKER_BUF_LEN) {
                // Output was truncated
            } else {
                // Output was not truncated
            }

            // Append value
            if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, worker_len)) goto too_long;
            strcat(log_string, worker);
            log_len += worker_len;

            // Append value separator
            if (id < max_id_to_print) {
                if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, value_sep_len)) goto too_long;
                strcpy(&log_string[log_len], VALUE_SEPARATOR);
                log_len += value_sep_len;
            }
        }

        if (i < circ_num_frames - 1) {
            if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, frame_sep_len)) goto too_long;
            strcpy(&log_string[log_len], FRAME_SEPARATOR);
            log_len += frame_sep_len;
        } else {
            if (!STR_HAS_SPACE(LOG_BUF_SIZE, log_len, 1)) goto too_long;
            strcpy(&log_string[log_len++], "}");
        }
    }
    
    // Terminate the end of our buffer for safety
    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';
    return log_len;

    too_long:
    log_string[PROFILER_3DS_LOG_STRING_TERMINATOR] = '\0';
    return -1 * log_len;
}
#undef LOG_BUF_SIZE
#undef WORKER_BUF_LEN
#undef FRAME_SEPARATOR
#undef VALUE_SEPARATOR

// Computes some useful information for the timestamps. Intended for debugger use.
void profiler_3ds_snoop_impl(UNUSED uint32_t snoop_id) {

    // Useful GDB prints:
    // p/f *lin_totals_per_id@20
    // p/f *long_averages_per_id@20
    // p/f *circ_averages_per_id@20
    // p/f *circ_durations_per_id@20
    // printf "%s\n", log_string        // This can be slow. I get 1400 chars/min. Faster in single-core.

    // IDs:
    // 0:  Misc
    // 1:  Run Level Script
    // 2:  Synchronous Audio Synthesis
    // 3:  Render Game
    // 4:  GFX RAPI Start Frame
    // 5:  GFX Run DL
    // 6:  gfx_sp_vertex
    // 7:  gfx_sp_tri1

    // Use with conditional breakpoints in GDB
    UNUSED volatile int i = 0;
    i++;
    i++;
    i++;
    i++;
    i++;

    // Use to break after some number of iterations
    if (snoop_id < PROFILER_3DS_NUM_TRACKED_SNOOP_IDS) {
        if (--snoop_counters[snoop_id] == 0) {
            snoop_counters[snoop_id] = snoop_interval;

            switch(snoop_id) { 
                case 0: {
                    profiler_3ds_average_calculate_average_impl();
                    profiler_3ds_linear_calculate_averages_impl();
                    profiler_3ds_circular_calculate_averages_impl();
                    UNUSED volatile int log_len = profiler_3ds_create_log_string_circular_impl(0, 7);
                    
                    i += 5; // Place a breakpoint here
                    break;
                }
            }
        }
    }

    // IDs beyond the limit are still valid, but untracked
    else
        i++;

    return; // Leave this here for breakpoints
}

#endif // #if PROFILER_3DS_ENABLE == 1
