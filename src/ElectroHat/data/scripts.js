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
  renderColorButtons();
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

  var el = (msgObj.el == "true");
  document.getElementById('elState').innerHTML = (el ? "ON" : "OFF");
  document.getElementById('el').checked = el;

  var rs = (msgObj.randomSequence == "true");
  document.getElementById('randomSequence').checked = rs;
  document.getElementById('sequenceNumber').disabled = rs;

  elem = document.getElementById('sequenceNumber');
  elem.value = msgObj.sequenceNumber;

  elem = document.getElementById('sequenceDelay');
  elem.value = msgObj.sequenceDelay;
  document.getElementById('seqDelay').innerHTML = parseInt(msgObj.sequenceDelay);

  var led = (msgObj.led == "true");
  document.getElementById('ledState').innerHTML = (led ? "ON" : "OFF");
  document.getElementById('led').checked = led;

  document.getElementById('randomPattern').checked = (msgObj.randomPattern == "true");

  elem = document.getElementById('patternNumber');
  elem.value = msgObj.patternNumber;

  elem = document.getElementById('patternDelay');
  elem.value = msgObj.patternDelay;
  document.getElementById('patDelay').innerHTML = parseInt(msgObj.patternDelay);

  elem = document.getElementById('patternColor');
  elem.value = "#" + (msgObj.patternColor).toString(16);

  setCustomColors(msgObj.customColors);

  document.getElementById('customBidir').checked = (msgObj.customBidir == "true");

  document.getElementById('save').disabled = false;

  enableModes();
}
function toggleCheckbox(element) {
  element.innerHTML = (element.checked ? "ON" : "OFF");
  var jsonMsg = JSON.stringify({"msgType": element.id, "state": element.checked});
  websocket.send(jsonMsg);
}
function setSequence() {
  var seqNum = document.getElementById('sequenceNumber').value;
  var seqDelay = document.getElementById('sequenceDelay').value;
  var jsonMsg = JSON.stringify({"msgType": "sequence", "sequenceNumber": seqNum, "sequenceDelay": seqDelay});
  websocket.send(jsonMsg);
}
function disableCustomColors(enb) {
  var elems = document.getElementsByClassName('colorsButton');
  for (var i = 0; i < elems.length; i++) {
    if (enb) {
      elems[i].disable = true;
    } else {
      elems[i].disable = false;
    }
  }
}
function ledModeA() {
  //// disable: patternColor, startColor, endColor, colorBoxes; enable: patternDelay
  document.getElementById('patternColor').disabled = true;
  document.getElementById('startColor').disabled = true;
  document.getElementById('endColor').disabled = true;
  disableCustomColors(true);
}
function ledModeB() {
  //// disable: startColor, endColor, colorBoxes; enable: patternDelay, patternColor
  document.getElementById('patternColor').disabled = false;
  document.getElementById('startColor').disabled = true;
  document.getElementById('endColor').disabled = true;
  disableCustomColors(true);
}
function ledModeC() {
  //// disable: patternColor; enable: patternDelay, startColor, endColor, colorBoxes
  document.getElementById('patternColor').disabled = true;
  document.getElementById('startColor').disabled = false;
  document.getElementById('endColor').disabled = false;
  disableCustomColors(false);
}
function enableModes() {
  if (document.getElementById('randomPattern').checked) {
    ledModeA();
  } else {
    switch (patternNames[document.getElementById('patternNumber').value]) {
      case "Rainbow Marquee":
        ledModeA();
        break;
      case "Rainbow":
        ledModeA();
        break;
      case "Color Wipe":
        ledModeB();
        break;
      case "Color Fill":
        ledModeB();
        break;
      case "Marquee":
        ledModeB();
        break;
      case "Custom":
        ledModeC();
        break;
      default:
        console.log("Unknown pattern: " + patternNames[patNum]);
    }
  }
}
function setPattern() {
  var patNum = document.getElementById('patternNumber').value;
  var patDelay = document.getElementById('patternDelay').value;
  var patColor = parseInt(document.getElementById('patternColor').value.substr(1), 16);
  var custColors = getCustomColors();
  var custBidir = document.getElementById('customBidir').value;
  var jsonMsg = JSON.stringify({'msgType': 'pattern',
                                'patternNumber': patNum,
                                'patternDelay': patDelay,
                                'patternColor': patColor,
                                'customColors': custColors,
                                'customBidir': custBidir
                              });
  console.log("!!!!" + jsonMsg);
  websocket.send(jsonMsg);
  enableModes();
}
function saveConfiguration() {
  //// FIXME make start and end color be lists
  var jsonMsg = JSON.stringify({'msgType': 'saveConf',
                                'ssid': document.getElementById('ssid').value,
                                'passwd': rot47(document.getElementById('password').value),
                                'elState': document.getElementById('el').checked,
                                'randomSequence': document.getElementById('randomSequence').checked,
                                'sequenceNumber': document.getElementById('sequenceNumber').value,
                                'sequenceDelay': document.getElementById('sequenceDelay').value,
                                'ledState': document.getElementById('led').checked,
                                'randomPattern': document.getElementById('randomPattern').checked,
                                'patternNumber': document.getElementById('patternNumber').value,
                                'patternDelay': document.getElementById('patternDelay').value,
                                'patternColor': parseInt(document.getElementById('patternColor').value.substr(1), 16),
                                'customColors': [[], []],
                                'customBidir': document.getElementById('customBidir').checked
                              });
  document.getElementById('save').disabled = true;
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
function renderColorButtons() {
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
//// FIXME
function colorsButtonClick(element) {
  var startColor = parseInt(document.getElementById('startColor').value.substr(1), 16);
  var endColor = parseInt(document.getElementById('endColor').value.substr(1), 16);
  console.log("LED #: " + element.name + ", " + startColor.toString(16) + ", " + endColor.toString(16));
  element.style.background = "linear-gradient(#" + startColor.toString(16) + ", #" + endColor.toString(16) + ")";
  console.log(getCustomColors());  //// TMP TMP TMP
}
function clearCustomColors() {
  var elems = document.getElementsByClassName('colorsButton');
  for (var i = 0; i < elems.length; i++) {
    elems[i].style.background = 'linear-gradient(#000000, #000000)';
  }
  setPattern();
}
//// FIXME
function getCustomColors() {
  var startColors = [];
  var endColors = [];
  var elems = document.getElementsByClassName('colorsButton');
  for (var i = 0; i < elems.length; i++) {
    console.log("***: " + elems[i].style.background);
  }
  return [startColors, endColors]
}
//// FIXME
function setCustomColors(customColors) {
  //// TODO
  var elems = document.getElementsByClassName('colorsButton');
  for (var i = 0; i < elems.length; i++) {
    elems[i].style.background = 'linear-gradient(#000000, #000000)';  //// FIXME
  }
}
