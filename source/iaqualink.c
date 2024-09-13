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
#include <string.h>
#include <stdlib.h>

#include "aq_serial.h"
#include "aqualink.h"
#include "packetLogger.h"

bool process_iaqualink_packet(unsigned char *packet, int length, struct aqualinkdata *aq_data)
{

  //debuglogPacket(IAQL_LOG, packet, length, true, true);

  return true;
}