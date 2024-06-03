function tab(e, id) {
  let tab = document.getElementsByClassName("tab");
  for (var i = 0; i < tab.length; i++)
    tab[i].style.display = "none";

  document.getElementById(id).style.display = "block";
  e.currentTarget.className += " active";
}

function netStats(){
	fetch(esp_url+"/wifi")
	.then(r => r.json())
	.then(js => {
		const div = document.getElementById('net');
		let sta = js.data.sta;

		let stats = `
			<small>
				Status: ${sta.status}<br>`;
		if(sta.connection)
			stats += `
				Network: ${sta.connection.ssid}<br>
				authmode: ${sta.connection.authmode}<br>
				RSSI: ${sta.connection.rssi} dB<br>`;

		stats +=`
			IP addr: ${sta.ip_info.ip}<br>
			netmask: ${sta.ip_info.netmask}<br>
			gateway: ${sta.ip_info.gw}
		</small>`;

		div.innerHTML = stats;
		return setTimeout(netStats, 2000);
	});
}

function getWiFiConfigForm(){
	fetch(esp_url+"/wifi?action=get_config")
	.then(r => r.json())
	.then(js => {
		const div = document.getElementById('wifi');

		let html = `
		<form id="wifi-form" method="post">
			SSID<br>
			<input type="text" name="ssid" value="${js.data.sta.ssid}" style="max-width:300px"><br>
			Password<br>
			<input type="password" name="passwd" style="max-width:300px"><br>
			<input type="submit" value="submit">
		</form>`;

		div.innerHTML = html;
		const form = document.getElementById('wifi-form');
		form.onsubmit = function(e){
			e.preventDefault();
			let data = decodeURIComponent(new URLSearchParams(new FormData(this)));

			fetch(esp_url+"/wifi?action=connect",
			{
				method: "POST",
				body: data
			})
			.then(r => r.json())
			.then(js => { alert("WiFi config changed"); });
		};
	})
}

function appInfo(){
	fetch(esp_url+"/info")
	.then(r => r.json())
	.then(js => {
		const div = document.getElementById('app');

		let info = `
			<h6>Firmware:</h6>
			<small>project: ${js.project_name}</small><br>
			<small>version: ${js.version}</small><br>
			<small>idf_ver: ${js.idf_ver}</small><br>
			<small>bulid date: ${js.date}</small><br>
			<small>bulid time: ${js.time}</small><br>`;

		div.innerHTML = info;
	});
}

function uploadFile(event){
	event.preventDefault();
	var xhttp = new XMLHttpRequest();
	var data = new FormData(event.target);

	xhttp.onload = function() {
		if(this.status == 200){
			var js = JSON.parse(this.responseText);
			alert((js.result==='ESP_OK')?('Update success:\n\n'+js.bytes_uploaded +'b uploaded'):('Update error'));
		} else {
			alert('Request Error');
		}
		event.target.style.display = 'block';
		loadbar.style.display = 'none';
	};

	xhttp.ontimeout = function(e) {
		info.innerHTML='Error: request timed out';
		event.target.style.display = 'block';
		loadbar.style.display = 'none';
	};

	xhttp.timeout = 120000;
	xhttp.open("POST",event.target.action, true);
	loadbar.style.display = 'block';
	event.target.style.display = 'none';
	xhttp.send(data);
}
