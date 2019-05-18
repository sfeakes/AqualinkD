/*
 * net_services_habridge.c
 *
 *  Created on: Apr 29, 2019
 *      Author: Lee_Ballard
 */

#include <syslog.h>
#include <pthread.h>
#include "net_services_habridge.h"
#include "aqualink.h"

static pthread_mutex_t _habridge_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t _habridge_update_cond = PTHREAD_COND_INITIALIZER;
static time_t _last_habridge_forced_update;

static aqledstate _last_habridge_ledstate[TOTAL_BUTTONS] = {
    LED_S_UNKNOWN,LED_S_UNKNOWN,LED_S_UNKNOWN,LED_S_UNKNOWN,LED_S_UNKNOWN,
    LED_S_UNKNOWN,LED_S_UNKNOWN,LED_S_UNKNOWN,LED_S_UNKNOWN,LED_S_UNKNOWN,
    LED_S_UNKNOWN,LED_S_UNKNOWN
};

struct habridge_updater_data
{
  struct aqconfig *aqconfig;
  struct aqualinkdata *aqdata;
};

// see https://github.com/bwssytems/ha-bridge#update-bridge-internal-light-state
//static const char *s_url =
//    "http://127.0.0.1:8080/api/username/lights/1/bridgeupdatestate";

//static struct aqualinkdata _last_habridge_aqualinkdata;

void* habridge_updater_routine(void* data)
{
  int ret = 0;
  struct aqualinkdata *aqdata = ((struct habridge_updater_data *)data)->aqdata;
  struct aqconfig *aqconfig = ((struct habridge_updater_data *)data)->aqconfig;
  char cmd_buff[256];
  int i;

  while(true)
    {
      pthread_mutex_lock(&_habridge_state_mutex);
      if ((ret = pthread_cond_wait(&_habridge_update_cond,
                                   &_habridge_state_mutex)))
        {
          LOG(NET_LOG, LOG_ERR, "start_habridge_updater cond wait err %s\n",
                      strerror(ret));
        }
      pthread_mutex_unlock(&_habridge_state_mutex);

      for (i=0; i < TOTAL_BUTTONS; i++)
        {
          if ((aqdata->aqbuttons[i].hab_id) &&
              (_last_habridge_ledstate[i] != aqdata->aqbuttons[i].led->state))
            {
              const char *on_value = "false";

              _last_habridge_ledstate[i] = aqdata->aqbuttons[i].led->state;
              if (aqdata->aqbuttons[i].led->state == ON)
                {
                  on_value = "true";
                }
              snprintf(cmd_buff, sizeof(cmd_buff),
                       "curl -sS -X PUT -d '{\"on\": %s}' http://%s/api/username/lights/%d/bridgeupdatestate > /dev/null",
                       on_value, aqconfig->habridge_server, aqdata->aqbuttons[i].hab_id);
              LOG(NET_LOG, LOG_DEBUG, "habridge_updater_routine %s\n", cmd_buff);
              if ((ret = system(cmd_buff)))
                {
                  LOG(NET_LOG, LOG_ERR, "start_habridge_updater system err %s\n",
                              strerror(ret));
                }
              else
                {
                  LOG(NET_LOG, LOG_DEBUG, "habridge_updater_routine success\n");
                }
            }
        }
    }

  return NULL;
}

bool start_habridge_updater(struct aqconfig *aqconfig, struct aqualinkdata *aqdata)
{
  static struct habridge_updater_data data;
  pthread_t thread_id;
  data.aqconfig = aqconfig;
  data.aqdata = aqdata;

  if (aqconfig->habridge_server != NULL)
    {
      time(&_last_habridge_forced_update);
      if( pthread_create( &thread_id , NULL , habridge_updater_routine, (void*)&data) < 0) {
          LOG(NET_LOG, LOG_ERR, "start_habridge_updater: could not create thread\n");
          return false;
      }
    }
  else
    {
      LOG(NET_LOG, LOG_DEBUG, "start_habridge_updater - habridge_server not set\n");
    }
  return true;
}

bool update_habridge_state(struct aqconfig *aqconfig,
                           struct aqualinkdata *aqdata)
{
  time_t now;
  int i;

  if (aqconfig->habridge_server == NULL)
    {
      return true;
    }

  time(&now);

  if (difftime(now, _last_habridge_forced_update) > 60)
    {
      LOG(NET_LOG, LOG_DEBUG, "update_habridge_state - force update\n");
      for (i=0; i < TOTAL_BUTTONS; i++)
        {
          _last_habridge_ledstate[i] = LED_S_UNKNOWN;
        }
      time(&_last_habridge_forced_update);
    }

  for (i=0; i < TOTAL_BUTTONS; i++)
    {
      if ((aqdata->aqbuttons[i].hab_id) &&
          (_last_habridge_ledstate[i] != aqdata->aqbuttons[i].led->state))
        {
          pthread_cond_signal(&_habridge_update_cond);
          break;
        }
    }

  return true;
}
