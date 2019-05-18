/*
 * net_services_habridge.h
 *
 *  Created on: Apr 29, 2019
 *      Author: Lee_Ballard
 */

#ifndef NET_SERVICES_HABRIDGE_H_
#define NET_SERVICES_HABRIDGE_H_

#include <stdbool.h>
#include "mongoose.h"
#include "config.h"

bool start_habridge_updater(struct aqconfig *aqconfig, struct aqualinkdata *aqdata);
bool update_habridge_state(struct aqconfig *aqconfig, struct aqualinkdata *aqdata);

#endif /* NET_SERVICES_HABRIDGE_H_ */
