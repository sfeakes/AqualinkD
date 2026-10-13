

<div align="right">
  <a href="https://aquadaemon.org">
    <img src="https://aquadaemon.org/images/aquadaemon-project2.png" height="48" alt="AquaDaemon">
  </a>
</div>

<p align="center">
  <a href="https://aqualinkd.com">
    <img src="https://aquadaemon.org/images/inline/aqualinkd.png" width="120" alt="AquachemD">
  </a>
</p>


<h1 align="center">AqualinkD</h1>
<p align="center"><b>Open-source, Linux daemon to control Jandy Aqualink RS pool controllers.</b></p>

---



Linux daemon to control Aqualink RS pool controllers. Provides web UI, MQTT client & HTTP API endpoints. Control your pool equipment from any phone/tablet or computer.  Is also compatible with most Home control systems including Apple HomeKit, Home Assistant, Samsung, Alexa, Google, etc.

<br>
Binaries are supplied for Raspberry Pi both 32 & 64 bit OS, Has been, and can be compiled for many different SBC's, Docker images also available.


### Security

**NEVER directly expose AqualinkD to the Internet. AqualinkD provides no user/password authentication. Remote access should be through a secured web proxy, VPN, or equivalent security layer. TLS client certificates (mTLS) are supported for client authentication.**


---
### Donation
If you like this project, you can buy me a cup of coffee :)
<br>
[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SEGN9UNS38TXJ)



---
### AqualinkD new home
AqualinkD has grown over the years and now has multiple repositories for software / hardware. We are brining them all together under one organization. [AqualinkD home page (under construction)](https://www.aqualinkd.com) -or- [AqualinkD organization](https://github.com/aqualinkd).<br>
AqualinkD will always be open source and so will every associated repository. Nothing will change from that perspective. You will always be able to run this on cheap off the shelf hardware.


---
### Legal and Safety Disclaimer
[Please read](https://github.com/aqualinkd/Disclaimer.md)


---
### AqualinkD discussions

* Please use github Discussions for questions (Link at top of page).
https://github.com/aqualinkd/AqualinkD/discussions
* For Bugs, please use issues link on top of page. ( please add AqualinkD version to posts )
https://github.com/aqualinkd/AqualinkD/issues

---
### Please see Wiki for installation instructions
https://github.com/aqualinkd/AqualinkD/wiki

### Release Notes & Changelog
For detailed release notes and version history, see [RELEASE.md](RELEASE.md)

---
<!--
### AqualinkD built in WEB Interface(s).
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
-->
### AqualinkD built in WEB Interface(s).

#### In Phone
<img src="extras/IMG_0251.PNG?raw=true" width="350">

<ul>
  <li>When the web page is loaded in a mobile device browser, you can <b>save to desktop</b> and an mobile app will be created for you.</li>
  <li>The order and options shown are configurable for your individual needs and/or preferences.</li>
</ul>

#### In web browser/tablet
<img src="extras/web_ui2.png?raw=true" width="800">

<ul>
  <li>The layout and functionality are from the Apple HomeKit interface.  This works in any browser or on any mobile device.</li>
  <li>Customizable tile icons & background images. (Tiles not used can be hidden).</li>
  <li>Thermostat, Switch, SWG & Light tiles have more options (ie: setting heater temperature, timers, variable speed pump RPM/GPM, salt generating percentage and light mode etc). These options are accessible by pressing and holding the tile icon.</li>
  <li>Supports live background images (ie: poll camera for still image every X seconds).</li>
</ul>

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
<!--* http://aqualink.ip/simple.html   <- (Simple opion if you don't like the above)-->
* http://aqualink.ip/simulator.html  <- (Displays all simulators in one page with tabs)
* http://aqualink.ip/aqmanager.html  <- (Manage AqualinkD configuration & runtime)
* http://aqualink.ip/allbutton_sim.html  <- (All Button Simulator)
* http://aqualink.ip/onetouch_sim.html  <- (One Touch Simulator)
* http://aqualink.ip/aquapda_sim.html  <- (PDA simulator)
#<a name="release"></a>


# ToDo (future release)
* Create iAqualink Touch Simulator
* AqualinkD to self configure. (Done for ID's, need to do for Panel type/size)
* Support for (non Jandy) external ORP and Ph sensors


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

-->

# Call for Help.
* The only Jandy devices I have not decoded yet are LX heater & Chemical Feeder. If you have either of these devices and are willing to post some logs, please let me know, or post in the [Discussions area](https://github.com/aqualinkd/AqualinkD/discussions)


<br>
<br>
<br>

# <span style="color: red;">Notice</span>

AqualinkD will soon be dropping support for OS's older than debian bullseye. (GLIBC 2.31 will be minimum).

AqualinkD will be moving over to github hosted runners for compiling, currently AqualinkD is using self hosted. This means supporting old OS releases like Stretch is not worth it. AqualinkD will still be tested and support local compiling on old OS's, just the binaries in the release packages will be for bullseye and newer
<hr>
<br>
<br>


<!-- 
NEED TO FIX NEXT THIS RELEASE.

Panel = RS-2/6 Dual will not have spa on auto config. (fine after saving config and restart).
Need to fix SWG when it's set to 0, thinks it's off and can't change, if it states 0 need to be enable, if blank turn off.
Need to look at sub panel (combined panels)
when serial port is wrong, can't edit config.



FIX THIS.
aqualinkd-cm test unit
Notice:AqualinkD: Starting Aqualink Daemon v3.1.1 (Dev2) !
Error: AqualinkD: Config error, VSP Pumps limited to 4, ignoring : button_01_pumpEfficiency=57.35
Info:  Error:   AqualinkD: Config error, VSP Pumps limited to 4, ignoring : button_01_pumpEfficiency=57.35

READ air/pool/spa temp from RS_serial protocol.  https://github.com/aqualinkd/AqualinkD/issues/528#issuecomment-4661954296
-->
## Release 3.1.1 (Dev)
* Changes for VSP and Virtual VSP for panel rev Yg
* Added Pump Efficiency, can monitor when filter / skimmers need cleaning on VSP pumps.[Click for more details](https://github.com/aqualinkd/VSP_Efficiency.md)
* Added support for Jandy Infinite Watercolor (RS485)
* Support for AquaRite on panel rev prior to L.
* Fixed issue for Dual panels and Spa missing.

## Release 3.1.0 (April 2026)
* Added device power to watts in MQTT discovery for power monitoring
* Updated timers to be finer grained, (seconds vs minutes). - ie support for dosing pumps.
* Added config options for button (circuit) runtimes to set default runtimes for a device.
* Added support for onetouch macro's to OneTouch protocol.

## Release 3.0.4 (March 2026)
* Fixed AqualinkD not starting when IP not assigned and MQTT enabled.
* Fixed aqmanager config editor issue when adding pumpType. 

## Updates in 3.0.3
* Fixed setting SWG for PDA v1.2
* Fixed bug with slider run times in web UI
* Fixed bug with heater max min in web UI
* Added light_programming_iaqualink_delay to delay aqualink protocol while light is being programmed, some users have encountered panel slowing down while light is being programmed through iAqualink.
* Increased allowed length of sensor labels.
* Minor updates to MQTT discovery protocol.

## Updates in 3.0.2
* Fixed bug with SWG being enabled if one is not present.
* Fixed bug with light_programs ending in 'show' aqualinkd.conf

## Updates in 3.0.1
* UI Update for web config.
* UI Now support for themes. (auto, dark, light -or- custom)
* Fixed UI not updating for sensors.
* Updates to UOM's for HA.   gal/min, W, rpm
* Fix issue with multiple bad sensors in config.
  
## Updates in 3.0.0
* <B>NOTE:- When upgradeing to v3.0.0</b> if you see bank AqualinkD screen (ie no buttons), please clear browser cache. This also goes for the mobile/webapp (you may need to delete and re-add to mobile home screen)
* Serial optimization for AqualinkD HAT.
* Can now edit webconfig in aqmanager, added many UI customization options.
  * [Click for details](https://github.com/aqualinkd/AqualinkD/wiki#Web-Config)
  * web/config.js is now web/config.json any custom settings will need to be migrated.
  * Added example plugin of how to get HomeAssistant devices to show up in AqualinkD UI. [Click for details](https://github.com/aqualinkd/AqualinkD/wiki#WebUIplugin)
* upgraded network library ( HTTP(S), MQTT(S), WS ) 
* Added support for HTTPS and MQTTS. 
  * HTTPS is for two way auth only, ie You create your own cert and load on both AqualinkD server and all client devices.
  * Example script to generate HTTPS certificates is in (./extras/generate-certs.sh)
* Optimized updates to MQTT, web sockets & WebUI (only update when absolutely necessary)
* Added option to select version to install, including dev releases.
* MQTT Discovery for all supporting hubs (HomeAssistant Domoticz Hubitat OpenHAB etc)
* Moved Domoticz support over to MQTT autodiscovery.
* Change tile color & label for ph / orp & ppm tiles when values are out of optimal range.
* Add mV as the UOM for ORP tile.
* Fixed bug with sensor UOM.
* UI code cleanup.
* Included program advance to AqualinkD programmable light mode. (lot quicker for lights that remember state)
* Changed caching of HTTP server. (Better for UI config updates)
* Autoconfigure will now get panel size/type for panels that support PC-Dock interface.
* Autoconfigure will *try* to work for PDA panels.
* PDA Color light fixes.
* PDA Dimmer lights now supported.
* Cleaned up exit & errors when running as daemon and docker.
* Fixed issues with external sensors and homekit.
* Added preliminary support for Jandy Infinite water color lights.
* Renamed serial_logger to rs485mon (more compliant with unix naming standards)

### More Release Notes & Changelog
For detailed release notes and version history, see [RELEASE.md](RELEASE.md)


# Please see Wiki for install instructions
https://github.com/aqualinkd/AqualinkD/wiki

#

# Aqualink Versions tested
This was designed for Jandy Aqualink RS, so does work with AqualinkRS and iAqualink Combo control panels. It will work with Aqualink PDA/AquaPalm; but with limitations.

AqualinkD is known to work with Panel Versions from Rev H to the Latest Rev Yg
<!--
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
-->
# License
## Non Commercial Project
All non commercial projects can be run using our open source code under GPLv2 licensing. As long as your project remains in this category there is no charge.
See License.md for more details.


# Legal and Safety Disclaimer
see [Safty & Legal](Disclaimer.md)
#

# Donation
If you still like this project, please consider buying me a cup of coffee :)
<br>
[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SEGN9UNS38TXJ)

