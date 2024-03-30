
#ifdef AQ_TM_DEBUG
#include <stdio.h>
#include <string.h>
#include "debug_timer.h"
#include "utils.h"
#include "timespec_subtract.h"

#define NUM_DEBUG_TIMERS 10

static struct timespec _start_time[NUM_DEBUG_TIMERS];
static int _timeid=0;

void init_aqd_timer(int *timeid)
{
  int i=0;
  for(i=0; i < NUM_DEBUG_TIMERS; i++) {
    _start_time[i].tv_sec = 0;
    _start_time[i].tv_nsec = 0;
  }
}

void start_aqd_timer(int *id)
{
  bool maxloop=false;
  // Just a sanity check, should be redundant.
  if (_timeid >= NUM_DEBUG_TIMERS)
    _timeid = 0;

  while (_start_time[_timeid].tv_sec != 0 && _start_time[_timeid].tv_nsec != 0) {
    if (++_timeid >= NUM_DEBUG_TIMERS) {
      if (maxloop) { // Means we ranover loop twice
        LOG(DBGT_LOG,LOG_ERR,"Ranout of debug timers\n");
        *id = -1;
        return;
      } else {
        _timeid = 0;
        maxloop=true;
      }
    }
  }

  clock_gettime(CLOCK_REALTIME, &_start_time[_timeid++]);
  *id = _timeid-1;
  //return _timeid-1;
}

void clear_aqd_timer(int timeid) {
  _start_time[timeid].tv_sec = 0;
  _start_time[timeid].tv_nsec = 0;
}

void stop_aqd_timer(int timeid, int16_t from, char *message)
{
  if (timeid < 0 || timeid >= NUM_DEBUG_TIMERS) {
    LOG(from,LOG_ERR,"Invalid timeid '%d' for message '%s'\n", timeid, message);
    return;
  }
  static struct timespec now;
  static struct timespec elapsed;
  clock_gettime(CLOCK_REALTIME, &now);
  timespec_subtract(&elapsed, &now, &_start_time[timeid]);
  //DBGT_LOG
  //LOG(from,LOG_NOTICE, "%s %d.%02ld sec (%08ld ns)\n", message, elapsed.tv_sec, elapsed.tv_nsec / 1000000L, elapsed.tv_nsec);
  LOG(DBGT_LOG,LOG_DEBUG, "%s %s %d.%02ld sec (%08ld ns)\n", logmask2name(from), message, elapsed.tv_sec, elapsed.tv_nsec / 1000000L, elapsed.tv_nsec);

  // We've used it so free it.
  clear_aqd_timer(timeid);
}



#endif


