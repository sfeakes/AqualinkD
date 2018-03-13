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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <libgen.h>

#include <sys/ioctl.h>
//#include <sys/socket.h>
//#include <sys/time.h>
//#include <syslog.h>
//#include <unistd.h>
#include <netdb.h>
//#include <linux/if.h>
//#include <sys/types.h>
#include <unistd.h>
#include <net/if.h>

#include "config.h"
#include "utils.h"
#include "aq_serial.h"

#define MAXCFGLINE 256

char *generate_mqtt_id(char *buf, int len);

/*
* initialize data to default values
*/
void init_parameters (struct aqconfig * parms)
{
  //char *p;
  parms->serial_port = DEFAULT_SERIALPORT;
  parms->log_level = DEFAULT_LOG_LEVEL;
  parms->socket_port = DEFAULT_WEBPORT;
  parms->web_directory = DEFAULT_WEBROOT;
  //parms->device_id = strtoul(DEFAULT_DEVICE_ID, &p, 16);
  parms->device_id = strtoul(DEFAULT_DEVICE_ID, NULL, 16);
  //sscanf(DEFAULT_DEVICE_ID, "0x%x", &parms->device_id);
  parms->override_freeze_protect = FALSE;

  parms->mqtt_dz_sub_topic = DEFAULT_MQTT_DZ_OUT;
  parms->mqtt_dz_pub_topic = DEFAULT_MQTT_DZ_IN;
  parms->mqtt_aq_topic = DEFAULT_MQTT_AQ_TP;
  parms->mqtt_server = DEFAULT_MQTT_SERVER;
  parms->mqtt_user = DEFAULT_MQTT_USER;
  parms->mqtt_passwd = DEFAULT_MQTT_PASSWD;

  parms->dzidx_air_temp = TEMP_UNKNOWN;
  parms->dzidx_pool_water_temp = TEMP_UNKNOWN;
  parms->dzidx_spa_water_temp = TEMP_UNKNOWN;
  //parms->dzidx_pool_thermostat = TEMP_UNKNOWN; // removed until domoticz has a better virtual thermostat
  //parms->dzidx_spa_thermostat = TEMP_UNKNOWN; // removed until domoticz has a better virtual thermostat
  parms->light_programming_mode = 0;
  parms->deamonize = true;
  parms->log_file = '\0';

  generate_mqtt_id(parms->mqtt_ID, MQTT_ID_LEN);
}


char *cleanalloc(char*str)
{
  char *result;
  str = cleanwhitespace(str);
  
  result = (char*)malloc(strlen(str)+1);
  strcpy ( result, str );
  //printf("Result=%s\n",result);
  return result;
}

char *cleanallocindex(char*str, int index)
{
  char *result;
  int i;
  int found = 1;
  int loc1=0;
  int loc2=strlen(str);
  
  for(i=0;i<loc2;i++) {
    if ( str[i] == ';' ) {
      found++;
      if (found == index)
        loc1 = i;
      else if (found == (index+1))
        loc2 = i;
    }
  }
  
  if (found < index)
    return NULL;

  // Trim leading & trailing spaces
  loc1++;
  while(isspace(str[loc1])) loc1++;
  loc2--;
  while(isspace(str[loc2])) loc2--;
  
  // Allocate and copy
  result = (char*)malloc(loc2-loc1+2*sizeof(char));
  strncpy ( result, &str[loc1], loc2-loc1+1 );
  result[loc2-loc1+1] = '\0';

  return result;
}

// Find the first network interface with valid MAC and put mac address into buffer upto length
bool mac(char *buf, int len)
{
  struct ifreq s;
  int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  struct if_nameindex *if_nidxs, *intf;

  if_nidxs = if_nameindex();
  if (if_nidxs != NULL)
  {
    for (intf = if_nidxs; intf->if_index != 0 || intf->if_name != NULL; intf++)
    {
      strcpy(s.ifr_name, intf->if_name);
      if (0 == ioctl(fd, SIOCGIFHWADDR, &s))
      {
        int i;
        if ( s.ifr_addr.sa_data[0] == 0 &&
             s.ifr_addr.sa_data[1] == 0 &&
             s.ifr_addr.sa_data[2] == 0 &&
             s.ifr_addr.sa_data[3] == 0 &&
             s.ifr_addr.sa_data[4] == 0 &&
             s.ifr_addr.sa_data[5] == 0 ) {
          continue;
        }
        for (i = 0; i < 6 && i * 2 < len; ++i)
        {
          sprintf(&buf[i * 2], "%02x", (unsigned char)s.ifr_addr.sa_data[i]);
        }
        return true;
      }
    }
  }

  return false;
}

char *generate_mqtt_id(char *buf, int len) {
  extern char *__progname; // glibc populates this
  int i;
  strncpy(buf, basename(__progname), len);
  i = strlen(buf);

  if (i < len) {
    buf[i++] = '_';
    // If we can't get MAC to pad mqtt id then use PID
    if (!mac(&buf[i], len - i)) {
      sprintf(&buf[i], "%.*d", (len-i), getpid());
    }
  }
  buf[len] = '\0';

  return buf;
}


//void readCfg (char *cfgFile)
void readCfg (struct aqconfig *config_parameters, struct aqualinkdata *aqdata, char *cfgFile)
{
  FILE * fp ;
  char bufr[MAXCFGLINE];
  //const char delim[2] = ";";
  //char *buf;
  //int line = 0;
  //int tokenindex = 0;
  char *b_ptr;

  if( (fp = fopen(cfgFile, "r")) != NULL){
    while(! feof(fp)){
      if (fgets(bufr, MAXCFGLINE, fp) != NULL)
      {
        b_ptr = &bufr[0];
        char *indx;
        // Eat leading whitespace
        while(isspace(*b_ptr)) b_ptr++;
        if ( b_ptr[0] != '\0' && b_ptr[0] != '#')
        {
          indx = strchr(b_ptr, '=');  
          if ( indx != NULL) 
          {
            if (strncasecmp (b_ptr, "socket_port", 11) == 0) {
              //config_parameters->socket_port = cleanint(indx+1);
              config_parameters->socket_port = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "serial_port", 11) == 0) {
              config_parameters->serial_port = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "log_level", 9) == 0) {
              config_parameters->log_level = text2elevel(cleanalloc(indx+1));
              // should fee mem here
            } else if (strncasecmp (b_ptr, "device_id", 9) == 0) {
              config_parameters->device_id = strtoul(cleanalloc(indx+1), NULL, 16);
              // should fee mem here
            } else if (strncasecmp (b_ptr, "web_directory", 13) == 0) {
              config_parameters->web_directory = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "log_file", 8) == 0) {
              config_parameters->log_file = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_address", 12) == 0) {
              config_parameters->mqtt_server = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_dz_sub_topic", 17) == 0) {
              config_parameters->mqtt_dz_sub_topic = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_dz_pub_topic", 17) == 0) {
              config_parameters->mqtt_dz_pub_topic = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_aq_topic", 13) == 0) {
              config_parameters->mqtt_aq_topic = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_user", 9) == 0) {
              config_parameters->mqtt_user = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "mqtt_passwd", 11) == 0) {
              config_parameters->mqtt_passwd = cleanalloc(indx+1);
            } else if (strncasecmp (b_ptr, "air_temp_dzidx", 14) == 0) {
              config_parameters->dzidx_air_temp = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "pool_water_temp_dzidx", 21) == 0) {
              config_parameters->dzidx_pool_water_temp = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "spa_water_temp_dzidx", 20) == 0) {
              config_parameters->dzidx_spa_water_temp = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "light_programming_mode", 21) == 0) {
              config_parameters->light_programming_mode = atof(cleanalloc(indx+1)); // should free this
            } else if (strncasecmp (b_ptr, "SWG_percent_dzidx", 17) == 0) {
              config_parameters->dzidx_swg_percent = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "SWG_PPM_dzidx", 13) == 0) {
              config_parameters->dzidx_swg_ppm = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "override_freeze_protect", 23) == 0) {
              config_parameters->override_freeze_protect = text2bool(indx+1);
            }/*else if (strncasecmp (b_ptr, "pool_thermostat_dzidx", 21) == 0) {      // removed until domoticz has a better virtual thermostat
              config_parameters->dzidx_pool_thermostat = strtoul(indx+1, NULL, 10);
            } else if (strncasecmp (b_ptr, "spa_thermostat_dzidx", 20) == 0) {
              config_parameters->dzidx_spa_thermostat = strtoul(indx+1, NULL, 10);
            } */else if (strncasecmp (b_ptr, "button_", 7) == 0) {
              int num = strtoul(b_ptr+7, NULL, 10) - 1;
              //logMessage (LOG_DEBUG, "Button %d\n", strtoul(b_ptr+7, NULL, 10));
              if (strncasecmp (b_ptr+9, "_label", 6) == 0) {
                //logMessage (LOG_DEBUG, "     Label %s\n", cleanalloc(indx+1));
                aqdata->aqbuttons[num].label = cleanalloc(indx+1);
              } else if (strncasecmp (b_ptr+9, "_dzidx", 6) == 0) {
                //logMessage (LOG_DEBUG, "     dzidx %d\n", strtoul(indx+1, NULL, 10));
                aqdata->aqbuttons[num].dz_idx = strtoul(indx+1, NULL, 10);
              }
            }
          } 
          //line++;
        }
      }
    }
    fclose(fp);
  } else {
    /* error processing, couldn't open file */
    displayLastSystemError(cfgFile);
    exit (EXIT_FAILURE);
  }
}









