# Aqualinkd  
linux daemon to control Aqualink RS pool controllers. Provides web UI, MQTT client & HTTP API endpoints. So you can control your pool equiptment from any phone/tablet or computer, and should work with just about Home control systems, including Apple HomeKit, Samsung, Alexa, Google, etc home hubs.

### It does not, and will never provide any layer of security. NEVER directly expose the device running this software to the outside world, only indirectly through the use of Home Automation hub's or other securty measures, e.g. VPNs.


## Donation
If you like this project, you can buy me a cup of coffee :)
<br>
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SEGN9UNS38TXJ)

## Builtin WEB Interface(s).

<table width="100%" border="0" cellpadding="20px">
 <tr><th width="50%">Default web interface</th><th wifth="50%">Simple web interface</img></th><tr>
 <tr><td><img src="extras/IMG_0251.PNG?raw=true" width="350"></img></td><td><img src="extras/simple.png?raw=true" width="350"</img></td></td></td>
  <tr><td colspan="2">
     Both Interfaces
     <ul>
        <li>If you load the web page in a mobile device browser, then save to desktop an app will be created for you.</li>
        <li>Order and options shown are configurable</li>
   </ul>
   </td></tr>
   <tr><td colspan="2">
     Default Interfaces
   <ul>
     <li>The layout & functionality are a from Appple HomeKit interface, only this works in any browser or mobile device.</li>
      <li>Customizable tile icons & background image. (can hide any tile)</li>
      <li>Thermostst, SWG & Light tiles have more options (like setting heater setpoint, light mode etc) that can be accessed with a long press</li>
      <li>Support live background imags (ie poll camera for still image every X seconds)</li>
      </ul>
   </td></tr>
 </table>


### Old web UI, (Still in release if you prefer it)
<img src="extras/web_ui.png?raw=true" width="700"></img><br>
### Simulator
Designed to mimic AqualinkRS6 All Button keypad, and just like the keypad you can use it to completley configure the master control panel<br>
<img src="extras/simulator.png?raw=true" width="550">

### In Apple Home app.
<img src="extras/HomeKit2.png?raw=true" width="800"></img>
* (Salt Water Generator is configured as Thermostat as it's the closest homekit accessory type, so &deg;=% and Cooling=Generating)
* Full support for homekit scenes, so can make a "Spa scene" to turn spa on, set spa heater particular temperature, turn spa blower on, etc etc) 

### In Home Assistant 
<img src="extras/HomeAssistant.png?raw=true" width="600"></img>

## All Web interfaces.
* http://aqualink.ip/     <- (New UI)
* http://aqualink.ip/old   <- (If you prefer the old UI, this is not maintained)
* http://aqualink.ip/simple.html   <- (Anothr opion if you don't like the above)
* http://aqualink.ip/simulator.html  <- (RS8 All Button Control Panel simulator)
## Updates is Release 1.0e
* Web UI out of Beta
* MQTT fix setpoints
* Simulator is now more stable.
* updates to serial logger
* UI updates
* bug fix in MQTT_flash (still not prefect fix)
## Latest update in Release 1.0c
* New Simple interface.
* Start of a RS8 Simulator :-
    * So you can program the AqualinkRS form a web interface and not control panel.
    * Please make sure all other browsers & tabs are not using AqualinkD. it doesn't support multiple devices when in simulator mode.
* Fixed a few bugs.
* -- Release 1.0b --
* NEW WEB UI !!!!!!!!!!!!!  (in beta)
* Flash buttons on/off in homekit for enabeling / disabeling / cooldown period as they do on control panel
* Full SWG support (setting %, not just reporting current state). Also reports Salt Cell status such as (no flow, low salt, high curent, clean cell, low voltage, water temp low, check PCB)
* Update to thermostats, colors are now correct in homekit, green=enabeled, orange=heating, blue=cooling (SWG only)
* Light show program mode should now support most vendors lights.
* config changes for (spa temp as pool temp / light program mode options / enable homekit button flash)
* updated to serial_logger.
* freeze protect, heater temperature & SWG set-points have been added to support for standard HTTP requests (MQTT & WS always had support)

# TL;DR Install
* For new install or update existing install follow the same procedures. Your configuration file will not be overriden, so on an update check you have added new config options to your config, your old config will work, you just may not have the new features enabeled. If you have modified any files in the web directory then you should back them up before upgrading.
## Quick instal if you are using Raspberry PI
* There is a chance the pre-compiled binary will run, copy the git repo and run the install.sh script from the release directory. ie from the install directory `sudo ./release/install.sh`
* try to run it with :-
    * `sudo aqualinkd -d -c /etc/aqualinkd.conf`
    * If it runs, then start configuring it to your setup.
    * If it fails, try making the execuatable, below are some instructions.

## Making & Install
* Depends on a linux computer connected to a USB 2 RS485 adapter, that's connected to your pool controller.
* Get a copy of the repo using git.
* run make
* run sudo make install
* edit `/etc/aqualinkd.conf` to your setup
* Test every works :-
    * `sudo aqualinkd -d -c /etc/aqualinkd.conf`
    * point a web browser to the IP of the box running aqualinkd
* If all is good enable as service
    * sudo `systemctl start aqualinkd`

### Note:-
The install script is designed for systemd / systemctl to run as a service or daemon. If you are using init / init-d then don't run the install script, install manually and the init-d script to use is in the xtras directory.
Manual install for init-d systems
* copy ./release/aqualinkd to /usr/local/bin
* copy ./release/aqualinkd.conf to /etc
* copy ./release/aqualinkd.service.avahi to /etc/avahi/services/aqualinkd.service
* copy ./extras/aqualinkd.init.d to /etc/init-d/aqualink
* copy recuesivley ./web/* to /var/www/aqualinkd/
* sudo update-rc.d aqualinkd defaults



## New WEB interface(s)
* Doesn't work in MS Edge or MS Exploder browsers. (Firefox, Chrome, Safari & mobile versions of each) all seem to be fine, use the old or simple.html interface if using those browsers.
* use http://aqualink.ip.address/ to access new UI
* look in `<install_dir>/web/hk` hopefully customizing icon and background images are self explanatory.
* icons should be around 50x50 pixles and in PNG format, background any size in JPG or just delete the file if you want solid color.
* edit `<install_dir>/web/config.js` there are two arrays that can be modified
    * `devices` List any device you want to see in there (commanent out the ones you don't), the odrer of that list will be the display order of devices.
    * `light_program`  If you have a programable light (and it's configured in `aqualinkd.conf`), then list the light modes here.
    * Plenty of colors to change if you like. 



# Aqualink Versions tested
This was designed for Jandy Aqualink RS, so should work with AqualinkRS and iAqualink Combo controll panels. At the moment it will not work with Aqualink PDA / AquaPalm and NON Combo iAqualink.
Below are varified versions (But should work witn any AqualinkRS) :-


| Version | Notes | 
| --- | --- |
| JANDY AquaLinkRS 8157 REV MMM      | Everything working  |
| Jandy AquaLinkRS 8157 REV JJ       | Everything working  |
| Jandy AquaLinkRS B0029223 REV T.2  | Everything working  |
| Jandy AquaLinkRS B0029235 REV T.1  | Everything working  |
| Jandy iAqualink E0260801 REV R     | All working, but sometimes programming is hit 'n miss. This is a combo board iAqualink & AqualinkRS |  
| AquaLink PDA / AquaPalm            | Not usable, work in progress.|


Please post details in issues section if you have one not listed above.

## Hardware
You will need a [USB2RS485](https://www.amazon.com/OctagonStar-Converter-Adapter-Interface-FT232RL/dp/B01LCFRR3E/) adapter connected to your pool equiptmeent RS buss interface.  (If you have an inside controller mounted on your wall, this is usually best place, if not the outside control panel is the next best place).  Then a computer running linux connected to that USB2RS485 adapter. Code is designed & developed for raspberry pi zero w, so any computer with that as a minimum should work.

Raspberry Pi Zero is the perfect device for this software. But all Raspberry PI's are inherently unstable devices to be running 24/7/365 in default Linux configrations. This is due to the way they use CF card, a power outage will generally cause CF card coruption. My recomendation is to use what's calles a "read only root" installation of Linux. Converting Raspbian to this is quite simple, but will require some Linux knoladge. There are two methods, one uses overlayfs and if you are not knolagable in Linux this is the easiest option. There are a some downsides to this method on a PI, so my prefered method is to simply use tmpfs on it's own without overlayfs ontop, this is easier to setup initially, but will probably require a few custom things to get right as some services will fail. Once you are up and running, You should search this topic, and there are a plenty of resources, and even some scripts the will do everything for you.  But below are two links that explain the process.

[Good overlayfs tutorial on PI Forums](https://www.raspberrypi.org/forums/viewtopic.php?t=161416)

[My prefered way to impliment](https://hallard.me/raspberry-pi-read-only/)

I have my own scripts to do this for me, and probably won't ever document or publish them, but thay are very similar to the 2nd link above.

## Aqualinkd Configuration
Please see the [aqualinkd.conf]
(https://github.com/sfeakes/aqualinkd/blob/master/extras/aqualinkd.conf) 
example in the release directory.  Many things are turned off by default, and you may need to enable or configure them for your setup.
Main item to configure is the Aqualink RS485 address so it doesn't conflict with any existing control panels or equiptment. By default it's set to 0x0a which is the second usable address of an allbutton control panel. Included is a serial loging tool that can let you know all information on the RS485 buss, you can use this to find a good address.
```
make slog    <-- will make the serial logging tool
./release/serial_logger /dev/ttyUSB0    <- is probably all you'll need to run.
--- example output ---
Notice: Logging serial information, please wait!
Notice: ID's found
Notice: ID 0x08 is in use 
Notice: ID 0x60 is not used 
Notice: ID 0x0a is not used  <-- can use for Aqualinkd
Notice: ID 0x0b is not used  <-- can use for Aqualinkd
Notice: ID 0x40 is not used 
Notice: ID 0x41 is not used 
Notice: ID 0x42 is not used 
Notice: ID 0x43 is not used 
<-- snip -->
Notice: ID 0x50 is not used 
Notice: ID 0x58 is not used 
Notice: ID 0x09 is not used  <-- can use for Aqualinkd
Notice: ID 0x88 is in use

Any address listed with Aqualinkd can be used.  This is the `device-id` address in the config file.

Other command like options for serial_ligger are :-
-d        (print out every packet)
-p 1000   (log 1000 packets, default is 200)
``` 
Specifically, make sure you configure your MQTT, Pool Equiptment Labels & Domoticz ID's in there, looking at the file it should be self explanatory. 

#
# Configuration with home automation hubs

AqualinkD offers 3 ways to connect to it from other devices MQTT, Web API's & Web Sockets.
* MQTT is a realtime pub/sub style event system and will require you to install a MQTT server, like Mosquitto
* WEB API's simple poll based way to get information and request something to be done
* WEB Sockets is realtime and realy only applicable for the WEB UI's included. it's not documented. If someone wants to use this, let me know and I can document it. (or look at the index.html and it should be easy to work out)

All Hubs will use either MQTT or WEB API to counicate with AqualinkD. MQTT is prefered, but it will depend on your setup and how you want to achieve the integration.  The Generic MQTT and WEB API defails are below, followed by specific implimentations for diferent hubs.
All Hubs you will Create a device for each piece of pool equiptment you have, eg Filter Pump, Spa Mode, Pool Light, Cleaner, Water Temperature etc then configure that device to use either the MQTT or API endpoints listed below to both get the status and set the status for that device.

## MQTT
Aqualinkd supports generic MQTT implimentations, as well as specific Domoticz one described above.
To enable, simply configure the main topic in `aqualinkd.conf`. 

```mqtt_aq_topic = aqualinkd```

Then status messages will be posted to the sub topics listed below, with appropiate information. Each button uses the standard Aqualink RS controller name, so Aux_1 might be your poool light.
Note, all Temperatures will be in C, if you have your pool controler configured to F aqualinkd will automatically do the conversion.
```
Temperatures simply be a number posted to the topic.
aqualinkd/Temperature/Air
aqualinkd/Temperature/Pool
aqualinkd/Temperature/Spa
aqualinkd/Pool_Heater/setpoint
aqualinkd/Spa_Heater/setpoint

Buttons will be a 1 or 0 for state. 1=on 0=off
aqualinkd/Filter_Pump
aqualinkd/Spa_Mode       
aqualinkd/Aux_1
....Aux_2-6....
aqualinkd/Aux_7
aqualinkd/Pool_Heater               (0 off, 1 on and heating)
aqualinkd/Pool_Heater/enabeled      (0 off, 1 on but not heating - water has reached target temp)
aqualinkd/Spa_Heater                (0 off, 1 on and heating )
aqualinkd/Spa_Heater/enabeled       (0 off, 1 but not heating - water has reached target temp)
aqualinkd/SWG                       (0 off, 2 on generating salt)
aqualinkd/SWG/enabeled              (0 off, 2 on but not generating salt - SWG reported no-flow or equiv)

Other Information (Salt Water Generator)
aqualinkd/SWG/Percent               ( SWG Generating %, i.e. 50)
aqualinkd/SWG/PPM                   ( SWG Parts Per Million i.e. 3100)
aqualinkd/SWG/Percent_f             (since we use a homekit thermostat for SWG and use degC as %, we need to pass degF for US phone)

```

To turn something on, or set information, simply add `set` to the end of the above topics, and post 1 or 0 in the message for a button, or a number for a setpoint. Topics Aqualinkd will act on.
```
aqualinkd/Filter_Pump/set
aqualinkd/Spa_Mode/set
aqualinkd/Aux_1/set
....Aux 2-6.../set
aqualinkd/Aux_7/set
aqualinkd/Pool_Heater/set
aqualinkd/Spa_Heater/set
aqualinkd/Pool_Heater/setpoint/set
aqualinkd/Spa_Heater/setpoint/set
aqualinkd/SWG/Percent/set           
aqualinkd/SWG/Percent_f/set          ( Set % as a degF to degC conversion for Homekit)
```

Example that would turn on the filter pump
```
aqualinkd/Filter_Pump/set 1
```

## Web API's
The following URL is an example of how to turn the pump on
```
http://aqualinkd.ip.address:port?command=Filter_Pump&value=on
```
each button ie command=xxxxx uses the default Aqualink name, and the value=xx is simply on or off. Valid options for command are :-
```
Filter_Pump
Spa_Mode
Aux_1
Aux_2
Aux_3
Aux_4
Aux_5
Aux_6
Aux_7
Pool_Heater
Spa_Heater
Solar_Heater
```

There are a few extended commands that will set Heater setpoints, Salt Water Generator %, and light mode (if configured), these are in the came form only the value is a number not a on/off
```
http://aqualinkd.ip.address:port?command=pool_htr_set_pnt&value=95
```
```
poollightmode
swg_percent
pool_htr_set_pnt
spa_htr_set_pnt
frz_protect_set_pnt
```
To get status that can be passed by any smart hub use the below url, a JSON string with the state of each device and current temprature will be returned.
Both return similar information, the `devices` will simply return more, but be harder to pass.  So pick which souits your configuration better.
```
http://aqualinkd.ip.address:port?command=status
or
http://aqualinkd.ip.address:port?command=devices
```
The status of any device can be :-
```
on
off
enabled     -> (for heaters, they are on but not heating)
flash       -> (delayed On or Off has been inicitated by the control panel)
```


#
# Specific Hub integrations

## Apple HomeKit
For the moment, native Homekit support has been removed, it will be added back in the future under a different implimentation. 
Recomended option for HomeKit support is to make use of the MQTT interface and use [HomeKit2MQTT](https://www.npmjs.com/package/homekit2mqtt) to bridge between Aqualinkd and you Apple (phone/tablet/tv & hub).
* If you don't already have an MQTT broker Installed, install one. Mosquitto is recomended, this can usually be installed with apt-get
* Install [HomeKit2MQTT](https://www.npmjs.com/package/homekit2mqtt). (see webpage for install)
* Then copy the [`homekit2mqtt.json`](https://github.com/sfeakes/aqualinkd/blob/master/extras/homekit2mqtt.json) configuration file found in the extras directory to your homekit2mqtt storage directory.

* If you want to run homekit2mqtt as a daemon service, there are files in the extras directory to help you do this.
  * copy [`homekit2mqtt.service`] to `/etc/systemd/system/homekit2mqtt.service`
  * copy [`homekit2mqtt.defaults`] to `/etc/defaults/homekit2mqtt`
  * create directory `/var/lib/homekitmqtt`
  * copy [`homekit2mqtt.json`](https://github.com/sfeakes/aqualinkd/blob/master/extras/homekit2mqtt.json) to `/var/lib/homekitmqtt`
  * edit `/etc/defaults/homekit2mqtt` to change and homekit2mqtt config parameters.
  * install and start service
    * `sudo systemctl enable homekit2mqtt`
    * `sudo systemctl daemon-reload`
    * `sudo systemctl start homekit2mqtt`


You can of course use a myriad of other HomeKit bridges with the URL endpoints listed in the `All other hubs section`, or MQTT topics listed in the `MQTT` section. The majority of them (including HomeBridge the most popular) use Node and HAP-Node.JS, neither of which I am a fan of for the RaspberryPI. But HomeKit2MQTT seemed to have the least overhead of them all. So that's why the recomendation.

#
# Domoticz
With MQTT
* Enable MQTT in Domoticz, and install a MQTT broker. 
* Create a virtual device for each piece of pool equiptment you have, eg Filter Pump, Spa Mode, Pool Light, Cleaner.
* Place the Domoticz IDX for each device in the aqualinkd.conf file under the appropiate button. Then modify each virtual device.

Without MQTT
* Follow generic hub setup.

#
# Home Assistant (HASS.IO)
Copy the sections from 
[`HASSIO Implimentation.txt`](https://github.com/sfeakes/aqualinkd/blob/master/extras/HASSIO.implimentation.txt)
 into your config.yaml

#
# MeteoHub
If you want to log/track water temps within MeteoHub, simple configure it to poll the below URL and water & air tempratures will be sent in a format native to MeteoHub. 
```
http://aqualinkd.ip.address:port?command=mhstatus
```
t0 will be air temp and t1 water temp.

To use.  copy the [meteohub-aq-plugin.sh script](https://github.com/sfeakes/aqualinkd/blob/master/extras/meteohub-aq-plugin.sh) from the extras directory to your meteohub box, edit the script and use your IP address in the line that makes the URL call, below.
```
wget -O /dev/stdout 'http://your.ip.address.here/?command=mhstatus' 2>/dev/null
```

In meteohub create a new weatherstation plug-in, plug-in path is the path to the above sctipt, and his save.  2 new sensors should now show up as thermo in the sensor page.

# License
## Non Commercial Project
All non commercial projects can be run using our open source code under GPLv2 licensing. As long as your project remains in this category there is no charge.
See License.md for more details.




# Donation
If you like this project, you can buy me a cup of coffee :)
<br>
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=SEGN9UNS38TXJ)

