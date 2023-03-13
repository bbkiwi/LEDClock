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
    connection.send("H");
}

function setminute() {
    connection.send("M");
}

function setsecond() {
    connection.send("s");
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
  var alarmnum = document.getElementById('alarmnum').value;
  var alarmtype = document.getElementById('alarmtype').value;
  var alarmrepeat = document.getElementById('alarmrepeat').value;
  var alarmduration = document.getElementById('alarmduration').value;
	console.log(savedate.getDay(), savedate.getHours(), savedate, alarmnum, alarmtype, alarmrepeat, alarmduration);
	//document.getElementById('whattime').innerHTML = savedate;
	connection.send("A" + alarmnum + " " + alarmtype + " " + alarmrepeat + " " + alarmduration + " " + savedate.getMonth() +" " + savedate);
}


function rainbowEffect(){
  var num = document.getElementById('numpattern').value;
  connection.send("R" + num);
    //document.getElementById('rainbow').style.backgroundColor = '#00878F';
}

function melodyEffect(){
    connection.send("L");
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
