
MUST DO BEFORE RELEASE

done - Check serial blocking mode for startup on getting probes.
done - Serialport timeouts / recovery in blockingmode
done - Serialport timeouts / recovery in non blocking mode.
done - Config "read_extra_devices" "read_pentair_packets" change to read_swg / read_pentair_pump / read_jandy_pump
done - Post MQTT status messages.  (clean up duplicates and non terminated strings)
done - tcdrain in blocking mode.
done - setpoint+1
done - doesn't look like protocol supports it. RS16 LED States in serialadapter (on/off, check flash & enable)  
done - Link Spa Mode & Spa Heater in web UI ?
done - Programming mode, getting message before it expects message. https://github.com/sfeakes/AqualinkD/issues/111
done (CHECK) - SWG enable/on mismatch when in Boost (restarting aqualinkd)
done - Added Boost to homekit
done - Timers.
done - crashed when passing -c but not file

Light mode from this post https://aqualinkd.freeforums.net/thread/109/lights-turning-on-lightmode?page=1&scrollTo=611

Spa reports 0 (not -17.78) on startup.
Set time (add ~5sec to the functions)
Fix Color lights turn on one press if aqualinkd controlled (looks like bug in programmer)
Fix PRESTATE_ONOFF with domoticz
Fix SWG state messages unknonw 0x0b 0x03 (and others)
Timer when Delay ()
Logging for ePump
simulate panel doesn;t turn off simulate_panel
Really remove simulate control panel

SWG "General fault" in panel, different message from SWG "clean cell"
Do I want to use RSSA to test service / timeout modes?

1/2 done - Colored light (dam hit enter), test if better over RS Serial Adapter.  (also hit enter on allbutton)
change all send_ack/message to bool types, and no pop off send queue until actually sent correctly. (for readaheadb4write)
Should I change DEBUG_SERIAL to RSSD_LOG ? (in config debug logging would be all masks except RSSD_LOG.)
Startup programmers from A interface starting before B interface. (look into further)

Pump update fails "can't find pump" in iAq when pumps are on during aqualinkd startup.  This may be when iAq starts before rsallbutton



Update Wiki PDA section.   (can't support it)

core dump on web port blocked in threadded mode
also check MQTT connection error

NICE
serial_logger add options to connect to panel, reply to ID allbutton ID messages.

OTHER
Update homekit to new API
Look at using specific MQTT topics to ocercome multiple requests messages when changing setpoints?

get_aux_information Loops too much over special devices

add ignore_swg_messags from (Allbutton, iAqua, onetou, RSbuss), probably ignore_pent_pump from above as well.




