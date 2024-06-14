
#ifndef COLOR_LIGHTS_H_
#define COLOR_LIGHTS_H_

#include "aqualink.h"
#include "aq_programmer.h"

#define LIGHT_COLOR_NAME    16
#define LIGHT_COLOR_OPTIONS 17
//#define LIGHT_COLOR_TYPES   LC_DIMMER+1

/*
// color light modes (Aqualink program, Jandy, Jandy LED, SAm/SAL, Color Logic, Intellibrite)
typedef enum clight_type {
  LC_PROGRAMABLE=0, 
  LC_JANDY, 
  LC_JANDYLED, 
  LC_SAL, 
  LC_CLOGIG, 
  LC_INTELLIB
} clight_type;
*/
//const char *light_mode_name(clight_type type, int index);
const char *light_mode_name(clight_type type, int index, emulation_type protocol);
int build_color_lights_js(struct aqualinkdata *aqdata, char* buffer, int size);



//char *_color_light_options_[LIGHT_COLOR_TYPES][LIGHT_COLOR_OPTIONS][LIGHT_COLOR_NAME];

#endif //COLOR_LIGHTS_H_

/*
Color Name      Jandy Colors    Jandy LED       SAm/SAL         Color Logic     IntelliBrite      dimmer
---------------------------------------------------------------------------------------------------------
Color Splash    11                              8                                              
Alpine White    1               1                                                              
Sky Blue        2               2                                                              
Cobalt Blue     3               3                                                              
Caribbean Blu   4               4                                                              
Spring Green    5               5                                                              
Emerald Green   6               6                                                              
Emerald Rose    7               7                                                              
Magenta         8               8                                                              
Garnet Red      9               9                                                              
Violet          10              10                                                             
Slow Splash                     11                                                             
Fast Splash                     12                                                             
USA!!!                          13                                                             
Fat Tuesday                     14                                                             
Disco Tech                      15                                                             
White                                           1                                              
Light Green                                     2                                              
Green                                           3                                              
Cyan                                            4                                              
Blue                                            5                                              
Lavender                                        6                                              
Magenta                                         7                                              
Light Magenta                                                                                  
Voodoo Lounge                                                   1                              
Deep Blue Sea                                                   2                              
Afternoon Skies                                                 3                              
Afternoon Sky                                                                                 
Emerald                                                         4                              
Sangria                                                         5                              
Cloud White                                                     6                              
Twilight                                                        7                              
Tranquility                                                     8                              
Gemstone                                                        9                             
USA!                                                            10                             
Mardi Gras                                                      11                             
Cool Cabaret                                                    12  
SAm                                                                             1              
Party                                                                           2              
Romance                                                                         3              
Caribbean                                                                       4              
American                                                                        5              
Cal Sunset                                                                      6              
Royal                                                                           7              
Blue                                                                            8              
Green                                                                           9              
Red                                                                             10             
White                                                                           11             
Magenta                                                                         12 
25%                                                                                             1
50%                                                                                             2
75%                                                                                             3
100%                                                                                            4
*/