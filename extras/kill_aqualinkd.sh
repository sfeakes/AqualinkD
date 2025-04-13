#!/bin/bash
#
# /*
# * Copyright (c) 2017 Shaun Feakes - All rights reserved
# *
# * You may use redistribute and/or modify this code under the terms of
# * the GNU General Public License version 2 as published by the 
# * Free Software Foundation. For the terms of this license, 
# * see <http://www.gnu.org/licenses/>.
# *
# * You are free to use this software under the terms of the GNU General
# * Public License, but WITHOUT ANY WARRANTY; without even the implied
# * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# * See the GNU General Public License for more details.
# *
# *  https://github.com/aqualinkd/aqualinkd
# */


if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi


kill -9 `ps -ef | grep aqualinkd | awk '{printf "%s ",$2}'`