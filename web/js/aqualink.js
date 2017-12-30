
if(window.innerHeight > window.innerWidth){
  orientation("portrait");
} else {
  orientation("landscape");
}

// Listen for orientation changes
window.addEventListener("orientationchange", function() {
  // Announce the new orientation number
  //alert(window.orientation);
  if(window.innerHeight > window.innerWidth)
    orientation("portrait");
  else
    orientation("landscape");
  
}, false);

function orientation(or){
  //alert(or);
  viewport = document.querySelector("meta[name=viewport]");
  if (or == "landscape") {
    viewport.setAttribute('content', 'content="width=device-width, user-scalable=yes, width=800"');
  } else {
    viewport.setAttribute('content', 'content="width=device-width, user-scalable=yes, width=540"');
  }
}

// aqualink.js

function BlockMove(event) {
  // Tell Safari not to move the window.
  event.preventDefault();
}

// Preload the images.
var delay = 10;
if (document.images) {
  //setTimeout(get_batt_ok_img, delay);
  setTimeout(loadImages, delay);
}

function loadImages() {
  document.getElementById('battery_ok').src = "images/battery_ok.png";
  document.getElementById('battery_low').src = "images/battery_low.png";
  document.getElementById('battery_blank').src = "images/battery_blank.png";
  
  for (var obj in aqualink_data.leds) {
    setButtonElementImage(obj+'_on_status', "images/led_on.png");
    setButtonElementImage(obj+'_off_status', "images/led_off.png");
    setButtonElementImage(obj+'_en_status', "images/led_enabled.png");
  }
  
  document.getElementById('net_off').src = "images/net_off.png";
  document.getElementById('net_yellow').src = "images/net_yellow.png";
  document.getElementById('net_green').src = "images/net_green.png";
  document.getElementById('net_red').src = "images/net_red.png";
}


//function set_button_led_images
function setButtonElementImage(element, image){
  el = document.getElementById(element);
  if (el != null) {
    el.src = image;
  }
}

function LedsData() {
  this.Filter_Pump = "off";
  this.Spa_Mode = "off";
  this.Aux_1 = "off";
  //this.aux1 = "flash";
  this.Aux_2 = "off";
  this.Aux_3 = "off";
  this.Aux_4 = "off";
  this.Aux_5 = "off";
  this.Aux_6 = "off";
  this.Aux_7 = "off";
  this.Pool_Heater = "off";
  this.Spa_Heater = "off";
  this.Solar_Heater = "off";
}

function AqualinkData() {
  this.version = "";
  this.date = "09/11/12 TUE";
  this.time = "10:33 AM";
  this.temp_units = "F";
  this.air_temp = "75";
  this.battery = "low";
  this.leds = new LedsData();
}

function TemperatureSetPoint() {
  this.modified = false;
  this.value = 0;
}

var pool_htr_set_point = new TemperatureSetPoint();
var spa_htr_set_point = new TemperatureSetPoint();
var freeze_protect_set_point = new TemperatureSetPoint();

var aqualink_data = new AqualinkData();
var net_connection = 'red';

    
function interval(duration, fn){
  this.baseline = undefined
  
  this.run = function(){
    if(this.baseline === undefined){
      this.baseline = new Date().getTime()
    }
    fn()
    var end = new Date().getTime()
    this.baseline += duration
 
    var nextTick = duration - (end - this.baseline)
    if(nextTick<0){
      nextTick = 0
    }
    (function(i){
        i.timer = setTimeout(function(){
        i.run(end)
      }, nextTick)
    }(this))
  }

  this.stop = function(){
   clearTimeout(this.timer)
 }
}

var timer = new interval(1000, function(){
      net_connection_icon();
      flash_battery_icon();
      //blink_filter_pump();
      blink_button_leds();
})
timer.run()


function net_connection_icon() {
  if (net_connection == "red") {
    document.getElementById('messages_div').innerHTML = "Not connected!";
    document.getElementById('net_green').style.visibility = 'hidden';
    document.getElementById('net_yellow').style.visibility = 'hidden';
    if (document.getElementById('net_red').style.visibility == 'visible') {
      document.getElementById('net_red').style.visibility = 'hidden';
    } else {
      document.getElementById('net_red').style.visibility = 'visible';
    }
  } else if (net_connection == "green") {
    document.getElementById('net_green').style.visibility = 'visible';
    document.getElementById('net_red').style.visibility = 'hidden';
    document.getElementById('net_yellow').style.visibility = 'hidden';
  } else if (net_connection == "yellow") {
    document.getElementById('net_green').style.visibility = 'hidden';
    document.getElementById('net_red').style.visibility = 'hidden';
    document.getElementById('net_yellow').style.visibility = 'visible';
  }
}
//setInterval(net_connection_icon, 500);

function net_activity() {
  // Set the icon yellow.
  document.getElementById('net_green').style.visibility = 'hidden';
  document.getElementById('net_red').style.visibility = 'hidden';
  document.getElementById('net_yellow').style.visibility = 'visible';
  // Wait 10mS then set to green.
  setTimeout(function() {
    document.getElementById('net_green').style.visibility = 'visible';
    document.getElementById('net_red').style.visibility = 'hidden';
    document.getElementById('net_yellow').style.visibility = 'hidden';
  }, 50);
}

function flash_battery_icon() {
  if (aqualink_data.battery == "low") {
    if (document.getElementById('battery_low').style.visibility == 'visible') {
      document.getElementById('battery_low').style.visibility = 'hidden';
    } else {
      document.getElementById('battery_low').style.visibility = 'visible';
    }
  }
}
//setInterval(flash_battery_icon, 500);
/*
function blink_filter_pump() {
  if (aqualink_data.leds.pump == "enabled") {
    document.getElementById("pump_status").src = (document.getElementById("pump_status").src.indexOf("images/led_on.png") == -1) ? "images/led_on.png" : "images/led_off.png";
  }
}
*/
//setInterval(blink_filter_pump, 500);


function blink_button_leds() {
  for (var obj in aqualink_data.leds) {
    if (aqualink_data.leds[obj] == "flash" && document.getElementById(obj+'_on_status') != null ) {
      if (document.getElementById(obj+'_on_status').style.visibility == 'hidden') {
        setElementVisibility(obj+'_on_status', 'visible');
        setElementVisibility(obj+'_off_status', 'hidden');
      } else {
        setElementVisibility(obj+'_on_status', 'hidden');
        setElementVisibility(obj+'_off_status', 'visible');
      }
    }
  }
}
//setInterval(blink_aux_leds, 1500);

// End - Methods to Manage Flashing Images

// Display Update Methods

function update_temp(id) {
  var el = document.getElementById(id);
  var temp_string = "";

  if (id == "air_temp") {
    temp_string = aqualink_data.air_temp;
  } else if (id == "pool_temp") {
    temp_string = aqualink_data.pool_temp;
    if (temp_string == " ") {
      temp_string = "---"
    }
  } else if (id == "spa_temp") {
    temp_string = aqualink_data.spa_temp;
    if (temp_string == " ") {
      temp_string = "---"
    }
  } else if (id == "pool_htr_set_pnt") {
    if (pool_htr_set_point.modified == true) {
      temp_string = pool_htr_set_point.value.toString();
      el.style.color = "PowderBlue";
    } else {
      temp_string = aqualink_data.pool_htr_set_pnt;
      pool_htr_set_point.value = parseInt(temp_string);
      el.style.color = "#f4f4f4";
    }
  } else if (id == "spa_htr_set_pnt") {
    if (spa_htr_set_point.modified == true) {
      temp_string = spa_htr_set_point.value.toString();
      el.style.color = "PowderBlue";
    } else {
      temp_string = aqualink_data.spa_htr_set_pnt;
      spa_htr_set_point.value = parseInt(temp_string);
      el.style.color = "#f4f4f4";
    }
  } else if (id == "frz_protect_set_pnt") {
    if (freeze_protect_set_point.modified == true) {
      temp_string = freeze_protect_set_point.value.toString();
      el.style.color = "PowderBlue";
    } else {
      temp_string = aqualink_data.frz_protect_set_pnt;
      freeze_protect_set_point.value = parseInt(temp_string);
      el.style.color = "#f4f4f4";
    }
  }

  el.innerHTML = temp_string + "&deg;" + aqualink_data.temp_units;
}

// Adjust the Pool Heater set point. Note that its maximum value
// is 104 and its minimum value is 0.
function incr_pool_htr(direction) {
  if (direction == '+') {
    if (pool_htr_set_point.value < 104) {
      pool_htr_set_point.value++;
    }
  } else {
    if (pool_htr_set_point.value > 0) {
      pool_htr_set_point.value--;
    }
  }

  if (pool_htr_set_point.value == parseInt(aqualink_data.pool_htr_set_pnt)) {
    pool_htr_set_point.modified = false;
  } else {
    pool_htr_set_point.modified = true;
  }

  update_temp("pool_htr_set_pnt");
}

// Adjust the Spa Heater set point. Note that its maximum value
// is 104 and its minimum value is 0.
function incr_spa_htr(direction) {
  if (direction == '+') {
    if (spa_htr_set_point.value < 104) {
      spa_htr_set_point.value++;
    }
  } else {
    if (spa_htr_set_point.value > 0) {
      spa_htr_set_point.value--;
    }
  }

  if (spa_htr_set_point.value == parseInt(aqualink_data.spa_htr_set_pnt)) {
    spa_htr_set_point.modified = false;
  } else {
    spa_htr_set_point.modified = true;
  }

  update_temp("spa_htr_set_pnt");
}

// Adjust the Freeze Protection set point. Note that its maximum value
// is 42 and its minimum value is 36.
function incr_frz_protect(direction) {
  if (direction == '+') {
    if (freeze_protect_set_point.value < 42) {
      freeze_protect_set_point.value++;
    }
  } else {
    if (freeze_protect_set_point.value > 36) {
      freeze_protect_set_point.value--;
    }
  }

  if (freeze_protect_set_point.value == parseInt(aqualink_data.frz_protect_set_pnt)) {
    freeze_protect_set_point.modified = false;
  } else {
    freeze_protect_set_point.modified = true;
  }

  update_temp("frz_protect_set_pnt");
}

function update_date_time() {
  var el = document.getElementById("date_time");

  el.innerHTML = aqualink_data.time + " - " + aqualink_data.date;
}

function setElementVisibility(element, visibility){
  //console.log("set "+element+" to "+visibility);
  el = document.getElementById(element);
  if (el != null) {
    el.style.visibility = visibility;
    //console.log("set "+element+" to "+visibility);
  }
}

function update_leds() {
//console.log("*****UPDATE LED*****");
  for (var obj in aqualink_data.leds) {
//console.log("*****setting "+obj+" to "+aqualink_data.leds[obj]);
    if (aqualink_data.leds[obj] == "on") {
      setElementVisibility(obj+'_on_status', 'visible');
      setElementVisibility(obj+'_off_status', 'hidden');
      setElementVisibility(obj+'_en_status', 'hidden');
    } else if (aqualink_data.leds[obj] == "enabled") {
      setElementVisibility(obj+'_on_status', 'hidden');
      setElementVisibility(obj+'_off_status', 'hidden');
      setElementVisibility(obj+'_en_status', 'visible');
    } else if (aqualink_data.leds[obj] == "off") {
      setElementVisibility(obj+'_on_status', 'hidden');
      setElementVisibility(obj+'_off_status', 'visible');
      setElementVisibility(obj+'_en_status', 'hidden');
    } else if (aqualink_data.leds[obj] == "flash") {
      // Don't set any other visibility or it'll mess up the flasher timer.
      setElementVisibility(obj+'_en_status', 'hidden');
    }
  }
}
  

function update_status(data) {
  aqualink_data = data;
  //console.log(aqualink_data.version);
  //console.log('updating status...');
  if (aqualink_data.battery == "ok") {
    document.getElementById('battery_low').style.visibility = 'hidden';
    document.getElementById('battery_ok').style.visibility = 'visible';
  } else {
    document.getElementById('battery_ok').style.visibility = 'hidden';
  }
  
  if (aqualink_data.status == "Ready") {
    document.getElementById('messages_div').innerHTML = "";
  } else {
    document.getElementById('messages_div').innerHTML = aqualink_data.status+" (please wait!)";
  }
  
  update_temp("air_temp");
  update_temp("pool_temp");
  update_temp("spa_temp");
  update_temp("pool_htr_set_pnt");
  update_temp("spa_htr_set_pnt");
  update_temp("frz_protect_set_pnt");

  update_date_time();

  update_leds();
}

function set_aux_button_labels(data) {
  //console.log("Aux 1=" + data.Aux_1);
  for (var obj in data) {
    //console.log("sent "+obj+" to "+data[obj]);
    button = document.getElementById(obj);
    if (button != null) {
      button.innerHTML = data[obj];
    }
  }
}

//alert(BrowserDetect.browser + ' : ' + BrowserDetect.version);

function get_appropriate_ws_url() {
  var pcol;
  var u = document.URL;

  /*
   * We open the websocket encrypted if this page came on an
   * https:// url itself, otherwise unencrypted
   */

  if (u.substring(0, 5) == "https") {
    pcol = "wss://";
    u = u.substr(8);
  } else {
    pcol = "ws://";
    if (u.substring(0, 4) == "http")
      u = u.substr(7);
  }

  u = u.split('/');

  //alert (pcol + u[0] + ":6500");

  return pcol + u[0];
}



/* dumb increment protocol */
var socket_di;

function startWebsockets() {
  socket_di = new WebSocket(get_appropriate_ws_url());
  /*
  if (BrowserDetect.browser == "Firefox" && BrowserDetect.version < 12) {
    //socket_di = new MozWebSocket(get_appropriate_ws_url(), "dumb-increment-protocol");
    socket_di = new MozWebSocket(get_appropriate_ws_url());
  } else {
    //socket_di = new WebSocket(get_appropriate_ws_url(), "dumb-increment-protocol");
    socket_di = new WebSocket(get_appropriate_ws_url());
  }
*/
  try {
    socket_di.onopen = function() {
      // success!
      get_aux_labels();
      net_connection = 'green';
    }

    socket_di.onmessage = function got_packet(msg) {
      net_activity();
      var data = JSON.parse(msg.data);
      if (data.type == 'status') {
        update_status(data);
      } else if (data.type == 'aux_labels') {
        set_aux_button_labels(data);
      }
    }

    socket_di.onclose = function() {
      // something went wrong
      net_connection = 'red';
      // Try to reconnect every 5 seconds.
      setTimeout(function() {
        startWebsockets()
      }, 5000);
    }
  } catch (exception) {
    alert('<p>Error' + exception);
  }
}

function reset() {
  socket_di.send("reset\n");
}

function get_aux_labels() {
  //socket_di.send("GET_AUX_LABELS");
  var msg = {command: "GET_AUX_LABELS"};
    // Send the msg object as a JSON-formatted string.
  socket_di.send(JSON.stringify(msg));
}

function queue_command(cmd) {
  var _cmd = {};
  _cmd.command = cmd;

  socket_di.send(JSON.stringify(_cmd));
}

function set_temperature(type) {
  var temperature = {};

  if (type == "POOL_HTR") {
    temperature.parameter = type;
    temperature.value = pool_htr_set_point.value;
    pool_htr_set_point.modified = false;
  } else if (type == "SPA_HTR") {
    temperature.parameter = type;
    temperature.value = spa_htr_set_point.value;
    spa_htr_set_point.modified = false;
  } else if (type == "FRZ_PROTECT") {
    temperature.parameter = type;
    temperature.value = freeze_protect_set_point.value;
    freeze_protect_set_point.modified = false;
  }

  socket_di.send(JSON.stringify(temperature));
}

function set_light_mode(value) {
  var mode = {};

  mode.parameter = 'POOL_LIGHT_MODE';
  mode.value = value;
  socket_di.send(JSON.stringify(mode));
}

window.onload = function() {
  startWebsockets();

}