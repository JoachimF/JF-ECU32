// Get current sensor readings when the page loads
window.addEventListener("load", getReadings);

//<canvas data-type="linear-gauge" data-width="400" data-height="150" data-min-value="0" data-max-value="220" data-major-ticks="0,20,40,60,80,100,120,140,160,180,200,220" 
//data-minor-ticks="2" data-stroke-ticks="true" data-highlights="false" data-color-plate="#fff" data-border-shadow-width="0" data-borders="false" data-bar-begin-circle="false" 
//data-bar-width="10" data-tick-side="left" data-number-side="left" data-needle-side="left" data-needle-type="line" data-needle-width="3" data-color-needle="#222" data-color-needle-end="#222"
//data-animation-duration="1500" data-animation-rule="linear" data-animation-target="plate" width="400" height="150" style="width: 400px; height: 150px;"></canvas>

//Create Temperature Gauge
var gaugeRC = new LinearGauge({
    renderTo: "rc-gauge",
	units: "µS",
    width:400 ,
    height:100 ,
    minValue:1000 ,
    maxValue:2000,
    majorTicks: ["1000","1250","1500","1750","2000" ],
    minorTicks:2 ,
    ticksAngle: 180,
    strokTicks:true, 
    highlights: [
		{
			from: 1000,
			to: 1100,
			color: "rgba(0, 50, 200, .75)",
		},
    ],
    colorPlate:"#fff" ,
    borderShadowWidth:0 ,
    borders:false ,
    barBeginCircle:false ,
    barWidth:10 ,
    tickSide:"left", 
    numberSide:"left", 
    needleSide:"left" ,
    needleType:"line" ,
    needleWidth:3 ,
    colorNeedle:"#222", 
    colorNeedleend:"#222", 
    animationDuration:50, 
    animationRule:"linear" ,
    animationTarget:"plate"    
}).draw();

var gaugeRC_aux = new LinearGauge({
    renderTo: "rc-aux-gauge",
	units: "µS",
    width:400 ,
    height:100 ,
    minValue:1000 ,
    maxValue:2000,
    majorTicks: ["1000","1250","1500","1750","2000" ],
    minorTicks:2 ,
    ticksAngle: 180,
    strokTicks:true, 
    highlights: [
		{
			from: 1000,
			to: 1100,
			color: "rgba(0, 50, 200, .75)",
		},
    ],
    colorPlate:"#fff" ,
    borderShadowWidth:0 ,
    borders:false ,
    barBeginCircle:false ,
    barWidth:10 ,
    tickSide:"left", 
    numberSide:"left", 
    needleSide:"left" ,
    needleType:"line" ,
    needleWidth:3 ,
    colorNeedle:"#222", 
    colorNeedleend:"#222", 
    animationDuration:50, 
    animationRule:"linear" ,
    animationTarget:"plate"    
}).draw();

var gaugeEGT ;
// Create EGT Gauge
var gaugeEGTconf = {
	renderTo: "gauge-EGT",
	width: 300,
	height: 300,
	units: "EGT °C",
	minValue: 0,
	maxValue: 1000,
	colorValueBoxRect: "#049faa",
	colorValueBoxRectEnd: "#049faa",
	colorValueBoxBackground: "#f1fbfc",
    valueDec: 0,
	valueInt: 3,
	majorTicks: ["0", "125", "375","250", "500","625", "750","875", "1000"],
	minorTicks: 4,
	strokeTicks: true,
	highlights: [
		{
			from: 900,
			to: 1000,
			color: "rgba(200, 50, 50, .75)",
		},
	],
	colorPlate: "#fff",
	borderShadowWidth: 0,
	borders: false,
	needleType: "line",
	colorNeedle: "#007F80",
	colorNeedleEnd: "#007F80",
	needleWidth: 2,
	needleCircleSize: 3,
	colorNeedleCircleOuter: "#007F80",
	needleCircleOuter: true,
	needleCircleInner: false,
	animationDuration: 50,
	animationRule: "linear",
};

// Create RPM Gauge
var gaugeRPM 
var gaugeRPMconf = {
	renderTo: "gauge-RPM",
	width: 300,
	height: 300,
	units: "RPM x1000",
	minValue: 0,
	//maxValue: 200,
	colorValueBoxRect: "#049faa",
	colorValueBoxRectEnd: "#049faa",
	colorValueBoxBackground: "#f1fbfc",
    valueDec: 0,
	valueInt: 3,
	//majorTicks: ["0", "25", "50","75", "100","125", "150","175", "200"],
	minorTicks: 5,
	strokeTicks: true,
	highlights: [
		{
			from: 160,
			to: 200,
			color: "rgba(200, 50, 50, .75)",
		},
	],
	colorPlate: "#fff",
	borderShadowWidth: 0,
	borders: false,
	needleType: "line",
	colorNeedle: "#007F80",
	colorNeedleEnd: "#007F80",
	needleWidth: 2,
	needleCircleSize: 3,
	colorNeedleCircleOuter: "#007F80",
	needleCircleOuter: true,
	needleCircleInner: false,
	animationDuration: 50,
	animationRule: "linear",
};

// Function to get current readings on the webpage when it loads for the first time
function getReadings() {
	var xhr = new XMLHttpRequest();
	xhr.onreadystatechange = function () {
		if (this.readyState == 4 && this.status == 200) {
			var myObj = JSON.parse(this.responseText);
			console.log(myObj);
			var ppm_gaz = myObj.ppm_gaz;
			var ppm_aux = myObj.ppm_aux;
			var egt = myObj.egt;
			var rpm = myObj.rpm;
			var pump1 = myObj.pump1;
			var pump2 = myObj.pump2;
			var glow = myObj.glow;
			var vanne1 = myObj.vanne1;
			var vanne2 = myObj.vanne2;
			var status = myObj.status;
			var error = myObj.error;
			var time = myObj.time;
			gaugeRC.value = ppm_gaz;
			gaugeRC_aux.value = ppm_aux;
			gaugeEGT.value = egt;
			gaugeRPM.value = rpm;
			document.getElementById('pump1').innerHTML = pump1;
			document.getElementById('pump2').innerHTML = pump2;
			document.getElementById('vanne1').innerHTML = vanne1;
			document.getElementById('vanne2').innerHTML = vanne2;
			document.getElementById('glow').innerHTML = glow;
			document.getElementById('status').innerHTML = status;
			document.getElementById('error').innerHTML = error;
			document.getElementById('time').innerHTML = time;
		}
	};
	xhr.open("GET", "/readings", true);
	xhr.send();
}
setInterval(getReadings,200) ;

var Params = function() {
	var xhr = new XMLHttpRequest();
	xhr.onreadystatechange = function () {
		if (this.readyState == 4 && this.status == 200) {
			var myObj = JSON.parse(this.responseText);
			console.log(myObj);
			var egt_hli = myObj.egt_red;
			var hlitmp = [
				{
					from: egt_hli,
					to: 1000,
					color: "rgba(200, 50, 50, .75)",
				}
			] ;
			Object.assign(gaugeEGTconf,{highlights:hlitmp}) ;
			//gaugeEGT.highlights.from = egt_hli;
			gaugeEGT = new RadialGauge(gaugeEGTconf) ;
			gaugeEGT.draw() ;
			var rpm1 = myObj.rpm1;
			var rpm2 = myObj.rpm2;
			var rpm3 = myObj.rpm3;
			var rpm4 = myObj.rpm4;
			var rpm5 = myObj.rpm5;
			var rpm6 = myObj.rpm6;
			var rpm7 = myObj.rpm7;
			var rpm8 = myObj.rpm8;
			var rpm_red = myObj.rpm_red;
			var rpm_max = myObj.rpm_max;
			var majorticksconf = ["0",rpm1,rpm2,rpm3,rpm4,rpm5,rpm6,rpm7,rpm8];
			var hlitmp = [
				{
					from: rpm_red,
					to: rpm_max,
					color: "rgba(200, 50, 50, .75)",
				}
			] ;
			Object.assign(gaugeRPMconf,{highlights:hlitmp}) ;
			Object.assign(gaugeRPMconf,{maxValue:rpm_max}) ;
			Object.assign(gaugeRPMconf,{majorTicks:majorticksconf}) ;
			gaugeRPM = new RadialGauge(gaugeRPMconf) ;
			gaugeRPM.draw() ;
		}
	};
	xhr.open("GET", "/g_params", true);
	xhr.send();
}(); // <--- () causes self execution



/*
if (!!window.EventSource) {
	var source = new EventSource("/events");

	source.addEventListener(
		"open",
		function (e) {
			console.log("Events Connected");
		},
		false
	);

	source.addEventListener(
		"error",
		function (e) {
			if (e.target.readyState != EventSource.OPEN) {
				console.log("Events Disconnected");
			}
		},
		false
	);

	source.addEventListener(
		"message",
		function (e) {
			console.log("message", e.data);
		},
		false
	);

	source.addEventListener(
		"new_readings",
		function (e) {
			console.log("new_readings", e.data);
			var myObj = JSON.parse(e.data);
			console.log(myObj);
			gaugeTemp.value = myObj.temperature;
			gaugeHum.value = myObj.humidity;
            gaugeRC.value = temp ;
		},
		false
	);
}*/
