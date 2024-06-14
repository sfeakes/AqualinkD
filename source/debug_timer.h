
#ifndef DEBUG_TIMER_H_
#define DEBUG_TIMER_H_

int timespec_subtract (struct timespec *result, const struct timespec *x, const struct timespec *y);

#ifdef AQ_TM_DEBUG
#include <time.h>
#include <stdint.h>
void init_aqd_timer();
//int timespec_subtract (struct timespec *result, const struct timespec *x, const struct timespec *y);
void stop_aqd_timer(int timeid, int16_t from, char *message);
void start_aqd_timer(int *timeid);
void clear_aqd_timer(int timeid);
#define DEBUG_TIMER_START(x) start_aqd_timer(x)
#define DEBUG_TIMER_STOP(x, y, z) stop_aqd_timer(x, y, z)
#define DEBUG_TIMER_CLEAR(x) clear_aqd_timer(x)

//#define DEBUG_TIMER_START1() int t; start_aqd_timer(&t)
//#define DEBUG_TIMER_STOP1(y, z) stop_aqd_timer(t, y, z)
#else
#define DEBUG_TIMER_START(x)
#define DEBUG_TIMER_STOP(x, y, z)
#define DEBUG_TIMER_CLEAR(x)
#endif

#endif //DEBUG_TIMER_H_
