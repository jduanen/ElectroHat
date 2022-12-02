var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onLoad);
function initWebSocket() {
  console.log('Trying to open a WebSocket connection...');
  websocket = new WebSocket(gateway);
  websocket.onopen    = onOpen;
  websocket.onclose   = onClose;
  websocket.onmessage = onMessage;
}
function onOpen(event) {
  console.log('Connection opened');
  initView();
}
function onClose(event) {
  console.log('Connection closed');
  setTimeout(initWebSocket, 2000);
}
function onLoad(event) {
  initWebSocket();
}
function onMessage(event) {
  var state;
  var elem;
  const msgObj = JSON.parse(event.data);
  console.log("msgObj: " + JSON.stringify(msgObj));

  document.getElementById('ssid').value = msgObj.ssid;

  elem = document.getElementById('password');
  if (msgObj.passwd != null) {
    elem.value = rot47(msgObj.passwd);
  } else {
    elem.value = "";
  }

  var el = msgObj.el;
  document.getElementById('elState').innerHTML = (el ? "ON" : "OFF");
  document.getElementById('el').checked = el;

  var rs = msgObj.randomSequence;
  document.getElementById('randomSequence').checked = rs;
  document.getElementById('sequenceNumber').disabled = rs;

  elem = document.getElementById('sequenceNumber');
  elem.value = msgObj.sequenceNumber;

  elem = document.getElementById('sequenceDelay');
  elem.value = msgObj.sequenceDelay;
  document.getElementById('seqDelay').innerHTML = parseInt(msgObj.sequenceDelay);

  var led = msgObj.led;
  document.getElementById('ledState').innerHTML = (led ? "ON" : "OFF");
  document.getElementById('led').checked = led;

  document.getElementById('randomPattern').checked = msgObj.randomPattern;

  elem = document.getElementById('patternNumber');
  elem.value = msgObj.patternNumber;

  elem = document.getElementById('patternDelay');
  elem.value = msgObj.patternDelay;
  document.getElementById('patDelay').innerHTML = parseInt(msgObj.patternDelay);
  elem.min = patternMinDelays[msgObj.patternNumber];
  elem.max = patternMaxDelays[msgObj.patternNumber];

  elem = document.getElementById('patternColor');
  elem.value = "#" + (msgObj.patternColor).toString(16);

  setCustomColors(msgObj.customColors);

  document.getElementById('customBidir').checked = msgObj.customBidir;

  customDelta = parseInt(msgObj.customDelta);

  document.getElementById('save').disabled = false;

  enableModes();
}
function toggleCheckbox(element) {
  element.innerHTML = (element.checked ? "ON" : "OFF");
  var jsonMsg = JSON.stringify({"msgType": element.id, "state": element.checked});
  websocket.send(jsonMsg);
}
function setSequence() {
  var seqNum = parseInt(document.getElementById('sequenceNumber').value);
  var seqDelay = parseInt(document.getElementById('sequenceDelay').value);
  var jsonMsg = JSON.stringify({"msgType": "sequence", "sequenceNumber": seqNum, "sequenceDelay": seqDelay});
  websocket.send(jsonMsg);
}
function disableCustomColors(enb) {
  for (var i = 0; (i < NUMBER_OF_LEDS); i++) {
    if (enb) {
      document.getElementById('cb' + i).disabled = true;
    } else {
      document.getElementById('cb' + i).disabled = false;
    }
  }
}
function ledModeA() {
  //// disable: patternColor, startColor, endColor, colorBoxes, clearAll; enable: patternNumber, patternDelay
  document.getElementById('patternNumberDiv').style.visibility = "visible";
  document.getElementById('patternColorDiv').style.visibility = "hidden";
  document.getElementById('patternDelayDiv').style.visibility = "visible";
  document.getElementById('customPatternDiv').style.visibility = "hidden";
//  disableCustomColors(true);
}
function ledModeB() {
  //// disable: patternDelay, startColor, endColor, colorBoxes, clearAll; enable: patternNumber, patternColor
  document.getElementById('patternNumberDiv').style.visibility = "visible";
  document.getElementById('patternColorDiv').style.visibility = "visible";
  document.getElementById('patternDelayDiv').style.visibility = "hidden";
  document.getElementById('customPatternDiv').style.visibility = "hidden";
//  disableCustomColors(true);
}
function ledModeC() {
  //// disable: startColor, endColor, colorBoxes, clearAll; enable: patternNumber, patternDelay, patternColor
  document.getElementById('patternNumberDiv').style.visibility = "visible";
  document.getElementById('patternColorDiv').style.visibility = "visible";
  document.getElementById('patternDelayDiv').style.visibility = "visible";
  document.getElementById('customPatternDiv').style.visibility = "hidden";
//  disableCustomColors(true);
}
function ledModeD() {
  //// disable: patternColor; enable: patternNumber, patternDelay, startColor, endColor, colorBoxes, clearAll
  document.getElementById('patternNumberDiv').style.visibility = "visible";
  document.getElementById('patternColorDiv').style.visibility = "hidden";
  document.getElementById('patternDelayDiv').style.visibility = "visible";
  document.getElementById('customPatternDiv').style.visibility = "visible";
//  disableCustomColors(false);
}
function ledModeE() {
  //// disable: patternNumber, patternColor, startColor, endColor, colorBoxes, clearAll; enable: patternDelay
  document.getElementById('patternNumberDiv').style.visibility = "hidden";
  document.getElementById('patternColorDiv').style.visibility = "hidden";
  document.getElementById('patternDelayDiv').style.visibility = "visible";
  document.getElementById('customPatternDiv').style.visibility = "hidden";
//  disableCustomColors(true);
}
function enableModes() {
  if (document.getElementById('randomPattern').checked) {
    ledModeE();
  } else {
    switch (patternNames[document.getElementById('patternNumber').value]) {
      case "Rainbow Marquee":
        ledModeA();
        break;
      case "Rainbow":
        ledModeA();
        break;
      case "Color Wipe":
        ledModeC();
        break;
      case "Color Fill":
        ledModeB();
        break;
      case "Marquee":
        ledModeC();
        break;
      case "Custom":
        ledModeD();
        break;
      default:
        console.log("Unknown pattern: " + patternNames[patNum]);
    }
  }
}
function setPattern() {
  var patNum = parseInt(document.getElementById('patternNumber').value);
  var elem = document.getElementById('patternDelay');
  var patDelay = patternDefDelays[patNum];
  elem.min = patternMinDelays[patNum];
  elem.max = patternMaxDelays[patNum];
  var patColor = parseInt(document.getElementById('patternColor').value.substr(1), 16);
  var custBidir = document.getElementById('customBidir').checked;
  var jsonMsg = JSON.stringify({'msgType': 'pattern',
                                'patternNumber': patNum,
                                'patternDelay': patDelay,
                                'patternColor': patColor,
                                'customBidir': custBidir,
                                'customDelta': customDelta,
                                'customColors': getCustomColors()
                              });
  console.log("setPattern: " + jsonMsg);
  websocket.send(jsonMsg);
  enableModes();
}
function saveConfiguration() {
  var jsonMsg = JSON.stringify({'msgType': 'saveConf',
                                'ssid': document.getElementById('ssid').value,
                                'passwd': rot47(document.getElementById('password').value),
                                'elState': document.getElementById('el').checked,
                                'randomSequence': document.getElementById('randomSequence').checked,
                                'sequenceNumber': parseInt(document.getElementById('sequenceNumber').value),
                                'sequenceDelay': parseInt(document.getElementById('sequenceDelay').value),
                                'ledState': document.getElementById('led').checked,
                                'randomPattern': document.getElementById('randomPattern').checked,
                                'patternNumber': parseInt(document.getElementById('patternNumber').value),
                                'patternDelay': parseInt(document.getElementById('patternDelay').value),
                                'patternColor': parseInt(document.getElementById('patternColor').value.substr(1), 16),
                                'customBidir': document.getElementById('customBidir').checked,
                                'customDelta': customDelta
                              });
//                                'customColors': getCustomColors()
  document.getElementById('save').disabled = true;
  console.log("saveC: " + jsonMsg);
  websocket.send(jsonMsg);
}
function toggleRandomSequence() {
  var randomSeq = document.getElementById('randomSequence').checked;
  document.getElementById('sequenceNumber').disabled = randomSeq;
  var jsonMsg = JSON.stringify({'msgType': 'randomSequence', 'state': randomSeq});
  websocket.send(jsonMsg);
}
function toggleRandomPattern() {
  var randomPat = document.getElementById('randomPattern').checked;
  document.getElementById('patternNumber').disabled = randomPat;
  if (randomPat) {
    ledModeA();
  }
  var jsonMsg = JSON.stringify({'msgType': 'randomPattern', 'state': randomPat});
  websocket.send(jsonMsg);
}
function escapeHTML(s) {
  return s.replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/'/g, '&apos;')
      .replace(/"/g, '&quot;')
      .replace(/\//g, '&sol;');
}
function rot13(str) {
  return str.split('').map(char => String.fromCharCode(char.charCodeAt(0) + (char.toLowerCase() < 'n' ? 13 : -13))).join('');
}
function rot47(x) {
  var s = [];
  for (var i = 0; (i < x.length); i++) {
    var j = x.charCodeAt(i);
    if ((j >= 33) && (j <= 126)) {
      s[i] = String.fromCharCode(33 + ((j + 14) % 94));
    } else {
      s[i] = String.fromCharCode(j);
    }
  }
  return s.join('');
}
function positionCustomColorButtons() {
  //// FIXME switch to getElementByName()
  var elems = document.getElementsByClassName('colorsButton');
  var increase = Math.PI * 2 / elems.length;
  var x = 0, y = 0, angle = -Math.PI / 2;

  for (var i = 0; i < elems.length; i++) {
    x = 100 * Math.cos(angle) + 122;
    y = 100 * Math.sin(angle) + 102;
    elems[i].style.position = 'absolute';
    elems[i].style.left = x + 'px';
    elems[i].style.top = y + 'px';
    angle += increase;
  }
}
function colorsButtonClick(element) {
  var startColor = parseInt(document.getElementById('startColor').value.substr(1), 16);
  var endColor = parseInt(document.getElementById('endColor').value.substr(1), 16);
  element.style.background = "linear-gradient(#" + startColor.toString(16).padStart(6, 0) + ", #" + endColor.toString(16).padStart(6, 0) + ")";
  setPattern();
//  var jsonMsg = JSON.stringify({'msgType': 'customColors', 'customColors': getCustomColors()});
//  websocket.send(jsonMsg);
}
function clearCustomColors() {
  for (var i = 0; (i < NUMBER_OF_LEDS); i++) {
    document.getElementById('cb' + i).style.background = "linear-gradient(#00ff00, #00ff00)";
  }
//  setPattern();
//  var jsonMsg = JSON.stringify({'msgType': 'customColors', 'customColors': getCustomColors()});
//  websocket.send(jsonMsg);
}
function rgbToHex(r, g, b) {
  return "#" + (1 << 24 | r << 16 | g << 8 | b).toString(16).slice(1);
}
function rgbToInt(r, g, b) {
  return ((parseInt(r) << 16) + (parseInt(g) << 8) + parseInt(b));
}
function rgbIntToHex(c) {
  return "#" + c.toString(16).padStart(6, 0);
}
function getCustomColors() {
  var customColors = [];
  for (var i = 0; (i < NUMBER_OF_LEDS); i++) {
    var btn = document.getElementById('cb' + i);
    if (btn.style.background == "") {
      btn.style.background = "linear-gradient(#ff0000, #0000ff)";
    }
    var s = [];
    for (m of btn.style.background.matchAll(/rgb\((\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)/g)) {
      s.push(rgbToInt(m[1], m[2], m[3]));
    }
    customColors.push(s);
  }
  return customColors;
}
function setCustomColors(customColors) {
  for (var i = 0; (i < NUMBER_OF_LEDS); i++) {
    var elemId = 'cb' + i;
    var bkg = "linear-gradient(" + rgbIntToHex(customColors[i][0]) + ", " + rgbIntToHex(customColors[i][1]) + ")";
    document.getElementById(elemId).style.background = bkg;
  }
}
