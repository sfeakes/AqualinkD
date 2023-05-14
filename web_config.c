#include <stdio.h>
#include <string.h>


#include "config.h"
#include "color_lights.h"
#include "utils.h"

int build_webconfig_js(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;

  length = build_color_lights_js(aqdata, buffer, size);

  length += sprintf(buffer+length, "var _enable_schedules = %s;\n",(_aqconfig_.enable_scheduler)?"true":"false");

  return length;
}