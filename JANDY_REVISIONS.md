# AquaLink RS Software and PCB Revisions

## Power Center software revisions

<table>
<thead><tr><th>Software Revision</th><th>Effective Date</th><th>System Changes</th><th>Comments</th></tr></thead>
<tbody>
<tr><td colspan="4"><b>PCB 44-Pin PPD Socketted</b></td></tr>
<tr>
  <td valign="top">Rev. A</td>
  <td valign="top">6/6/1994</td>
  <td valign="top">
    • Alpha test units.
  </td>
  <td valign="top">
    PCB # 7074
  </td>
</tr>
<tr>
  <td valign="top">Rev. B</td>
  <td valign="top">1/1/1995</td>
  <td valign="top">
    • Beta test units.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. C</td>
  <td valign="top">2/15/1996</td>
  <td valign="top">
    • Aux 1 must be used to activate a non-booster pump pool cleaner.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. D</td>
  <td valign="top">2/29/1996</td>
  <td valign="top">
    • Ties the Cleaner JVA to the pool filter pump operation to correct non-booster pump pool
        cleaner.<br>
    • System will not run programs after freeze protection is activated.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. E</td>
  <td valign="top">3/26/1996</td>
  <td valign="top">
    • Programs will now run after freeze protection is activated.<br>
    • System Lockout now fully locks out all programming.<br>
    • Simplification of Menu (add System Setup to Menu).
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. F</td>
  <td valign="top">7/10/1996</td>
  <td valign="top">
    • Remove automatic battery low message from display and add the battery low message to
        Diagnostics.<br>
    • Support for 2/6 system sharing one heater and solar system (adjustable Freeze Sensor needed if
        RS 2/6 is activating a solar system).<br>
    • Add light dimming to RS 4.<br>
    • Allows 3 Spa Side Switches with RS 4.<br>
    • Add to Menu under LOCKOUT the word DEVICE; this allows an individual item to be locked out so
        it will not operate.<br>
    • Adds ability to label Solar button when it is used as an auxiliary circuit programming and or
        Reviewing more then one item.<br>
    • Ability to label extra Aux button.<br>
    • Corrects Two Speed programming problems.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. G</td>
  <td valign="top">3/7/1997</td>
  <td valign="top">
    • Freeze activation temperature is now adjustable down to 34 degrees F. Default activation
        temperature is still 38 degrees F.<br>
    • Pressing, then releasing the Reset button at the Control displays both the Control software
        and Power Center PPD part number and rev letter.<br>
    • LED will light when an auxiliary, assigned to light dimming, is turned on.<br>
    • Solar button available to be used for Solar Control or Extra Auxiliary on AquaLink RS 4.<br>
    • Filter pump LED will remain on when filter pump is on even if heater has been turned on.<br>
    • Supports SpaLink function<br>
    • Cleaner JVA can always be assigned, whether dip switch 1 or dip switch 7 is on or off.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. GG</td>
  <td valign="top">4/8/1997</td>
  <td valign="top">
    • With G software, pressing the CANCEL button could lockup the system
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. GGG</td>
  <td valign="top">7/24/1997</td>
  <td valign="top">
    • GG software and the PPD hardware had a capability problem. GGG corrects this problem.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. H</td>
  <td valign="top">10/13/1998</td>
  <td valign="top">
    • Memory expansion from the current 64k to 128k of read only memory (ROM). This will allow for
        future peripheral expansion.<br>
    • SpaLink display will read "OFF" rather than three dashed lines when both the pool or spa is
        off. The pool temperature will be displayed when the pool is circulating, and spa
        temperature will be displayed when the spa is circulating.<br>
    • Laars LX heater interface. Compatible with LX software revisions C04 to C08<br>
    • When water temperature is less than the thermostat setting, heater is enabled and filter pump
        is on, the heater will now fire consistently.<br>
    • Default year is 1998<br>
    • Improve clock operation<br>
    • Additional items have been added to the auxiliary labels menu: cleaner, fan, filter pump,
        heater, HI-E2, home heat, home a/c, lamp, Laars lite, lights, music, ray vac, solar pump,
        spa, sprinkler 1, sprinkler 2, sprinkler 3, stereo, color wheel, equipment light, fan light,
        fence light, garage light, hall light, left light, light 1, light 2, light 3, kitchen light,
        perimeter light, room light, sauna light, table light, basement light, bedroom light
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. HH</td>
  <td valign="top">10/15/1998</td>
  <td valign="top">
    • Corrects clock operation
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. I</td>
  <td valign="top">5/9/2000</td>
  <td valign="top">
    • Supports OneTouch control panel along with All Button Control panel<br>
    • Supports AquaRite chlorine generator system.<br>
    • Add SOLAR PRIORITY. When activated system will heat with solar first and then switch to the
        gas heater if necessary. If desired temperature is not achieved system will switch on the
        gas heater to bring the water up to temperature.<br>
    • Add INFLOOR. When activated a JVA plugged into the Solar JVA socket will rotate every 20
        minutes. If Solar sensor is installed this operation moves to Aux 3, a Ji/JVA AUX Board is
        needed (p/n 5254) if this is the case.<br>
    • Correct operation of SPILLOVER from Power Center PCB so it is not as confusing.<br>
    • Adds flexibility to the SPARE AUX socket. If dip switch 6 is off SPARE AUX is off when system
        is off, on when pool is circulating and off when spa is circulating. Turning dip 6 on
        reverses this operation. Note: System must be a Pool and Spa Combination.<br>
    • Corrects the confusion which occurs when trying to cancel program groups.<br>
    • Solar heating limited to 104 F in service modes.<br>
    • Light dimming can be assigned to EXTRA AUX.<br>
    • Corrects solar operation with Dual Equipment models.<br>
    • Changes the water temperature sensing from the LX thermister to the RS thermister. This
        corrects a problem with solar operation where the solar would turn on then quickly off
        because the LX sensor would registered the warmth of the water. Compatible with LX software
        revisions C10 and beyond.<br>
    • Supports Serial Adapter.
  </td>
  <td valign="top">
    Power Center Printed Circuit Board (PCB) must be Revision I to be compatible with Revision I or
        II software (PPD).<br>
    To identify a revision I PCB look for the letter I written next to the Assembly #, Revision I on
        a sticker in the lower right hand corner or the number 32 in the part number of the …
        <i>(text cut off in source PDF)</i><br>
    Light dimming (EXTRA AUX): Light Dimming does not work on Aux B1 thru B4, C1 thru C4, or D1 thru
        D4 on Aux. PCB. On RS 10's or larger must use Aux B5 - B8, C5 - C8, or D5 thru D8 on Aux PCB
        for Light … <i>(text cut off in source PDF)</i>
  </td>
</tr>
<tr>
  <td valign="top">Rev. II</td>
  <td valign="top">1/1/2002</td>
  <td valign="top">
    • Corrects a problem where freeze protected items assigned from the OneTouch control panel would
        be lost when the system is switch from Auto to Service then Time Out and back to Auto.
  </td>
  <td valign="top">
    Last software written to a 44 pin PPD.
  </td>
</tr>
<tr><td colspan="4"><b>PCB 52-Pin PPD Socketted</b></td></tr>
<tr>
  <td valign="top">Rev. J</td>
  <td valign="top">11/1/2002</td>
  <td valign="top">
    • Default modes for the OneTouch buttons have been setup from the factory and replace ONETOUCH
        1, ONETOUCH 2. They now read SPA MODE and CLEAN MODE. SPA MODE will activate the spa and spa
        heater, CLEAN MODE activates the pool circulation pump unless dip switch one is on, then it
        also activates the cleaner circuit (Aux 1). ONETOUCH 3 has no devices assigned to it.<br>
    • When the Power Center is in Service or Time Out Mode, any activated equipment will be
        displayed as "equipment on" at the OneTouch Control panel.<br>
    • If an LX heater is connected to the RS via the four-conductor wire, the heater button on the
        AquaLink RS Power Center Bezel will control the heaters operation. From the Power Center, a
        service person can then turn on or off the heater, however the RS must be in Service or Time
        Out Mode.<br>
    • On Dual Equipment models: a) Displays water temperature when the circulation pump is on
        without the need to activate the heater. b) Displays AquaRite status when pump is
        circulating without the need to activate the heater. c) Allows solar to function when
        sharing one heater.<br>
    • When equipment is on, the OneTouch will switch to the EQUIPMENT ON screen every 10 seconds
        rather than every 15 seconds. This screen will be displayed for 5 seconds.<br>
    • A heater enable message has been added to the EQUIPMENT ON screen. If either the pool or spa
        heater has been enabled its status will be indicated in the EQUIPMENT ON screen.<br>
    • Since the Jandy Illuminate project has been shelved for now, the term COLOR WHEEL and its
        function has been removed from the System Setup Menu.<br>
    • With the OneTouch control panel the characters to the right of the filter pump will flash when
        the system is in cool down mode. If the system needs more than one screen to display all the
        auxiliaries (AqL RS 12 or larger), the first auxiliary on each of the subsequent screens
        would also flash when the filter pump circuit is in cool down mode. Revision JJ corrects
        this problem.<br>
    • Corrects problem, which caused the last auxiliary to be hidden from auxiliary freeze
        assignment when dip switch 1 (Cleaner) was turned on.
  </td>
  <td valign="top">
    Size of the PPD, and thus its socket, increases from a 44 pin on previous PCBs to a 52 pin on
        boards with Assembly # 8125. The software written to the larger chip starts with Revision J.
  </td>
</tr>
<tr>
  <td valign="top">Rev. JJ</td>
  <td valign="top">1/1/2002</td>
  <td valign="top">
    • Corrects a problem where freeze protected items assigned from the OneTouch control panel would
        be lost when the system is switch from Auto to Service then Time Out and back to Auto. This
        is the same change as Revision II except the software information is programmed to the
        larger PPD chip.<br>
    • Corrects the screen jumping when assigning SpaLink RS buttons.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. K</td>
  <td valign="top">8/15/2003</td>
  <td valign="top">
    • Freeze Protection wording changed to a separate line below AIR TEMP. Freeze Protection wording
        on JJ replace AIR TEMP causing confusion for the end user.<br>
    • Solar operates when the system is set to display Celsius.<br>
    • When equipment is turned on manually, that piece of equipment will come back on automatically
        when the system is switched from AUTO to SERVICE to TEMP OUT and back to AUTO; Look Back
        feature.<br>
    • FLOWLINK - Displays percentage of cleanliness of the filter in 25% increments. If the filter
        pressure drops below 2 PSI or rises above 40 PSI this indicates a major problem which will
        shut down the filter pump and flash the display back light.<br>
    • DAY LIGHT SAVINGS - System automatically switches to daylight savings. Feature can be
        disabled.<br>
    • PUMP LOCKOUT - Pool/Spa Combos can be set to keep pump on during valve rotation.<br>
    • Spa water temperature can be calibrated on dual equipment systems.<br>
    • Corrects problem which caused all setup values (programming, labels, assigning JVAs etc.) to
        be lost when a second (or multiple) ON/OFF times is added prior to a power outage.<br>
    • During programming (setting ON/OFF times) if the current time falls within the set time that
        piece of equipment will activate when programming is complete. It is no longer necessary to
        switch from AUTO to SERVICE to TIME OUT and back to AUTO to activate the equipment.<br>
    • When connected to a AquaRite system the Filter Pump will continue to run at the end of Super
        Chlorination time if a programmed ON/OFF time is calling for operation.<br>
    • When a Dual Equipment RS system is connected to a AquaRite system the percentage of salt will
        be displayed when the Pool or Spa Pump is on.<br>
    • ASSIGN JVAs - Multiple JVAs can be assigned to one auxiliary.<br>
    • The EXTRA AUX can now be assigned to Freeze Protection.<br>
    • When AquaRite is connected to the RS 485 line, SuperChlorinate can be selected to operate in
        pool or SPILLOVER (dip switch 3 ON) mode.<br>
    • Corrects problem where HEATER ERROR was displayed at OneTouch Panel when heater was off line.<br>
    • MAINTAIN TEMP - Located in the TEMP SET area the system can be setup to have the temperature
        drive filter pump operation beyond the program time. Allows customer to set temperature for
        the pool or spa, and the system will maintain that temperature by turning on the filter
        pump, checking temperature and activating the heater if necessary. Heater must be enabled
        for this feature to operate.<br>
    • HEAT PUMP label. If a 2.2 K Ohms resistor is installed at the solar sensor terminals, the
        terminology SOLAR PUMP ON/OFF will change to HEAT PUMP ON/OFF after 24 hours.<br>
    • ALL OFF. Items turned on manually can all be turned off by activating ALL OFF. A time can be
        set for this to occur everyday.<br>
    • DUSK - Items) can be set to turn ON automatically at dusk.<br>
    • Additional chlorine generator communication. Will directly communicate with AutoPilot,
        AutoClear as well as AquaRite.<br>
    • Default date - 01/31/03<br>
    • When system is in freeze mode all on/off equipment status is remembered.<br>
    • Water temperature is used to determine whether to run the chlorine generator.<br>
    • In freeze mode chlorine generator output switches to 0%.<br>
    • Firmware part number and revision letter appear on startup or if RESET button is held down for
        4 seconds.<br>
    • When connected to a LX heater via the 4 conductor wire, the display now reads HEATER OFF LINE
        rather than HEATER ERROR.
  </td>
  <td valign="top">
    Requires PCB # 8194<br>
    FLOWLINK: Requires a Jandy Filter and pressure sensor. Hardware available in second quarter of
        2004.
  </td>
</tr>
<tr>
  <td valign="top">Rev. L</td>
  <td valign="top">10/1/2004</td>
  <td valign="top">
    • Jandy PC Docking Station allows a pool professional to download AquaLink RS configurations and
        programs to a new or existing AquaLink RS system from a PC and control via WebEx, PC
        Anywhere, etc..<br>
    • The new Flowlink feature monitors the pool system's water pressure, reports cleanliness of the
        filtration system and when service is needed at the indoor control panel. This feature
        requires the additional FlowLink hardware kit.<br>
    • Heat Pump Chiller Control allows the user to enter a chill set point from the AquaLink RS
        indoor control panel and when the pool water reaches that set point the system will
        automatically activate the chiller. This feature requires an Air Energy Heat Pump/Chiller.<br>
    • Laminar Pulse Control will pulse the Jandy Laminar Jet and create a burst of light that
        travels through the stream of water and into the body of water. From the AquaLink RS indoor
        control panel choose from eight different effects.<br>
    • JandyColors Smart Light Control allows the user to choose one of several Jandy Light colors or
        cycle through all colors continuously from the AquaLink RS indoor control panel.<br>
    • Run Time feature gives the user the ability to assign a runtime for any device or a OneTouch
        macro button.<br>
    • Chlorine Generator Interfaces: In revision L, the Jandy AquaPure and ClorMatic brands have
        exclusive interfaces with the AquaLink RS. Previously, AquaRite (Goldline) and Autoclear
        units had interfaced with the AquaLink RS, however, revision L or later will no longer
        support these brands. When these manufacturer's chlorine generator are connected to the
        AquaLink RS 485 network, the AquaLink RS Indoor Control Panel will display, "NOT COMPATIBLE"
        IN Menu/Help/Diagnostics.
  </td>
  <td valign="top">
    Flowlink: FlowLink hardware not available at this time 4/15/08
  </td>
</tr>
<tr>
  <td valign="top">Rev. M</td>
  <td valign="top">2/20/2006</td>
  <td valign="top">
    • Allows separate AquaPure set points for pool and spa. No longer automatically changes chlorine
        output to 10% for pool setting.<br>
    • Changes to the chlorine output percentage is now in 5% increments (was 1%)<br>
    • Chlorine output of the AquaPure can be set for both the pool and spa from the equipment pad.
        This feature requires the AquaPure to be the latest revision 11230A05 also.<br>
    • Support for AquaPalm wireless handheld.<br>
    • If there is a power outage when the system is in SERVICE or TIME MODES, the system will revert
        back to AUTO when power is restored.<br>
    • PRODUCT INFO - When entering numbers, the system did not allow you to go backwards once you
        get to "9", only forward.<br>
    • Jandy Color Lights can now be assigned to Dusk and OneTouch Buttons.<br>
    • Assign OneTouch buttons to SpaLink.<br>
    • Assign OneTouch buttons to AquaPalm HOT KEYs.<br>
    • If AquaRite chlorine generator is connected to the PCB permanent damage of PCB will result on
        power up.
  </td>
  <td valign="top">
    Equipment pad chlorine output: Requires the AquaPure firmware to be 11230A05 of newer.
  </td>
</tr>
<tr>
  <td valign="top">Rev. MM</td>
  <td valign="top">3/21/2006</td>
  <td valign="top">
    • Allows access to Lights sub-menu with All Button Control Panel.<br>
    • Default and maximum BOOST are now set to 24 hrs.<br>
    • Corrects problem where Program Time of Spa overrides Run Time.<br>
    • TeleLink now voices custom label as the original name of the auxiliary, e.g. custom label for
        AUX 3 will be heard as auxiliary three on/off.<br>
    • New daylight savings schedule for 2007 has been implemented.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. MMM</td>
  <td valign="top">7/26/2006</td>
  <td valign="top">
    • AquaPalm: From an AquaPalm handheld, you can adjust the Light Dimming % of any Auxiliary when
        a Light Dimming Relay is added to the AquaLink RS system.<br>
    • AquaPalm: Eliminates the skipping of lines on the handheld when the UP or DOWN arrow is
        pressed.<br>
    • AquaPalm: Increase speed for quicker response. Handheld &amp; J-Box must also be 3.0 or newer.<br>
    • 12V JandyColor Lights: Adjusted the synchronization of 12V Pool Lights with 12V Spa Lights.<br>
    • Dual Equipment and Pool Only RS Systems with an AquaPure/PureLink: Rev MMM firmware now allows
        adjustment of the AquaPure or PureLink chlorine generator output percentage (%) from the
        Control Box or Power Center at the equipment. When the AquaLink RS Power Center is placed
        into SERVICE MODE, the user can adjust the % output from the equipment.
  </td>
  <td valign="top">
    Last Rev to use PCB # 8194<br>
    All systems <i>(applies to the AquaPalm line-skipping and speed changes, the 12V JandyColor
        sync, and the AquaPure/PureLink output adjustment)</i>
  </td>
</tr>
<tr><td colspan="4"><b>PCB 50-Pin CPU #E02606</b></td></tr>
<tr>
  <td valign="top">Rev. N</td>
  <td valign="top">9/24/2007</td>
  <td valign="top">
    • ESP Link - Season Adjust Energy Saving Program: Eliminates the need to manually change
        programs when outdoor temperatures typically change. The OneTouch system will automatically
        adjust % output settings and program times for Filtration, Water Purification, Lighting,
        Sprinklers, Water Leveling, and Cleaner based on the requirements for that month.<br>
    • Heat Pump / Chiller auto change from indoor remote.<br>
    • Sprinkler Control: The new Jandy Sprinkler Module controls up to 8 sprinkler valves from the
        AquaLink RS OneTouch control panel, eliminating the need to learn or step outside to review
        or change sprinklers settings.<br>
    • RS InterLink: Jandy's new RS InterLink module allows control of more than 3 bodies of water
        from a single OneTouch control panel. If interfacing with Home Automation must use 2 Serial
        Adapters and connect from individual Power Centers, not from the OneTouch remote.<br>
    • ColorLogic Interface: Smart interface with the Hayward ColorLogic LED Light, so consumers can
        select color schemes from the OneTouch indoor remote or AquaPalm wireless remote.<br>
    • Password Protect: The entire system, remotes, or devices can be locked out and only accessed
        with a password, perfect for rental properties.<br>
    • LXi and New Heat Pump Interface: RS485 interface with Jandy's new LXi heater and Heat Pump for
        operation convenience.
  </td>
  <td valign="top">
    ESP Link: Requires PCB # R0466700<br>
    Heat Pump / Chiller auto change: OneTouch Only<br>
    Sprinkler Control: OneTouch Only<br>
    RS InterLink: OneTouch Only<br>
    ColorLogic Interface: OneTouch and All Button<br>
    Password Protect: OneTouch Only<br>
    LXi and New Heat Pump Interface: OneTouch and All Button
  </td>
</tr>
<tr>
  <td valign="top">Rev. O.1 <i>(Released to Anthony &amp; Sylvan, Mission Pools only)</i></td>
  <td valign="top">1/15/2008</td>
  <td valign="top">
    • Variable Speed Pump Control (OneTouch only) Via the RS485 connection automate the Pentair
        Variable Speed, Pentair Variable Flow, Pump's eliminating the need for any extra hardware.
        Filtration pump set up only. No Aux Pump labels. Freeze speed NOT adjustable.<br>
    • Jandy WaterColors LED Light Smart Interface (OneTouch only) Select one of 9 vibrant colors or
        5 dazzling color schemes directly from the OneTouch or AquaPalm.
  </td>
  <td valign="top">
    Variable Speed Pump Control: Requires PCB # R0466700; OneTouch only<br>
    WaterColors LED Light Smart Interface: OneTouch and All Button
  </td>
</tr>
<tr>
  <td valign="top">Rev. O.2</td>
  <td valign="top">4/21/2008</td>
  <td valign="top">
    • Variable Speed Pump Control (OneTouch only) Via the RS485 connection automate the Pentair
        Variable Speed, Pentair Variable Flow Pump's eliminating the need for any extra hardware.
        Filtration and Aux.Pump set up, including Aux. Pump labels. Freeze speed now adjustable.<br>
    • Jandy WaterColors LED Light Smart Interface (OneTouch only) Select one of 9 vibrant colors or
        5 dazzling color schemes directly from the OneTouch or AquaPalm.
  </td>
  <td valign="top">
    Variable Speed Pump Control: Requires PCB # R0466700; OneTouch only<br>
    WaterColors LED Light Smart Interface: OneTouch and All Button
  </td>
</tr>
<tr>
  <td valign="top">Rev. P</td>
  <td valign="top">2/12/2009</td>
  <td valign="top">
    • Jandy ePump Control. Via the RS485, red 4-pin connection, automate Jandy ePump Variable Speed
        Pumps.<br>
    • Jandy ChemLink Chemical Automation Controller. Via the RS485 red 4-pin, connection and a C1900
        ChemLink automate any Salt Water Chlorinator or liquid chemical feeders.<br>
    • VSP: Add "restore factory settings" to VSP menu.<br>
    • VSP: Control multiple, various brand VSP's at the same time from a single OneTouch or PDA.<br>
    • VSP: Adjust the VSP speed from the Equip on/off menu when Freeze Protect is active.<br>
    • VSP: Minimum RPM level changed from 400 to 600 RPM.<br>
    • VSP: Display of "Pump Offline" in the System Setup menu ONLY if a VSP is powered ON but not
        responding to commands.<br>
    • VSP: Priming Duration default changed from 1 to 3 minutes.<br>
    • VSP: Filtration Speed default changed from 1725 to 1750 RPM.<br>
    • VSP: * *Priming* * is displayed in Equipment Status screen.<br>
    • VSP: No longer display the RPM "read value " from pumps, rather show the "command value".<br>
    • VSP: If a speed is assigned to an AUX, turning on the AUX will turn on the Filtration Pump as
        well.<br>
    • VSP: Fixed a problem where and AC VSP was reporting that it was NOT CONNECTED.<br>
    • VSP: Manual activation of a speed assigned to an AUX will override a program speed.<br>
    • VSP: Show VSP in System Setup menu when a Heat Pump is connected to the RS485 network.<br>
    • VSP: Eliminate invalid WATT readings.<br>
    • VSP: With Dip S1-5 ON, ePump status byte is now displayed.<br>
    • VSP: Added the ePump firmware version number to the diagnostic screen.<br>
    • VSP: When exiting SERVICE MODE, the pump would skip the priming stage (if the pump is on in
        AUTO mode)<br>
    • Auxiliary PCB's now control light dimmers on any Aux.<br>
    • Added a second screen for display of additional Remotes in Diagnostics &amp; Equipment Status
        Screen; use the Up/Down arrow keys to advance to the next screen. Press Back to return to
        main.<br>
    • Sprinkler Control: Added EVEN or ODD watering days/<br>
    • Sprinkler Control: Added manual start times for individual sprinkler valves.<br>
    • Interface with Zodiac's LM3 and DuoClear systems, allows entering set points &amp; viewing
        ppm's<br>
    • Allow SWC BOOST mode to run in SPILLOVER mode<br>
    • Prevents gas heaters and heat pumps from cycling on and off under 5 minutes.<br>
    • Improvements to the internal clock eliminating "noise" from advancing the time.<br>
    • If a device is assigned to an AUX and contains the word "PUMP;, then prompt the user with a
        message reminding them to enable Freeze Protection.<br>
    • Added new daylight saving time changeover dates to the PDA.<br>
    • Added Toronto, Vancouver, and Mexico City to the Dusk feature.
  </td>
  <td valign="top">
    ePump Control: Rev O.2 will not properly control the Jandy ePump.<br>
    ChemLink: Requires ChemLink part # C1900<br>
    Gas heater / heat pump cycling fix: Issue appears on Rev N or later
  </td>
</tr>
<tr>
  <td valign="top">Rev. Q</td>
  <td valign="top">4/21/2010</td>
  <td valign="top">
    • AquaLink Touch Screen firmware support
  </td>
  <td valign="top">
    Touch Screen
  </td>
</tr>
<tr>
  <td valign="top">Rev. QQ</td>
  <td valign="top">9/10/2010</td>
  <td valign="top">
    • Variable Speed Pump speeds assigned for Pool/Spa Heater and Solar Heat: The pump will operate
        at the speed that is assigned to an auxiliary or manual operation. The exception to this
        rule will be when the pool/spa heater is firing or when the solar heat is ON. To protect the
        heater or solar panels we do not allow the user to select a lower speed. We will not allow a
        user to select a lower pump speed than that of the pool/spa heater or solar heat.
        Auxiliaries with lower speed settings than the Gas or Solar Heat Speed settings will not
        allow manual override. If you have several auxiliaries that have speeds assigned and if they
        are ON, and you then turn off one of the auxiliaries, the pump will switch to the highest
        speed that is assigned to any of the auxiliaries that are ON. SUMMARY, if the pool/spa
        heater or solar heat is ON, we do not allow the user to select a lower pump speed.<br>
    • Variable Speed Pump and Scheduled operations: The pump will operate at the speed programmed.
        However, if the heater is firing or the solar heat is ON, the pump will not operate lower
        than the heater or solar setting. <i>(text cut off in source PDF)</i><br>
    • Variable Speed Pump and adjusting speeds from the Other Devices or Equipment On/Off screen:
        From the OTHER DEVICES menu you can set the pump to whatever speed you want for that time of
        operation. The pump speeds are not saved so if you turn the pump OFF and then ON again. The
        pump will revert back to the default values in the Variable Speed Pump Setting menu.<br>
    • Variable Speed Pump and Freeze Protection: In Freeze Protect mode the pump will turn ON and
        operate at the freeze protect speed. If the Filter Pump speed is higher than the Freeze
        protect speed, then the pump will operate at the Filter Pump speed. Also if speeds have been
        assigned to auxiliaries and if those auxiliaries are freeze protected, the pump will operate
        at the highest speed.<br>
    • Variable Speed Pump speed settings activated with Auxiliaries: The speed assigned to the Aux
        that was pressed last takes precedence. For example, if the pump is at 3000RPM, and then
        AUX1 is pressed, the pump will switch to the speed that is assigned to AUX1 regardless of
        whether the speed is higher or lower than the speed that the pump is currently running at.<br>
    • Variable Speed Pump speed settings activated with Auxiliaries: If you have several auxiliaries
        ON and all of them have a speed assigned to them, the pump will switch to the highest speed.<br>
    • Variable Speed Pump and Scheduled Pump Speeds: The pump will go to the speed that is being
        turned ON by the program regardless of what the speed currently operating. If a program time
        ends and there are other programs overlapping, the pump will operate at the highest
        programmed speed.<br>
    • Variable Speed Pump and Scheduled Auxiliaries with Pump Speeds: The pump will go to the speed
        that is assigned to the aux that is being turned ON, regardless of what speed currently
        operating. If several programs are overlapping for auxiliaries that have speeds assigned to
        them, the pump will operate at the speed of the aux that was last turned ON.<br>
    • Variable Speed Pumps In freeze protect mode, it is possible to switch the pump to a higher
        speed. The Equipment Status screen now displays the speed active.<br>
    • Variable Speed Pumps issue corrected if a speed was assigned to Auxiliaries B1-B8, Auxiliaries
        C1-C8, Auxiliaries D1-D8, and extra Auxiliary, occasionally the pump would not go to the
        correct speed if one of those auxiliaries were pressed.<br>
    • Variable Speed Pump with competing Heater and Cleaner Speeds: Fixed a VSP problem where if the
        Filter Pump and Cleaner were ON and the Heater was ENABLED, the pump speed would drop to the
        Heater speed and shut OFF the Cleaner because there was not enough water flow. Rev QQ now
        leaves the Pump at the highest speed (cleaner in this case).<br>
    • Variable Speed Pump with Heater and Solar Speeds: Fixed a problem where the Solar Heat and
        Pool Heat Speed messages where being displayed anytime an Aux. was pressed, even if it did
        not have a speed assigned to it. These are the messages that inform the user that the pump
        speed was not changed in order to protect the heater or solar panels.<br>
    • Variable Speed Pump and Aux.: Messages On DUAL EQUIPMENT systems, it was possible in spa cool
        down for the VSP to turn OFF, even if it still had power applied to the pump.<br>
    • Variable Speed Pump Speed Adjust: The Pump must be ON in order to adjust the speed.<br>
    • Customize Home Screen: Removed the occasion where an icon on the home screen of "AUX J74"
        would display.<br>
    • Schedule Screen: Corrected an issue if more than 12 schedules were added, the 3rd page of the
        list box would not display properly.<br>
    • Schedule Screen: When adding or modifying schedules, the item schedule you are working on is
        now listed first on the schedule and at the top of the screen eliminating the need to page
        down or page up to continue.<br>
    • Schedule Screen: Removed the occasion when two or more of the DAY buttons are highlighted, to
        where only one Day at a time should be highlighted.<br>
    • Schedule Screen: Removed the occasion that the unit was displaying a blank screen at the list
        box with a blank line highlighted if there were 6 or more scheduled times setup on a system.<br>
    • Home Screen: Removed the occasion that "Auxiliary 11" would display on the Home Screen.<br>
    • Other Devices: from Home Screen Eliminates the display automatically reverting back to the
        Home screen when the Heater Button is pressed.<br>
    • Lockouts: Device lockouts now work for Aux. C1-C8 and D1-D8.
  </td>
  <td valign="top">
    All Variable Speed Pump changes: OneTouch and Touch Screen<br>
    Customize Home Screen, Schedule Screen, Home Screen, Other Devices and Lockouts changes: Touch
        Screen
  </td>
</tr>
<tr>
  <td valign="top">Rev. R</td>
  <td valign="top">2011</td>
  <td valign="top">
    • Add support for iAquaLink<br>
    • Add support for Smart JVA<br>
    • Add support for Cover Pool control<br>
    • Corrects problem where Heat Pump/ Chiller does not show properly on Touch Sreen
  </td>
  <td valign="top">
    Smart JVA: OneTouch and Touch Screen<br>
    Cover Pool control: OneTouch and Touch Screen<br>
    Heat Pump / Chiller fix: Touch Screen
  </td>
</tr>
<tr>
  <td valign="top">Rev. T.2</td>
  <td valign="top">2013</td>
  <td valign="top">
    • Lots of firmware fixes, currently the most stable revision of both hardware and firmware<br>
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. W</td>
  <td valign="top">2022</td>
  <td valign="top">
    • Support up to 16 variable-speed pumps that utilize a preassigned PUMP ADDRESS (RS485 address).<br>
    • Added custom naming for pumps.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. Y</td>
  <td valign="top">2022</td>
  <td valign="top">
    • Jandy Infinite WaterColors LED Light Controller support (RS485). <br>
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">Rev. Yg</td>
  <td valign="top">2024</td>
  <td valign="top">
    • 24 virtual auxiliary assignments (virtual)<br>
  </td>
  <td valign="top">
  </td>
</tr>
<tr><td colspan="4"><b>ARM 32 processor</b> - This can be loaded with firmware back to T.2.<br> Currently has issues reading water temperature that seem to he hardware related</td></tr>
<tr>
  <td valign="top">Rev. Yg</td>
  <td valign="top">-</td>
  <td valign="top">Same at Rev Yg, different board.</td>
  <td valign="top"></td>
</tr>
<tr>
  <td valign="top">Rev. T.2g</td>
  <td valign="top">-</td>
  <td valign="top">Same at Rev T.2, different board.</td>
  <td valign="top"></td>
</tr>
</tbody>
</table>

## PDA software, J-Box and Handheld revisions

<table>
<thead><tr><th>Software Revision</th><th>Effective Date</th><th>System Changes</th><th>Comments</th></tr></thead>
<tbody>
<tr><td colspan="4"><b>PDA — PCB 52-Pin PPD Socketted</b></td></tr>
<tr>
  <td valign="top">1.2</td>
  <td valign="top">May '05</td>
  <td valign="top">
    • First production PPD
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">2.6.1</td>
  <td valign="top">April '06</td>
  <td valign="top">
    • Separate pool and spa chorine production output settings.<br>
    • Adjustment of chlorine production output from equipment pad<br>
    • Interface with Color Logic lights
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">3.0</td>
  <td valign="top">July '07</td>
  <td valign="top">
    • Increase speed for quicker response. Handheld &amp; J-Box must also be Rev 3.0 or newer.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">4.0 <i>(Anthony/Sylvan &amp; Mission Pools)</i></td>
  <td valign="top">Jan. '08</td>
  <td valign="top">
    • Variable Speed Pump Control (Pentair VP &amp; VF) Limited to Filtration Pump, no adj. Freeze
        speed.<br>
    • Add control of Jandy WaterColors (LED) Lights.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">4.1</td>
  <td valign="top">April '08</td>
  <td valign="top">
    • Variable Speed Pump Control (Pentair VS/VF, Hayward AC)<br>
    • Control of Jandy LXi Heater via RS485 Network (red 4 pin)<br>
    • Control of Jandy WaterColors (LED) Lights.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">5.0</td>
  <td valign="top">January '09</td>
  <td valign="top">
    • Jandy ePump control . Via the RS485, red 4-pin connection, automate Jandy ePump Variable Speed
        Pumps.<br>
    • Jandy ChemLink Chemical Automation Controller. Via the RS485 red 4-pin, connection and a C1900
        ChemLink automate any Salt Water Chlorinator or liquid chemical feeders.<br>
    • VSP: Add "restore factory settings" to VSP menu<br>
    • VSP: Control multiple, various brand VSP's at the same time from a single PDA.<br>
    • VSP: Adjust the VSP speed from the Equip on/off menu when Freeze Protect is active.<br>
    • VSP: Minimum RPM level changed from 400 to 600 RPM.<br>
    • VSP: Display of "Pump Offline" in the System Setup menu ONLY if a VSP is powered ON but not
        responding to commands.<br>
    • VSP: Priming Duration default changed from 1 to 3 minutes.<br>
    • VSP: Filtration Speed default changed from 1725 to 1750 RPM.<br>
    • VSP: * * *Priming* * * is displayed in the Equipment Status screen.<br>
    • VSP: No longer display the RPM "read value " from pumps, rather show the "command value".<br>
    • VSP: If a speed is assigned to an AUX, turning on the AUX will turn on the Filtration Pump as
        well.<br>
    • VSP: Fixed a problem where and AC VSP was reporting that it was NOT CONNECTED.<br>
    • VSP: Manual activation of a speed assigned to an AUX will override a program speed.<br>
    • VSP: Eliminate invalid WATT readings.<br>
    • VSP: With Dip S1-5 ON, ePump status byte is now displayed.<br>
    • VSP: Added the ePump firmware version number to the diagnostic screen.<br>
    • VSP: When exiting SERVICE MODE, the pump would skip the priming stage (if the pump is on in
        AUTO mode)<br>
    • Added a second screen for display of additional Remotes in Diagnostics and Equipment Status
        Screen; Use the Up/Down arrow keys to advance to the next screen. Press Back to return to
        main screen.<br>
    • Interface with Zodiac's LM3 and DuoClear systems to allow entering set points and viewing ppm
        from OneTouch or PDA remotes.<br>
    • Allow SWC BOOST mode to run in SPILLOVER mode<br>
    • Changed AquaPure house max value from 96 to 24 hrs.<br>
    • Prevents gas heaters and heat pumps from cycling on and off under 5 minutes.<br>
    • If a device is assigned to an AUX and contains the word "PUMP;, then prompt the user with a
        message reminding them to enable Freeze Protection.<br>
    • Added new daylight saving time changeover dates to the PDA.<br>
    • <i>(PCB 50-Pin CPU #E02606)</i> Add communication with iAquaLink
  </td>
  <td valign="top">
    ePump control: PDA Rev 4.1 will not properly control the Jandy ePump.<br>
    ChemLink: Requires ChemLink part # C1900
  </td>
</tr>
<tr><td colspan="4"><b>J-Box</b></td></tr>
<tr>
  <td valign="top">1.26</td>
  <td valign="top">April '05</td>
  <td valign="top">
    • First production J-Box unit released
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">1.27</td>
  <td valign="top">May '05</td>
  <td valign="top">
    • Back button will exit unit out of waiting
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">1.28</td>
  <td valign="top">July '05</td>
  <td valign="top">
    • Added Cancel Delay feature to J-Box
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">1.29</td>
  <td valign="top">Dec. '05</td>
  <td valign="top">
    • Added temperature/frequency compensation so J-Box will continue to operate when ambient
        temperature is over 120 ºF.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">3.0</td>
  <td valign="top">July '07</td>
  <td valign="top">
    • Increase speed for quicker response. Handheld &amp; PPD must also be 3.0 or newer.
  </td>
  <td valign="top">
  </td>
</tr>
<tr><td colspan="4"><b>Handheld</b></td></tr>
<tr>
  <td valign="top">1.33</td>
  <td valign="top">April '05</td>
  <td valign="top">
    • First production J-Box unit released
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">1.34</td>
  <td valign="top">May '05</td>
  <td valign="top">
    • Change battery icon to blinking when low
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">1.35</td>
  <td valign="top">July '05</td>
  <td valign="top">
    • Added Cancel Delay feature to Handheld (hold down DOWN &amp; BACK buttons together). When
        batteries are installed, the Handheld unit will not automatically power up.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">1.36</td>
  <td valign="top">Nov. '05</td>
  <td valign="top">
    • Removed vertical and horizontal bars from screen.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">1.37</td>
  <td valign="top">Dec. '05</td>
  <td valign="top">
    • Changed text on screen from PDA to Handheld.<br>
    • Corrected problem of Handheld being stuck on waiting when power is turned off then on within
        10 seconds.
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">1.38</td>
  <td valign="top">Aug. '06</td>
  <td valign="top">
    • Firmware modified for new RF chip (old RF chip supported)
  </td>
  <td valign="top">
  </td>
</tr>
<tr>
  <td valign="top">3</td>
  <td valign="top">July '07</td>
  <td valign="top">
    • Increase speed for quicker response. J-Box &amp; PPD must also be 3.0 or newer.
  </td>
  <td valign="top">
  </td>
</tr>
</tbody>
</table>