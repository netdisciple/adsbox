var Graph = {};
var DataTimer;

function padNumber(n) {
	return (n < 10 ? "0" : "") + n;
}

function timeBetween(seconds) {
	var result = '';
	var s;
	days = (seconds - seconds % 86400) / 86400;
	if (days > 0) result = days + 'd ';
	s = seconds - days * 86400;
	hours = (s - s % 3600) / 3600;
	result += padNumber(hours) + ':';
	s = s - hours * 3600;
	min = (s - s % 60) / 60;
	result += padNumber(min) + ':';
	s = Math.round(s - min * 60);
	result += padNumber(s);
	return result;
}

function updateSources() {
	var table = document.getElementById("sources");
	var tableRows = table.getElementsByTagName("tr");

	query_db('SELECT S.ID, S.TXT, S.LAT, S.LON, S.PKTS, S.BYTES, ((JULIANDAY("NOW")-2440587.5)*86400), COUNT(F.ICAO), \
(SELECT MAX(R.DIST) FROM  RADAR_RANGE R WHERE R.ID=S.ID) M, HZ, NAME \
FROM V_SOURCES S LEFT JOIN FLIGHTS F ON S.ID=F.ID_SOURCE GROUP BY S.ID ORDER BY S.ID', function(data) {
        	var json_obj = JSON.parse(data);

		for (i in json_obj.sqlite_rows) {
			if (i > 0) {
				var source = json_obj.sqlite_rows[i];
				
				if (table.rows.namedItem(source['c0'])) {
					//console.log('update row');
					var row = table.rows.namedItem(source['c0']);
					var cells = row.cells;
					Graph[source['c0']].peak = 0;
					for (j=1; j<100; j++) { 
					   Graph[source['c0']].data[j-1] = Graph[source['c0']].data[j];
					   if (Graph[source['c0']].peak < Graph[source['c0']].data[j])
						Graph[source['c0']].peak = Graph[source['c0']].data[j];
					} 
					Graph[source['c0']].data[99] = Math.round((source['c4'] - Graph[source['c0']].last_data)/(source['c6'] - Graph[source['c0']].last_timestamp));
					Graph[source['c0']].last_data = source['c4'];
					Graph[source['c0']].last_timestamp = source['c6'];
					
					var html = source['c1'] + '<br>';
					if (source['c10'] != '')
						html = html +'Name:&nbsp;' + source['c10'] + '<br>';
					if (Number(source['c2']) && Number(source['c3']))
						html = html +'Lat:&nbsp;' + source['c2'] +'&nbsp;Lon:&nbsp;' + source['c3'] + '<br>' + 
							'Max. range&nbsp;' + Math.round(source['c8'], 1) + '&nbsp;km<br>';
					if (!isNaN(source['c9']))
						html = html +'Frequency:&nbsp;' + source['c9'] + '&nbsp;Hz<br>';
					html = html +'Packets:&nbsp;' + source['c4'] + '<br>';
					html = html +'Bytes:&nbsp;' + source['c5'] + '<br>';
					html = html +'Rate:&nbsp;' + Graph[source['c0']].data[99] + '&nbsp;pps<br>';
					html = html +'Peak:&nbsp;' + Graph[source['c0']].peak + '&nbsp;pps<br>';
					html = html +'Aircrafts:&nbsp;' + source['c7'] + '<br>';
					cells[0].innerHTML = html;
					
					var c = document.getElementById("graph" + source['c0']);
					var ctx = c.getContext("2d");
					ctx.lineWidth = 1;
					ctx.fillStyle = "#ffffff"; // graph background color
					ctx.fillRect(0,0,250,120); // clear
					
					// draw graph
					ctx.setLineDash([]);
					var max = Math.ceil((Graph[source['c0']].peak + 1) / 10) * 10;
					ctx.font = "12px Arial";
					ctx.textAlign = "right";
					ctx.fillStyle = "#000000"; // text color
					ctx.fillText(max, 45, 20);
					ctx.fillText(max/2, 45, 70);
					// draw graph
					ctx.beginPath();
					ctx.moveTo(50, 120);
					for (j=0; j<100; j++) {
					  ctx.lineTo(50 + 2*j, 120 - 100 * Graph[source['c0']].data[j] / max);
					}
					ctx.lineTo(248, 120);
					ctx.closePath();
					ctx.strokeStyle = "#000099"; // graph border color
					ctx.stroke();
					ctx.fillStyle = "#cccceb"; // graph fill color
					ctx.fill();
					
					// draw graph axis
					ctx.strokeStyle = "#000000";
					ctx.beginPath();
					ctx.moveTo(50, 0);
					ctx.lineTo(50, 119);
					ctx.lineTo(250, 119);
					ctx.stroke();
					ctx.closePath();
					
					// draw units
					ctx.setLineDash([5]);
					ctx.beginPath();
					for (var j=0; j<4; j++) {
					  ctx.moveTo(50, 20 + j*25);
					  ctx.lineTo(250, 20 + j*25);
					}
					ctx.stroke();
					ctx.closePath();
					
				} else {
					var row = table.tBodies[0].insertRow(1);
					row.id = source['c0'];
					var cell = row.insertCell(-1);
					//cell.style.width = '250px';
					var html = '<img align="right" src="img/loading9.gif">'
					cell.innerHTML = html;
					cell = row.insertCell(-1);
					html = '<canvas id="graph' + source['c0'] + '" width="250" height="120"></canvas>';
					cell.innerHTML = html;
					Graph[source['c0']] = {
					  data: new Array(),
					  last_data: source['c4'],
					  last_timestamp : source['c5'],
					  peak: 0
					}
					for (j=0; j<100; j++) {
					  Graph[source['c0']].data[j] = 0;
					}
				}
			}
		}
	});
}

function updateThreads() {
	var table = document.getElementById("threads");
	var tableRows = table.getElementsByTagName("tr");
	var active = {};

	query_db('SELECT CLOCKID, CPU_LOAD, DESCRIPTION, (JULIANDAY("NOW")-2440587.5)*86400 - TIME_START FROM THREADS', function(data) {
        	var json_obj = JSON.parse(data);

        	for (i in json_obj.sqlite_rows)
				if (i > 0) {
					var thread = json_obj.sqlite_rows[i];
					active[thread['c0']] = thread['c0'];
				
					if (table.rows.namedItem(thread['c0'])) {
						var row = table.rows.namedItem(thread['c0']);
						var cells = row.cells;
						cells[0].innerHTML = thread['c2'];
						if (!isNaN(thread['c1']))
							cells[1].innerHTML = Number(thread['c1']).toFixed(1) + "%";
						cells[2].innerHTML = timeBetween(thread['c3']);

					} else {
						var row = table.tBodies[0].insertRow(-1);
						row.id = thread['c0'];

						var cell = row.insertCell(-1);
						cell.innerHTML = thread['c2'];
						var cell = row.insertCell(-1);
						if (!isNaN(thread['c1']))
							cell.innerHTML = Number(thread['c1']).toFixed(1) + "%";
						else
							cell.innerHtml = "";
						var cell = row.insertCell(-1);
						cell.innerHTML = timeBetween(thread['c3']);
					}
				}
			
			// delete rows for non existing threads
			for (t=2; t<tableRows.length; t++)	{
				if (!(tableRows[t].id in active)) {
					table.deleteRow(t);
				}
			}
        });
}

function initializeData() {
	updateSources();
 	DataTimer = setInterval(function(){updateSources(); updateThreads();},3000);
}

function finalizeData() {
	clearInterval(DataTimer);
}
