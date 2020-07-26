
#ifdef AQ_TM_DEBUG
#include <stdio.h>
#include <string.h>
#include "debug_timer.h"
#include "utils.h"
//#include "timespec_subtract.h"

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

/* Copyright (c) 1991, 1999 Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */


/* Based on https://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html
   Subtract the struct timespec values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */

//#include <string.h>
//#include "timespec_subtract.h"

int timespec_subtract (struct timespec *result, const struct timespec *x,
                       const struct timespec *y)
{
  struct timespec tmp;

  memcpy (&tmp, y, sizeof(struct timespec));
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_nsec < tmp.tv_nsec)
    {
      int nsec = (tmp.tv_nsec - x->tv_nsec) / 1000000000 + 1;
      tmp.tv_nsec -= 1000000000 * nsec;
      tmp.tv_sec += nsec;
    }
  if (x->tv_nsec - tmp.tv_nsec > 1000000000)
    {
      int nsec = (x->tv_nsec - tmp.tv_nsec) / 1000000000;
      tmp.tv_nsec += 1000000000 * nsec;
      tmp.tv_sec -= nsec;
    }

  /* Compute the time remaining to wait.
   tv_nsec is certainly positive. */
  result->tv_sec = x->tv_sec - tmp.tv_sec;
  result->tv_nsec = x->tv_nsec - tmp.tv_nsec;

  /* Return 1 if result is negative. */
  return x->tv_sec < tmp.tv_sec;
}

#endif

