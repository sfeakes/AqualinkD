
#include <stdio.h>
#include <stdlib.h>
#include <errno.h> 

#include "sensors.h"
#include "aqualink.h"


/*
 read sensor value from ie /sys/class/thermal/thermal_zone0/temp

 return true if current reading is different to last value stored
 */

bool read_sensor(external_sensor *sensor) {

  FILE *fp;
  float value;

  fp = fopen(sensor->path, "r");
  if (fp == NULL) {
    LOGSystemError(errno, AQUA_LOG, sensor->path);
    LOG(AQUA_LOG,LOG_ERR, "Reading sensor %s %s\n",sensor->label, sensor->path);
    return FALSE;
  }

  fscanf(fp, "%f", &value);
  fclose(fp);

   // Convert usually from millidegrees Celsius to degrees Celsius
  //printf("Factor = %f - value %f\n",sensor->factor, value);
  //printf("Read Sensor value %f %.2f\n",value,value * sensor->factor);
  value = value * sensor->factor;
  //printf("Converted value %f\n",value);
  LOG(AQUA_LOG,LOG_DEBUG, "Read sensor %s value=%.2f\n",sensor->label, value);

  if (sensor->value != value) {
    sensor->value = value;
    return TRUE;
  }

  return FALSE;
}