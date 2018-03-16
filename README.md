# Aqualinkd  
linux daemon to control Aqualink RS pool controllers. Provides web UI, MQTT client & HTTP API endpoints. So you can control your pool equiptment from any phone/tablet or computer, and should work with just about Home control systems, including Apple HomeKit, Samsung, Alexa, Google, etc home hubs.

### It does not, and will never provide any layer of security. NEVER directly expose the device running this software to the outside world, only indirectly through the use of Home Automation hub's or other securty measures, e.g. VPNs.

## Builtin WEB Interface.
![Image](extras/web_ui.png?raw=true)

## In Apple Home app.
![Image](extras/HomeKit.png?raw=true)

# TL;DR Install
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



## TODO
* Only WEB interface (WS) & AQ_MQTT can change freeze & heater temprature set-points. Need to add support for standard HTTP. (DOMOTICZ_MQTT won't be supported until Domoticz create a better virtual thermostat)
* Web interface has a lot of fixed layout items that as specific to my implimentation. The HTML & CSS need a complete overhall and re-though to support different configurations.
* There is code to control different light modes/shows, but it's not finished and no documentation will be provided until it is finished. It will not work unless you have this exact setup Haywood ColorLogic/Aqualink RS8.

# Aqualink Versions tested
This was designed for Jandy Aqualink RS, so should work with AqualinkRS and iAqualink Combo controll panels. At the moment it will not work with Aqualink PDA / AquaPalm and NON Combo iAqualink.
Below are versions :-
| Version | Notes | 
|---|---|
| JANDY AquaLinkRS 8157 REV MMM      | Everything working  | 
| Jandy AquaLinkRS B0029223 REV T.2  | Everything working  |  
| Jandy iAqualink E0260801 REV R     | All working, but sometimes programming is hit 'n miss. This is a combo board iAqualink & AqualinkRS  |  
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
Specifically, make sure you configure your MQTT, Pool Equiptment Labels & Domoticz ID's in there, looking at the file it should be self explanatory. 

## Configuration with home automation hubs
## Domoticz
With MQTT
* Enable MQTT in Domoticz, and install a MQTT broker. 
* Create a virtual device for each piece of pool equiptment you have, eg Filter Pump, Spa Mode, Pool Light, Cleaner.
* Place the Domoticz IDX for each device in the aqualinkd.conf file under the appropiate button. Then modify each virtual device.

Without MQTT
* Follow generic hub setup.

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
aqualinkd/Pool_Heater
aqualinkd/Spa_Heater

Other Information (Salt Water Generator)
aqualinkd/SWG/Percent
aqualinkd/SWG/PPM
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
```

Example that would turn on the filter pump
```
aqualinkd/Filter_Pump/set 1
```

## All other hubs (excluding Apple HomeKit) Amazon,Samsung,Google etc
Create a device for each piece of pool equiptment you have, eg Filter Pump, Spa Mode, Pool Light, Cleaner. Then add the following URL to program the switching of each device
```
http://aqualinkd.ip.address:port?command=Filter_Pump&value=on
```
each button ie command=xxxxx uses the default Aqualink name so valid options are
* Filter_Pump
* Spa_Mode
* Aux_1
* Aux_2
* Aux_3
* Aux_4
* Aux_5
* Aux_6
* Aux_7
* Pool_Heater
* Spa_Heater
* Solar_Heater

To get status than can be passed by any smart hub use the below url, a JSON string with the state of each device and current temprature will be returned.
```
http://aqualinkd.ip.address:port?command=status
```


## Apple HomeKit
For the moment, native Homekit support has been removed, it will be added back in the future under a different implimentation. 
Recomended option for HomeKit support is to make use of the MQTT interface and use [HomeKit2MQTT](https://www.npmjs.com/package/homekit2mqtt) to bridge between Aqualinkd and you Apple (phone/tablet/tv & hub).
* If you don't already have an MQTT broker Installed, install one. Mosquitto is recomended, this can usually be installed with apt-get
* Install [HomeKit2MQTT](https://www.npmjs.com/package/homekit2mqtt). (see webpage for install)
* Then copy the [homekit2mqtt configuration](https://github.com/sfeakes/aqualinkd/blob/master/extras/homekit2mqtt.json) file found in the extras directory `homekit2mqtt.json`


You can of course use a myriad of other HomeKit bridges with the URL endpoints listed in the `All other hubs section`, or MQTT topics listed in the `MQTT` section. The majority of them (including HomeBridge the most popular) use Node and HAP-Node.JS, neither of which I am a fan of for the RaspberryPI. But HomeKit2MQTT seemed to have the least overhead of them all. So that's why the recomendation.


## MeteoHub
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
