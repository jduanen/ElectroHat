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

    document.getElementById("ssid").value = msgObj.ssid;

    elem = document.getElementById("password");
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
    document.getElementById("seqDelay").innerHTML = parseInt(msgObj.sequenceDelay);

    var led = (msgObj.led == "true");
    document.getElementById('ledState').innerHTML = (led ? "ON" : "OFF");
    document.getElementById('led').checked = led;

    var rp = (msgObj.randomPattern == "true");
    document.getElementById('randomPattern').checked = rp;
    document.getElementById('patternNumber').disabled = rp;

    elem = document.getElementById('patternNumber');
    elem.value = msgObj.patternNumber;

    elem = document.getElementById('patternDelay');
    elem.value = msgObj.patternDelay;
    document.getElementById("patDelay").innerHTML = parseInt(msgObj.patternDelay);

    document.getElementById("save").disabled = false;
  }
  function toggleCheckbox(element) {
    element.innerHTML = (element.checked ? "ON" : "OFF");
    var jsonMsg = JSON.stringify({"msgType": element.id, "state": element.checked});
    websocket.send(jsonMsg);
  }
  function setSequence() {
    var seqNum = document.getElementById("sequenceNumber").value;
    var seqDelay = document.getElementById("sequenceDelay").value;
    var jsonMsg = JSON.stringify({"msgType": "sequence", "sequenceNumber": seqNum, "sequenceDelay": seqDelay});
    websocket.send(jsonMsg);
  }
  function setPattern() {
    var patNum = document.getElementById("patternNumber").value;
    var patDelay = document.getElementById("patternDelay").value;
    var jsonMsg = JSON.stringify({"msgType": "pattern", "patternNumber": patNum, "patternDelay": patDelay});
    websocket.send(jsonMsg);
  }
  function saveConfiguration() {
    var jsonMsg = JSON.stringify({"msgType": "saveConf",
                                  "ssid": document.getElementById("ssid").value,
                                  "passwd": rot47(document.getElementById("password").value),
                                  "elState": document.getElementById('el').checked,
                                  "randomSequence": document.getElementById("randomSequence").checked,
                                  "sequenceNumber": document.getElementById('sequenceNumber').value,
                                  "sequenceDelay": document.getElementById('sequenceDelay').value,
                                  "ledState": document.getElementById('led').checked,
                                  "randomPattern": document.getElementById("randomPattern").checked,
                                  "patternNumber": document.getElementById('patternNumber').value,
                                  "patternDelay": document.getElementById('patternDelay').value
                                });
    document.getElementById("save").disabled = true;
    websocket.send(jsonMsg);
  }
  function toggleRandomSequence() {
    var randomSeq = document.getElementById("randomSequence").checked;
    document.getElementById("sequenceNumber").disabled = randomSeq;
    var jsonMsg = JSON.stringify({"msgType": "randomSequence", "state": randomSeq});
    websocket.send(jsonMsg);
  }
  function toggleRandomPattern() {
    var randomPat = document.getElementById("randomPattern").checked;
    document.getElementById("patternNumber").disabled = randomPat;
    var jsonMsg = JSON.stringify({"msgType": "randomPattern", "state": randomPat});
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
    for(var i = 0; (i < x.length); i++) {
      var j = x.charCodeAt(i);
      if ((j >= 33) && (j <= 126)) {
        s[i] = String.fromCharCode(33 + ((j + 14) % 94));
      } else {
        s[i] = String.fromCharCode(j);
      }
    }
    return s.join('');
  }