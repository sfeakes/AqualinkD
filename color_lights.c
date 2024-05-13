

#include <stdio.h>
#include <string.h>

//#define COLOR_LIGHTS_C_
#include "color_lights.h"


/****** This list MUST be in order of clight_type enum *******/
const char *_color_light_options[NUMBER_LIGHT_COLOR_TYPES][LIGHT_COLOR_OPTIONS] = 
{
   // AqualnkD Colors ignored as no names in control panel.
   { "bogus" },
   { // Jandy Color
        "Alpine White",
        "Sky Blue",
        "Cobalt Blue",
        "Caribbean Blue",
        "Spring Green",
        "Emerald Green",
        "Emerald Rose",
        "Magenta",
        "Violet",
        "Color Splash"
  },
  { // Jandy LED
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
        "White",
        "Light Green",
        "Green",
        "Cyan",
        "Blue",
        "Lavender",
        "Magenta"
  },
  { // Color Logic
        "Voodoo Lounge",
        "Deep Blue Sea",
        //"Royal Blue",
        "Afternoon Skies", // 'Afternoon Sky' on allbutton, Skies on iaqtouch
        //"Aqua Green",
        "Emerald",
        "Sangria",
        "Cloud White",
        //"Warm Red",
        //"Flamingo",
        //"Vivid Violet",
        //"Sangria",
        "Twilight",
        "Tranquility",
        "Gemstone",
        "USA",
        "Mardi Gras",
        "Cool Cabaret"
  },                    
  { // IntelliBrite
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
  { // Dimmer
        "25%",
        "50%",
        "75%",
        "100%"
  }
};


const char *light_mode_name(clight_type type, int index, emulation_type protocol)
{
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

int build_color_lights_js(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int i, j;

  length += sprintf(buffer+length, "var _light_program = [];\n");
  length += sprintf(buffer+length, "_light_program[0] = light_program;\n");
   
  for (i=1; i < NUMBER_LIGHT_COLOR_TYPES; i++) {
    length += sprintf(buffer+length, "_light_program[%d] = [", i);
    for (j=0; j < LIGHT_COLOR_OPTIONS; j++) {
      if (_color_light_options[i][j] != NULL)
        length += sprintf(buffer+length, "\"%s%s\",", _color_light_options[i][j], (isShowMode(_color_light_options[i][j])?" - Show":"") );
    }
    buffer[--length] = '\0';
    length += sprintf(buffer+length, "];\n");
  }

  return length;
}

