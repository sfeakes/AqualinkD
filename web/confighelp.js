var _confighelp = {};
_confighelp["panel_type"]="Your RS panel type & size. ie 4, 6, 8, 12 or 16 relates to RS4, RS6, RS8, RS12 or RS16. Must be in format XX-N ZZZZ (XX=RS or PD, N=Circuits, ZZZ=Combo or Only or Dual)";
_confighelp["device_id"]="The id of the AqualinkD to use, use serial_logger to find ID's. If your panel is a PDA only model then PDA device ID is 0x60. set to device_id to 0xFF for to autoconfigure all this section";
_confighelp["mqtt_address"]="MQTT address has to be set to ip:port enable MQTT"
_confighelp["read_RS485_swg"]="Read device information directly from RS485 bus"
_confighelp["force_swg"]="Force any devices to be active at startup. Must set these for Home Assistant integration"
_confighelp["enable_scheduler"]="AqualinkD's internal scheduler"
_confighelp["event_check_use_scheduler_times"]="Turn on filter pump from events that can cause it to turn off"