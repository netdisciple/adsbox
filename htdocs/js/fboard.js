var Header = ["ICAO","LAT","LON","ALT","FLIGHT","","SPEED","VSPEED","HEADING","SQUAWK","CAT","SEEN_TIME","SOURCE", "DF MASK"];
var df_mask_list = [];
var FBTimer;


function DF_mask(id, cell) {
	this.df_types = [0,4,5,11,16,17,18,20,21];
	this.df = {};
	this.id = id;

	for (type in this.df_types) {
		this.df[type] = document.createElement('span');
		this.df[type].innerHTML = this.df_types[type];
		this.df[type].style.margin = '1px';
		cell.appendChild(this.df[type]);
	}

	this.setMask = function(mask) {
		for (type in this.df_types) {
			if (mask & (1<<this.df_types[type])) {
				this.df[type].style.background = 'lightgreen';
			}
		}
	}
}

function initTable() {
	var table = document.getElementById("fboard");
	table.innerHTML = ''; // clear table first
	var header = table.createTHead();
	var body = table.createTBody();
	
	var row = header.insertRow(0);
	row.className = "table_head";
	for (var cells in Header) {
		th = document.createElement('th');
		th.innerHTML = Header[cells];
		row.appendChild(th);
	}
}

function updateTable() {
	var active = {};
	
	var table = document.getElementById("fboard");
	var tableRows = table.getElementsByTagName("tr");
	
	query_db('SELECT inttohex(F.ICAO), F.LAT, F.LON, F.ALT, F.CALLSIGN, CAST(ROUND(F.SPEED) AS INTEGER), \
		F.VSPEED, CAST(ROUND(F.HEADING) AS INTEGER), CASE WHEN F.SQUAWK IS NULL THEN "" ELSE inttooct(F.SQUAWK) END, \
		CAST(ROUND((julianday("now") - 2440587.5)*86400 - F.SEEN_TIME, 0) AS INTEGER), S.TXT, F.LATLON_INDIRECT, F.CAT, F.GND, \
		F.HEADING_INDIRECT, F.VSPEED_INDIRECT, F.SPEED_INDIRECT, F.DF_MASK \
		FROM FLIGHTS F LEFT JOIN (SELECT ID, TXT FROM V_SOURCES) S ON S.ID = F.ID_SOURCE ORDER BY F.APP_TIME DESC', function(data) {
			var json_obj = JSON.parse(data);

			for (i in json_obj.sqlite_rows) {
				if (i > 0) {
					var flight = json_obj.sqlite_rows[i];
					
					active[flight['c0']] = flight['c0'];
					
					if (table.rows.namedItem(flight['c0'])) {
						var row = table.rows.namedItem(flight['c0']);
						var cells = row.cells;
						if (flight['c11'] == 1) // latlon is indirect
							$(cells[1]).css('background-color', 'yellow');
						else    
							$(cells[1]).css('background-color', '');
						if (!isNaN(flight['c1']))
						    cells[1].innerHTML = Number(flight['c1']).toFixed(3)+'&deg;';
						
						if (flight['c11'] == 1) // latlon is indirect
							$(cells[2]).css('background-color', 'yellow');
						else    
							$(cells[2]).css('background-color', '');
						if (!isNaN(flight['c2']))
						    cells[2].innerHTML = Number(flight['c2']).toFixed(3)+'&deg;';
						
						if (!isNaN(flight['c3']))
						    cells[3].innerHTML = flight['c3'];
						cells[4].innerHTML = flight['c4'];
						if ((flight['c13'] == -1) && (cells[5].innerHTML.toString() == "<img src=\"img/onground.png\">")) {}
						else if ((flight['c13'] == -1) && (cells[5].innerHTML.toString() != "<img src=\"img/onground.png\">"))
							cells[5].innerHTML = "<img src=\"img/onground.png\">";
						else if ((flight['c6'] > 128) && (cells[5].innerHTML.toString() != "<img src=\"img/depart.png\">")) 
							cells[5].innerHTML = "<img src=\"img/depart.png\">";
						else if ((flight['c6'] < -128) && (cells[5].innerHTML.toString() != "<img src=\"img/arrive.png\">")) 
							cells[5].innerHTML = "<img src=\"img/arrive.png\">";
						else if ((Math.abs(flight['c6']) < 129) && (cells[5].innerHTML.toString() != "<img src=\"img/flight.png\">"))  
							cells[5].innerHTML = "<img src=\"img/flight.png\">";
						
						if (flight['c16'] == 1) // speed is indirect
						    $(cells[6]).css('background-color', 'yellow');
						else    
						    $(cells[6]).css('background-color', '');
						if (!isNaN(flight['c5']))
						    cells[6].innerHTML = flight['c5'];
						    
						if (flight['c15'] == 1) // vspeed is indirect
						    $(cells[7]).css('background-color', 'yellow');
						else    
						    $(cells[7]).css('background-color', '');
						if (!isNaN(flight['c6']))
						    cells[7].innerHTML = flight['c6']; //vspeed
						
						if (flight['c14'] == 1) // heading is indirect
						    $(cells[8]).css('background-color', 'yellow');
						else
						    $(cells[8]).css('background-color', '');
						if (!isNaN(flight['c7']))
						    cells[8].innerHTML = flight['c7'] + "&deg;"; // heading
					
					cells[9].innerHTML = flight['c8']; // squawk
					cells[10].innerHTML = flight['c12']; // category
					cells[11].innerHTML = flight['c9']; // seen time
					cells[12].innerHTML = flight['c10']; // source
					df_mask_list[flight['c0']].setMask(flight['c17']); // DF mask
				} else {
					var row = table.tBodies[0].insertRow(0);
					row.id = flight['c0'];
					var cell = row.insertCell(-1);
					cell.innerHTML = flight['c0'];
					var cell = row.insertCell(-1);
					if (!isNaN(flight['c1']))
					    cell.innerHTML = Number(flight['c1']).toFixed(3)+'&deg;';
					var cell = row.insertCell(-1);
					if (!isNaN(flight['c2']))
					    cell.innerHTML = Number(flight['c2']).toFixed(3)+'&deg;';
					var cell = row.insertCell(-1);
					if (!isNaN(flight['c3']))
					    cell.innerHTML = flight['c3'];
					var cell = row.insertCell(-1);
					cell.innerHTML = flight['c4'];
					var cell = row.insertCell(-1);
					if (flight['c13'] == -1) 
						cell.innerHTML = "<img src=\"img/onground.png\">"
					else if (flight['c6'] > 128) 
						cell.innerHTML = "<img src=\"img/depart.png\">"
					else if (flight['c6'] < -128) 
						cell.innerHTML = "<img src=\"img/arrive.png\">"
					else 
						cell.innerHTML = "<img src=\"img/flight.png\">"
					var cell = row.insertCell(-1);
					
					if (flight['c16'] == 1) // speed is indirect
					    $(cell).css('background-color', 'yellow');
					else    
					    $(cell).css('background-color', '');
					if (!isNaN(flight['c5']))
					    cell.innerHTML = flight['c5'];
					
					var cell = row.insertCell(-1);
					if (flight['c15'] == 1) // vspeed is indirect
					    $(cell).css('background-color', 'yellow');
					else    
					    $(cell).css('background-color', '');
					if (!isNaN(flight['c6']))
					    cell.innerHTML = flight['c6'];
					
					var cell = row.insertCell(-1);
					if (flight['c14'] == 1) // heading is indirect
					    $(cell).css('background-color', 'yellow');
					else
					    $(cell).css('background-color', '');
					if (!isNaN(flight['c7']))
					    cell.innerHTML = Math.round(flight['c7']) + "&deg;";
					
					var cell = row.insertCell(-1);
					cell.innerHTML = flight['c8'];
					var cell = row.insertCell(-1); // category
					cell.innerHTML = flight['c12'];
					var cell = row.insertCell(-1); // seen time
					cell.innerHTML = flight['c9'];
					var cell = row.insertCell(-1); // source
					cell.innerHTML = flight['c10'];
					var cell = row.insertCell(-1); // DF mask
					df_mask_list[flight['c0']] = new DF_mask(flight['c0'], cell);
					df_mask_list[flight['c0']].setMask(flight['c17']);
				}
			}
		}

		for (f=1; f<tableRows.length; f++)	{
			if (!(tableRows[f].id in active)) {
				delete df_mask_list[tableRows[f].id]; // delete
				table.deleteRow(f);
			}
		}
		if (tableRows.length > 0)
			setWidth();
	});
}

function setWidth() { 
	var headerColumns = $('#fboard thead tr:first-child th'); 
	var contentColumns = $('#fboard tbody tr:first-child td'); 
	for (i = 0; i < contentColumns.length; i++) { 
		cellwidth = ($(headerColumns[i]).width() > $(contentColumns[i]).width() ?
			 $(headerColumns[i]).width() : $(contentColumns[i]).width()); 
		$(headerColumns[i]).css('width', cellwidth); 
		$(contentColumns[i]).css('width', cellwidth);
	} 
}; 

function initializeFboard() {
	initTable();
	updateTable();
	FBTimer = setInterval(function(){updateTable()},3000);
}

function finalizeFboard() {
	clearInterval(FBTimer);
}