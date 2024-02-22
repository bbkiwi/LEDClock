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
    if (e.data === 'MUSIC') {
      document.getElementById("Melody-Button").style.display = "block";
    } else {
      document.getElementById('whattime').innerHTML = e.data;
    }
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
  var alarmnum = document.getElementById('alarmnum').value;
  var alarmtype = document.getElementById('numpattern').value;
  var parm1 = document.getElementById('pat_parm1').value;
  var parm2 = document.getElementById('pat_parm2').value;
  var parm3 = document.getElementById('pat_parm3').value;
  var parm4 = document.getElementById('pat_parm4').value;
  var parm5 = document.getElementById('pat_parm5').value;
  var parm6 = document.getElementById('pat_parm6').value;
  var alarmrepeat = document.getElementById('alarmrepeat').value;
  var alarmduration = document.getElementById('alarmduration').value;
  var dayonlysign = "";
  if (document.getElementById('dayonly').innerHTML === "Day Only") {
      dayonlysign = "-";
  }
	console.log(savedate.getDay(), savedate.getHours(), savedate, alarmnum, alarmtype, alarmrepeat, alarmduration);
	//document.getElementById('whattime').innerHTML = savedate;
	connection.send("A" + alarmnum + " " + dayonlysign + alarmtype + " " + parm1 + " " + parm2 + " " + parm3 + " " + parm4 + " " + parm5 + " " + parm6 + " " +  alarmrepeat + " " + alarmduration + " " + savedate.getMonth() +" " + savedate);
}

function togdayonly() {
    if (document.getElementById('dayonly').innerHTML === "Day Only") {
      document.getElementById('dayonly').innerHTML = "Day and Night";
    } else {
      document.getElementById('dayonly').innerHTML = "Day Only";
    }
}
function getRandomIntInclusive(min, max) {
  const minCeiled = Math.ceil(min);
  const maxFloored = Math.floor(max);
  return Math.floor(Math.random() * (maxFloored - minCeiled + 1) + minCeiled); // The maximum is inclusive and the minimum is inclusive
}


function patternEffect(){
  var num = document.getElementById('numpattern').value;
  var parm1 = document.getElementById('pat_parm1').value;
  var parm2 = document.getElementById('pat_parm2').value;
  var parm3 = document.getElementById('pat_parm3').value;
  var parm4 = document.getElementById('pat_parm4').value;
  var parm5 = document.getElementById('pat_parm5').value;
  var parm6 = document.getElementById('pat_parm6').value;
  if (num == 50) {
    num = getRandomIntInclusive(1, 41);
  }
  connection.send("P" + num + " " + parm1 + " " + parm2 + " " + parm3 + " " + parm4 + " " + parm5 + " " + parm6);
    //document.getElementById('rainbow').style.backgroundColor = '#00878F';
}

function togForceDay(){
    connection.send("F");
    //document.getElementById('forceday').style.backgroundColor = '#00878F';
}

function togForceNight(){
    connection.send("G");
    //document.getElementById('forcenight').style.backgroundColor = '#00878F';
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
	var els = ["Clock-Control", "Display-Control", "Alarm-Control"];
	  els.forEach(function(el){
	    if (el != elementId) {
	      hideDiv(el);
	    }
	  });
    if (ledControl.style.display === "block") {
      hideDiv(elementId);
    } else {
      showDiv(elementId);
    }
}
