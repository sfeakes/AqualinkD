#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "aq_panel.h"
#include "aq_serial.h"
#include "serialadapter.h"

static void check_panel(const char *name, int count, bool spa, bool dual)
{
  struct aqualinkdata data = {0};
  setPanelByName(&data, name);
  assert(data.total_buttons == count);
  assert(strcmp(data.aqbuttons[0].name, BTN_PUMP) == 0);
  assert(data.aqbuttons[0].code == KEY_PUMP);
  assert(data.aqbuttons[0].led == &data.aqualinkleds[6]);
  int aux = spa ? 2 : 1;
  if (spa) {
    assert(strcmp(data.aqbuttons[1].name, BTN_SPA) == 0);
    assert(data.aqbuttons[1].code == KEY_SPA);
    assert(data.aqbuttons[1].rssd_code == RS_SA_SPA);
    assert(data.aqbuttons[1].led == &data.aqualinkleds[5]);
  }
  assert(strcmp(data.aqbuttons[aux].name, BTN_AUX1) == 0);
  assert(data.aqbuttons[aux].code == KEY_AUX1);
  assert(data.aqbuttons[aux].led == &data.aqualinkleds[4]);
  assert(strcmp(data.aqbuttons[count-3].label, spa ? "Pool Heater" : "Temp1 Heater") == 0);
  assert(strcmp(data.aqbuttons[count-2].label, spa ? "Spa Heater" : "Temp2 Heater") == 0);
  if (dual && count > 11) {
    assert(strcmp(data.aqbuttons[8].name, BTN_AUXB1) == 0);
    assert(data.aqbuttons[8].code == KEY_RS16_AUX7);
    assert(data.aqbuttons[8].led == &data.aqualinkleds[8]);
  }
  printf("PASS %s\n", name);
}

int main(void)
{
  check_panel("RS-2/6 Dual", 11, true, true);
  check_panel("RS-2/10 Dual", 15, true, true);
  check_panel("RS-2/14 Dual", 19, true, true);
  check_panel("RS-8 Combo", 12, true, false);
  check_panel("RS-8 Only", 11, false, false);
  return 0;
}
