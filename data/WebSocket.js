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
    } else if (e.data.startsWith('ALARMINFO:')) {
      var alarminfo = e.data.split(',');
      console.log(alarminfo);
      document.getElementById('numpattern').value = Math.abs(Number(alarminfo[3]));
      document.getElementById('pat_parm1').value = Number(alarminfo[4]);
      document.getElementById('pat_parm2').value = Number(alarminfo[5]);
      document.getElementById('pat_parm3').value = Number(alarminfo[6]);
      document.getElementById('pat_parm4').value = Number(alarminfo[7]);
      document.getElementById('pat_parm5').value = Number(alarminfo[8]);
      document.getElementById('pat_parm6').value = Number(alarminfo[9]);
      document.getElementById('alarmrepeat').value = alarminfo[10];
      if (document.getElementById('alarmrepeat').value === '') {
        document.getElementById('alarmrepeat').value = 'other';
        document.getElementById('othervalue').hidden =  false;
        document.getElementById('othervalue').value = alarminfo[10];
      } else {
        document.getElementById('othervalue').hidden =  true;
      }

      document.getElementById('alarmduration').value = Number(alarminfo[11]);
      savedate = new Date(Date.parse(alarminfo[12]));
      document.getElementById('saveddatetime').innerHTML = savedate;
      if (alarminfo[2] === '1') {
        if (Number(alarminfo[3]) < 0) {
          document.getElementById('dayonly').innerHTML ="Day Only";
        } else {
          document.getElementById('dayonly').innerHTML = "Day and Night";
        }
      } else {
        document.getElementById('dayonly').innerHTML = "Off or Unset";
        //document.getElementById('saveddatetime').innerHTML = 'NOT SET MUST CHOOSE WHEN';
      }
      document.getElementById('AdjMorn').value = Number(alarminfo[13]);
      document.getElementById('AdjNight').value = Number(alarminfo[14]);
    } else if (e.data.startsWith('DISPLAYINFO:')) {
      var displayinfo = e.data.split(',');
      console.log(displayinfo);
      document.getElementById('dispind').value = Math.abs(Number(displayinfo[1]));
      document.getElementById('widthhour').value = Number(displayinfo[2]);
      document.getElementById('widthminute').value = Number(displayinfo[3]);
      document.getElementById('blinkminute').checked = 1 === Number(displayinfo[4]);
      document.getElementById('widthsecond').value = Number(displayinfo[5]);
    } else if (e.data.startsWith('WHATTIME')) {
      document.getElementById('whattime').innerHTML = e.data.substring(8);
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
    connection.send("h" + " " + document.getElementById('widthhour').value);
}

function setminute() {
    var blink = (document.getElementById('blinkminute').checked) ? 1: 0
    connection.send("m" + blink + " " + document.getElementById('widthminute').value);
}

function setsecond() {
    connection.send("s" + " " + document.getElementById('widthsecond').value);
}

function sethourcol() {
    connection.send("H");
}

function setminutecol() {
    connection.send("M");
}

function setsecondcol() {
    connection.send("S");
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
    connection.send("C");
}

function pickerTimeDate(date) {
  var alarmnum = document.getElementById('alarmnum').value;
  savedate = date;
	console.log(date.getDay(), date.getHours(), date, alarmnum);
	document.getElementById('saveddatetime').innerHTML = date;
	//connection.send("A" + alarmnum + " " + date.getMonth() +" " + date);
}

function RequestPopulate() {
  var alarmnum = document.getElementById('alarmnum').value;
  console.log('Request Populate Alarm ' + alarmnum);
  connection.send("a" + alarmnum);
}

function AlarmrepeatChanged() {
  if (document.getElementById('alarmrepeat').value === "other") {
    document.getElementById('othervalue').hidden =  false;
  } else {
    document.getElementById('othervalue').hidden =  true;
  }
}

function setalarm() {
  var alarmnum = document.getElementById('alarmnum').value;
  if (document.getElementById('dayonly').innerHTML === "Off or Unset") {
      connection.send("R" + alarmnum);
    return;
  }
  var alarmtype = document.getElementById('numpattern').value;
  var parm1 = document.getElementById('pat_parm1').value;
  var parm2 = document.getElementById('pat_parm2').value;
  var parm3 = document.getElementById('pat_parm3').value;
  var parm4 = document.getElementById('pat_parm4').value;
  var parm5 = document.getElementById('pat_parm5').value;
  var parm6 = document.getElementById('pat_parm6').value;
  var alarmrepeat = document.getElementById('alarmrepeat').value;
  var adjmorn = document.getElementById('AdjMorn').value;
  var adjnight = document.getElementById('AdjNight').value;
  if (alarmrepeat === 'other') {
    alarmrepeat = document.getElementById('othervalue').value;
  }
  var alarmduration = document.getElementById('alarmduration').value;
  var dayonlysign = "";
  if (document.getElementById('dayonly').innerHTML === "Day Only") {
      dayonlysign = "-";
  }
	console.log(savedate.getDay(), savedate.getHours(), savedate, alarmnum, alarmtype, alarmrepeat, alarmduration);
	//document.getElementById('whattime').innerHTML = savedate;
	connection.send("A" + alarmnum + " " + dayonlysign + alarmtype + " " + parm1 + " " + parm2 + " " + parm3 + " " + parm4 + " " + parm5 + " " + parm6 + " " +  alarmrepeat + " " + alarmduration + " " + savedate.getMonth() +" " + savedate);
	connection.send("J" + adjmorn + " " + adjnight);
}

function handleWhen() {
    if (document.getElementById('dayonly').innerHTML === "Day Only") {
      document.getElementById('dayonly').innerHTML = "Day and Night";
    } else if (document.getElementById('dayonly').innerHTML === "Day and Night") {
      document.getElementById('dayonly').innerHTML = "Off or Unset";
    }  else if (document.getElementById('dayonly').innerHTML === "Off or Unset") {
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
