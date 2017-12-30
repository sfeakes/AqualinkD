# Aqualinkd  
linux daemon to control Aqualink RS pool controllers. Provides web UI, MQTT client & HTTP API endpoints. So you can control your pool equiptment from any phone/tablet or computer, and should work with just about Home control systems, including Apple HomeKit, Samsung, Alexa, Google, etc home hubs.

### It does not, and will never profide any layer of security. NEVER directly expose the device running this software to the outside world, only indirectly through the use of Home Automation hub's or other securty measures, like VPNs.

![Image](extras/web_ui.png?raw=true)
![Image](extras/HomeKit.png?raw=true)

# TL;DR Install

* Get a linux computer (like a Raspberry PI) linked up to the RS485 interface of your pool controller.
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
The install script is designed for systemd / systemctl to run as a service or daemon. If you are using init / init-d then don't run the install script, install manuallt and the init-d script to use is in the xtras directory.
Manual install for init-d systems
* copy ./release/aqualinkd to /usr/local/bin
* copy ./release/aqualinkd.conf to /etc
* copy ./release/aqualinkd.service.avahi to /etc/avahi/services/aqualinkd.service
* copy ./extras/aqualinkd.init.d to /etc/init-d/aqualink
* copy recuesivley ./web/* to /var/www/aqualinkd/
* sudo update-rc.d aqualinkd defaults



## TODO
* Only WEB interface (WS) & AQ_MQTT can change freeze & heater temprature set-points. Need to add support for standard HTTP. (DOMOTICZ_MQTT don't be supported until Domoticz create a better virtual thermostat)
* Web interface has a lot of fixed layout items that as specific to my implimentation. The HTML & CSS need a complete overhall and re-though to support different configurations.
* There is code to control different light modes/shows, but it's not finished and no documentation will be provided until it is finished. It will not work unless you have this exact setup Haywood ColorLogic/Aqualink RS8. 

## Hardware
You will need a [USB2RS485](https://www.amazon.com/OctagonStar-Converter-Adapter-Interface-FT232RL/dp/B01LCFRR3E/) adapter connected to your pool equiptmeent RS buss interface.  (If you have an inside controller mounted on your wall, this is usually best place, if not the outside control panel is the next best place).  Then a computer running linux connected to that USB2RS485 adapter. Code is designed & developed for raspberry pi zero w, so any computer with that as a minimum should work.

## Configuratio with home automation hubs
## Domoticz
Enable MQTT in Domoticz, and install a MQTT broker. (If you don;t want to do this, then follow generic hub setup)
Create a virtual device for each piece of pool equiptment you have, eg Filter Pump, Spa Mode, Pool Light, Cleaner then place the Domoticz IDX for each device in the aqualinkd.conf file under the appropiate button.

## MQTT
Aqualinkd supports generic MQTT implimentations, as well as specific Domoticz one described above.
To enable, simply configure the main topic in `aqualinkd.conf`. 

```mqtt_aq_topic = aqualinkd```

Then status messages will be posted to the sub topics listed below, with appropiate information. Each button uses the standard Aqualink RS controller name, so Aux_1 might be your poool light.
```
Tempratures will be a simply number posted to the topic.
aqualinkd/Temperature/Air
aqualinkd/Temperature/Pool
aqualinkd/Temperature/Spa
aqualinkd/Pool_Heater/setpoint
aqualinkd/Spa_Heater/setpoint

Buttons will be a 1 or 0 for state. 1=on 0=off
aqualinkd/Filter_Pump
aqualinkd/Spa_Mode       
aqualinkd/Aux_1
........./Aux_.
aqualinkd/Aux_7
aqualinkd/Pool_Heater
aqualinkd/Spa_Heater
```

To turn something on, or set information, simply add `set` to the end of the above topics, and post 1 or 0 in the message for a button, or a number for a setpoint. Topics Aqualinkd will act on.
```
aqualinkd/Filter_Pump/set
aqualinkd/Spa_Mode/set
aqualinkd/Aux_1/set
aqualinkd/Aux_7/set
aqualinkd/Pool_Heater/set
aqualinkd/Spa_Heater/set
aqualinkd/Pool_Heater/setpoint
aqualinkd/Spa_Heater/setpoint
```

Example that would turn on the filter pump
```
aqualinkd/Filter_Pump/set 1
```

## All other hubs excluding Apple HomeKit (Amazon,Samsung,Google etc)
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