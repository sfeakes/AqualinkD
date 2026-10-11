/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#define _GNU_SOURCE 1 // for strcasestr & strptime
#define __USE_XOPEN 1
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <termios.h>
#include <signal.h>

#include <time.h> // Need GNU_SOURCE & XOPEN defined for strptime

#define AQUALINKD_C
#include "rs_devices.h"
#include "mongoose.h"
#include "aqualink.h"
#include "utils.h"
#include "config.h"
#include "aq_serial.h"
#include "aq_panel.h"
#include "aq_programmer.h"
#include "net_services.h"
#include "pda_menu.h"
#include "pda.h"
#include "devices_pentair.h"
#include "pda_aq_programmer.h"
#include "packetLogger.h"
#include "devices_jandy.h"
#include "allbutton.h"
#include "allbutton_aq_programmer.h"
#include "onetouch.h"
#include "onetouch_aq_programmer.h"
#include "iaqtouch.h"
#include "iaqtouch_aq_programmer.h"
#include "iaqualink.h"
#include "version.h"
#include "rs_msg_utils.h"
#include "serialadapter.h"
#include "simulator.h"
#include "debug_timer.h"
#include "aq_scheduler.h"
#include "json_messages.h"
#include "aq_systemutils.h"
#include "auto_configure.h"

#ifdef AQ_MANAGER
#include "rs485mon.h"
#endif

/*
#if defined AQ_DEBUG || defined AQ_TM_DEBUG
  #include "timespec_subtract.h"
#endif
*/
//#define DEFAULT_CONFIG_FILE "./aqualinkd.conf"

static volatile bool _keepRunning = true;

#ifdef SELF_RESTART
static volatile bool _restart = false;
#endif
//char** _argv;
//static struct aqconfig _aqconfig_;
static struct aqualinkdata _aqualink_data;

// NSF Need to remove self
char *_self;
char *_cfgFile;
int _cmdln_loglevel = -1;
bool _cmdln_debugRS485 = false;
bool _cmdln_lograwRS485 = false;
bool _cmdln_nostartupcheck = false;
bool _cmdln_log_msec_ts = false;

#ifdef AQ_TM_DEBUG
  //struct timespec _rs_packet_readitme;
  int _rs_packet_timer;
#endif

#define AddAQDstatusMask(mask) (_aqualink_data.status_mask |= mask)
#define RemoveAQDstatusMask(mask) (_aqualink_data.status_mask &= ~mask)

// Keep running on all errors (we can) if we are in a container or demonized
#if defined(AQ_CONTAINER)
  #define SHOULD_KEEP_RUNNING()  (true)
#else
  #define SHOULD_KEEP_RUNNING()  (_aqconfig_.deamonize)
#endif


void main_loop();
int startup(char *self, char *cfgFile);


/* One shot, armed in startup() when force_panel_time_sync_at_startup is set.  Holds the
   time the forced sync becomes due (0 = not armed).  Makes that one comparison report the
   panel as wrong even when it is inside ACCEPTABLE_TIME_DIFF, so the panel clock gets
   rewritten once per daemon start.  Deliberately NOT due immediately: setting the clock
   takes over the panel menus for a while, which is the last thing we want while startup
   is still collecting state. */
static time_t _force_panel_time_sync_after = 0;

bool isAqualinkDStopping() {
  return !_keepRunning;
}

void intHandler(int sig_num)
{
  if (sig_num == SIGRUPGRADE) {
    if (! run_aqualinkd_upgrade(_aqualink_data.upgrade_version)) {
      LOG(AQUA_LOG,LOG_ERR, "AqualinkD upgrade failed!\n");
    }
    return; // Let the upgrade process terminate us.
  }

  LOG(AQUA_LOG,LOG_WARNING, "Stopping!\n");

  _keepRunning = false;

  if (sig_num == SIGRESTART) {
    LOG(AQUA_LOG,LOG_WARNING, "Restarting AqualinkD!\n");
    // If we are deamonized, we need to use the system
    if (_aqconfig_.deamonize) {
      if(fork() == 0) {
        sleep(2);
        char *newargv[] = {"/bin/systemctl", "restart", "aqualinkd", NULL};
        char *newenviron[] = { NULL };
        execve(newargv[0], newargv, newenviron);
        exit (EXIT_SUCCESS);
      }
#ifdef SELF_RESTART
    } else {
      _restart = true;
#endif
    }
  }
  //LOG(AQUA_LOG,LOG_NOTICE, "Stopping!\n");
  //if (dummy){}// stop compile warnings

  stopPacketLogger();
  close_serial_port(-1);

}

bool isVirtualButtonEnabled() {
  return _aqualink_data.virtual_button_start>0?true:false;
}


/* --------------------------------------------------------------------------
 * Panel minute rollover timing.
 *
 * The panel reports HH:MM with no seconds, so one reading only locates its clock
 * to within a minute; we assume the midpoint, which is +/-30s of noise however
 * accurate the panel actually is.  But the panel repeats its display every few
 * seconds, so the moment the displayed minute CHANGES pins the panel's HH:MM:00
 * to within one message gap - about 8s on an RS panel, so +/-4s once we take the
 * middle of the gap.  That is tight enough to judge against
 * ACCEPTABLE_TIME_DIFF_PRECISE instead of ACCEPTABLE_TIME_DIFF.
 *
 * Only used where the panel reports a real date.  PDA reports a weekday and its
 * check has its own day-of-week handling, so that path keeps the coarse estimate.
 * -------------------------------------------------------------------------- */
static char   _ro_last_time[AQ_MSGLEN] = "";
static time_t _ro_prev_sample = 0;   /* when the previous time message arrived */
static time_t _ro_at          = 0;   /* when we first saw the new minute */
static int    _ro_window      = 0;   /* gap between those two messages */
static time_t _ro_panel_min   = 0;   /* when that minute SHOULD have started */

/* Parse the panel's 'MM/DD/YY' + 'H:MM XM' into the time_t its minute started. */
static bool panel_minute_start(time_t now, time_t *out)
{
  char datestr[DATE_STRING_LEN];
  struct tm tm;
  size_t dlen = strlen(_aqualink_data.date);
  size_t tlen = strlen(_aqualink_data.time);

  /* 'MM/DD/YY' is 8, 'H:MM XM' is 7 and 'HH:MM XM' is 8. Anything else is not
     something the surgery below can lay out, and would risk overrunning datestr. */
  if (dlen < 8 || dlen > 12 || tlen < 7 || tlen > 8)
    return false;

  memset(&tm, 0, sizeof(tm));
  memcpy(&datestr[0], _aqualink_data.date, 8);
  datestr[8] = ' ';
  memcpy(&datestr[9], _aqualink_data.time, tlen);
  datestr[9 + tlen] = '\0';

  if (strptime(datestr, "%m/%d/%y %I:%M %p", &tm) == NULL)
    return false;

  tm.tm_sec = 0;                                   /* the START of the minute */
  tm.tm_isdst = localtime(&now)->tm_isdst;
  *out = mktime(&tm);
  return true;
}

/*
* Is now a reasonable moment to take over the panel's menus and rewrite its clock?
*
* Setting the clock holds SET TIME for up to a minute, so routine corrections are confined
* to a configured quiet window.  A gross offset is not routine - the panel has lost its
* clock rather than drifted - and does not wait for the window.
*/
static bool panel_time_sync_allowed_now(time_t now, int time_difference)
{
  struct tm tm_now;
  int start = _aqconfig_.panel_time_sync_start_hour;
  int end   = _aqconfig_.panel_time_sync_end_hour;

  if (abs(time_difference) >= AQ_TIME_DIFF_URGENT) {
    LOG(AQUA_LOG,LOG_NOTICE, "Panel clock is %d seconds out, too far to wait for the quiet window\n",
        time_difference);
    return true;
  }

  if (start == end)                       /* window disabled, any time will do */
    return true;

  localtime_r(&now, &tm_now);
  if (start < end) {
    if (tm_now.tm_hour >= start && tm_now.tm_hour < end)
      return true;
  } else {                                /* window wraps midnight, e.g. 22 to 4 */
    if (tm_now.tm_hour >= start || tm_now.tm_hour < end)
      return true;
  }

  LOG(AQUA_LOG,LOG_INFO, "Panel clock is %d seconds out, waiting for the %02d:00-%02d:00 window to correct it\n",
      time_difference, start, end);
  return false;
}

/* Called for every panel time message, before any of the rate limiting below. */
static void observe_panel_rollover(time_t now)
{
  time_t minstart;

#ifdef AQ_PDA
  if (isPDA_PANEL && !isPDA_IAQT)
    return;
#endif

  if (strncmp(_aqualink_data.time, _ro_last_time, sizeof(_ro_last_time)) != 0) {
    /* Displayed minute just changed, so its :00 fell between the previous message
       and this one.  _ro_prev_sample of 0 means this is our first message and we
       have no window to measure, so only remember the string. */
    if (_ro_prev_sample != 0 && panel_minute_start(now, &minstart)) {
      _ro_at        = now;
      _ro_window    = (int)(now - _ro_prev_sample);
      _ro_panel_min = minstart;
      LOG(AQUA_LOG,LOG_DEBUG, "Panel minute rolled to '%s' %ds after the previous message\n",
          _aqualink_data.time, _ro_window);
    }
    strncpy(_ro_last_time, _aqualink_data.time, sizeof(_ro_last_time) - 1);
    _ro_last_time[sizeof(_ro_last_time) - 1] = '\0';
  }
  _ro_prev_sample = now;
}

/*
* Throw away what we know about the panel's minute rollover.
*
* Called when we are about to rewrite the panel clock.  After that the panel's reported
* time JUMPS, and a jump is not a rollover - treating it as one measures the size of the
* correction we just made rather than any error, which is how a 903,657,588 second offset
* got reported.  Clearing _ro_prev_sample also makes the first message after the change
* establish a baseline instead of being paired with one from before it.
*/
static void panel_rollover_forget(void)
{
  _ro_last_time[0] = '\0';
  _ro_prev_sample  = 0;
  _ro_at           = 0;
  _ro_window       = 0;
  _ro_panel_min    = 0;
}

/* Offset in seconds, positive meaning the panel clock is BEHIND system time -
   the same sense as the coarse difftime() comparison. */
static bool panel_rollover_offset(time_t now, int *offset, int *accuracy)
{
  /* These report at INFO rather than DEBUG on purpose: when the rollover estimate is
     not used, the reason for falling back to the coarse +/-30s figure needs to be
     visible in a normal log. */
  if (_ro_at == 0 || _ro_window <= 0) {
    LOG(AQUA_LOG,LOG_INFO, "No panel minute rollover timed yet, using the +/-30s HH:MM estimate\n");
    return false;
  }
  if (_ro_window > AQ_ROLLOVER_MAX_WINDOW) {
    LOG(AQUA_LOG,LOG_INFO, "Panel minute rollover only pinned to %ds, no better than the HH:MM estimate, using that\n",
        _ro_window);
    return false;
  }
  if (now - _ro_at > AQ_ROLLOVER_MAX_AGE) {
    LOG(AQUA_LOG,LOG_INFO, "Last panel minute rollover is %ds old, using the +/-30s HH:MM estimate\n",
        (int)(now - _ro_at));
    return false;
  }

  *offset   = (int)((_ro_at - (_ro_window / 2)) - _ro_panel_min);
  *accuracy = (_ro_window / 2) + 1;
  return true;
}

// Should move to panel.
bool checkAqualinkTime()
{
  static time_t last_checked = 0;
  time_t now = time(0); // get time now
  int time_difference;
  struct tm aq_tm;
  time_t aqualink_time;

  if (_aqconfig_.sync_panel_time != true)
    return true; 

  // Must run on EVERY time message, so it goes before all the rate limiting below.
  observe_panel_rollover(now);

  // A due forced sync has to skip the hourly rate limit, otherwise arming it at startup
  // would not take effect until the next scheduled check up to an hour later.
  bool force_due = (_force_panel_time_sync_after != 0 && now >= _force_panel_time_sync_after);

  time_difference = (int)difftime(now, last_checked);
  if (!force_due && time_difference < TIME_CHECK_INTERVAL)
  {
    LOG(AQUA_LOG,LOG_DEBUG, "time not checked, will check in %d seconds\n", TIME_CHECK_INTERVAL - time_difference);
    return true;
  }
  else if (strlen(_aqualink_data.date) <=0 ||
           strlen(_aqualink_data.time) <=0) 
  {
    LOG(AQUA_LOG,LOG_DEBUG, "time not checked, no time from panel\n");
    return true;
  }
  else
  {
    last_checked = now;
    //return false;
  }

  char datestr[DATE_STRING_LEN];
#ifdef AQ_PDA
  if (isPDA_PANEL && !isPDA_IAQT) {
    LOG(AQUA_LOG,LOG_DEBUG, "PDA Time Check\n");
    // date is simply a day or week for PDA.
    localtime_r(&now, &aq_tm);
    int real_wday = aq_tm.tm_wday; // NSF Need to do this better, we could be off by 7 days
    snprintf(datestr, DATE_STRING_LEN, "%.12s %.8s",_aqualink_data.date,_aqualink_data.time);

    if (strptime(datestr, "%a %I:%M%p", &aq_tm) == NULL) {
      LOG(AQUA_LOG,LOG_ERR, "Could not convert PDA RS time string '%s'", datestr);
      last_checked = (time_t)NULL;
      return true;
    }
    if (real_wday != aq_tm.tm_wday) {
      LOG(AQUA_LOG,LOG_INFO, "PDA Day of the week incorrect - request time set\n");
      return false;
    }
  } 
  else
#endif // AQ_PDA
  {
    
    strcpy(&datestr[0], _aqualink_data.date);
    datestr[8] = ' ';
    strcpy(&datestr[9], _aqualink_data.time);
    //datestr[16] = ' ';
    if (strlen(_aqualink_data.time) <= 7) {
      datestr[13] = ' ';
      datestr[16] ='\0';
    } else {
      datestr[14] = ' ';
      datestr[17] ='\0';
    }
    if (strptime(datestr, "%m/%d/%y %I:%M %p", &aq_tm) == NULL)
    
    //sprintf(datestr, "%s %s", _aqualink_data.date, _aqualink_data.time);
    //if (strptime(datestr, "%m/%d/%y %a %I:%M %p", &aq_tm) == NULL)
    {
      LOG(AQUA_LOG,LOG_ERR, "Could not convert RS time string '%s'\n", datestr);
      last_checked = (time_t)NULL;
      return true;
    }
  }

  aq_tm.tm_isdst = localtime(&now)->tm_isdst; // ( Might need to use -1) set daylight savings to same as system time
  // The panel only reports HH:MM, it has no seconds field.  A panel showing '10:05'
  // is actually somewhere in 10:05:00 - 10:05:59, so the best estimate of the panel
  // clock is the MIDPOINT of the minute it is displaying.  Using 0 here made every
  // comparison read 0-59 seconds slow (+30 on average) even for a perfectly synced
  // panel, which both hid real drift and made the panel look ~1 minute behind.
  // NOTE: seconds must be set to something deterministic, strptime() leaves whatever
  // was on the stack in tm_sec.
  aq_tm.tm_sec = 30;

  char buff[30];
  
  LOG(AQUA_LOG,LOG_DEBUG, "Aqualinkd created time from : %s\n", datestr);
  strftime(buff, 30, "%Y-%m-%d %H:%M:%S %a", &aq_tm);
  LOG(AQUA_LOG,LOG_DEBUG, "Aqualinkd created time      : %s\n", buff);

  aqualink_time = mktime(&aq_tm);

  strftime(buff, 30, "%Y-%m-%d %H:%M:%S", localtime(&aqualink_time));
  LOG(AQUA_LOG,LOG_DEBUG, "Aqualinkd converted time    : %s\n", buff);
  strftime(buff, 30, "%Y-%m-%d %H:%M:%S", localtime(&now));
  LOG(AQUA_LOG,LOG_DEBUG, "System time                 : %s\n", buff);


  time_difference = (int)difftime(now, aqualink_time);

  strftime(buff, 30, "%m/%d/%y %I:%M %p", localtime(&now));
  LOG(AQUA_LOG,LOG_INFO, "Aqualink time '%s' is off system time '%s' by %d seconds...\n", datestr, buff, time_difference);

  /* Prefer the rollover timed offset when we have a usable one.  It is roughly an
     order of magnitude tighter than reading HH:MM and assuming the midpoint, so it
     is judged against a correspondingly tighter tolerance. */
  /* How close the panel can actually be held depends on which setter AQ_SET_TIME lands
     on.  Only the allbutton one commits on a minute boundary; the iAQ Touch and PDA
     setters cannot land closer than a few tens of seconds, so tightening the tolerance
     for them would just re-program the panel every hour to the same wrong time. */
  bool boundary_aware = isPanelTimeSetterBoundaryAware();
  int tolerance = boundary_aware ? ACCEPTABLE_TIME_DIFF : ACCEPTABLE_TIME_DIFF_LEGACY;
  int precise, accuracy;
  if (panel_rollover_offset(now, &precise, &accuracy))
  {
    /* The rollover measurement is good regardless of which setter we use, so always
       prefer it for the figure we report and act on.  Only the TOLERANCE depends on what
       the setter can achieve. */
    /* Both figures measure the same thing by different routes, so a wild disagreement
       means the rollover reading is not sound - stale state, or the panel's date and time
       messages momentarily describing different moments (they arrive separately). Keep
       the coarse figure, which cannot be skewed that way. */
    if (abs(precise - time_difference) > AQ_ROLLOVER_SANITY) {
      LOG(AQUA_LOG,LOG_WARNING, "Ignoring minute rollover estimate of %+ds, it disagrees with the %+ds from the panel display by more than %ds\n",
          precise, time_difference, AQ_ROLLOVER_SANITY);
      panel_rollover_forget();
      goto rollover_unusable;
    }

    time_difference = precise;
    if (boundary_aware) {
      /* Only act on an offset bigger than what we can actually resolve.  Scaling by the
         measured window rather than applying a fixed cutoff means a rollover we only
         pinned loosely still gets used, just held to a looser figure - and it is never
         worse than the coarse path it replaces. */
      tolerance = accuracy + AQ_ROLLOVER_SLACK;
      if (tolerance < ACCEPTABLE_TIME_DIFF_PRECISE)
        tolerance = ACCEPTABLE_TIME_DIFF_PRECISE;
      if (tolerance > ACCEPTABLE_TIME_DIFF)
        tolerance = ACCEPTABLE_TIME_DIFF;
    }
    LOG(AQUA_LOG,LOG_INFO, "Panel clock is %+d seconds off system time (+/-%ds, timed from the panel's minute rollover over %ds), tolerance %ds\n",
        precise, accuracy, _ro_window, tolerance);
  }
rollover_unusable:

  if (force_due)
  {
    // Startup sync was requested and is now due.  Consumed here rather than in startup()
    // so we inherit every check above, the panel has to be initialised and have actually
    // told us its time before we start walking its menus.
    _force_panel_time_sync_after = 0;
    LOG(AQUA_LOG,LOG_NOTICE, "Forcing panel time sync (%s=yes), panel is off by %d seconds\n",
        CFG_N_force_panel_time_sync_at_startup, time_difference);
    panel_rollover_forget();
    return false;
  }

  if (abs(time_difference) < tolerance)
  {
    // Within tolerance, leave the panel alone.
    return true;
  }

  /* Out of tolerance, but rewriting the clock takes over the panel for up to a minute,
     so unless it is badly out this waits for the quiet window. */
  if (! panel_time_sync_allowed_now(now, time_difference))
    return true;

  /* The clock is about to be set, so a forced startup sync still waiting to fire would
     only take the panel over a second time for nothing. */
  if (_force_panel_time_sync_after != 0) {
    LOG(AQUA_LOG,LOG_INFO, "Setting the clock now, so the pending startup sync is no longer needed\n");
    _force_panel_time_sync_after = 0;
  }
  panel_rollover_forget();
  return false;
}



void action_delayed_request()
{
  char sval[10];
  snprintf(sval, 9, "%d", _aqualink_data.unactioned.value);

  // If we don't know the units yet, we can't action setpoint, so wait until we do.
  if (_aqualink_data.temp_units == UNKNOWN && 
     (_aqualink_data.unactioned.type == POOL_HTR_SETPOINT || _aqualink_data.unactioned.type == SPA_HTR_SETPOINT || _aqualink_data.unactioned.type == FREEZE_SETPOINT || _aqualink_data.unactioned.type == CHILLER_SETPOINT))
    return;

  if (_aqualink_data.unactioned.type == POOL_HTR_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(POOL_HTR_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.pool_htr_set_point != _aqualink_data.unactioned.value)
    {
#ifdef NEW_AQ_PROGRAMMER
      aq_programmer(AQ_SET_POOL_HEATER_TEMP, NULL, _aqualink_data.unactioned.value, AQP_NULL, &_aqualink_data);
#else
      aq_programmer(AQ_SET_POOL_HEATER_TEMP, sval, &_aqualink_data);
#endif
      LOG(AQUA_LOG,LOG_NOTICE, "Setting pool heater setpoint to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Pool heater setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == SPA_HTR_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(SPA_HTR_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.spa_htr_set_point != _aqualink_data.unactioned.value)
    {
#ifdef NEW_AQ_PROGRAMMER
      aq_programmer(AQ_SET_SPA_HEATER_TEMP, NULL, _aqualink_data.unactioned.value, AQP_NULL, &_aqualink_data);
#else
      aq_programmer(AQ_SET_SPA_HEATER_TEMP, sval, &_aqualink_data);
#endif
      LOG(AQUA_LOG,LOG_NOTICE, "Setting spa heater setpoint to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Spa heater setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == FREEZE_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(FREEZE_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.frz_protect_set_point != _aqualink_data.unactioned.value)
    {
#ifdef NEW_AQ_PROGRAMMER
      aq_programmer(AQ_SET_FRZ_PROTECTION_TEMP, NULL, _aqualink_data.unactioned.value, AQP_NULL, &_aqualink_data);
#else
      aq_programmer(AQ_SET_FRZ_PROTECTION_TEMP, sval, &_aqualink_data);
#endif
      LOG(AQUA_LOG,LOG_NOTICE, "Setting freeze protect to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Freeze setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == CHILLER_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(CHILLER_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    if (_aqualink_data.chiller_set_point != _aqualink_data.unactioned.value)
    {
#ifdef NEW_AQ_PROGRAMMER
      aq_programmer(AQ_SET_CHILLER_TEMP, NULL, _aqualink_data.unactioned.value, AQP_NULL, &_aqualink_data);
#else
      aq_programmer(AQ_SET_CHILLER_TEMP, sval, &_aqualink_data);
#endif
      LOG(AQUA_LOG,LOG_NOTICE, "Setting Chiller setpoint to %d\n", _aqualink_data.unactioned.value);
    }
    else
    {
      LOG(AQUA_LOG,LOG_NOTICE, "Chiller setpoint is already %d, not changing\n", _aqualink_data.unactioned.value);
    }
  }
  else if (_aqualink_data.unactioned.type == SWG_SETPOINT)
  {
    _aqualink_data.unactioned.value = setpoint_check(SWG_SETPOINT, _aqualink_data.unactioned.value, &_aqualink_data);
    //if (_aqualink_data.ar_swg_status == SWG_STATUS_OFF)
    if (_aqualink_data.swg_led_state == OFF)
    {
      // SWG is off, can't set %, so delay the set until it's on.
      LOG(AQUA_LOG,LOG_NOTICE, "SWG is off, delaying request to set %% to %d\n", _aqualink_data.unactioned.value);
      _aqualink_data.swg_delayed_percent = _aqualink_data.unactioned.value;
    }
    else
    {
      if (_aqualink_data.swg_percent != _aqualink_data.unactioned.value)
      {
#ifdef NEW_AQ_PROGRAMMER
        aq_programmer(AQ_SET_SWG_PERCENT, NULL, _aqualink_data.unactioned.value, AQP_NULL, &_aqualink_data);
#else
        aq_programmer(AQ_SET_SWG_PERCENT, sval, &_aqualink_data);
#endif
        LOG(AQUA_LOG,LOG_NOTICE, "Setting SWG %% to %d\n", _aqualink_data.unactioned.value);
      }
      else
      {
        LOG(AQUA_LOG,LOG_NOTICE, "SWG %% is already %d, not changing\n", _aqualink_data.unactioned.value);
      }
    }
    // Let's just tell everyone we set it, before we actually did.  Makes homekit happy, and it will re-correct on error.
    //_aqualink_data.swg_percent = _aqualink_data.unactioned.value;
#ifdef PRESTATE_SWG_SETPOINT
    setSWGpercent(&_aqualink_data, _aqualink_data.unactioned.value);
#endif
  }
  else if (_aqualink_data.unactioned.type == SWG_BOOST)
  {
    //LOG(AQUA_LOG,LOG_NOTICE, "SWG BOST to %d\n", _aqualink_data.unactioned.value);
    //if (_aqualink_data.ar_swg_status == SWG_STATUS_OFF) {
    if ((_aqualink_data.swg_led_state == OFF) && (_aqualink_data.boost == false)) {
      LOG(AQUA_LOG,LOG_ERR, "SWG is off, can't Boost pool\n");
    } else if (_aqualink_data.unactioned.value == _aqualink_data.boost ) {
      LOG(AQUA_LOG,LOG_ERR, "Request to turn Boost %s ignored, Boost is already %s\n",_aqualink_data.unactioned.value?"On":"Off", _aqualink_data.boost?"On":"Off");
    } else {
#ifdef NEW_AQ_PROGRAMMER
      aq_programmer(AQ_SET_BOOST, NULL, _aqualink_data.unactioned.value, AQP_NULL, &_aqualink_data);
#else
      aq_programmer(AQ_SET_BOOST, sval, &_aqualink_data);
#endif
    }
    // Let's just tell everyone we set it, before we actually did.  Makes homekit happy, and it will re-correct on error.
    _aqualink_data.boost = _aqualink_data.unactioned.value;
  }
  else if (_aqualink_data.unactioned.type == PUMP_RPM)
  {
    snprintf(sval, 9, "%1d|%d", _aqualink_data.unactioned.id, _aqualink_data.unactioned.value);
    //printf("**** program string '%s'\n",sval);
#ifdef NEW_AQ_PROGRAMMER
    aq_programmer(AQ_SET_PUMP_RPM, NULL, _aqualink_data.unactioned.value,  _aqualink_data.unactioned.id, &_aqualink_data);
#else
    aq_programmer(AQ_SET_PUMP_RPM, sval, &_aqualink_data);
#endif
  }
  else if (_aqualink_data.unactioned.type == PUMP_VSPROGRAM)
  {
    snprintf(sval, 9, "%1d|%d", _aqualink_data.unactioned.id, _aqualink_data.unactioned.value);
    //printf("**** program string '%s'\n",sval);
#ifdef NEW_AQ_PROGRAMMER
    aq_programmer(AQ_SET_PUMP_VS_PROGRAM, NULL, _aqualink_data.unactioned.value,  _aqualink_data.unactioned.id, &_aqualink_data);
#else
    aq_programmer(AQ_SET_PUMP_VS_PROGRAM, sval, &_aqualink_data);
#endif
  }
  else if (_aqualink_data.unactioned.type == POOL_HTR_INCREMENT && isRSSA_ENABLED) // RSSA for this to work
  {
    LOG(AQUA_LOG,LOG_NOTICE, "Changing pool heater setpoint by %d | %s\n", _aqualink_data.unactioned.value, sval);
#ifdef NEW_AQ_PROGRAMMER
    aq_programmer(AQ_ADD_RSSADAPTER_POOL_HEATER_TEMP, NULL, _aqualink_data.unactioned.value, AQP_NULL, &_aqualink_data);
#else
    aq_programmer(AQ_ADD_RSSADAPTER_POOL_HEATER_TEMP, sval, &_aqualink_data);
#endif
  }
  else if (_aqualink_data.unactioned.type == SPA_HTR_INCREMENT && isRSSA_ENABLED)  // RSSA for this to work
  {
    LOG(AQUA_LOG,LOG_NOTICE, "Changing spa heater setpoint by %d\n", _aqualink_data.unactioned.value);
#ifdef NEW_AQ_PROGRAMMER
    aq_programmer(AQ_ADD_RSSADAPTER_SPA_HEATER_TEMP, NULL, _aqualink_data.unactioned.value, AQP_NULL, &_aqualink_data);
#else
    aq_programmer(AQ_ADD_RSSADAPTER_SPA_HEATER_TEMP, sval, &_aqualink_data); 
#endif
  } 
  else if (_aqualink_data.unactioned.type == LIGHT_MODE) {
    panel_device_request(&_aqualink_data, LIGHT_MODE, _aqualink_data.unactioned.id, _aqualink_data.unactioned.value, UNACTION_TIMER);
  }
  else if (_aqualink_data.unactioned.type == LIGHT_BRIGHTNESS) {
    panel_device_request(&_aqualink_data, LIGHT_BRIGHTNESS, _aqualink_data.unactioned.id, _aqualink_data.unactioned.value, UNACTION_TIMER);
  }
  else 
  {
    LOG(AQUA_LOG,LOG_ERR, "Unknown request of type %d\n", _aqualink_data.unactioned.type);
  }

  _aqualink_data.unactioned.type = NO_ACTION;
  _aqualink_data.unactioned.value = -1;
  _aqualink_data.unactioned.id = -1;
  _aqualink_data.unactioned.requested = 0;
}

void printHelp()
{
  if (GIT_HASH[0] != '\0') {
    printf("%s %s (rev %s)\n", AQUALINKD_NAME, AQUALINKD_VERSION, GIT_HASH);
  } else {
    printf("%s %s\n", AQUALINKD_NAME, AQUALINKD_VERSION);
  }
  printf("\t-h         (this message)\n");
  printf("\t-d         (do not deamonize)\n");
  printf("\t-c <file>  (Configuration file)\n");
  printf("\t-v         (Debug logging)\n");
  printf("\t-vv        (Serial Debug logging)\n");
  printf("\t-m         (Millisecond timestamps and thread timing)\n");
  printf("\t-rsd       (RS485 debug)\n");
  printf("\t-rsrd      (RS485 raw debug)\n");
}

int main(int argc, char *argv[])
{
#ifdef SELF_RESTART
  _restart = false;
#endif
  char defaultCfg[] = "./aqualinkd.conf";
  char *cfgFile;

  _aqualink_data.num_pumps = 0;
  _aqualink_data.num_lights = 0;
  _aqualink_data.num_sensors = 0;

#ifdef AQ_TM_DEBUG
  addDebugLogMask(DBGT_LOG);
  init_aqd_timer(); // Must clear timers.
#endif
  // Any debug logging masks
  //addDebugLogMask(IAQT_LOG);
  //addDebugLogMask(ONET_LOG);
  //addDebugLogMask(ALLB_LOG);
  //addDebugLogMask(PDA_LOG);
  //addDebugLogMask(NET_LOG);
  //addDebugLogMask(AQUA_LOG);
  //addDebugLogMask(DJAN_LOG);
  //addDebugLogMask(DPEN_LOG);

  if (argc > 1 && strcmp(argv[1], "-h") == 0)
  {
    printHelp();
    return 0;
  }

  // struct lws_context_creation_info info;
  // Log only NOTICE messages and above. Debug and info messages
  // will not be logged to syslog.
#ifndef AQ_MANAGER
  setlogmask(LOG_UPTO(LOG_NOTICE));
#endif

  if (getuid() != 0)
  {
    //LOG(AQUA_LOG,LOG_ERR, "%s Can only be run as root\n", argv[0]);
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Initialize the daemon's parameters.
  init_config();
  cfgFile = defaultCfg;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-h") == 0)
    {
      printHelp();
      return 0;
    }
    if (strcmp(argv[i], "-d") == 0)
    {
      _aqconfig_.deamonize = false;
    }
    else if (strcmp(argv[i], "-c") == 0)
    {
      cfgFile = argv[++i];
    }
    else if (strcmp(argv[i], "-vv") == 0)
    {
      _cmdln_loglevel = LOG_DEBUG_SERIAL;
    }
    else if (strcmp(argv[i], "-v") == 0)
    {
      _cmdln_loglevel = LOG_DEBUG;
    }
    else if (strcmp(argv[i], "-m") == 0)
    {
      _cmdln_log_msec_ts = true;
    }
    else if (strcmp(argv[i], "-rsd") == 0)
    {
      _cmdln_debugRS485 = true;
    }
    else if (strcmp(argv[i], "-rsrd") == 0)
    {
      _cmdln_lograwRS485 = true;
    }
    else if (strcmp(argv[i], "-nc") == 0)
    {
      _cmdln_nostartupcheck = true;
    }
  }

  // Set this here, so it doesn;t get reset if the manager restarts the AqualinkD process.
  _aqualink_data.aqManagerActive = false;

  return startup(argv[0], cfgFile);
}

void check_upgrade_log()
{
  FILE *fp;
  size_t len = 0;
  ssize_t read_size;
  char *line = NULL;

  fp = fopen("/tmp/aqualinkd_upgrade.log", "r");
  if (fp == NULL)
  {
    // No upgrade file
    return;
  }

  LOG(AQUA_LOG,LOG_NOTICE, "--- AqualinkD Upgrade log ----\n");
  while ((read_size = getline(&line, &len, fp)) != -1) {
    LOG(AQUA_LOG,LOG_NOTICE, "%s", line);
  }
  LOG(AQUA_LOG,LOG_NOTICE, "--- End AqualinkD Upgrade log ----\n");

  free(line);
  fclose(fp);

  remove("/tmp/aqualinkd_upgrade.log");
  // Need to delete the file here.
}


int startup(char *self, char *cfgFile) 
{
  _self = self;
  _cfgFile = cfgFile;

  AddAQDstatusMask(CHECKING_CONFIG);
  AddAQDstatusMask(NOT_CONNECTED);
  
  SET_DIRTY(_aqualink_data.is_dirty);
  //_aqualink_data.chiller_button == NULL; // HATE having this here, but needs to be null before config.

  //sd_journal_print(LOG_NOTICE, "Starting %s v%s !\n", AQUALINKD_NAME, AQUALINKD_VERSION);


  // Setup a log level just to get this message out, will be re-set once config is read
  setSystemLogLevel(LOG_NOTICE);
  if (GIT_HASH[0] != '\0') {
    LOG(AQUA_LOG,LOG_NOTICE, "Starting %s v%s (rev %s) !\n", AQUALINKD_NAME, AQUALINKD_VERSION, GIT_HASH);
  } else {
    LOG(AQUA_LOG,LOG_NOTICE, "Starting %s v%s !\n", AQUALINKD_NAME, AQUALINKD_VERSION);
  }

  snprintf(_aqualink_data.self, sizeof(_aqualink_data.self), "%s",
           basename(self));
  clearDebugLogMask();
  read_config(&_aqualink_data, cfgFile);

  if (_cmdln_loglevel != -1)
    _aqconfig_.log_level = _cmdln_loglevel;

  if (_cmdln_debugRS485)
    _aqconfig_.log_protocol_packets = true;

  if (_cmdln_lograwRS485)
    _aqconfig_.log_raw_bytes = true;

  if (_cmdln_log_msec_ts)
    _aqconfig_.log_msec_ts = true;

  setMsecTimestampLog(_aqconfig_.log_msec_ts);

  // Arm the one shot startup time sync, due AQ_STARTUP_TIME_SYNC_DELAY from now so it
  // runs in the background once startup has settled rather than stalling it.  Done here
  // (rather than at the declaration) so a self restart, which calls startup() again,
  // re-arms it.
  if (_aqconfig_.sync_panel_time && _aqconfig_.force_panel_time_sync_at_startup) {
    _force_panel_time_sync_after = time(0) + AQ_STARTUP_TIME_SYNC_DELAY;
    LOG(AQUA_LOG,LOG_NOTICE, "Panel time will be synced in %d seconds (%s=yes)\n",
        AQ_STARTUP_TIME_SYNC_DELAY, CFG_N_force_panel_time_sync_at_startup);
  } else {
    _force_panel_time_sync_after = 0;
  }


#ifdef AQ_MANAGER
  setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, (_aqconfig_.display_warnings_web?_aqualink_data.last_display_message:NULL));
#else
  if (_aqconfig_.display_warnings_web == true)
    setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, _aqconfig_.log_file, _aqualink_data.last_display_message);
  else
    setLoggingPrms(_aqconfig_.log_level, _aqconfig_.deamonize, _aqconfig_.log_file, NULL);
#endif

  //LOG(AQUA_LOG,LOG_NOTICE, "Starting %s v%s !\n", AQUALINKD_NAME, AQUALINKD_VERSION);

  check_upgrade_log();

  check_print_config(&_aqualink_data);
  

  // NSF Below probably should be moved into check_print_config()

  // Sanity check on Device ID's against panel type
  if (isRS_PANEL) {
    if ( is_allbutton_id(_aqconfig_.device_id) || _aqconfig_.device_id == 0x00 ||  _aqconfig_.device_id == 0xFF) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "Device ID 0x%02hhx does not match RS panel, Going to search for ID!\n", _aqconfig_.device_id);
      _aqconfig_.device_id = 0x00;
      //return EXIT_FAILURE;
    }
  } else if (isPDA_PANEL) {
    if ( is_pda_id(_aqconfig_.device_id) || _aqconfig_.device_id == 0x33 ||  _aqconfig_.device_id == 0xFF) {
      if ( _aqconfig_.device_id == 0x33 ) {
        LOG(AQUA_LOG,LOG_NOTICE, "Enabeling iAqualink protocol.\n");
        _aqconfig_.enable_iaqualink = true;
      }
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "Device ID 0x%02hhx does not match PDA panel, please check config!\n", _aqconfig_.device_id);
      if ( !SHOULD_KEEP_RUNNING() ) {return EXIT_FAILURE;}
    }
  } else {
    LOG(AQUA_LOG,LOG_ERR, "Error unknown panel type, please check config!\n");
    if (!SHOULD_KEEP_RUNNING()) {return EXIT_FAILURE;}
  }

  if (_aqconfig_.rssa_device_id != 0x00) {
    if ( is_rsserialadapter_id(_aqconfig_.rssa_device_id) ) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "RSSA Device ID 0x%02hhx does not match RS panel, please check config!\n", _aqconfig_.rssa_device_id);
      if (!SHOULD_KEEP_RUNNING()) {return EXIT_FAILURE;}
    }
  }


  if (_aqconfig_.extended_device_id != 0x00) {
    if ( is_aqualink_touch_id(_aqconfig_.extended_device_id) ||  is_onetouch_id(_aqconfig_.extended_device_id ) ) {
      // We are good
    } else {
      LOG(AQUA_LOG,LOG_ERR, "Extended Device ID 0x%02hhx does not match OneTouch or AqualinkTouch ID, please check config!\n", _aqconfig_.extended_device_id);
      if (!SHOULD_KEEP_RUNNING()) {return EXIT_FAILURE;}
    }
  }



  if (_aqconfig_.deamonize == true)
  {
    char pidfile[256];
    // sprintf(pidfile, "%s/%s.pid",PIDLOCATION, basename(argv[0]));
    //sprintf(pidfile, "%s/%s.pid", "/run", basename(argv[0]));
    //sprintf(pidfile, "%s/%s.pid", "/run", basename(self));
    sprintf(pidfile, "%s/%s.pid", "/run", _aqualink_data.self);
    daemonise(pidfile, main_loop);
  }
  else
  {
    main_loop();
  }

  exit(EXIT_SUCCESS);
}

/*
#define MAX_BLOCK_ACK 12
#define MAX_BUSY_ACK  (50 + MAX_BLOCK_ACK)
*/

/* Point of this is to sent ack as quickly as possible, all checks should be done prior to calling this.*/
void caculate_ack_packet(int rs_fd, unsigned char *packet_buffer, emulation_type source) 
{
  unsigned char *cmd;
  int size;

  switch (source) {
    case ALLBUTTON:
      send_extended_ack(rs_fd, (packet_buffer[PKT_CMD]==CMD_MSG_LONG?ACK_SCREEN_BUSY_SCROLL:ACK_NORMAL), pop_allb_cmd(&_aqualink_data));
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"AllButton Emulation type Processed packet in");
    break;
    case RSSADAPTER:
      send_jandy_command(rs_fd, get_rssa_cmd(packet_buffer[PKT_CMD]), 4);
      remove_rssa_cmd();
    /*
      if (packet_buffer[PKT_CMD] == CMD_PROBE)
        send_extended_ack(rs_fd, 0x00, 0x05);
      else
        send_extended_ack(rs_fd, 0x00, 0x00);*/
    break;
    case ONETOUCH:
      send_extended_ack(rs_fd, ACK_ONETOUCH, pop_ot_cmd(packet_buffer[PKT_CMD]));
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"OneTouch Emulation type Processed packet in");
    break;
    case IAQTOUCH:
      if (packet_buffer[PKT_CMD] != CMD_IAQ_CTRL_READY)
        send_extended_ack(rs_fd, ACK_IAQ_TOUCH, pop_iaqt_cmd(packet_buffer[PKT_CMD]));
      else {
        size = ref_iaqt_control_cmd(&cmd);
        send_jandy_command(rs_fd, cmd, size);
        rem_iaqt_control_cmd(cmd);
      }
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"AquaTouch Emulation type Processed packet in");
    break;
    case IAQUALNK:
      //send_iaqualink_ack(rs_fd, packet_buffer);
      size = get_iaqualink_cmd(packet_buffer[PKT_CMD], &cmd);
      if (size == 2){
        send_extended_ack(rs_fd, cmd[0], cmd[1]);
      } else {
        send_jandy_command(rs_fd, cmd, size);
      }
      remove_iaqualink_cmd();
    break;
#ifdef AQ_PDA
    case AQUAPDA:
      if (_aqconfig_.pda_sleep_mode && pda_shouldSleep()) {
        LOG(PDA_LOG,LOG_DEBUG, "PDA Aqualink daemon in sleep mode\n");
        return;
      } else {
        send_extended_ack(rs_fd, ACK_PDA, pop_pda_cmd(&_aqualink_data));
      }
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"PDA Emulation type Processed packet in");
    break;
#endif
    case SIMULATOR:
      if (_aqualink_data.simulator_active == ALLBUTTON) {
        send_extended_ack(rs_fd, (packet_buffer[PKT_CMD]==CMD_MSG_LONG?ACK_SCREEN_BUSY_SCROLL:ACK_NORMAL), pop_simulator_cmd(packet_buffer[PKT_CMD]));
      } else if (_aqualink_data.simulator_active == ONETOUCH) {
        send_extended_ack(rs_fd, ACK_ONETOUCH, pop_simulator_cmd(packet_buffer[PKT_CMD]));
      } else if (_aqualink_data.simulator_active == IAQTOUCH) {
        LOG(SIM_LOG,LOG_WARNING, "IAQTOUCH not implimented yet!\n");
      } else if (_aqualink_data.simulator_active == AQUAPDA) {
        send_extended_ack(rs_fd, ACK_PDA, pop_simulator_cmd(packet_buffer[PKT_CMD]));
      } else {
        LOG(SIM_LOG,LOG_ERR, "No idea on this protocol (%d), not implimented!!!\n",_aqualink_data.simulator_active);
      }
    break;

    default:
      LOG(AQUA_LOG,LOG_WARNING, "Can't caculate ACK, No idea what packet this source packet was for!\n");
      //DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"Unknown Emulation type Processed packet in");
    break;
  }
}


unsigned char find_unused_address(unsigned char* packet) {
  static int ID[4] = {0,0,0,0};  // 0=0x08, 1=0x09, 2=0x0A, 3=0x0B
  static unsigned char lastID = 0x00;

  if (packet[PKT_DEST] >= 0x08 && packet[PKT_DEST] <= 0x0B && packet[PKT_CMD] == CMD_PROBE) {
    //printf("Probe packet to keypad ID 0x%02hhx\n",packet[PKT_DEST]);
    lastID = packet[PKT_DEST];
  } else if (packet[PKT_DEST] == DEV_MASTER && lastID != 0x00) {
    lastID = 0x00;
  } else if (lastID != 0x00) {
    ID[lastID-8]++;
    if (ID[lastID-8] >= 3) {
      LOG(AQUA_LOG,LOG_NOTICE, "Found valid unused ID 0x%02hhx\n",lastID);
      LOG(AQUA_LOG,LOG_WARNING, "Please add 'device_id=0x%02hhx', to AqualinkD's configuration file\n",lastID);
      return lastID;
    }
    lastID = 0x00;
  } else {
    lastID = 0x00;
  }

  return 0x00;
}

void main_loop()
{
  int exit_code = EXIT_SUCCESS;
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN+1];
  int i;
  //int delayAckCnt = 0;
  bool got_probe = false;
  bool got_probe_extended = false;
  bool got_probe_rssa = false;
  bool print_once = false;
  int blank_read_reconnect = MAX_ZERO_READ_BEFORE_RECONNECT; // Will get reset if non blocking
  bool auto_config_complete = true;


  //_aqualink_data.panelstatus = STARTING;
  AddAQDstatusMask(CHECKING_CONFIG);
  //_aqualink_data.panel_rev = NULL;
  //_aqualink_data.panel_cpu = NULL;
  //_aqualink_data.panel_string = NULL;
  SET_DIRTY(_aqualink_data.is_dirty);
  sprintf(_aqualink_data.last_display_message, "%s", "Connecting to Control Panel");
  _aqualink_data.is_display_message_programming = false;
  //_aqualink_data.simulate_panel = false;
  _aqualink_data.active_thread.thread_id = 0;
  _aqualink_data.air_temp = TEMP_UNKNOWN;
  _aqualink_data.pool_temp = TEMP_UNKNOWN;
  _aqualink_data.spa_temp = TEMP_UNKNOWN;
  _aqualink_data.frz_protect_set_point = TEMP_UNKNOWN;
  _aqualink_data.pool_htr_set_point = TEMP_UNKNOWN;
  _aqualink_data.spa_htr_set_point = TEMP_UNKNOWN;
  _aqualink_data.chiller_set_point = TEMP_UNKNOWN;
  //_aqualink_data.chiller_state = LED_S_UNKNOWN;
  _aqualink_data.unactioned.type = NO_ACTION;
  _aqualink_data.swg_percent = TEMP_UNKNOWN;
  _aqualink_data.swg_ppm = TEMP_UNKNOWN;
  _aqualink_data.ar_swg_device_status = SWG_STATUS_UNKNOWN;
  _aqualink_data.heater_err_status = NUL; // 0x00 is no error
  _aqualink_data.swg_led_state = LED_S_UNKNOWN;
  _aqualink_data.swg_delayed_percent = TEMP_UNKNOWN;
  _aqualink_data.temp_units = UNKNOWN;
  _aqualink_data.service_mode_state = OFF;
  _aqualink_data.frz_protect_state = OFF;
  _aqualink_data.battery = OK;
  _aqualink_data.open_websockets = 0;
  _aqualink_data.ph = TEMP_UNKNOWN;
  _aqualink_data.orp = TEMP_UNKNOWN;
  _aqualink_data.simulator_id = NUL;
  _aqualink_data.simulator_active = SIM_NONE;
  _aqualink_data.boost_duration = 0;
  _aqualink_data.boost = false;
  

  pthread_mutex_init(&_aqualink_data.active_thread.thread_mutex, NULL);
  pthread_cond_init(&_aqualink_data.active_thread.thread_cond, NULL);

  //for (i=0; i < MAX_PUMPS; i++) {
  for (i=0; i < _aqualink_data.num_pumps; i++) {
    _aqualink_data.pumps[i].rpm = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].gpm = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].watts = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].mode = TEMP_UNKNOWN;
    //_aqualink_data.pumps[i].driveState = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].status = TEMP_UNKNOWN;
    _aqualink_data.pumps[i].pStatus = PS_OFF;
    _aqualink_data.pumps[i].pressureCurve = TEMP_UNKNOWN;

    if (_aqualink_data.pumps[i].maxSpeed <= 0) {
      _aqualink_data.pumps[i].maxSpeed = (_aqualink_data.pumps[i].pumpType==VFPUMP?PUMP_GPM_MAX:PUMP_RPM_MAX);
    }
    if (_aqualink_data.pumps[i].minSpeed <= 0) {
      _aqualink_data.pumps[i].minSpeed = (_aqualink_data.pumps[i].pumpType==VFPUMP?PUMP_GPM_MIN:PUMP_RPM_MIN);
    }

    //printf("arrayindex=%d, pump=%d, min=%d, max=%d\n",i,_aqualink_data.pumps[i].pumpIndex, _aqualink_data.pumps[i].minSpeed ,_aqualink_data.pumps[i].maxSpeed);
  }

  for (i=0; i < MAX_LIGHTS; i++) {
     //_aqualink_data.lights[i].currentValue = TEMP_UNKNOWN;
     _aqualink_data.lights[i].currentValue = 0;
     _aqualink_data.lights[i].RSSDstate = OFF;
  }

  for (i=0; i < _aqualink_data.num_sensors; i++) {
    _aqualink_data.sensors[i].value = TEMP_UNKNOWN;
  }

  if (ENABLE_SWG) {
    //_aqualink_data.ar_swg_device_status = SWG_STATUS_OFF;
    _aqualink_data.swg_led_state = OFF;
    _aqualink_data.swg_percent = 0;
    _aqualink_data.swg_ppm = 0;
  }

  if (ENABLE_CHEM_FEEDER) {
    _aqualink_data.ph = 0;
    _aqualink_data.orp = 0;
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);
  signal(SIGQUIT, intHandler);
  signal(SIGRESTART, intHandler);
  signal(SIGRUPGRADE, intHandler);

  if (!start_net_services(&_aqualink_data))
  {
    LOG(AQUA_LOG,LOG_ERR, "Can not start webserver at address %s.\n", _aqconfig_.listen_address);
    exit(EXIT_FAILURE);
  }

  startPacketLogger();

  int blank_read = 0;

  rs_fd = init_serial_port(_aqconfig_.serial_port);

  if (is_valid_port(rs_fd)) {
    LOG(AQUA_LOG,LOG_NOTICE, "Listening to Aqualink %s on serial port: %s\n", getPanelString(), _aqconfig_.serial_port);
  } else {
    LOG(AQUA_LOG,LOG_ERR, "Bad serial port: %s\n", _aqconfig_.serial_port);
    AddAQDstatusMask(ERROR_SERIAL);
  }
/*
#ifdef AQ_PDA
  if (isPDA_PANEL) {
    init_pda(&_aqualink_data);
    if (_aqconfig_.extended_device_id != 0x00)
    {
      LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon can't use extended_device_id in PDA mode, ignoring value '0x%02hhx' from cfg\n",_aqconfig_.extended_device_id);
      _aqconfig_.extended_device_id = 0x00;
      _aqconfig_.extended_device_id_programming = false;
    }
  }
#endif
*/
  // Set probes to true for any device we are not searching for.
   
  RemoveAQDstatusMask(CHECKING_CONFIG);
  SET_DIRTY(_aqualink_data.is_dirty);
  
  if (_aqconfig_.rssa_device_id == 0x00)
    got_probe_rssa = true;

  if (_aqconfig_.extended_device_id == 0x00)
    got_probe_extended = true;

  if (_aqconfig_.device_id == 0x00) {
    LOG(AQUA_LOG,LOG_WARNING, "Searching for valid ID, please configure `device_id` for faster startup");
  }

  if (_aqconfig_.device_id == 0xFF) {
    LOG(AQUA_LOG,LOG_NOTICE, "Waiting for Control Panel information");
    LOG(AQUA_LOG,LOG_WARNING, "Using Auto configure, this will take some time, (make sure to update aqualinkd configuration to speed up startup!)\n");
    auto_config_complete = false;
    //_aqualink_data.panelstatus = LOOKING_IDS;
    AddAQDstatusMask(AUTOCONFIGURE_ID);
    SET_DIRTY(_aqualink_data.is_dirty);
  } else {
    LOG(AQUA_LOG,LOG_NOTICE, "Waiting for Control Panel probe\n");
    //_aqualink_data.panelstatus = CONECTING;
    AddAQDstatusMask(CONNECTING);
    SET_DIRTY(_aqualink_data.is_dirty);
  }
  i=0;

  // Loop until we get the probe messages, that means we didn;t start too soon after last shutdown.
  while ( is_valid_port(rs_fd) && (got_probe == false || got_probe_rssa == false || got_probe_extended == false || auto_config_complete == false) && _keepRunning == true && _cmdln_nostartupcheck == false)
  {
    if (blank_read == blank_read_reconnect / 2) {
      LOG(AQUA_LOG,LOG_ERR, "Nothing read on '%s', are you sure that's right?\n",_aqconfig_.serial_port);
        // Reset blank reads here, we want to ignore TTY errors in container to keep it running
        blank_read = 1;
      if (_aqconfig_.device_id == 0x00) {
        blank_read = 1; // if device id=0x00 it's code for don't exit
      }
      SET_DIRTY(_aqualink_data.is_dirty); // Make sure to show erros if ui is up
    } else if (blank_read == blank_read_reconnect*2 ) {
      if (SHOULD_KEEP_RUNNING()){
        LOG(AQUA_LOG,LOG_ERR, "You are wasting my time, please check '%s'\n",_aqconfig_.serial_port);
      } else {
        LOG(AQUA_LOG,LOG_ERR, "I'm done, exiting, please check '%s'\n",_aqconfig_.serial_port);
        stopPacketLogger();
        close_serial_port(rs_fd);
        stop_net_services();
        stop_sensors_thread();
        exit_code=EXIT_FAILURE;
        _keepRunning=false;
        return;
      }
    }
/*
    if (_aqconfig_.log_raw_RS_bytes)
      packet_length = get_packet_lograw(rs_fd, packet_buffer);
    else
      packet_length = get_packet(rs_fd, packet_buffer);
*/ 
    packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length > 0 && auto_config_complete == false) {
      blank_read = 0;
      auto_config_complete = auto_configure(&_aqualink_data, packet_buffer, packet_length, rs_fd);
      AddAQDstatusMask(AUTOCONFIGURE_ID);
      SET_DIRTY(_aqualink_data.is_dirty);
      if (auto_config_complete) {
        //if (_aqconfig_.device_id != 0x00)
          got_probe = true;
        //if (_aqconfig_.rssa_device_id != 0x00)
          got_probe_rssa = true;
        //if (_aqconfig_.extended_device_id != 0x00)
          got_probe_extended = true;
      }
      continue;
    }
    if (packet_length > 0 && _aqconfig_.device_id == 0x00) {
      blank_read = 0;
      AddAQDstatusMask(AUTOCONFIGURE_ID);
      SET_DIRTY(_aqualink_data.is_dirty);
      _aqconfig_.device_id = find_unused_address(packet_buffer);
      continue;
    }
    else if (packet_length > 0 && packet_buffer[PKT_DEST] == _aqconfig_.device_id && got_probe == false) {
      blank_read = 0;
      if (packet_buffer[PKT_CMD] == CMD_PROBE) {
         got_probe = true;
         LOG(AQUA_LOG,LOG_NOTICE, "Got probe on '0x%02hhx' Standard Protocol\n",_aqconfig_.device_id);
      } else {
        if(!print_once) {
          LOG(AQUA_LOG,LOG_NOTICE, "Got message but no probe on '0x%02hhx', did we start too soon? (waiting for probe)\n",_aqconfig_.device_id);
          print_once=true;
        } 
      }
      // NSF Should put some form of timeout here and exit.
    }
    else if (packet_length > 0 && packet_buffer[PKT_DEST] == _aqconfig_.rssa_device_id && got_probe_rssa == false) {
      blank_read = 0;
      if (packet_buffer[PKT_CMD] == CMD_PROBE) {
         got_probe_rssa = true;
         LOG(AQUA_LOG,LOG_NOTICE, "Got probe on '0x%02hhx' RS SerialAdapter Protocol\n",_aqconfig_.rssa_device_id);
      } else {
        if(!print_once) {
          LOG(AQUA_LOG,LOG_NOTICE, "Got message but no probe on '0x%02hhx', did we start too soon? (waiting for probe)\n",_aqconfig_.rssa_device_id);
          print_once=true;
        } 
      }
      // NSF Should put some form of timeout here and exit.
    }
    else if (packet_length > 0 && packet_buffer[PKT_DEST] == _aqconfig_.extended_device_id && got_probe_extended == false) {
      blank_read = 0;
      if (packet_buffer[PKT_CMD] == CMD_PROBE) {
         got_probe_extended = true;
         LOG(AQUA_LOG,LOG_NOTICE, "Got probe on '0x%02hhx' Extended Protocol\n",_aqconfig_.extended_device_id);
      } else {
        if(!print_once) {
          LOG(AQUA_LOG,LOG_NOTICE, "Got message but no probe on '0x%02hhx', did we start too soon? (waiting for probe)\n",_aqconfig_.extended_device_id);
          print_once=true;
        }
      }
      // NSF Should put some form of timeout here and continue with no extended ID
    }

    else if (packet_length <= 0) {
      blank_read++;
      LOG(AQUA_LOG,LOG_DEBUG, "Blank RS485 read\n");
    }
    else if (packet_length > 0) {
      blank_read = 0;
      if (i++ > 2000) {
        if(!got_probe) {
          if (_aqconfig_.deamonize) {
            LOG(AQUA_LOG,LOG_ERR, "No probe on device_id '0x%02hhx', Can't start! (please check config)\n",_aqconfig_.device_id);
            i=0;
          } else {
            if (SHOULD_KEEP_RUNNING()){
              LOG(AQUA_LOG,LOG_ERR, "You are wasting my time, please check config line 'device_id = 0x%02hhx'\n",_aqconfig_.device_id);
            } else {
              LOG(AQUA_LOG,LOG_ERR, "No probe on device_id '0x%02hhx', giving up! (please check config)\n",_aqconfig_.device_id);
              stopPacketLogger();
              close_serial_port(rs_fd);
              stop_net_services();
              stop_sensors_thread();
              return;
            }
          }  
        }
        if(!got_probe_rssa) {
          LOG(AQUA_LOG,LOG_ERR, "No probe on '0x%02hhx', disabling rssa_device_id (please check config)\n",_aqconfig_.rssa_device_id);
          _aqconfig_.rssa_device_id = 0x00;
          got_probe_rssa = true;
        }
        if(!got_probe_extended) {
          LOG(AQUA_LOG,LOG_ERR, "No probe on '0x%02hhx', disabling extended_device_id (please check config)\n",_aqconfig_.extended_device_id);
          _aqconfig_.extended_device_id = 0x00;
          _aqconfig_.extended_device_id_programming = false;
          _aqconfig_.enable_iaqualink = false;
          got_probe_extended = true;
        }
      }
    }
  }
  
  RemoveAQDstatusMask(AUTOCONFIGURE_ID);
  RemoveAQDstatusMask(NOT_CONNECTED);
  AddAQDstatusMask(CONNECTING);
  SET_DIRTY(_aqualink_data.is_dirty);

  //At this point we should have correct ID and seen probes on those ID's.
  // Setup the panel
  if (_aqconfig_.device_id <= 0x08 && _aqconfig_.device_id >= 0x0B && _aqconfig_.device_id != 0x60 && _aqconfig_.device_id != 0x33) {
    LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon has no valid device_id, can't connect to control panel");
    //_aqualink_data.panelstatus = NO_IDS_ERROR;
    RemoveAQDstatusMask(CONNECTING); // Not sure if we should remove this
    AddAQDstatusMask(ERROR_NO_DEVICE_ID);
    SET_DIRTY(_aqualink_data.is_dirty);
  }

#ifdef AQ_PDA
  if (isPDA_PANEL) {
    init_pda(&_aqualink_data);
    if (_aqconfig_.extended_device_id != 0x00)
    {
      LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon can't use extended_device_id in PDA mode, ignoring value '0x%02hhx' from cfg\n",_aqconfig_.extended_device_id);
      _aqconfig_.extended_device_id = 0x00;
      _aqconfig_.extended_device_id_programming = false;
    }
  }
#endif

  if ( is_rsserialadapter_id(_aqconfig_.rssa_device_id )) {
    addPanelRSserialAdapterInterface();
  }

  if ( is_onetouch_id(_aqconfig_.extended_device_id)) {
    addPanelOneTouchInterface();
  } else if ( is_aqualink_touch_id(_aqconfig_.extended_device_id)) {
    addPanelIAQTouchInterface();
  }

  // We can only get panel size info from extended ID
  if (_aqconfig_.extended_device_id != 0x00) {
    RemoveAQDstatusMask(AUTOCONFIGURE_PANEL);
    SET_DIRTY(_aqualink_data.is_dirty);
  }

  if (_aqconfig_.extended_device_id_programming == true && (isONET_ENABLED || isIAQT_ENABLED) )
  {
    changePanelToExtendedIDProgramming();
  } else if (_aqconfig_.extended_device_id_programming == true) {
    LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon has no valid extended_device_id, ignoring value '%s' from cfg\n",CFG_N_extended_device_id_programming);
    _aqconfig_.extended_device_id = 0x00;
    _aqconfig_.extended_device_id_programming = false;
  }



  if ( _aqualink_data.num_sensors > 0){
    start_sensors_thread(&_aqualink_data);
  }

  /*
   *
   *    This is the main loop  
   * 
   * 
   * 
   */

  LOG(AQUA_LOG,LOG_NOTICE, "Starting communication with Control Panel\n");

  // Not the best way to do this, but ok for moment

  //int loopnum=0;
  blank_read = 0;
  // OK, Now go into infinate loop
  while (_keepRunning == true)
  {
    //printf("%d ",blank_read);
    while ((rs_fd < 0 || blank_read >= blank_read_reconnect) && _keepRunning == true)
    {
      //printf("rs_fd  =% d\n",rs_fd);
      if (!is_valid_port(rs_fd))
      {
        sleep(1);
        LOG(AQUA_LOG,LOG_ERR, "Bad serial port '%s', are you sure that's right?\n",_aqconfig_.serial_port);   
        sprintf(_aqualink_data.last_display_message, CONNECTION_ERROR);
        //LOG(AQUA_LOG,LOG_ERR, "Serial port error, Aqualink daemon waiting to connect to master device...\n");    
        SET_DIRTY(_aqualink_data.is_dirty);
        AddAQDstatusMask(ERROR_SERIAL);
        //broadcast_aqualinkstate_error(CONNECTION_ERROR);
        broadcast_aqualinkstate_error(getAqualinkDStatusMessage(&_aqualink_data));
        sleep(10);
        // broadcast_aqualinkstate_error(mgr.active_connections, "No connection to RS control panel");
      }
      else
      {
        sprintf(_aqualink_data.last_display_message, CONNECTION_ERROR);
        LOG(AQUA_LOG,LOG_ERR, "Aqualink daemon looks like serial error, resetting.\n");
        SET_DIRTY(_aqualink_data.is_dirty);
        AddAQDstatusMask(ERROR_SERIAL);
        //broadcast_aqualinkstate_error(CONNECTION_ERROR);
        broadcast_aqualinkstate_error(getAqualinkDStatusMessage(&_aqualink_data));
        close_serial_port(rs_fd);
        //rs_fd = init_serial_port(_aqconfig_.serial_port);
      }
      rs_fd = init_serial_port(_aqconfig_.serial_port);
      blank_read = 0;
    }

#ifdef AQ_MANAGER
    if (_aqualink_data.run_slogger) {
       LOG(AQUA_LOG,LOG_WARNING, "Starting rs485mon, this will take some time!\n");
       broadcast_aqualinkstate_error(CONNECTION_RUNNING_SLOG);

       if (_aqualink_data.slogger_debug)
         addDebugLogMask(SLOG_LOG);

       rs485mon(rs_fd, _aqconfig_.serial_port, _aqualink_data.slogger_debug?LOG_DEBUG:getSystemLogLevel(), _aqualink_data.slogger_packets, _aqualink_data.slogger_ids);
       _aqualink_data.run_slogger = false;

       if (_aqualink_data.slogger_debug)
         removeDebugLogMask(SLOG_LOG);
    }
#endif


    packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length <= 0 && _keepRunning)
    {
      // AQSERR_2SMALL // no reset (-5)
      // AQSERR_2LARGE // no reset (-4)
      // AQSERR_CHKSUM // no reset (-3)
      // AQSERR_TIMEOUT // reset blocking mode (-2)
      // AQSERR_READ // reset (-1)   
      if (packet_length == AQSERR_TIMEOUT) {
        LOG(AQUA_LOG,LOG_WARNING, "Timeout read on serial port\n");
        //blank_read = blank_read_reconnect;
      } else if (packet_length == AQSERR_READ) {
        LOG(AQUA_LOG,LOG_ERR, "Error read on serial port, resetting\n");
        blank_read = blank_read_reconnect;
      } else {
        // In non blocking, so sleep for 2 milliseconds
        LOG(AQUA_LOG,LOG_WARNING, "Nothing read on serial port\n");
      }
      //if (blank_read > max_blank_read) {
      //  LOG(AQUA_LOG,LOG_NOTICE, "Nothing read on serial %d\n",blank_read);
      //  max_blank_read = blank_read;
      //}
      blank_read++;
    }
    else if (packet_length > 0)
    {
      RemoveAQDstatusMask(ERROR_SERIAL);
      RemoveAQDstatusMask(CONNECTING);
      AddAQDstatusMask(CONNECTED);
      //_aqualink_data.is_dirty = true;
      DEBUG_TIMER_START(&_rs_packet_timer);

      blank_read = 0;
      //changed = false;

      bool simulator_packet = false;

      if (_aqualink_data.simulator_active != SIM_NONE) {
        // Check if we have a valid connection
        if ( _aqualink_data.simulator_id != NUL && packet_buffer[PKT_DEST] == _aqualink_data.simulator_id) {
          // ACK before broadcasting to WebSockets.  The broadcast can block long
          // enough for the panel to retransmit and collide with a late ACK.
          caculate_ack_packet(rs_fd, packet_buffer, SIMULATOR);
          simulator_packet = true;
        }
        else if ( _aqualink_data.simulator_id == NUL   
                  && packet_buffer[PKT_CMD] == CMD_PROBE 
                  && packet_buffer[PKT_DEST] != _aqconfig_.device_id // Check no conflicting id's
                  && packet_buffer[PKT_DEST] != _aqconfig_.extended_device_id // Check no conflicting id's
                  ) 
        {
          if (is_simulator_packet(&_aqualink_data, packet_buffer, packet_length)) {
            _aqualink_data.simulator_id = packet_buffer[PKT_DEST];
            // reply to probe
            LOG(SIM_LOG,LOG_NOTICE, "Got probe on '0x%02hhx', using for simulator ID\n",packet_buffer[PKT_DEST]);
            caculate_ack_packet(rs_fd, packet_buffer, SIMULATOR);
            simulator_packet = true;
          } else {
            LOG(SIM_LOG,LOG_INFO, "Got probe on '0x%02hhx' Still waiting for valid simulator probe\n",packet_buffer[PKT_DEST]);
          }
        }
      }

      // Process and packets of devices we are acting as
      if (packet_length > 0 && getProtocolType(packet_buffer) == JANDY && packet_buffer[PKT_DEST] != 0x00 &&
          (packet_buffer[PKT_DEST] == _aqconfig_.device_id ||
           packet_buffer[PKT_DEST] == _aqconfig_.rssa_device_id ||
           packet_buffer[PKT_DEST] == _aqconfig_.extended_device_id ||
           packet_buffer[PKT_DEST] == _aqconfig_.extended_device_id2
           ))
      {
        AddAQDstatusMask(CONNECTED);
        switch(getJandyDeviceType(packet_buffer[PKT_DEST])){
          case ALLBUTTON:
            process_allbutton_packet(packet_buffer, packet_length, &_aqualink_data);
            if (!simulator_packet)
              caculate_ack_packet(rs_fd, packet_buffer, ALLBUTTON);
          break;
          case RSSADAPTER:
            process_rssadapter_packet(packet_buffer, packet_length, &_aqualink_data);
            if (!simulator_packet)
              caculate_ack_packet(rs_fd, packet_buffer, RSSADAPTER);
          break;
          case IAQTOUCH:
            process_iaqtouch_packet(packet_buffer, packet_length, &_aqualink_data);
            if (!simulator_packet)
              caculate_ack_packet(rs_fd, packet_buffer, IAQTOUCH);
          break;
          case ONETOUCH:
            process_onetouch_packet(packet_buffer, packet_length, &_aqualink_data);
            if (!simulator_packet)
              caculate_ack_packet(rs_fd, packet_buffer, ONETOUCH);
          break;
          case AQUAPDA:
            process_pda_packet(packet_buffer, packet_length);
            if (!simulator_packet)
              caculate_ack_packet(rs_fd, packet_buffer, AQUAPDA);
          break;
          case IAQUALNK:
            process_iaqualink_packet(packet_buffer, packet_length, &_aqualink_data);
            if (!simulator_packet)
              caculate_ack_packet(rs_fd, packet_buffer, IAQUALNK);
          break;
          default:
          break;
        }
#ifdef AQ_TM_DEBUG
        char message[128];
        sprintf(message,"%s Emulation Processed packet in",getJandyDeviceName(getJandyDeviceType(packet_buffer[PKT_DEST])));
        DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,message);
#endif
      }
      // Process any packets to readonly devices.
      else if (!simulator_packet && packet_length > 0 && _aqconfig_.read_RS485_devmask > 0)
      {
        if (getProtocolType(packet_buffer) == JANDY)
        {
          processJandyPacket(packet_buffer, packet_length, &_aqualink_data);
        }
        // Process Pentair Device Packed (pentair have to & from in message, so no need to)
        else if (getProtocolType(packet_buffer) == PENTAIR && READ_RSDEV_vsfPUMP) {
          processPentairPacket(packet_buffer, packet_length, &_aqualink_data);
          // In the future probably add code to catch device offline (ie missing reply message)
        }
        DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"Processed (readonly) packet in");
      } else if (!simulator_packet) {
        DEBUG_TIMER_CLEAR(_rs_packet_timer); // Clear timer, no need to print anything
      }

      if (simulator_packet) {
        // Keep normal device processing above when the simulator shares the
        // configured PDA ID, but publish the packet only after its ACK is sent.
        processSimulatorPacket(packet_buffer, packet_length, &_aqualink_data);
        DEBUG_TIMER_STOP(_rs_packet_timer,AQUA_LOG,"Simulator Emulation Processed packet in");
      }
    }
    // Any unactioned commands
    if (_aqualink_data.unactioned.type != NO_ACTION)
    {
      time_t now;
      time(&now);
      if (difftime(now, _aqualink_data.unactioned.requested) > 2)
      {
        LOG(AQUA_LOG,LOG_DEBUG, "Actioning delayed request\n");
        action_delayed_request();
      }
    }

    
    //tcdrain(rs_fd); // Make sure buffer has been sent.
    //delay(10);
  }
  
  //if (_aqconfig_.debug_RSProtocol_packets) stopPacketLogger();
  stopPacketLogger();

#ifdef SELF_RESTART
  if (! _restart) 
#endif
  {
     // Stop network if we are not restarting
     stop_net_services();
     stop_sensors_thread();
  }

  // Reset and close the port.
  close_serial_port(rs_fd);
  // Clear webbrowser
  //mg_mgr_free(&mgr);

  #ifdef SELF_RESTART
   if (! _restart) {
    LOG(AQUA_LOG,LOG_WARNING, "Waiting for process to fininish!\n");
    delay(5 * 1000);
    LOG(AQUA_LOG,LOG_WARNING, "Restarting!\n");
    _keepRunning = true;
    _restart = false;
    startup(_self, _cfgFile);
   } else
  #else
  {
    // NSF need to run through config memory and clean up.
    LOG(AQUA_LOG,LOG_NOTICE, "Exit!\n");
    //exit(EXIT_FAILURE);
    exit(exit_code);
  }
  #endif

}

/*

void debugtestePump()
{
  LOG(DJAN_LOG, LOG_INFO, "Jandy Pump code check\n");

  unsigned char toPumpWatts[] = {0x10,0x02,0x78,0x45,0x00,0x05,0xd4,0x10,0x03};
  unsigned char fromPumpWatts[] = {0x10,0x02,0x00,0x1f,0x45,0x00,0x05,0x1d,0x05,0x9d,0x10,0x03};
                                 

  unsigned char toPumpRPM[] = {0x10,0x02,0x78,0x44,0x00,0x60,0x27,0x55,0x10,0x03};
  unsigned char fromPumpRPM[] = {0x10,0x02,0x00,0x1f,0x44,0x00,0x60,0x27,0x00,0xfc,0x10,0x03};

  processJandyPacket(toPumpWatts, 8, &_aqualink_data);
  processJandyPacket(fromPumpWatts, 11, &_aqualink_data);

  processJandyPacket(toPumpRPM, 8, &_aqualink_data);
  processJandyPacket(fromPumpRPM, 11, &_aqualink_data);
}
*/
