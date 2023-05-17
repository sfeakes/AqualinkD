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
#include <string.h>
#include <ctype.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <regex.h>


#include "mongoose.h"
#include "aqualink.h"
#include "aq_scheduler.h"
#include "config.h"
//#include "utils.h"



/*
Example /etc/cron.d/aqualinkd

01 10 1 * * curl localhost:80/api/Filter_Pump/set -d value=2 -X PUT
*/

bool remount_root_ro(bool readonly) {
  // NSF Check if config is RO_ROOT set

  if (readonly) {
    LOG(SCHD_LOG,LOG_INFO, "reMounting root RO\n");
    mount (NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
    return true;
  } else {
    struct statvfs fsinfo;
    statvfs("/", &fsinfo);
    if ((fsinfo.f_flag & ST_RDONLY) == 0) // We are readwrite, ignore
      return false;

    LOG(SCHD_LOG,LOG_INFO, "reMounting root RW\n");
    mount (NULL, "/", NULL, MS_REMOUNT, NULL);
    return true;
  }
}


bool passJson_scObj(char* line, int length, aqs_cron *values)
{
  int keystart=0;
  int keyend=0;
  int valuestart=0;
  int captured=0;
  bool readingvalue=false;
  bool invalue=false;
  //char value;

  LOG(SCHD_LOG,LOG_DEBUG, "Obj body:'%.*s'\n", length, line);

  for (int i=0; i < length; i++) {
    if (line[i] == '}') {
      return (captured >= 7)?true:false;
    } else if (line[i] == '"' && keystart==0 && invalue==false && readingvalue==false) {
      keystart=i+1;
    } else if (line[i] == '"' && keystart > 0 && invalue==false && readingvalue==false) {
      keyend=i;
    } else if (line[i] == ':' && keystart > 0 ) {
      invalue=true;
    } else if (line[i] == '"' && invalue == true && readingvalue == false && keystart > 0 ) {
      readingvalue=true;
      valuestart=i+1;
    } else if (line[i] == '"' && readingvalue == true) {
      // i is end of key
      if ( strncmp(&line[keystart], "min", 3) == 0) {
        strncpy(values->minute, &line[valuestart], (i-valuestart) );
        values->minute[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "hour", 4) == 0) {
        strncpy(values->hour, &line[valuestart], (i-valuestart) );
        values->hour[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "daym", 4) == 0) {
        strncpy(values->daym, &line[valuestart], (i-valuestart) );
        values->daym[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "month", 5) == 0) {
        strncpy(values->month, &line[valuestart], (i-valuestart) );
        values->month[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "dayw", 4) == 0) {
        strncpy(values->dayw, &line[valuestart], (i-valuestart) );
        values->dayw[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "url", 3) == 0) {
        strncpy(values->url, &line[valuestart], (i-valuestart) );
        values->url[i-valuestart] = '\0';
        captured++;
      } else if( strncmp(&line[keystart], "value", 5) == 0) {
        strncpy(values->value, &line[valuestart], (i-valuestart) );
        values->value[i-valuestart] = '\0';
        captured++;
      }
      keystart=0;
      keyend=0;
      valuestart=0;
      invalue=false;
      readingvalue=false;
    }
  }

  return (captured >= 7)?true:false;
}

int save_schedules_js(char* inBuf, int inSize, char* outBuf, int outSize)
{
  int length=0;
  FILE *fp;
  int i;
  bool inarray = false;
  aqs_cron cline;

  if ( !_aqconfig_.enable_scheduler) {
    LOG(SCHD_LOG,LOG_WARNING, "Schedules are disabled\n");
    length += sprintf(outBuf, "{\"message\":\"Error Schedules disabled\"}");
    return length;
  }

  LOG(SCHD_LOG,LOG_NOTICE, "Saving Schedule:\n");
  
  bool fs = remount_root_ro(false);
  fp = fopen(CRON_FILE, "w");
  if (fp == NULL) {
    LOG(SCHD_LOG,LOG_ERR, "Open file failed '%s'\n", CRON_FILE);
    remount_root_ro(true);
    length += sprintf(outBuf, "{\"message\":\"Error Saving Schedules\"}");
    return length;
  }
  fprintf(fp, "#***** AUTO GENERATED DO NOT EDIT *****\n");
  fprintf(fp, "PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin\n");

  LOG(SCHD_LOG,LOG_DEBUG, "Schedules Message body:\n'%.*s'\n", inSize, inBuf);

  length += sprintf(outBuf, "{\"message\":\"Saved Schedules\"}");

  for (i=0; i < inSize; i++) {
      if ( inBuf[i] == '[' ) {
        inarray=true;
      } else if ( inBuf[i] == ']' ) {
        inarray=false;
      } else if ( inarray && inBuf[i] == '{') {
        passJson_scObj( &inBuf[i], (inSize-i), &cline);
        //LOG(SCHD_LOG,LOG_NOTICE,"Saving Cron %s %s %s %s %s %s %s\n",cline.minute, cline.hour, cline.daym, cline.month, cline.dayw, cline.url, cline.value);
        LOG(SCHD_LOG,LOG_INFO, "Write to cron Min=%s Hour=%s DayM=%s Month=%s DayW %s URL %s Value %s\n",cline.minute,cline.hour,cline.daym,cline.month,cline.dayw,cline.url,cline.value);
        LOG(SCHD_LOG,LOG_INFO, "%s %s %s %s %s curl localhost:%s%s -d value=%s -X PUT\n",cline.minute, cline.hour, cline.daym, cline.month, cline.dayw, _aqconfig_.socket_port, cline.url, cline.value);
        fprintf(fp, "%s %s %s %s %s root curl localhost:%s%s -d value=%s -X PUT\n",cline.minute, cline.hour, cline.daym, cline.month, cline.dayw, _aqconfig_.socket_port, cline.url, cline.value);
      } else if ( inarray && inBuf[i] == '}') {
        //inobj=false;
        //objed=i;
      }
  }
    

  fprintf(fp, "#***** AUTO GENERATED DO NOT EDIT *****\n");
  fclose(fp);

  remount_root_ro(fs);

  return length;
}

int build_schedules_js(char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  FILE *fp;
  char *line = NULL;
  int length = 0;
  int rc;
  aqs_cron cline;
  size_t len = 0;
  ssize_t read_size;
  regex_t regexCompiled;

  if ( !_aqconfig_.enable_scheduler) {
    LOG(SCHD_LOG,LOG_WARNING, "Schedules are disabled\n");
    length += sprintf(buffer, "{\"message\":\"Error Schedules disabled\"}");
    return length;
  }

  // Below works for curl but not /usr/bin/curl in command.  NSF come back and fix the regexp
  //char *regexString="([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s.*(/api/.*)\\s-d value=([^\\d]+)\\s(.*)";
  // \d doesn't seem to be supported, so using [0-9]+ instead
  //char *regexString="([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s.*(\\/api\\/.*\\/set).* value=([0-9]+).*";
  char *regexString="([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s.*(\\/api\\/.*\\/set).* value=([0-9]+).*";
  //char *regexString="([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s([^\\s]+)\\s.*(/api/.*/set).*value=([0-9]+).*";


  size_t maxGroups = 15;
  regmatch_t groupArray[maxGroups];
  //static char buf[100];

  length += sprintf(buffer+length,"{\"type\": \"schedules\",");

  if (0 != (rc = regcomp(&regexCompiled, regexString, REG_EXTENDED))) {
      LOG(SCHD_LOG,LOG_ERR, "regcomp() failed, returning nonzero (%d)\n", rc);
      length += sprintf(buffer+length,"\"message\": \"Error reading schedules\"}");
      return length;
   }

  fp = fopen(CRON_FILE, "r");
  if (fp == NULL) {
    LOG(SCHD_LOG,LOG_ERR, "Open file failed '%s'\n", CRON_FILE);
    length += sprintf(buffer+length,"\"message\": \"Error reading schedules\"}");
    return length;
  }

  length += sprintf(buffer+length,"\"schedules\": [ ");

  while ((read_size = getline(&line, &len, fp)) != -1) {
    //printf("Read from cron:-\n  %s", line);
    //lc++;
    //rc = regexec(&regexCompiled, line, maxGroups, groupArray, 0);
    if (0 == (rc = regexec(&regexCompiled, line, maxGroups, groupArray, REG_EXTENDED))) {
      // Group 1 is minute
      // Group 2 is hour
      // Group 3 is day of month
      // Group 4 is month
      // Group 5 is day of week
      // Group 6 is URL
      // Group 7 is value
      if (groupArray[8].rm_so == (size_t)-1) {
        LOG(SCHD_LOG,LOG_ERR, "No matching information from cron file\n");
      } else {
        sprintf(cline.minute, "%.*s", (groupArray[1].rm_eo - groupArray[1].rm_so), (line + groupArray[1].rm_so));
        sprintf(cline.hour, "%.*s",   (groupArray[2].rm_eo - groupArray[2].rm_so), (line + groupArray[2].rm_so));
        sprintf(cline.daym, "%.*s",   (groupArray[3].rm_eo - groupArray[3].rm_so), (line + groupArray[3].rm_so));
        sprintf(cline.month, "%.*s",  (groupArray[4].rm_eo - groupArray[4].rm_so), (line + groupArray[4].rm_so));
        sprintf(cline.dayw, "%.*s",   (groupArray[5].rm_eo - groupArray[5].rm_so), (line + groupArray[5].rm_so)); 
        sprintf(cline.url, "%.*s",    (groupArray[8].rm_eo - groupArray[8].rm_so), (line + groupArray[8].rm_so));
        sprintf(cline.value, "%.*s",  (groupArray[9].rm_eo - groupArray[9].rm_so), (line + groupArray[9].rm_so));
        LOG(SCHD_LOG,LOG_INFO, "Read from cron. Min=%s Hour=%s DayM=%s Month=%s DayW=%s URL=%s Value=%s\n",cline.minute,cline.hour,cline.daym,cline.month,cline.dayw,cline.url,cline.value);
        length += sprintf(buffer+length, "{\"min\": \"%s\",\"hour\": \"%s\",\"daym\": \"%s\",\"month\": \"%s\",\"dayw\": \"%s\",\"url\": \"%s\",\"value\": \"%s\"},",
                cline.minute,
                cline.hour,
                cline.daym,
                cline.month,
                cline.dayw,
                cline.url,
                cline.value);
        //LOG(SCHD_LOG,LOG_DEBUG, "Read from cron Day %d | Time %d:%d | Zone %d | Runtime %d\n",day,hour,minute,zone,runtime);
      }
    } else {
      LOG(SCHD_LOG,LOG_DEBUG, "regexp no match (%d) %s\n", rc, line);
    }
  }

  buffer[--length] = '\0';
  length += sprintf(buffer+length,"]}\n");

  fclose(fp);
  regfree(&regexCompiled);

  return length;
}