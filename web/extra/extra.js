

/*
 // put in config.js
 if (navigator.userAgent.search('Android') != -1 || navigator.userAgent.search('Nook') != -1 ) {
   document.writeln("<script type='text/javascript' src='extra/extra.js'></script>");
 }
 */

GroupSwitchType = -99;
GroupLightType = -98;
GroupAudioType = -97;

setTimeout(function() {
  //extra_domoticz('get', 'type=devices&rid=12');  // Absolute radio poller
  //extra_domoticz('get', 'type=devices&rid=281'); // Audiotron Switch
  //extra_domoticz('get', 'type=devices&rid=279'); // Audiotron Selector
  //extra_domoticz('get', 'type=scenes', 8, GroupAudioType);
  extra_domoticz('get', 'type=devices&rid=332'); // Patio audio virtual switch
  extra_domoticz('get', 'type=devices&rid=147');
  extra_domoticz('get', 'type=devices&rid=148');
  extra_domoticz('get', 'type=devices&rid=391');
  extra_domoticz('get', 'type=devices&rid=392');
  //extra_domoticz('get', 'type=devices&rid=21'); // Patio String Lights
  //extra_domoticz('get', 'type=devices&rid=55'); // Patio String Lights #2
  //extra_domoticz('get', 'type=devices&rid=193');
  extra_domoticz('get', 'type=scenes', 2, GroupLightType);
  extra_domoticz('get', 'type=scenes', 6, GroupLightType);
}, 1000);

function switchTileState_extra(id, idx, type) {
  // console.log("Click "+id+" "+idx);
  state = (document.getElementById(id).getAttribute('status') == 'off') ? 'on' : 'off';
  var switchval;
  // console.log("Status "+state);

  if (type == 19) // Reverse on off for doors
    switchval = ((state == 'on') ? 'Off' : 'On');
  else
    switchval = ((state == 'on') ? 'On' : 'Off');

  if (type == GroupSwitchType || type == GroupLightType || type == GroupAudioType)
    extra_domoticz('set', 'type=command&param=switchscene&idx=' + idx + '&switchcmd=' + switchval);
  else if (type == 18) // selectorswitch
    //http://trident/json.htm?type=command&param=switchlight&idx=279&switchcmd=Set%20Level&level=10
    extra_domoticz('set', 'type=command&param=switchlight&idx=' + idx + '&switchcmd=Set%20Level&level=' + ((switchval=='On') ? 10 : 0) );
  else
    extra_domoticz('set', 'type=command&param=switchlight&idx=' + idx + '&switchcmd=' + switchval);

  setTileOn(id, state);
}

function getLevelName(levelNames, levelIndex) {
  var names = decode64(levelNames).split("|");
  return names[levelIndex / 10];
}

function extra_updateDevice(data) {
  //console.log(data);

  var tile
  // console.log(data.Name+" - hw "+data.HardwareTypeVal+" - sw "+data.SwitchTypeVal);
  if ((tile = document.getElementById(data.ID)) == null) {
    // console.log(data.Name+" - create");
    switch (data.SwitchTypeVal) {
    case 0:  // switch
    case GroupSwitchType: // Group switch
      if ( data.idx == "332" ) // If music switch change icon
        add_tile(data.ID, data.Name, data.Status, 'switch', 'switch', 'extra/switch_8-off.png', 'extra/switch_8-on.png');
      else
        add_tile(data.ID, data.Name, data.Status, 'switch', 'switch', 'extra/switch_0-off.png', 'extra/switch_0-on.png');
      break;
    case 21: // light
    case 7:  // dimmable light
    case GroupLightType: // Group switch
      add_tile(data.ID, data.Name, data.Status, 'switch', 'switch', 'extra/switch_7-off.png', 'extra/switch_7-on.png');
      break;
    case GroupAudioType:
      add_tile(data.ID, data.Name, data.Status, 'switch', 'switch', 'extra/switch_8-off.png', 'extra/switch_8-on.png');
      break;
    case 19:
      add_tile(data.ID, data.Name, data.Status, 'switch', 'switch', 'extra/switch_19-off.png', 'extra/switch_19-on.png');
      break;
    case 18: // Selector switch
      add_tile(data.ID, data.Name, data.Status, 'switch', 'switch', 'extra/switch_0-off.png', 'extra/switch_0-on.png');
      break;
    case undefined:
      if (data.HardwareTypeVal == 15) {
        add_tile(data.ID, data.Name, 'off', 'value', 'temperature');
      }
      break;
    }
    subdiv = document.getElementById(data.ID);
    // subdiv.setAttribute('id', id + '_status');
    subdiv.setAttribute('onclick', "switchTileState_extra('" + data.ID + "', '" + data.idx + "', '" + data.SwitchTypeVal + "')");
    tile = document.getElementById(data.ID);
  } else {
    //console.log("ID exists "+data.ID);
  }

  status = tile.getAttribute('status');
  //console.log(data.Name+" status: "+status+" | data.status: "+data.Status);
  switch (data.SwitchTypeVal) {
  //case 0:
  case 7:
    //console.log(data.Name+" status: "+status+" | data.status: "+data.Status);
    if (data.Status.search('%') >= 0) // is status is 97% rather than on, don't change tile status. This stops flashing back on once turned off.
      break;
  case 0:
  case 21:
  case GroupSwitchType: // Group switch
  case GroupLightType: // Group light
  case GroupAudioType:
    if (status != ((data.Status == 'Off') ? 'off' : 'on')) {
      setTileOn(data.ID, ((data.Status == 'Off') ? 'off' : 'on'), null);
    }
    break;
  case 18: // Selector switch
    if (status != ((data.Status == 'Off') ? 'off' : 'on')) {
      setTileOn(data.ID, ((data.Status == 'Off') ? 'off' : 'on'), null);
    }
    level = getLevelName(data.LevelNames, data.Level);
    if (level != document.getElementById(data.ID + '_status').innerHTML) {
      document.getElementById(data.ID + '_status').innerHTML = level;
    }
    break;
  case 19:
    if (status != ((data.Status == 'Locked') ? 'off' : 'on')) {
      setTileOn(data.ID, ((data.Status == 'Locked') ? 'off' : 'on'), null);
      document.getElementById(data.ID + '_status').innerHTML = ((data.Data == 'Locked') ? 'Closed' : 'Open');
    }
    break;
  case undefined:
    if (data.HardwareTypeVal == 15) {
      setTileValue(data.ID, parseFloat(data.Data).toString());
    }
    // Check "Type" : "Group" = data.Type = "Group"
    break;
  }
}

function extra_domoticz(type, data, group_idx=0, group_type=0) {
  var http = new XMLHttpRequest();
  if (http) {
    http.onreadystatechange = function() {
      if (http.readyState === 4) {
        if (http.status == 200 && http.status < 300) {
          var data = JSON.parse(http.responseText);
          if (data.title == 'Devices')
            extra_updateDevice(data.result[0]);
          if (data.title == 'Scenes') { // this is Group
            for(var i = 0; i <= data.result.length; i++) {
              if(data.result[i].idx == group_idx) {
                if (group_type == 0)
                  data.result[i].SwitchTypeVal = GroupSwitchType;
                else
                  data.result[i].SwitchTypeVal = group_type;

                data.result[i].ID = "Group_"+group_idx;
                extra_updateDevice(data.result[i]);
                break;
              }
            }
          }
        } else if (http.status >= 400 || http.status == 0) {
          //document.getElementById('message-text').innerHTML = 'Error connecting to server';
        }
      }
    }
  };

  http.open('GET', 'http://trident/json.htm?' + data, true);
  http.send(null);
  if (type == 'get') {
    _poller = setTimeout(function() { extra_domoticz('get', data, group_idx, group_type); }, 5000);
  }
}



function decode64(input) {
     
  var keyStr = "ABCDEFGHIJKLMNOP" +
  "QRSTUVWXYZabcdef" +
  "ghijklmnopqrstuv" +
  "wxyz0123456789+/" +
  "=";

  var output = "";
  var chr1, chr2, chr3 = "";
  var enc1, enc2, enc3, enc4 = "";
  var i = 0;

  // remove all characters that are not A-Z, a-z, 0-9, +, /, or =
  var base64test = /[^A-Za-z0-9\+\/\=]/g;
  if (base64test.exec(input)) {
      alert("There were invalid base64 characters in the input text.\n" +
      "Valid base64 characters are A-Z, a-z, 0-9, '+', '/',and '='\n" +
      "Expect errors in decoding.");
  }
  input = input.replace(/[^A-Za-z0-9\+\/\=]/g, "");

  do {
      enc1 = keyStr.indexOf(input.charAt(i++));
      enc2 = keyStr.indexOf(input.charAt(i++));
      enc3 = keyStr.indexOf(input.charAt(i++));
      enc4 = keyStr.indexOf(input.charAt(i++));
      chr1 = (enc1 << 2) | (enc2 >> 4);
      chr2 = ((enc2 & 15) << 4) | (enc3 >> 2);
      chr3 = ((enc3 & 3) << 6) | enc4;
      output = output + String.fromCharCode(chr1);

      if (enc3 != 64) {
          output = output + String.fromCharCode(chr2);
      }
      if (enc4 != 64) {
          output = output + String.fromCharCode(chr3);
      }
      chr1 = chr2 = chr3 = "";
      enc1 = enc2 = enc3 = enc4 = "";
  } while (i < input.length);

  return decodeURI(output);
}

