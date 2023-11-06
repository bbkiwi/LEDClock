var rainbowEnable = false;
var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
//var connection = new WebSocket('wss://echo.websocket.org/');
var savedate;

connection.onopen = function () {
    connection.send('Connect ' + new Date());
};
connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
};
connection.onmessage = function (e) {
	const $eventLog = document.querySelector('.event-log');
    console.log('Server: ', e.data);
    document.getElementById('whattime').innerHTML = e.data;
	$eventLog.innerHTML =  e.data + '\n' + $eventLog.innerHTML;
};
connection.onclose = function(){
    console.log('WebSocket connection closed');
};

function clean_value(x) {
  if (isNaN(x.value)){
    x.value = 0;
    return 0;
  }
  else
  {
    x.value = Math.round(+x.value);
    return Math.round(+x.value);
  }
}

function sendRGB() {
    var r = document.getElementById('r').value**2/1023; // non linear
    var g = document.getElementById('g').value**2/1023;
    var b = document.getElementById('b').value**2/1023;

    var rgb = r << 20 | g << 10 | b;
    var rgbstr = '#'+ rgb.toString(16);
    console.log('RGB: ' + rgbstr);
    connection.send(rgbstr);
}

function setbackground() {
    connection.send("B");
}

function sethour() {
    connection.send("H" + document.getElementById('widthhour').value);
}

function setminute() {
    var blink = (document.getElementById('blinkminute').checked) ? 1: 0
    connection.send("M" + blink + " " + document.getElementById('widthminute').value);
}

function setsecond() {
    connection.send("s" + " " + document.getElementById('widthsecond').value);
}

function setdispind() {
      connection.send("D" + " " + document.getElementById('dispind').value);
}

function set12() {
    connection.send("t");
}

function setquarter() {
    connection.send("q");
}

function setdivision() {
    connection.send("d");
}

function requestSaveConfig() {
    connection.send("V");
}

function whattimeAnswer() {
    connection.send("W");
}

function calcSunsets() {
    connection.send("S");
}

function pickerTimeDate(date) {
  var alarmnum = document.getElementById('alarmnum').value;
  savedate = date;
	console.log(date.getDay(), date.getHours(), date, alarmnum);
	document.getElementById('saveddatetime').innerHTML = date;
	//connection.send("A" + alarmnum + " " + date.getMonth() +" " + date);
}

function setalarm() {
  var alarmnum = clean_value(document.getElementById('alarmnum'));
  var alarmtype = clean_value(document.getElementById('alarmtype'));
  var alarmrepeat = clean_value(document.getElementById('alarmrepeat'));
  var alarmduration = clean_value(document.getElementById('alarmduration'));
	console.log(savedate.getDay(), savedate.getHours(), savedate, alarmnum, alarmtype, alarmrepeat, alarmduration);
	//document.getElementById('whattime').innerHTML = savedate;
	connection.send("A" + alarmnum + " " + alarmtype + " " + alarmrepeat + " " + alarmduration + " " + savedate.getMonth() +" " + savedate);
}


function lightEffect(){
  var pwps = clean_value(document.getElementById('pcntwheelpersec'));
  var pwpstr = clean_value(document.getElementById('pcntwheelperstrip'));
  var firstcol = clean_value(document.getElementById('firstcolor'));
  var nodec2 = clean_value(document.getElementById('nodec2'));
  var nodec1 = clean_value(document.getElementById('nodec1'));
  var nodec0 = clean_value(document.getElementById('nodec0'));
  var framerate = clean_value(document.getElementById('framerate'));
  var lightalarmnum = clean_value(document.getElementById('lightalarmnum'));
  var lightalarmex = clean_value(document.getElementById('lightalarmex'));
  var duration = clean_value(document.getElementById('duration'));
  var funcptstr = " " + points.length;
  for (let i = 0; i < points.length; i++) {
    funcptstr += " " + points[i].x + " " + points[i].y;
  }
  connection.send("L" + pwps + " " + pwpstr + " " + firstcol + " " + nodec2 + " "  + nodec1 + " " + nodec0 + " " + framerate + " " + lightalarmnum + " " + lightalarmex + " " + duration + funcptstr);
    //document.getElementById('rainbow').style.backgroundColor = '#00878F';
}

function melodyEffect(){
    connection.send("T");
    //document.getElementById('melody').style.backgroundColor = '#00878F';
}


function showDiv(elementId) {
	var ledControl = document.getElementById(elementId);
    ledControl.style.display = "block";
}

function hideDiv(elementId) {
	var ledControl = document.getElementById(elementId);
    ledControl.style.display = "none";
}

function toggleShowHide(elementId) {
	var ledControl = document.getElementById(elementId);
    if (ledControl.style.display === "block") {
      hideDiv(elementId)
    } else {
      showDiv(elementId)
    }
}
