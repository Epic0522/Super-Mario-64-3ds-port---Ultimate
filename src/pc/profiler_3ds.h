#ifndef PROFILER_3DS_H
#define PROFILER_3DS_H

#include <types.h>

#define PROFILER_3DS_ENABLE 0


#if PROFILER_3DS_ENABLE == 1

// Maximum ID number for a timestamp.
// WYATT_TODO off-by-one: value of 32 gives 33 entries
#define PROFILER_3DS_NUM_IDS 32

// Number of frames to average in the circular buffer.
#define PROFILER_3DS_NUM_CIRCULAR_FRAMES 90

// Maximum number of timestamps that can be stored without a reset.
// Additional timestamps will be dropped.
#define PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH 1024

// Maximum number of snoop IDs to track. If a given ID exceeds this
// value, it will trigger every snoop.
#define PROFILER_3DS_NUM_TRACKED_SNOOP_IDS 64

// Length of the log string.
// For the circular log, estimate 10 * <max id> * num circular frames, plus some extra for formatting.
#define PROFILER_3DS_LOG_STRING_LENGTH 20000

// Convenience define. Represents where to place the buffer terminator.
#define PROFILER_3DS_LOG_STRING_TERMINATOR (PROFILER_3DS_LOG_STRING_LENGTH - 1)

#else

// Profiler is disabled, so all values are 0 to reduce memory footprint.
#define PROFILER_3DS_NUM_IDS 0
#define PROFILER_3DS_NUM_CIRCULAR_FRAMES 0
#define PROFILER_3DS_TIMESTAMP_HISTORY_LENGTH 0
#define PROFILER_3DS_NUM_TRACKED_SNOOP_IDS 0
#define PROFILER_3DS_LOG_STRING_LENGTH 1
#define PROFILER_3DS_LOG_STRING_TERMINATOR (PROFILER_3DS_LOG_STRING_LENGTH - 1)

#endif

// Function definitions and #define relays to them.
#if PROFILER_3DS_ENABLE == 1

// Loggers and Calculators
void profiler_3ds_log_time_impl(uint32_t id); // Logs a time with the given ID.
void profiler_3ds_average_calculate_average_impl(); // Calculates the long-term average
void profiler_3ds_linear_calculate_averages_impl(); // Calculates the averages over the linear log's history.
void profiler_3ds_circular_advance_frame_impl(); // Advances one frame in the circular log
void profiler_3ds_circular_calculate_averages_impl(); // Calculates the averages for the circular log

// Resets and Initializers
void profiler_3ds_average_reset_impl(); // Resets the long-term average log.
void profiler_3ds_linear_reset_impl(); // Resets the linear log.
void profiler_3ds_circular_reset_impl(); // Resets the circular log.
void profiler_3ds_init_impl(); // Initializes the snoop counters and resets all logs.

// Getters, Inspectors, and Snoopers
double profiler_3ds_average_get_average_impl(uint32_t id); // Returns the average time from the long-term average log.
double profiler_3ds_linear_get_elapsed_time_impl(); // Returns the total elapsed time of the linear log in milliseconds.
double profiler_3ds_linear_get_average_impl(uint32_t id); // Returns the average time for the given ID from the linear log in milliseconds. Must be calculated first.
double profiler_3ds_circular_get_duration_impl(uint32_t frame, uint32_t id); // Returns the duration for a given frame and ID from the circular log.
double profiler_3ds_circular_get_average_time_impl(uint32_t id); // Returns the average time for an ID from the circular log. Must be calculated first.
void   profiler_3ds_set_snoop_counter_impl(uint32_t snoop_id, uint8_t frames_until_snoop); // Sets the interval for a snoop counter.
int    profiler_3ds_create_log_string_circular_impl(uint32_t min_id_to_print, uint32_t max_id_to_print); // Creates a log string of the circular buffer and stores it in log_string. Returns 0 if successful, or -1 if the buffer could not fit the string.
void   profiler_3ds_snoop_impl(uint32_t snoop_id); // Computes some useful information for the timestamps. Intended for debugger use.


// Loggers and Calculators
#define profiler_3ds_log_time(id)                     profiler_3ds_log_time_impl(id) // Logs a time with the given ID.
#define profiler_3ds_average_calculate_averages()     profiler_3ds_average_calculate_average_impl() // Calculates the long-term average
#define profiler_3ds_linear_calculate_averages()      profiler_3ds_linear_calculate_averages_impl() // Calculates the averages over the linear log's history.
#define profiler_3ds_circular_advance_frame()         profiler_3ds_circular_advance_frame_impl() // Advances one frame in the circular log
#define profiler_3ds_circular_calculate_averages()    profiler_3ds_circular_calculate_averages_impl() // Calculates the averages for the circular log

// Resets and Initializers
#define profiler_3ds_average_reset()                  profiler_3ds_average_reset_impl() // Resets the long-term average log.
#define profiler_3ds_linear_reset()                   profiler_3ds_linear_reset_impl() // Resets the linear log.
#define profiler_3ds_circular_reset()                 profiler_3ds_circular_reset_impl() // Resets the circular log.
#define profiler_3ds_init()                           profiler_3ds_init_impl() // Initializes the snoop counters and resets all logs.

// Getters, Inspectors, and Snoopers
#define profiler_3ds_average_get_average(id)              profiler_3ds_average_get_average_impl(id) // Returns the average time from the long-term average log.
#define profiler_3ds_linear_get_elapsed_time()            profiler_3ds_linear_get_elapsed_time_impl() // Returns the total elapsed time of the linear log in milliseconds.
#define profiler_3ds_linear_get_average(id)               profiler_3ds_linear_get_average_impl(id) // Returns the average time for the given ID from the linear log in milliseconds. Must be calculated first.
#define profiler_3ds_circular_get_total_time(f, id)       profiler_3ds_circular_get_total_time_impl(f, id) // Returns the duration for a given frame and ID from the circular log.
#define profiler_3ds_circular_get_average_time(f, id)     profiler_3ds_circular_get_average_time_impl(f, id) // Returns the average time for an ID from the circular log. Must be calculated first.
#define profiler_3ds_set_snoop_counter(sid, ftl)          profiler_3ds_set_snoop_counter_impl(sid, ftl) // Sets the interval for a snoop counter.
#define profiler_3ds_create_log_string_circular(min, max) profiler_3ds_create_log_string_circular_impl(min, max) // Creates a log string of the circular buffer and stores it in log_string. Returns 0 if successful, or -1 if the buffer could not fit the string.
#define profiler_3ds_snoop(sid)                           profiler_3ds_snoop_impl(sid) // Computes some useful information for the timestamps. Intended for debugger use.

// Stubs used when the profiler is disabled. Also note that functions aren't even defined.
#else

#define profiler_3ds_log_time(id)                                    do {} while (0) // Profiler is disabled.
#define profiler_3ds_linear_log_start_time()                         do {} while (0) // Profiler is disabled.
#define profiler_3ds_linear_calculate_averages()                     do {} while (0) // Profiler is disabled.
#define profiler_3ds_circular_advance_frame()                        do {} while (0) // Profiler is disabled.
#define profiler_3ds_circular_calculate_averages()                   do {} while (0) // Profiler is disabled.
#define profiler_3ds_average_reset()                                 do {} while (0) // Profiler is disabled.
#define profiler_3ds_linear_reset()                                  do {} while (0) // Profiler is disabled.
#define profiler_3ds_circular_reset()                                do {} while (0) // Profiler is disabled.
#define profiler_3ds_init()                                          do {} while (0) // Profiler is disabled.
#define profiler_3ds_average_get_average(id)                                     0.0 // Profiler is disabled.
#define profiler_3ds_linear_get_elapsed_time()                                   0.0 // Profiler is disabled.
#define profiler_3ds_linear_get_average(id)                                      0.0 // Profiler is disabled.
#define profiler_3ds_circular_get_total_time(frame, id)                          0.0 // Profiler is disabled.
#define profiler_3ds_circular_get_average_time(frame, id)                        0.0 // Profiler is disabled.
#define profiler_3ds_set_snoop_counter(snoop_id, frames_until_snoop) do {} while (0) // Profiler is disabled.
#define profiler_3ds_create_log_string_circular(min, max)            do {} while (0) // Profiler is disabled.

#define profiler_3ds_snoop(snoop_id)                                 do {} while (0) // Profiler is disabled.

#endif // PROFILER_3DS_ENABLE

#endif // PROFILER_3DS_H
