#ifndef SENSORS_H_
#define SENSORS_H_

#include <stdbool.h>

typedef struct external_sensor{
  char *path;
  float factor;
  char *label;
  float value;
} external_sensor;

bool read_sensor(external_sensor *sensor);

#endif