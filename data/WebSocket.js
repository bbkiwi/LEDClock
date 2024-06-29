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
      document.getElementById('green_nfrac').value = Number(alarminfo[14]);
      document.getElementById('blue_nfrac').value = Number(alarminfo[15]);
      document.getElementById('pat_parm7').value = Number(alarminfo[16]);
      document.getElementById('pat_parm8').value = Number(alarminfo[17]);
      document.getElementById('alarmrepeat').value = alarminfo[10];
      if (document.getElementById('alarmrepeat').value === '') {
        document.getElementById('alarmrepeat').value = 'other';
        document.getElementById('othervalue').hidden =  false;
        document.getElementById('othervalue').value = alarminfo[10];
      } else {
        document.getElementById('othervalue').hidden =  true;
      }
      document.getElementById('dayactive1').checked = Number(alarminfo[11]) & 2;
      document.getElementById('dayactive2').checked = Number(alarminfo[11]) & 4;
      document.getElementById('dayactive3').checked = Number(alarminfo[11]) & 8;
      document.getElementById('dayactive4').checked = Number(alarminfo[11]) & 16;
      document.getElementById('dayactive5').checked = Number(alarminfo[11]) & 32;
      document.getElementById('dayactive6').checked = Number(alarminfo[11]) & 64;
      document.getElementById('dayactive7').checked = Number(alarminfo[11]) & 128;

      document.getElementById('alarmduration').value = Number(alarminfo[12]);
      savedate = new Date(Date.parse(alarminfo[13]));
      document.getElementById('saveddatetime').innerHTML = savedate.toString().slice(0,-36);

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
    } else if (e.data.startsWith('DISPLAYINFO:')) {
      var displayinfo = e.data.split(',');
      console.log(displayinfo);
      document.getElementById('dispind').value = Math.abs(Number(displayinfo[1]));
      document.getElementById('widthhour').value = Number(displayinfo[2]);
      document.getElementById('widthminute').value = Number(displayinfo[3]);
      document.getElementById('blinkminute').checked = 1 === Number(displayinfo[4]);
      document.getElementById('widthsecond').value = Number(displayinfo[5]);
      document.getElementById('nightbright').value = Number(displayinfo[6]);
      document.getElementById('daybright').value = Number(displayinfo[7]);
    } else if (e.data.startsWith('MODEINFO:')) {
      var modeinfo = e.data.split(',');
      console.log(modeinfo);
      fillTable(modeinfo);
    } else if (e.data.startsWith('WHATTIME')) {
      document.getElementById('whattime').innerHTML = e.data.substring(8);
    }
	$eventLog.innerHTML =  e.data + ' <br> ' + $eventLog.innerHTML;
};
connection.onclose = function(){
    console.log('WebSocket connection closed');
};


function sendColor() {
    var color = document.getElementById('cp').value;
    var r = parseInt(color.substring(1,3),16)**2/255;
    var g = parseInt(color.substring(3,5),16)**2/255;
    var b = parseInt(color.substring(5,7),16)**2/255;
    var rgb = r << 22 | g << 12 | b << 2;
    var rgbstr = '#'+ rgb.toString(16);
    console.log('sendColor: ' + color + ' r = ' + r + ', g= ' + g + ',b= ' + b + ', rgbstr= ' +rgbstr);
    //console.log('COLOR PICKER: ' + rgbstr);
    connection.send(rgbstr);
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
    sendColor();
    connection.send("H");
}

function setminutecol() {
    sendColor();
    connection.send("M");
}

function setsecondcol() {
    sendColor();
    connection.send("S");
}

function setdispind() {
      connection.send("D" + " " + document.getElementById('dispind').value);
}

function setbackground() {
    sendColor();
    connection.send("B");
}

function set12() {
    sendColor();
    connection.send("t");
}

function setquarter() {
    sendColor();
    connection.send("q");
}

function setdivision() {
    sendColor();
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
	document.getElementById('saveddatetime').innerHTML = date.toString().slice(0,-36);
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

function setbright() {
  var daybright = document.getElementById('daybright').value;
  var nightbright = document.getElementById('nightbright').value;
  console.log('Set bright night ' + nightbright + ' day ' + daybright);
  connection.send("b" + nightbright + " " + daybright);
}

function setalarm() {
  var cmd = 'A';
  var alarmnum = document.getElementById('alarmnum').value;
  if (document.getElementById('dayonly').innerHTML === "Off or Unset") {
    cmd = 'R';
  }
  var alarmtype = document.getElementById('numpattern').value;
  var parm1 = document.getElementById('pat_parm1').value;
  var parm2 = document.getElementById('pat_parm2').value;
  var parm3 = document.getElementById('pat_parm3').value;
  var parm4 = document.getElementById('pat_parm4').value;
  var parm5 = document.getElementById('pat_parm5').value;
  var parm6 = document.getElementById('pat_parm6').value;
  var parm9 = document.getElementById('pat_parm7').value;
  var parm10 = document.getElementById('pat_parm8').value;
  var parm7 = document.getElementById('green_nfrac').value;
  var parm8 = document.getElementById('blue_nfrac').value;
  var alarmrepeat = document.getElementById('alarmrepeat').value;
  var daysactive = ((document.getElementById('dayactive1').checked) ? 2: 0) +
                   ((document.getElementById('dayactive2').checked) ? 4: 0) +
                   ((document.getElementById('dayactive3').checked) ? 8: 0) +
                   ((document.getElementById('dayactive4').checked) ? 16: 0) +
                   ((document.getElementById('dayactive5').checked) ? 32: 0) +
                   ((document.getElementById('dayactive6').checked) ? 64: 0) +
                   ((document.getElementById('dayactive7').checked) ? 128: 0);
  //var adjmorn = document.getElementById('AdjMorn').value;
  //var daystart = 0 // document.getElementById('daystart').value;
  //var nightstart = 0 // document.getElementById('nightstart').value;
  //var adjnight = document.getElementById('AdjNight').value;
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
	connection.send(cmd + alarmnum + " " + dayonlysign + alarmtype + " " + parm1 + " " + parm2 + " " + parm3 + " " + parm4 + " " + parm5 + " " + parm6 + " "
	 +  alarmrepeat + " " +  daysactive + " " + alarmduration + " " + savedate.getMonth() +" " + savedate + " " + parm7 + " " + parm8 + " " + parm9 + " " + parm10);
	//connection.send("J" + adjmorn + " " + adjnight + " " + daystart + " " + nightstart);
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


function patternEffect() {
  var num = document.getElementById('numpattern').value;
  var parm1 = document.getElementById('pat_parm1').value;
  var parm2 = document.getElementById('pat_parm2').value;
  var parm3 = document.getElementById('pat_parm3').value;
  var parm4 = document.getElementById('pat_parm4').value;
  var parm5 = document.getElementById('pat_parm5').value;
  var parm6 = document.getElementById('pat_parm6').value;
  var parm9 = document.getElementById('pat_parm7').value;
  var parm10 = document.getElementById('pat_parm8').value;
  var parm7= document.getElementById('green_nfrac').value;
  var parm8 = document.getElementById('blue_nfrac').value;
  if (num == 43) {
    num = getRandomIntInclusive(1, 42);
  }
  connection.send("P" + num + " " + parm1 + " " + parm2 + " " + parm3 + " " + parm4 + " " + parm5 + " " + parm6 + " " + parm7 + " " + parm8 + " " + parm9 + " " + parm10 + " " + sendRGBBparms());
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
	var els = ["Clock-Control", "Pattern-Control", "Display-Control", "Alarm-Control", "Mode-Control"];
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

// Function to toggle visibility of inputs based on checkbox state
function toggleInputs(checkbox) {
    var isChecked = checkbox.checked;
    var row = checkbox.parentNode.parentNode; // Get the row containing the checkbox
    var cells = row.cells;

    // Find the input elements within the corresponding cell
    var inputs = cells[checkbox.parentNode.cellIndex].querySelectorAll('input[type="time"], input[type="text"]');

    if (isChecked) {
        inputs[0].style.display = 'none'; // Hide time input
        inputs[1].style.display = 'inline-block'; // Show text input
    } else {
        inputs[1].style.display = 'none'; // Hide text input
        inputs[0].style.display = 'inline-block'; // Show time input
    }
}


// fills an existing table by over writing data
// data[0] ignored, data[1] is check, data[2] is hour:min, data[3] is deviation, etc
function fillTable(data) {
    var table = document.getElementById('ModeTimesTable');
    var tbody = table.querySelector('tbody');
    var rows = tbody.getElementsByTagName('tr');
    //console.log(rows)
    var dataind = 1;
    for (var irow = 0; irow < 7; irow++) {
        var row = rows[irow];
        var cells = row.getElementsByTagName('td');
        var jcell = 1;
        for (var j = 0; j < 2; j++) {
            var checkbox = cells[jcell].querySelector('input[type="checkbox"]');
            var timeInput = cells[jcell].querySelector('input[type="time"]');
            var textInput = cells[jcell].querySelector('input[type="text"]');
            checkbox.checked = data[dataind]==1;
            timeInput.value =  data[dataind+1];
            textInput.value = data[dataind+2];
            console.log(dataind + " " + (data[dataind]==1) + " " + data[dataind+1] + " " + data[dataind+2])
            dataind += 3;
            checkbox.addEventListener('change', function() {
                toggleInputs(this);
            });
            if (checkbox.checked) {
                textInput.style.display = 'inline-block'; // Show text input if checkbox is checked
                timeInput.style.display = 'none'; // Hide time input initially
            } else {
                textInput.style.display = 'none'; // Hide text input initially
                timeInput.style.display = 'inline-block'; // Show time input if checkbox is unchecked
            }
            jcell ++;
        }
    }
}


// Return string (blank separated) of  Red, Green, Blue, Bright parms
function sendRGBBparms() {
  var RGBBparms = document.getElementById('RGBBparms');
  var inputs = RGBBparms.getElementsByTagName('input');
  var values = "";
  for (var i = 0; i < inputs.length; i++) {
    var input = inputs[i];
    //console.log(input.id + ":" + input.value);
    if (input.id) {
    values += input.value + " ";
    }
  }
  //console.log("Returned String " + values);
  return values;
}



// Send  table data back create a string in the format of exampleData
function sendTableData() {
    var table = document.getElementById('ModeTimesTable');
    var tbody = table.querySelector('tbody');
    var rows = tbody.getElementsByTagName('tr');

    var outputStr = "";
    for (var i = 0; i < rows.length; i++) {
        var cells = rows[i].getElementsByTagName('td');

        for (var j = 1; j < cells.length; j++) {
            var checkbox = cells[j].querySelector('input[type="checkbox"]');
            var timeInput = cells[j].querySelector('input[type="time"]');
            var textInput = cells[j].querySelector('input[type="text"]');

            var isChecked = checkbox.checked?1:0;
            var timeValue = timeInput.value;
            var textValue = textInput.value;

            outputStr += (isChecked + " " + timeValue + " " + textValue +",")
        }
    }

    console.log(outputStr)
    connection.send("J" + outputStr);

}
