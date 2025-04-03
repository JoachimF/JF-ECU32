window.addEventListener("load", getReadings);
function getReadings() {
	var xhr = new XMLHttpRequest();
	xhr.onreadystatechange = function () {
		if (this.readyState == 4 && this.status == 200) 
		{
			var myObj = JSON.parse(this.responseText);
			//console.log(myObj);
			var status = myObj.status;
			var error = myObj.error;
            var time = myObj.time;
			document.getElementById('status').innerHTML = status;
			document.getElementById('error').innerHTML = error;
			document.getElementById('time').innerHTML = time;
		}
	};
	xhr.open("GET", "/c_readings", true);
	xhr.send();
}
setInterval(getReadings,500) ;