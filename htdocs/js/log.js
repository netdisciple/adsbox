var LastMsgDate = 0;

function toggleLog() {
	$("#log_content").animate({height: 'toggle'}, 200);
}

function getPopupLog() {
	query_db('SELECT DATETIME(DATE, "UNIXEPOCH", "LOCALTIME"), MSG, DATE, \
		(julianday("now") - 2440587.5)*86400 - DATE, ICAO, inttohex(ICAO) \
		FROM LOG WHERE DATE=(SELECT MAX(DATE) FROM LOG) AND \
		CAST (DATE AS INTEGER) > '+ LastMsgDate + ' ORDER BY DATE', function(data) {
			var json_obj = JSON.parse(data);
			for (i in json_obj.sqlite_rows) {
				var r = json_obj.sqlite_rows[i];
				if (i > 0) {
				    if (Number(r['c2']) > LastMsgDate) {
					LastMsgDate  = Number(r['c2']);
					var msg = r['c0'] + ' ' + r['c5'] + ': ' + r['c1'];
					var tableRef = document.getElementById('log_table').getElementsByTagName('tbody')[0];
					var newRow = tableRef.insertRow(0);
					var newCell = newRow.insertCell(0);
					newCell.innerHTML = msg;
					if (document.getElementById('log_table').rows.length == 6)
						document.getElementById('log_table').deleteRow(5); // rows in log table
				    }

				    for (m in Markers)
				    	if (r['c4'] == m)
				    		Markers[m].setOptions({msg: {txt: r['c1'], date: new Date().getTime()}});
				}
			}	
	});
}
