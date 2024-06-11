# Aqualinkd  
Linux daemon to control Aqualink RS pool controllers. Provides web UI, MQTT client & HTTP API endpoints. Control your pool equipment from any phone/tablet or computer.  Is also compatible with most Home control systems including Apple HomeKit, Home Assistant, Samsung, Alexa, Google, etc.
<br>
Binaries are supplied for Raspberry Pi both 32 & 64 bit OS, Has been, and can be compiled for many different SBC's, and a Docker is also available.

### It does not, and will never provide any layer of security. NEVER directly expose the device running this software to the outside world; only indirectly through the use of Home Automation hub's or other security measures. e.g. VPNs.


## Donation
If you like this project, you can buy me a cup of coffee :)
<br>
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SEGN9UNS38TXJ)

## AqualinkD discussions

* Please use github Discussions for questions (Link at top of page).
https://github.com/sfeakes/AqualinkD/discussions
* For Bugs, please use issues link on top of page. ( please add AqualinkD version to posts )
https://github.com/sfeakes/AqualinkD/issues

## Please see Wiki for installation instructions
https://github.com/sfeakes/AqualinkD/wiki

<!--
For information on Control panel versions and upgrading the chips.<br>
https://github.com/sfeakes/AqualinkD/wiki/Upgrading-Jandy-Aqualink-PDA-to-RS-panel
-->
<!--
Here's where I started to document what I know about the Jandy RS485 protocol.<br>
https://github.com/sfeakes/AqualinkD/wiki/Jandy-Aqualink-RS485-protocol
-->

## AqualinkD built in WEB Interface(s).

<table width="100%" border="0" cellpadding="20px">
 <tr><th width="50%">Default web interface</th><th wifth="50%">Simple web interface</img></th><tr>
 <tr><td><img src="extras/IMG_0251.PNG?raw=true" width="350"></img></td><td><img src="extras/simple.png?raw=true" width="350"</img></td></td></td>
  <tr><td colspan="2">
     Both Interfaces
     <ul>
        <li>If loading the web page in a mobile device browser, you will need to save to desktop where an app will be created for you.</li>
        <li>The order and options shown are configurable for your individual needs and/or preferences.</li>
   </ul>
   </td></tr>
   <tr><td colspan="2">
     Default Interfaces
   <ul>
     <li>The layout and functionality are from the Apple HomeKit interface.  This works in any browser or on any mobile device.</li>
      <li>Customizable tile icons & background images. (Tiles not used can be hidden).</li>
      <li>Thermostat, Switch, SWG & Light tiles have more options (ie: setting heater temperature, timers, salt generating percentage and light mode etc). These options are accessible by pressing and holding the tile icon.</li>
      <li>Supports live background images (ie: poll camera for still image every X seconds).</li>
      </ul>
   </td></tr>
   <tr><td colspan="2">
     In web browser/tablet
   <ul>
   <img src="extras/web_ui2.png?raw=true" width="800">
   </td></tr>
 </table>

### Simulators
Designed to mimic AqualinkRS devices, used to fully configure the master control panel<br>
<img src="extras/onetouch_sim.png?raw=true">
<img src="extras/allbutton_sim.png?raw=true">

### In Apple Home app.
<img src="extras/HomeKit2.png?raw=true" width="800"></img>
* (NOTE: Salt Water Generator is configured as a Thermostat.  It is the closest homekit accessory type; so &deg;=% and Cooling=Generating).
* Full support for homekit scenes: ie: Create a "Spa scene" to: "turn spa on, set spa heater to X temperature and turn spa blower on", etc etc).

### In Home Assistant 
<img src="extras/HASSIO.png?raw=true" width="800"></img>

## All Web interfaces.
* http://aqualink.ip/     <- (Standard WEB UI
* http://aqualink.ip/simple.html   <- (Simple opion if you don't like the above)
* http://aqualink.ip/simulator.html  <- (Displays all simulators in one page with tabs)
* http://aqualink.ip/aqmanager.html  <- (Manage AqualinkD configuration & runtime)
* http://aqualink.ip/allbutton_sim.html  <- (All Button Simulator)
* http://aqualink.ip/onetouch_sim.html  <- (One Touch Simulator)
* http://aqualink.ip/aquapda_sim.html  <- (PDA simulator)
#<a name="release"></a>
# ToDo (future release)
* Allow selecting of pre-defined VSP programs (Aqualink Touch & OneTouch protocols.)
* Add set time to OneTouch protocol.
* Update AqualinkD Management console to manage configuration
* Create iAqualink Touch Simulator
* Probably decoded enough protocols for AuqlinkD to self configure.

<!--
* NEED TO FIX for PDA and iAQT protocol.
  * Not always doing on/off
  * Heaters are slow to turn on, need to hit extra button
  * Spa turns on Spa Heat (first button on home page ???)
  * SWG Stays on
  * serial_logger
  * Add wiki documentation 
    * about Heat vs Heater
    * Panel version
    * can't use iaquatouch panel / wireless

* Added iAqualinkTouch support for PDA only panels that can use that protocol.
  * PDA panel needs to be Rev 6.0 or newer.
  * This makes the PDA only panels quicker and less error prone.
  * Introduces color light support and VSP
  * Consider this PDA support Beta.
  * Read PDA Wiki
-->

# Call for Help.
* The only Jandy devices I have not decoded yet are LX heater & Chemical Feeder. If you have either of these devices and are willing to post some logs, please let me know, or post in the [Discussions area](https://github.com/sfeakes/AqualinkD/discussions)

# Updates in Release 2.3.7
* Fix for Pentair VSP losing connection & bouncing SWG to 0 and back.
* Added more VSP data (Mode, Status, Pressure Curve, both RPM & GPM) for all Pentair Pumps (VS/VF/VSF).
* Few updates to HomeAssistant integration.
  * Will now convert from C to F so setting `convert_mqtt_temp_to_c` doesn't effect hassio anymore
  * Added VSP support to change RPM/GPM (uses fan type since hassio doesn't support RPM, so it's a % setting or the full RPM or GPM range of your pump) 
* Updates to serial_logger.
* Few updates to UI.
  * Will display both RPM & GPM for VSP (space providing)
  * Fix freeze protect button in UI not showing enabled.
* Few updates to AQmanager UI.

# Update in Release 2.3.6
* No functionality changes
* Build & Docker changes
* Going forward AqualinkD will release binaries for both armhf & arm64
  * armhf = any Pi (or equiv board) running 32 bit Debain based OS, stretch or newer
  * arm64 = Pi3/4/2w running 64 bit Debain based OS, buster or newer

# Update in Release 2.3.5
* Added Home Assistant integration through MQTT discover
  * Please read the Home Assistant section of the [Wiki - HASSIO](https://github.com/sfeakes/AqualinkD/wiki#HASSIO)
  * There are still some enhacments to come on this.
* Included Docker into main releases
  * Please read Docker section of the  [Wiki - Docker](https://github.com/sfeakes/AqualinkD/wiki#Docker)
* Added support for reading extended information for Jandy JXi heaters.
* Added Color Light to iAqualinkTouch protocol.
* Fixed issue mqtt_timed_update (1~2 min rather than between 2 & 20 min)
  
# Update in Release 2.3.4
* Changes for Docker
* Updated simulator code base and added new simulators for AllButton, OneTouch & PDA.
  * <aqualinkd.ip>/allbutton_sim.html
  * <aqualinkd.ip>/onetouch_sim.html
  * <aqualinkd.ip>/aquapda_sim.html
    * On PDA only panel AqualinkD has to share the same ID with the PDA simulator. Therefore for AqualinkD will not respond to commands while simulator is active.
* Now you can completely program the control panel with the simulators removing the need to have Jandy device.   

# Update in Release 2.3.3
* Introduced Aqualink Manager UI http://aqualink.ip/aqmanager.html
  * [AqualinkD Manager](https://github.com/sfeakes/AqualinkD/wiki#AQManager)
* Moved logging into systemd/journal (journalctl) from syslog
  * [AqualinkD Log](https://github.com/sfeakes/AqualinkD/wiki#Log)
* Updated to scheduler
  * [AqualinkD Scheduler](https://github.com/sfeakes/AqualinkD/wiki#Scheduler)
* Introduced RS485 frame delay / timer. 
  * Improve PDA panels reliability (PDA pannels are slower than RS panels)
  * Potentially fixed Pentair VSP / SWG problems since Pentair VSP use a different protocol, this will allow a timed delay for the VSP to post a status messages. Seems to only effect RS485 bus when both a Pentair VSP and Jandy SWG are present.
  * Add ```rs485_frame_delay = 4``` to /etc/aqualinkd.conf, 4 is number of milliseconds between frames, 0 will turn off ie no pause.
* PDA Changes to support SWG and Boot.

# Update in Release 2.3.2
* Added support for VSP on panel versions REV 0.1 & 0.2
* Can change heater sliver min/max values in web UI. `./web/config.js`
* Added reading ePump RPM/Watts directly from RS485 bus.

# Update in Release 2.3.1
* Changed a lot of logic around different protocols.
* Added low latency support for FTDI usb driver.
* AqualinkD will find out the fastest way to change something depending on the protocols available.
* Added scheduler (click time in web ui). supports full calendar year (ie seasons), See wiki for details. 
* Added timers for devices (ie can turn on Pump for x minutes), Long press on device in WebUI.
* Timers supported in MQTT/API.
* Support for external chem feeders can post to AqualinkD (pH & ORP shown in webUI / homekit etc)
* Add support for dimmers.
* Extended SWG status now in web UI.
* Serial logging / error checking enhancements.
* Added simulator back. (+ Improved UI).
* Fix issue with incorrect device state after duplicate MQTT messages being sent in rapid succession ( < 0.5 second).
* Found a workaround for panel firmware bug in iAqualink Touch protocol where VSP updates sometimes got lost.
* Fix bug in IntelliBrite color lights
* Install script checks for cron and it's config (needed for scheduler)
* serial-logger will now give recommended values for aqualinkd.conf
* Lock the serial port to stop duplicate process conflict.
* Lots of code cleanup & modifying ready for AqualinkD Management console in a future release.

# Update in Release 2.2.2
* Fixed some Web UI bugs
* Color lights now quicker when selecting existing color mode.

# Update in Release 2.2.1
* Supports serial adapter protocol `rssa_device_id`, (provides instant heater setpoint changes & setpoint increment)
* Can use seperate threads for network & RS485 interface. (optimisation for busy RS485 bus implimentations)
* Display messages now posted to MQTT
* Finilized all pre-repease work in 2.2.0(a,b,c)
* Optimized the USB2RS485 Serial adapter logic.
* Logging bitmasks to focus debugging information
* Serial Logger changes to test for speed / errors & busy network bus
* Changed raw RS485 device reading, to specific devices rather than all
  * Change `read_all_devices` & `read_pentair_packets` to `read_RS485_swg`, `read_RS485_ePump`, `read_RS485_vsfPump`
  * Since you can now target specific device, I've reverted back to displaying the real information of the device & not trying to hide it like the panel does. please see wiki config section for more details
* Can link Spa Mode and Spa Heater (web UI only). add `var link_spa_and_spa_heater = true;` to config.js

# Update in (Pre) Release 2.2.0c
* Cleaned up Makefile (and adding debug timings).
* Changed loggin infrastructure.
* Added expermental options for Pi4.
* 2.2.0a (had some issues with compiler optimisation), please don't use or compile yourself.
* Fixed RS-4 panel bug.
* Fixed some AqualinkTouch programming issues.
* Increased timeout for startup probe.
* This release WILL require you to make aqualinkd.conf changes. <b>Make sure to read wiki section https://github.com/sfeakes/AqualinkD/wiki#Version_2</b>
* Extensive work to reduce CPU cycles and unnesessary logic.
* iAqualink Touch protocol supported for VSP & extended programming.
  * This protocol is a lot faster for programming, ID's are between 0x38 & 0x3B `extended_device_id`, use Serial_logger to find valid ID.
  * If your panel supports this it's strongly recomended to use it and also set `extended_device_id_programming=yes`
* New panel config procedure VERY dependant on panel type, you must change config, must set `panel_type` in config.
  * Buttons (specifically) heaters will be different from previous versions. (all info in wiki link above) 
* Simulator support removed for the moment.
* Few changes in RS protocol and timings.
* Fixed bug with Watts display for VSP.
* Fixed bug with colored lights.
* RS16 panels no longer require `extended_device_id` to be set
* More compile flags. See notes in wiki on compiling.
* Extensive SWG logic changes.
* AqualinkD startup changed to fix some 'systemctl restart' issues.
* More detailed API replys.
# Update in Release 2.1.0
* Big update, lots of core changes, <b>please read wiki section https://github.com/sfeakes/AqualinkD/wiki#Version_2</b>
* Full Variable Speed Pump support. (Can read,set & change RPM,GPM)
* Full support for all Colored Lights (even if Jandy Control Panel doesn't support them)
* Chemlink pH & ORP now supported. (along with posting MQTT information)
* There are some configuration changes, make sure to read wiki (link above)
* RS12 & RS16 Panels now supported. (RS16 will also need `extended_device_id` set for full AUXB5-B8 support)
* New UI option(s) `turn_on_sensortiles = true` & `show_vsp_gpm=false` in `config.js`
* Added compile flags. If you make your own aqualinkd and have no need for PDA or RS16 support, edit the Makefile.
* Completley new API.
# Update in Release 1.3.9a
* Improved Debugging for serial.
* Added panel Timeout mode support to UI and MQTT
* Fixed SWG bug while in Service & Timeout modes
* Cleanded up SWG code and MQTT status messages for SWG and SWG/enabled
* Fixed SWG bounce when changing SWG % 
# Update in Release 1.3.8
* Fixed PDA mode from 1.3.7
* Added SWG Boost to PDA
* More updates to protocol code for Jandy and Pentair.  
# Update in Release 1.3.7
* PDA SUPPORT IS BROKEN IN 1.3.7 DON'T UPGRADE IF YOU'RE USING PDA Mode
* PDA Note:- Due to changes to speed up programming the control panel, PDA mode does not function correctly, I will come back and fix this, but I don't have the time for this release. 
* SWG updates
* Simulator update
* Added boost functionality for SWG. (Web UI & MQTT only, not Apple homekit yet)
    * MQTT boost status is aqualinkd/SWG/Boost
    * MQTT boost on/off is aqualinkd/SWG/Boost/set
    * Web UI, long press on SWG icon for boost & percent options
    * Simple Web Ui, extra button called Boost
* Changed how programming works. (Please test fully things like, changing heater setpoints, SWG percent etc, be prepared to role back)
* Added raw RS485 logging
# Update in Release 1.3.6
* Can now debug inline from a web ui. (http://aqualinkd.ip.address/debug.html)
* Fix SWG in homekit sometimes displaying wrong value. Note to Homekit users, Upgrading to 1.3.5c (and above) will add an aditional SWG PPM tile, (look in default room). You'll need to update homebridge-aqualinkd to 0.0.8 (or later) to remove the old PPM tile (or delete you homebridge cache). This is due to a bug in homebridge-aqualinkd < 0.0.7 that didn't delete unused tiles.
* Logic for SWG RS486 checksum_errors.
* Fixed pentair packet logging, missing last byte.
* Support for two programmable lights. (Note must update your aqualinkd.conf).
* Can now display warnings and errors in the web UI (as well as log).
* Fix memory issue with PDA.
* Better support for "single device mode" on PDA.
* Fix memory leak in web UI with some browsers.
* Changes for better portability when compiling on other systems.
# Update in Release 1.3.5
* Fixed SWG bug showing off/0% every ~15 seconds (introduced in 1.3.3).
* PDA updates for freeze protect/SWG and general speed increase.
## Update in Release 1.3.4 (a)
* Logging changes.
* Fix issues in programming mode.
* Update to simulation mode.
* Changed to serial logger.
* PDA changes for SWG and Setpoints.
## Update in Release 1.3.3 (a,b,c,e,f)
* Incremental PDA fixes/enhancements.
* SWG bug fix.
## Update in Release 1.3.3
* AqualinkD will now automaticaly find a usable ID if not specifically configured.
* Support for reading (up to 4) Variable Speed Pump info and assigning per device. (Please see wiki for new config options).
  * <span style="color:red">*At present only Pentair VSP is supported, if you have Jandy VSP (ePump) and are willing to do some testing, please post on forum as I'd like to get this supported as well.*</span>
  * For VSP you will need to check configuration for `read_all_devices = yes` & `read_pentair_packets = yes` and assign RS485 Pump ID to Device ID in configuration.  serial_logger should find ID's for you.
  * WebUI will display Pump RPM only. RPM, Watts and GPH information is also available from MQTT & API.
## Update in Release 1.3.2c
* Miscellaneous bug fixes and buffer overrun (could cause core dump).
* VSP update & Pantair device support.
## Update in Release 1.3.1
* Changed the way PDA mode will sleep.
* Added preliminary support for Variable Speed Pumps. (Many limitations on support).
* Added int status to Web API.
## Update in Release 1.3.0
* Large update for PDA only control panels (Majority of this is ballle98 work)
* Can distinguish between AquaPalm and PDA supported control panels.
* PDA Freeze & Heater setpoints are now supported.
* Added PDA Sleep mode so AqualinkD can work in conjunction with a real Jandy PDA.
* Speeded up many PDA functions.
* Fixed many PDA bugs.
* Non PDA specific updates :-
* Can get button labels from control panel (not in PDA mode).
* RS485 Logging so users can submit information on Variable Speed Pumps & other devices for future support.
* Force SWG status on startup, rather than wait for pump to turn on.
* General bug fixes and improved code in many areas.
## Update in Release 1.2.6f
* Solution to overcome bug in Mosquitto 1.6.
* Fixed Salt Water Generator when % was set to 0.
* Added support for different SWG % for pool & spa. (SWG reports and sets the mode that is currently active).
* Increased speed of SWG messages.
* Few other bug fixes (Thanks to ballle98).
## Update in Release 1.2.6e (This is a quick update, please only use if you need one of the items below.)
* Unstable update.
## Update in Release 1.2.6c
* Fixed some merge issues.
* Added MQTT topic for delayed start on buttons.
* Removed MQTT flash option for delayed start (never worked well anyway).
## Update in Release 1.2.6b
* Added MQTT topic for full SWG status (MQTT section in see wiki).
* Configured option to turn on/off listening to extended device information.
* Added service mode topic to MQTT   (Thanks to tcm0116).
* Added report zero pool temp  (Thanks to tcm0116).
## Update in Release 1.2.6a
* More PDA fixes (Thanks to ballle98).
* Fix in MQTT requests to change temperature when temperature units are unkown. 
## Update in Release 1.2.6
* Fix for PDA with SPA messages.  (Thanks to ballle98).
* Added report 0 for pool temperature when not available.  (Thanks to tcm0116).
## Update in Release 1.2.5a
* Fix bug for MQTT freeze protect. 
## Update in Release 1.2.4
* Small fix for Freeze Protect.
## Update in Release 1.2.3
* Fix for setpoints on "Pool Only" configurations.
## Update in Release 1.2.2
* Support for Spa OR Pool ONLY mode with setpoints; (previous setpoints expected Spa & Pool mode)
* Added support for MQTT Last Will Message.
* NOTE: Fixed spelling errors will effect your configuration and the install.sh script will not overwrite.
    * Please compare /var/www/aqualinkd/config.js to the new one, you will need to manually edit or overide.
    * MQTT spelling for "enabled" is now accurate, so anything using the /enabled message will need to be changed.
    * Homekit will also need to be changed. Please see the new homekit2mqtt.json or modify your existing one. 
## Updates in Release 1.2
* PDA support in BETA. (Please see WiKi for details).
* Fixed bug in posting Heater enables topics to MQTT. (order was reversed).
* Serial read change. (Detect escaped DTX in packet, 1 in 10000 chance of happening).
## Updates in Release 1.1
* Changed the way AqualinkD reads USB, fixes the checksum & serial read too small errors that happened on some RS485 networks. 
* Figex bug in SWG would read "high voltage" and not "check cell".
## Updates in release 1.0e
* Web UI out of Beta.
* MQTT fix setpoints.
* Simulator is now more stable.
* Updates to serial logger.
* UI updates.
* Bug fix in MQTT_flash (still not perfect).
## Updates in Release 1.0c
* New Simpler interface.
* Start of a RS8 Simulator :-
    * You can now program the AqualinkRS from a web interface and not just the control panel.
    * Please make sure all other browsers and tabs are not using AqualinkD as it does not support multiple devices when in simulator mode.
* Fixed a few bugs.
* -- Release 1.0b --
* NEW WEB UI !!!!!!!!!!!!!  (in beta).
* Flash buttons on/off in homekit for enabeling/disabling/cooldown period as they do on the control panel.
* Full SWG support (setting %, not just reporting current state). Also reports Salt Cell status such as (no flow, low salt, high curent, clean cell, low voltage, water temp low, check PCB).
* Update to thermostats, colors are now correct in homekit, green=enabeled, orange=heating, blue=cooling (SWG only).
* Light show program mode should now support most vendors lights.
* Configuration changes for: Spa temp as pool temp/light program mode options/enable homekit button flash.
* Updated to serial_logger.
* Freeze protect, heater temperature and SWG set-points have been added to support for standard HTTP requests (MQTT & WS always had support).

# Please see Wiki for install instructions
https://github.com/sfeakes/AqualinkD/wiki

#

# Aqualink Versions tested
This was designed for Jandy Aqualink RS, so should work with AqualinkRS and iAqualink Combo control panels. It will work with Aqualink PDA/AquaPalm and NON Combo iAqualink; but with limitations.
Below are verified versions, but should work with any AqualinkRS :-


| Version | Notes | 
| --- | --- |
| Jandy Aqualink   6524 REV GG       | Everything working  |
| Jandy AquaLinkRS 8157 REV JJ       | Everything working  |
| Jandy AquaLinkRS 8157 REV MMM      | Everything working  |
| Jandy AquaLinkRS 8159 REV MMM      | Everything working  |
| Jandy AquaLinkRS B0029221 REV T    | Everything working  |
| Jandy AquaLinkRS B0029223 REV T.2  | Everything working  |
| Jandy AquaLinkRS B0029235 REV T.1  | Everything working  |
| Jandy iAqualink E0260801 REV R     | Everything working  |
| AquaLink PDA / AquaPalm            | Works, please see WiKi for limitations|

If you have tested a version not listed here, please let me know by opening an issue.
#

# License
## Non Commercial Project
All non commercial projects can be run using our open source code under GPLv2 licensing. As long as your project remains in this category there is no charge.
See License.md for more details.




# Donation
If you still like this project, please consider buying me a cup of coffee :)
<br>
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SEGN9UNS38TXJ)

