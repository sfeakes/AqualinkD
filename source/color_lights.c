

#include <stdio.h>
#include <string.h>

//#define COLOR_LIGHTS_C_
#include "color_lights.h"


/*
Jandy Colors
Jandy LED Light
Sam/SL
                          Color Logic
Intelibright
Haywood Universal Color

*/

/****** This list MUST be in order of clight_type enum *******/
const char *_color_light_options[NUMBER_LIGHT_COLOR_TYPES][LIGHT_COLOR_OPTIONS] = 
//char *_color_light_options[NUMBER_LIGHT_COLOR_TYPES][LIGHT_COLOR_OPTIONS] = 
{
   // AqualnkD Colors ignored as no names in control panel.
   { "bogus" },
   { // Jandy Color
        "",
        "Alpine White",  // 0x41
        "Sky Blue",
        "Cobalt Blue",
        "Caribbean Blue",
        "Spring Green",
        "Emerald Green",
        "Emerald Rose",
        "Magenta",
        "Garnet Red",
        "Violet",
        "Color Splash"
  },
  { // Jandy LED
        "",
        "Alpine White",
        "Sky Blue",
        "Cobalt Blue",
        "Caribbean Blue",
        "Spring Green",
        "Emerald Green",
        "Emerald Rose",
        "Magenta",
        "Violet",
        "Slow Splash",
        "Fast Splash",
        "USA",
        "Fat Tuesday",
        "Disco Tech"
  },
  { // SAm/SAL
        "",
        "White",
        "Light Green",
        "Green",
        "Cyan",
        "Blue",
        "Lavender",
        "Magenta"
  },
  { // Color Logic 
        "",
        "Voodoo Lounge",   // 0x41 (both home and sim)
        "Deep Blue Sea",   // 0x42 (both gome and sim)
        "Afternoon Skies", // 0x44 home // 0x43 sim // 'Afternoon Sky' on allbutton, Skies on iaqtouch
        "Emerald",         // 0x44
        "Sangria",
        "Cloud White",     // 0x46
        "Twilight",        // 0x4c (home panel) // 0x47
        "Tranquility",     // 0x4d (home panel) // 0x48
        "Gemstone",        // 0x4e (home panel) // 0x49 (simulator)
        "USA",             // 0x4f (home panel) // 0x4a (simulator)
        "Mardi Gras",      // 0x50 (home panel) // 0x4b (simulator)
        "Cool Cabaret"     // 0x51 (home panel) // 0x4c
  },                 
  { // IntelliBrite
        "",
        "SAm",
        "Party",
        "Romance",
        "Caribbean",
        "American",
        "Cal Sunset",
        "Royal",
        "Blue",
        "Green",
        "Red",
        "White",
        "Magenta"
  },
  { // Haywood Universal Color
        "",
        "Voodoo Lounge",   // 0x41 (both home and sim) // Looks like 28 + <value or index> = 0x41 = 1st index
        "Deep Blue Sea",   // 0x42 (both gome and sim)
        "Royal Blue",    // // 0x43 home // non
        "Afternoon Skies", // 0x44 home // 0x43 sim // 'Afternoon Sky' on allbutton, Skies on iaqtouch
        "Aqua Green",    //  
        "Emerald",         // 0x44
        "Cloud White",     // 0x46
        "Warm Red",      //  
        "Flamingo",      //  
        "Vivid Violet",  //  
        "Sangria",       // 0x4b (home panel) // Non existant
        "Twilight",        // 0x4c (home panel) // 0x47
        "Tranquility",     // 0x4d (home panel) // 0x48
        "Gemstone",        // 0x4e (home panel) // 0x49 (simulator)
        "USA",             // 0x4f (home panel) // 0x4a (simulator)
        "Mardi Gras",      // 0x50 (home panel) // 0x4b (simulator)
        "Cool Cabaret"     // 0x51 (home panel) // 0x4c
  },
  {/*Spare 1*/},
  {/*Spare 2*/},
  {/*Spare 3*/},
  { // Dimmer    // From manual this is 0 for off, 128+<value%> so 153 = 25%  = 0x99
        "",
        "25%",  // 0x99 (simulator)  = 153 dec
        "50%",  // 0xb2 (simulator)  = 178 dec  same as (0x99 + 25)
        "75%",  // 0xcb (simulator)  = 203 dec
        "100%"  // 0xe4              = 228 dec
  }
};



/*
void deleteLightOption(int type, int index) 
{
  int arrlen = LIGHT_COLOR_OPTIONS;
  memmove(_color_light_options[type]+index, _color_light_options[type]+index+1, (--arrlen - index) * sizeof *_color_light_options[type]);
}

void setColorLightsPanelVersion(uint8_t supported)
{
  static bool set = false;
  if (set)
    return;

  if ((supported & REP_SUP_CLIT4) == REP_SUP_CLIT4)
    return; // Full panel support, no need to delete anything

  //deleteLightOption(4, 11); // Color Logic "Sangria"
  deleteLightOption(4, 9); // Color Logic "Vivid Violet",
  deleteLightOption(4, 8);  // Color Logic "Flamingo"
  deleteLightOption(4, 7);  // Color Logic "Warm Red",
  deleteLightOption(4, 4);  // Color Logic "Aqua Green"
  deleteLightOption(4, 2);  // Color Logic "Royal Blue"
  
  set = true;
}
*/

const char *light_mode_name(clight_type type, int index, emulation_type protocol)
{
  if (index <= 0 || index > LIGHT_COLOR_OPTIONS ){
    return "";
  }

  // Rename any modes depending on emulation type
  if (protocol == ALLBUTTON) {
    if (strcmp(_color_light_options[type][index],"Afternoon Skies") == 0) {
      return "Afternoon Sky";
    }
  }

  return _color_light_options[type][index];
}

bool isShowMode(const char *mode)
{
  if (strcmp(mode, "Color Splash") == 0 ||
      strcmp(mode, "Slow Splash") == 0 ||
      strcmp(mode, "Fast Splash") == 0 ||
      strcmp(mode, "Fat Tuesday") == 0 ||
      strcmp(mode, "Disco Tech") == 0 ||
      strcmp(mode, "Voodoo Lounge") == 0 ||
      strcmp(mode, "Twilight") == 0 ||
      strcmp(mode, "Tranquility") == 0 ||
      strcmp(mode, "Gemstone") == 0 ||
      strcmp(mode, "USA") == 0 ||
      strcmp(mode, "Mardi Gras") == 0 ||
      strcmp(mode, "Cool Cabaret") == 0 ||
      strcmp(mode, "SAm") == 0 ||
      strcmp(mode, "Party") == 0 ||
      strcmp(mode, "Romance") == 0 ||
      strcmp(mode, "Caribbean") == 0 ||
      strcmp(mode, "American") == 0 ||
      strcmp(mode, "Cal Sunset") == 0)
    return true;
  else
    return false;
}

void set_currentlight_value(clight_detail *light, int index)
{
  // We want to leave the last color, so if 0 don't do anything, but set to 0 if bad value
  if (index < 0 || index > LIGHT_COLOR_OPTIONS)
    light->currentValue = 0;
  else if (index > 0 && index < LIGHT_COLOR_OPTIONS)
    light->currentValue = index;
}

int build_color_lights_js(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int i, j;

  length += sprintf(buffer+length, "var _light_program = [];\n");
  length += sprintf(buffer+length, "_light_program[0] = light_program;\n");
   
  for (i=1; i < NUMBER_LIGHT_COLOR_TYPES; i++) {
    length += sprintf(buffer+length, "_light_program[%d] = [ ", i);
    for (j=1; j < LIGHT_COLOR_OPTIONS; j++) { // Start a 1 since index 0 is blank
      if (_color_light_options[i][j] != NULL)
        length += sprintf(buffer+length, "\"%s%s\",", _color_light_options[i][j], (isShowMode(_color_light_options[i][j])?" - Show":"") );
    }
    buffer[--length] = '\0';
    length += sprintf(buffer+length, "];\n");
  }

  return length;
}

