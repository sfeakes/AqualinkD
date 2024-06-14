#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "aqualink.h"
#include "utils.h"
#include "aq_timer.h"


struct timerthread {
  pthread_t thread_id;
  pthread_mutex_t thread_mutex;
  pthread_cond_t thread_cond;
  aqkey *button;
  int deviceIndex;
  struct aqualinkdata *aq_data;
  int duration_min;
  struct timespec timeout;
  time_t started_at;
  struct timerthread *next;
  struct timerthread *prev;
};

/*volatile*/ static struct timerthread *_timerthread_ll = NULL;

void *timer_worker( void *ptr );

struct timerthread *find_timerthread(aqkey *button)
{
  struct timerthread *t_ptr;

  if (_timerthread_ll != NULL) {
    for (t_ptr = _timerthread_ll; t_ptr != NULL; t_ptr = t_ptr->next) {
      if (t_ptr->button == button) {
        return t_ptr;
      }
    }
  }

  return NULL;
}

int get_timer_left(aqkey *button)
{
  struct timerthread *t_ptr = find_timerthread(button);

  if (t_ptr != NULL) {
    time_t now = time(0);
    double seconds = difftime(now, t_ptr->started_at);
    return (int) ((t_ptr->duration_min - (seconds / 60)) +0.5) ;
  }

  return 0;
}

void clear_timer(struct aqualinkdata *aq_data, /*aqkey *button,*/ int deviceIndex)
{
  //struct timerthread *t_ptr = find_timerthread(button);
  struct timerthread *t_ptr = find_timerthread(&aq_data->aqbuttons[deviceIndex]);

  if (t_ptr != NULL) {
    LOG(TIMR_LOG, LOG_INFO, "Clearing timer for '%s'\n",t_ptr->button->name);
    t_ptr->duration_min = 0;
    pthread_cond_broadcast(&t_ptr->thread_cond);
  }
}

void start_timer(struct aqualinkdata *aq_data, /*aqkey *button,*/ int deviceIndex, int duration)
{
  aqkey *button = &aq_data->aqbuttons[deviceIndex];
  struct timerthread *t_ptr = find_timerthread(button);

  if (t_ptr != NULL) {
    LOG(TIMR_LOG, LOG_INFO, "Timer already active for '%s', resetting\n",t_ptr->button->name);
    t_ptr->duration_min = duration;
    pthread_cond_broadcast(&t_ptr->thread_cond);
    return;
  }

  struct timerthread *tmthread = calloc(1, sizeof(struct timerthread));
  tmthread->aq_data = aq_data;
  tmthread->button = button;
  tmthread->deviceIndex = deviceIndex;
  tmthread->thread_id = 0;
  tmthread->duration_min = duration;
  tmthread->next = NULL;

  if( pthread_create( &tmthread->thread_id , NULL ,  timer_worker, (void*)tmthread) < 0) {
    LOG(TIMR_LOG, LOG_ERR, "could not create timer thread for button '%s'\n",button->name);
    free(tmthread);
    return;
  }

  if (_timerthread_ll == NULL) {
    _timerthread_ll = tmthread;
    _timerthread_ll->prev = NULL;
    //LOG(TIMR_LOG, LOG_NOTICE, "Added Timer '%s' at beginning LL\n",_timerthread_ll->button->name);
  }
  else
  {
    for (t_ptr = _timerthread_ll; t_ptr->next != NULL; t_ptr = t_ptr->next) {} // Simply run to the end of the list
    t_ptr->next = tmthread;
    tmthread->prev = t_ptr;
    //LOG(TIMR_LOG, LOG_NOTICE, "Added Timer '%s' at end LL \n",tmthread->button->name);
  }
  
  if ( tmthread->thread_id != 0 ) {
    pthread_detach(tmthread->thread_id);
  }
}

#define WAIT_TIME_BEFORE_ON_CHECK 1000 // 1 second

void *timer_worker( void *ptr )
{
  struct timerthread *tmthread;
  tmthread = (struct timerthread *) ptr;
  int retval = 0;

  LOG(TIMR_LOG, LOG_NOTICE, "Start timer for '%s'\n",tmthread->button->name);

  // Add mask so we know timer is active
  tmthread->button->special_mask |= TIMER_ACTIVE;

#ifndef PRESTATE_ONOFF
  delay(WAIT_TIME_BEFORE_ON_CHECK);
  LOG(TIMR_LOG, LOG_DEBUG, "wait finished for button state '%s'\n",tmthread->button->name);
#endif

  // device should be on, but check, ignore for PDA as that may not have been turned on yet
  if (!isPDA_PANEL && tmthread->button->led->state == OFF) {
    if ((tmthread->button->special_mask & PROGRAM_LIGHT) == PROGRAM_LIGHT && in_light_programming_mode(tmthread->aq_data)) {
      LOG(TIMR_LOG, LOG_NOTICE, "Not turning on '%s' as programmer is\n",tmthread->button->name);
    } else {
      LOG(TIMR_LOG, LOG_NOTICE, "turning on '%s'\n",tmthread->button->name);
      panel_device_request(tmthread->aq_data, ON_OFF, tmthread->deviceIndex, false, NET_TIMER);
    }
  }

  pthread_mutex_lock(&tmthread->thread_mutex);

  do {
    if (retval != 0) {
      LOG(TIMR_LOG, LOG_ERR, "pthread_cond_timedwait failed for '%s', error %d %s\n",tmthread->button->name,retval,strerror(retval));
      break;
    } else if (tmthread->duration_min <= 0) {
      //LOG(TIMR_LOG, LOG_INFO, "Timer has been reset to 0 for '%s'\n",tmthread->button->name);
      break;
    }
    clock_gettime(CLOCK_REALTIME, &tmthread->timeout);
    tmthread->timeout.tv_sec += (tmthread->duration_min * 60);
    tmthread->started_at = time(0);
    LOG(TIMR_LOG, LOG_INFO, "Will turn off '%s' in %d minutes\n",tmthread->button->name, tmthread->duration_min);
  } while ((retval = pthread_cond_timedwait(&tmthread->thread_cond, &tmthread->thread_mutex, &tmthread->timeout)) != ETIMEDOUT);


  pthread_mutex_unlock(&tmthread->thread_mutex);

  LOG(TIMR_LOG, LOG_NOTICE, "End timer for '%s'\n",tmthread->button->name);    

  if (tmthread->button->led->state != OFF) {
    LOG(TIMR_LOG, LOG_INFO, "Timer waking turning '%s' off\n",tmthread->button->name);
    panel_device_request(tmthread->aq_data, ON_OFF, tmthread->deviceIndex, false, NET_TIMER);
  } else {
    LOG(TIMR_LOG, LOG_INFO, "Timer waking '%s' is already off\n",tmthread->button->name);
  }

  // remove mask so we know timer is dead
  tmthread->button->special_mask &= ~ TIMER_ACTIVE;

  if (tmthread->next != NULL && tmthread->prev != NULL){
    // Middle of linked list
    tmthread->next->prev = tmthread->prev;
    tmthread->prev->next = tmthread->next;
    //LOG(TIMR_LOG, LOG_NOTICE, "Removed Timer '%s' from middle LL\n",tmthread->button->name);
  } else if (tmthread->next == NULL && tmthread->prev != NULL){
    // end of linked list
    tmthread->prev->next = NULL;
    //LOG(TIMR_LOG, LOG_NOTICE, "Removed Timer '%s' from end LL\n",tmthread->button->name);
  } else if (tmthread->next != NULL && tmthread->prev == NULL){
    // beginning of linked list
    _timerthread_ll = tmthread->next;
    _timerthread_ll->prev = NULL;
    //LOG(TIMR_LOG, LOG_NOTICE, "Removed Timer '%s' from beginning LL\n",tmthread->button->name);
  } else if (tmthread->next == NULL && tmthread->prev == NULL){
    // only item in list
    _timerthread_ll = NULL;
    //LOG(TIMR_LOG, LOG_NOTICE, "Removed Timer '%s' last LL\n",tmthread->button->name);
  }

  free(tmthread);
  pthread_exit(0);

  return ptr;
}
