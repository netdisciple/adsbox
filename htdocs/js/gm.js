	Flight_list = {};
	Map = null;
	Track_ids = [];
	var Path = [];
	Selected_icao = null;
	var settings_ac_label_rows=[];
	settings_ac_label_rows['first_row'] = '1 row', 
	settings_ac_label_rows['second_row']= '2 row';
	settings_ac_label_rows['third_row'] = '3 row', 
	settings_ac_label_rows['fourth_row']= '4 row';
	settings_ac_label_rows['fifth_row']= '5 row';
	var Radars = {};
	var Markers = {};
	var Airports = {};
	var GMDataTimer;
	var UpdateMarkerTimer;
	var PlayerMode = 0; // history data playing mode

	var map_styles = [];
	map_styles['light'] = [{}];
	map_styles['gray'] =
	 [
		{
			"stylers":[
				{"visibility": "on"},
				{"saturation": -64},
				{"lightness": -15},
				{"gamma": 0.85}
			]
		}
	  ];
	map_styles['dark'] =
	 [
		{
			"stylers":[
				{"visibility": "on"},
				{"saturation": -82},
				{"lightness": -53},
				{"gamma": 0.88}
			]
		}
	  ];

	var measureMarkers = [];
	var measurePolyline;
	var measure_click_handler;
	var measureCoordinates = [];
	var HyperlineLeft = null;
	var HyperlineRight = null;

	function dist(begin, end) {
	return (6371 * Math.acos(Math.sin(begin.lat() * Math.PI / 180) * Math.sin(end.lat() * Math.PI / 180) + Math.cos(
			begin.lat() * Math.PI / 180) * Math.cos(end.lat() * Math.PI / 180) * Math.cos((begin.lng() - end.lng())
			* Math.PI / 180)));
	}

	function pathLength(path) {
		var result = 0;

		if (path.length == 0)
			return 0;
		for (p = 1; p < path.length; p++)
			result += dist(path[p], path[p-1]);
		
		return result;
	}

	function azimuth(begin, end) {
		var result;

		var lat1 = begin.lat() * Math.PI / 180;
		var lon1 = begin.lng() * Math.PI / 180;
		var lat2 = end.lat() * Math.PI / 180;
		var lon2 = end.lng() * Math.PI / 180;

		var b = Math.acos(Math.sin(lat2) * Math.sin(lat1) + Math.cos(lat2) * Math.cos(lat1) * Math.cos(lon2 - lon1));
		if (Math.sin(b) == 0) {
			if (lon1 < lon2)
				result = Math.PI;
			else
				result = 2 * Math.PI;
		} else {
			result = Math.asin(Math.cos(lat2) * Math.sin(lon2 - lon1) / Math.sin(b));

			if ((lat2 > lat1) && (lon2 > lon1)) {
			} else if ((lat2 < lat1) && (lon2 < lon1)) {
				result = Math.PI - result;
			} else if ((lat2 < lat1) && (lon2 > lon1)) {
				result = Math.PI - result;
			} else if ((lat2 > lat1) && (lon2 < lon1)) {
				result = result + 2 * Math.PI;
			} else if ((lon1 = lon2) && (lat1 > lat2)) {
				result = Math.PI;
			}
		
		}
		result = result * 180 / Math.PI;
		return result;
	}

	function latlon(lat1_deg, lon1_deg, d, a_deg) {
		var lat, lon;
		var lat1 = lat1_deg * Math.PI / 180.0;
		var lon1 = lon1_deg * Math.PI / 180.0;
		var a = a_deg * Math.PI / 180.0;
		
		lat = Math.asin(Math.sin(lat1) * Math.cos(d/6378.1) + Math.cos(lat1) * Math.sin(d/6378.1) * Math.cos(a));
		lon = lon1 + Math.atan2(Math.sin(a) * Math.sin(d/6378.1) * Math.cos(lat1),
			Math.cos(d/6378.1) - Math.sin(lat1) * Math.sin(lat));

		lat = lat * 180.0 / Math.PI;
		lon = lon * 180.0 / Math.PI;

		return new google.maps.LatLng(lat, lon);
	}

	function planeIcon(flight) {
		var iconColor;
		var scale;
		var path;
		var ac_path = 'm0.291118,-22.619192c-0.575451,0 -1.08749,0.234985 -1.519359,0.665649c-0.431809,0.42688 -0.788609, \
	1.040253 -1.085949,1.808105c-0.593941,1.531158 -0.95956,3.681854 -1.159203,6.261719c-0.199467,2.572479 -0.233219, \
	5.567566 -0.15033,8.775634c-7.108228,2.915528 -21.525589,8.934815 -22.683679,10.281281c-1.54183,1.787629 -1.043558, \
	3.841767 -0.422167,5.202515l23.701248,-4.824829c0.500931,6.289032 1.213131, \
	12.513184 1.833881,17.39212c-2.317822,0.67868 -6.64439,2.01828 -7.537341,2.848236c-1.23055, \
	1.142456 -1.23055,4.839844 -1.23055, 4.839844l9.700557,-0.790009c0.228252,1.595032 0.371323, \
	2.522459 0.371323,2.522459l0.01432,0.084106l0.076439,0l0.178528,0l0.076462,0l0.014381,-0.084106c0,0 0.14267, \
	-0.927427 0.371319,-2.522459l9.703541,0.790009c0,0 0,-3.697388 -1.2304,-4.839844c-0.893612,-0.829956 -5.223493,-2.172974 -7.540463, \
	-2.851593c0.619503,-4.865326 1.33041,-11.066406 1.831173,-17.331177l23.423596,4.767242c0.62133, \
	-1.360748 1.122322,-3.414886 -0.419777,-5.202515c-1.143545,-1.32962 -15.212379,-7.216278 -22.408552,-10.170349c0.08593, \
	-3.248626 0.054611,-6.285491 -0.14716,-8.886566l0,-0.005127c-0.200289,-2.576385 -0.566008,-4.725433 -1.159449,-6.256592c-0.297218, \
	-0.767853 -0.651359,-1.379547 -1.083069,-1.808105c-0.431911,-0.430664 -0.943932,-0.665649 -1.519322,-0.665649l0,0z';

		var qm_path = "m0.063941,-20.001297c-11.077144,0 -20.063636,8.975159 -20.063636,20.053055c0, \
	11.08847 8.986492,20.074203 20.063636,20.074203c11.083184,0 20.062121,-8.985733 20.062121,-20.074203c0, \
	-11.077896 -8.978937,-20.053055 -20.062121,-20.053055zm1.42977,31.217056c-0.466014,0.414658 -1.006052, \
	0.623116 -1.617838,0.623116c-0.632938,0 -1.185058,-0.204681 -1.656362,-0.614807c-0.472058,-0.40937 -0.708466, \
	-0.982635 -0.708466,-1.719048c0,-0.653336 0.228851,-1.203186 0.685053,-1.648804c0.456195, \
	-0.445625 1.015872,-0.668442 1.679774,-0.668442c0.653327,0 1.203179,0.222816 1.64956,0.668442c0.445621, \
	0.445618 0.669192,0.995468 0.669192,1.648804c-0.000755,0.725838 -0.234139,1.296082 -0.700914,1.710739zm5.810478, \
	-14.404968c-0.358008,0.663902 -0.783241,1.23642 -1.276448,1.719048c-0.491697,0.482635 -1.376145, \
	1.293823 -2.652588,2.434319c-0.35197,0.321754 -0.635205,0.604233 -0.847448,0.847435c-0.21223,0.243965 -0.370847, \
	0.466774 -0.474318,0.669189c-0.10423,0.202423 -0.184294,0.404839 -0.240939,0.607262c-0.05665, \
	0.20166 -0.141998,0.557404 -0.257558,1.06572c-0.196372,1.07856 -0.813448,1.617836 -1.850471,1.617836c-0.53928, \
	0 -0.992455,-0.17598 -1.361794,-0.528702c-0.367828,-0.352722 -0.551366,-0.876144 -0.551366,-1.571014c0,-0.87085 0.135198,-1.625389 \ 0.404839,-2.263618c0.268126,-0.638222 0.626894,-1.197891 1.072516,-1.680527c0.446379, \
	-0.482635 1.047595,-1.055145 1.805152,-1.719048c0.663909,-0.580818 1.143516,-1.01889 1.438841, \
	-1.314217c0.296074,-0.296074 0.544561,-0.625381 0.746224,-0.987923c0.203177, \
	-0.363297 0.302873,-0.756805 0.302873,-1.18203c0,-0.83007 -0.307402,-1.529472 -0.925234,-2.099724c-0.617071,-0.570244 -1.41315, \
	-0.855743 -2.388239,-0.855743c-1.141247,0 -1.981137,0.287766 -2.520417,0.863297c-0.539278, \
	0.575539 -0.994724,1.422981 -1.36859,2.543076c-0.353479,1.172218 -1.022672,1.758324 -2.006819, \
	1.758324c-0.580822,0 -1.071009,-0.204681 -1.470558,-0.614052c-0.398796,-0.40937 -0.598195, \
	-0.85273 -0.598195,-1.33007c0,-0.984909 0.316468,-1.983406 0.948652,-2.994743c0.632936, \
	-1.011337 1.555906,-1.848961 2.769667,-2.512108c1.212996,-0.663902 2.629175,-0.996231 4.24626, \
	-0.996231c1.503792,0 2.830847,0.277946 3.981911,0.833084c1.151063,0.55439 2.040807,1.308929 2.668451, \
	2.263618c0.626896,0.953941 0.941098,1.990959 0.941098,3.111061c0.001511,0.879913 -0.177494,1.65258 -0.535503,2.316483z";

		if (Map.getZoom() > 8)
			scale = 0.5;
		else
			scale = 0.33;

		if (flight['c6'] > 128) // VSPEED
			iconColor = "cyan";
		else if (flight['c6'] < -128)
			iconColor = "red";
		else
			iconColor = "yellow";

		if (isNaN(flight['c7']))
			path = qm_path;
		else
			path = ac_path;

		return {
			path: path, 
			scale: scale,
			rotation: Number(flight['c7']), // heading
			fillColor: iconColor,
			fillOpacity: 0.9,
			strokeColor: "brown",
			strokeWeight: 1
			};
	}

	function popupObject() {
		this.date = new Date().getTime();
		this.txt = '';
		this.lat = 0;
		this.lon = 0;
	}

	function createMarker(flight) {
	  var selected = flight['c0'];

	  var marker = new MarkerWithLabel({
	    position: new google.maps.LatLng(flight['c1'], flight['c2']), // lat, lon
		map: Map,
		icon: planeIcon(flight),
		labelContent: getLabelHTML(flight),
		labelAnchor: new google.maps.Point(-8,12),
		msg: new popupObject() // popup message object
		});
	  marker.setTitle(("000000" + Number(flight['c0']).toString(16)).substr(-6).toUpperCase());

	  // click on marker
	  google.maps.event.addListener(marker,
	   'click', function() {
		toggleInfo(selected);
	  });

	  return marker;
	}

function radarIcon() {
	return {
		path: "M -10 0 L 10 0 L 0 -20 L -10 0",
		scale: 0.5,
		rotation: 0,
		fillColor: "blue",
		fillOpacity: 1.0,
		strokeColor: "gray",
		strokeWeight: 1
	};
}

function createRadarMarker(location, title, radar) {
  var marker = new MarkerWithLabel({
    position: location,
	map: null,
	icon: radarIcon(),
	labelContent: title,
	labelAnchor: new google.maps.Point(-5,-5),
	labelClass: "labels",
	show_range: false
	});
  marker.setTitle(title);

  // click on marker
  google.maps.event.addListener(marker,
   'click', function() {
   		marker.show_range = !marker.show_range;

   		 if ((Radars[radar].marker.show_range) && (Radars[radar].marker.map != null)) { // show radar area
		    	showRadarArea(Radars[radar]);
		    } else { // hide radar area and ranges
		    	if (Radars[radar].areaPolygon)
		    		Radars[radar].areaPolygon.setMap(null);
		    }
	});

  return marker;
}

function createAirportMarker(location, title, name, icao) {
  var marker = new MarkerWithLabel({
    position: location,
	map: Map,
	icon: "/img/airport.svg",
	labelContent: title,
	labelAnchor: new google.maps.Point(-5,-5),
	labelClass: "labels",
	});
  marker.setTitle(name); // popup txt

  // click on marker
  google.maps.event.addListener(marker,
   'click', function() {
   		toggleAirportInfo(icao);
	});

  return marker;
}

function getData_() {
		var active = {};
		var bounds = Map.getBounds();
		var ne = bounds.getNorthEast();
		var sw = bounds.getSouthWest();
		var sql;

		if ((sw.lng() > 0) && (ne.lng() < 0)) // New Zeland fix
			sql = 'SELECT F.ICAO, F.LAT, F.LON, F.ALT, F.CALLSIGN, CAST(ROUND(F.SPEED) AS INTEGER) SPEED, F.VSPEED, \
	CAST(ROUND(F.HEADING) AS INTEGER) HEADING, CASE WHEN F.SQUAWK IS NULL THEN "" ELSE inttooct(F.SQUAWK) END SQUAWK, \
	CAST(ROUND((julianday("now") - 2440587.5)*86400 - F.SEEN_TIME, 0) AS INTEGER) SEEN_TIME, S.TXT, \
	F.VSPEED_INDIRECT, F.HEADING_INDIRECT, F.SPEED_INDIRECT, F.LATLON_INDIRECT, F.COUNTRY, IFNULL(F.AIRLINE, ""), IFNULL(F.AIRLINE_COUNTRY, "") \
	FROM FLIGHTS F LEFT JOIN (SELECT ID, TXT FROM V_SOURCES) S ON S.ID = F.ID_SOURCE \
	WHERE F.LAT BETWEEN ' + sw.lat() + ' AND ' + ne.lat() + ' AND (F.LON BETWEEN ' + sw.lng() + 
	' AND 180 OR F.LON BETWEEN -180 AND ' + ne.lng() + ')';
		else
			sql = 'SELECT F.ICAO, F.LAT, F.LON, F.ALT, F.CALLSIGN, CAST(ROUND(F.SPEED) AS INTEGER) SPEED, F.VSPEED, \
	CAST(ROUND(F.HEADING) AS INTEGER) HEADING, CASE WHEN F.SQUAWK IS NULL THEN "" ELSE inttooct(F.SQUAWK) END SQUAWK, \
	CAST(ROUND((julianday("now") - 2440587.5)*86400 - F.SEEN_TIME, 0) AS INTEGER) SEEN_TIME, S.TXT, \
	F.VSPEED_INDIRECT, F.HEADING_INDIRECT, F.SPEED_INDIRECT, F.LATLON_INDIRECT, F.COUNTRY, IFNULL(F.AIRLINE, ""), IFNULL(F.AIRLINE_COUNTRY, "") \
	FROM FLIGHTS F LEFT JOIN (SELECT ID, TXT FROM V_SOURCES) S ON S.ID = F.ID_SOURCE \
	WHERE F.LAT BETWEEN ' + sw.lat() + ' AND ' + ne.lat() + ' AND F.LON BETWEEN ' + sw.lng() + ' AND ' + ne.lng();

		query_db(sql, function(data){ 
			var json_obj = JSON.parse(data);

			if (json_obj.err != "SQLITE_OK")
				return;

			for (i in json_obj.sqlite_rows) {
				if (i > 0) {
					var flight = json_obj.sqlite_rows[i];
					
					active[flight['c0']] = 1;
							
					if (Flight_list[flight['c0']]) {
						Flight_list[flight['c0']] = flight;

						var marker_data = Object(Markers[flight['c0']]).msg;
						if ((flight['c1'] != Object(marker_data).lat) || (flight['c2'] != Object(marker_data).lon)) {
						Object(marker_data).lat = flight['c1'];
						Object(marker_data).lon = flight['c2'];
						Markers[flight['c0']].setPosition(
							new google.maps.LatLng(flight['c1'], flight['c2']));
						}
						Markers[flight['c0']].setIcon(planeIcon(flight));
						Markers[flight['c0']].labelContent = getLabelHTML(flight);
						if (readCookie('label_opacity') == 'true') {
							Markers[flight['c0']].setOptions({opacity: 1 - flight['c9'] / 60}) // opacity as last seen time ago
						} else {
							Markers[flight['c0']].setOptions({opacity: 1})
							Markers[flight['c0']].setOptions({labelStyle: {opacity: 0.75}});
						}
						Markers[flight['c0']].label.draw();
					} else if ((flight['c1']!=0) && (flight['c2']!=0)){
						Flight_list[flight['c0']] = flight;
						Markers[flight['c0']] = createMarker(flight);
					}
				}
			}

			for (f in Flight_list)	{
				if (!active[Flight_list[f]['c0']]) {
					Markers[Flight_list[f]['c0']].setMap(null);
					
					if (Flight_list[f]['c0'] == Selected_icao)
						toggleInfo(Selected_icao);
					
					delete Markers[Flight_list[f]['c0']]; // delete these inactive
					delete Flight_list[f];
				}
			}

			getFlightHTML(); // set info text
	 
	 	})

	 	// Draw flight track if selected
	 	if (Selected_icao) {
	 		query_db('SELECT LAT, LON, ALT, ROWID FROM TRACKS WHERE ICAO=' + 
				Selected_icao + ' AND ALT>0 ORDER BY SEEN_TIME', function(data) {
				var json_obj = JSON.parse(data);
				
				for (i=1; i<json_obj.sqlite_rows.length-1; i++) {
					var p = json_obj.sqlite_rows[i];
					var p1 = json_obj.sqlite_rows[i+1];
					if ((!Track_ids[p['c3']]) && (Selected_icao)) {
						Track_ids[p['c3']] = 1;
						Path.push(new google.maps.Polyline ({
							path: [new google.maps.LatLng(p['c0'], p['c1']), new google.maps.LatLng(p1['c0'], p1['c1'])],
							strokeColor: (p['c2']<50 ? Colors[0] : 
								(p['c2']<40000 ? Colors[Math.round(p['c2']/40000*255)]: Colors[255])),
							strokeOpacity: 1.0,
							strokeWeight: 2,
							map: Map
						}));
					}
				}
			});
	 	}

		// Draw hyper lines
		if (Selected_icao && (Flight_list[Selected_icao]['c14'] == 1) && (readCookie('show_hyperline')=='true')) {
			query_db('SELECT L.HLAT, L.HLON FROM LINES L WHERE L.ICAO=' + 
				Selected_icao +' AND L.SIDE=0', function(data) {
			var json_obj = JSON.parse(data);
				var Hyper = []
				for (i in json_obj.sqlite_rows) {
					if (i > 0) {
						var l = json_obj.sqlite_rows[i];
						Hyper.push(new google.maps.LatLng(l['c0'], l['c1']));
					}
				}
				HyperlineLeft.setPath(Hyper);
				HyperlineLeft.setMap(Map);
			});

			query_db('SELECT L.HLAT, L.HLON FROM LINES L WHERE L.ICAO=' + 
				Selected_icao +' AND L.SIDE=1', function(data) {
			var json_obj = JSON.parse(data);
				var Hyper = []
				for (i in json_obj.sqlite_rows) {
					if (i > 0) {
						var l = json_obj.sqlite_rows[i];
						Hyper.push(new google.maps.LatLng(l['c0'], l['c1']));
					}
				}
				HyperlineRight.setPath(Hyper);
				HyperlineRight.setMap(Map);
			});
		} else {
				HyperlineLeft.setMap(null);
				HyperlineRight.setMap(null);	
		}	
	}

	// draw aircraft markers on map
	/*function drawMarkers(data) {
		var json_obj = JSON.parse(data);

		for (i in json_obj.sqlite_rows) {
			if (i > 0) {
				var flight = json_obj.sqlite_rows[i];

				active[flight['c0']] = 1;

				if (Flight_list[flight['c0']]) {
					Flight_list[flight['c0']] = flight; // update info

					var marker_data = Object(Markers[flight['c0']]).msg;
					if ((flight['c1'] != Object(marker_data).lat) || (flight['c2'] != Object(marker_data).lon)) {
					Object(marker_data).lat = flight['c1'];
					Object(marker_data).lon = flight['c2'];
					Markers[flight['c0']].setPosition(
						new google.maps.LatLng(flight['c1'], flight['c2']));
					}
					Markers[flight['c0']].setIcon(planeIcon(flight));
					Markers[flight['c0']].labelContent = getLabelHTML(flight);
					if (readCookie('label_opacity') == 'true') {
						Markers[flight['c0']].setOptions({opacity: 1 - flight['c9'] / 60}) // opacity as last seen time ago
					} else {
						Markers[flight['c0']].setOptions({opacity: 1})
						Markers[flight['c0']].setOptions({labelStyle: {opacity: 0.75}});
					}
					Markers[flight['c0']].label.draw();
				} else if ((flight['c1']!=0) && (flight['c2']!=0)) { // create one
					Flight_list[flight['c0']] = flight;
					Markers[flight['c0']] = createMarker(flight);
				}
			}
		}

		for (f in Flight_list)	{
			if (!active[Flight_list[f]['c0']]) {
				Markers[Flight_list[f]['c0']].setMap(null);
					
				if (Flight_list[f]['c0'] == Selected_icao)
					toggleInfo(Selected_icao);
					
				delete Markers[Flight_list[f]['c0']]; // delete these inactive
				delete Flight_list[f];
			}
		}
	
		getFlightHTML(); // set info text
	}*/

	function getFlightPicture(icao) {
		var req = new XMLHttpRequest();
		var image_div = document.getElementById("flight_img");
		var author_div = document.getElementById("img_author");

		image_div.innerHTML = '';
		author_div.innerHTML = '';

		req.open('GET', 'proxy=http://www.airport-data.com/api/ac_thumb.json?m=' + Number(icao).toString(16).toUpperCase() + '&n=1', true);
		req.onreadystatechange = function() {
			if (req.readyState === 4) {
				if (req.status >= 200 && req.status < 400) {	
					var json_obj = JSON.parse(req.responseText);
					if (json_obj.status == 200) {
						image_div.innerHTML = '<img src=' + json_obj.data[0]['image'] + '></img>';
						author_div.innerHTML = '&copy;' + json_obj.data[0]['photographer'];
					}
				} else
					console.log('Error getting flight picture ' + req.status);
			}
		};
		req.send();
	}

	function hideTrack() {
		for (i=0; i<Path.length; i++) {
			Path[i].setMap(null);
		}
		while (Path.length > 0) Path.pop();
		while (Track_ids.length > 0) Track_ids.pop();
		// remove MLAT hyperlines if any
		if (HyperlineLeft != null) HyperlineLeft.setMap(null);
		if (HyperlineRight != null) HyperlineRight.setMap(null);
	}

	function hideInfo() {
		if ($("#info").is(":visible"))
			$("#info").animate({width: 'toggle'}, 300);
		Selected_icao = null;
		hideTrack();
	}

	// popup left side Info window
	function toggleInfo(icao) {
		if (!$("#info").is(":visible")) { 
			$("#info").animate({width: 'toggle'}, 300);
	  	}
		
		hideTrack();
		if (icao == Selected_icao) {
			hideInfo();
		} else {
			Selected_icao = icao;
			getFlightPicture(icao);
		}
		getFlightHTML();
	}
	
	// popup Settings window
	function toggleSettings() {
		var dialog = document.getElementById("overlay");

		if (dialog.style.visibility == "visible") {
			$("#overlay").animate({opacity: 'toggle'}, 300, function() {dialog.style.visibility="hidden"});
		} else {
			dialog.style.visibility="visible";
			$("#overlay").animate({opacity: 'toggle'}, 300);
		}
	}

	function toggleMeasure() {
		if (!document.getElementById('measure_btn').style.backgroundColor) {
			document.getElementById('measure_btn').style.backgroundColor = "#ffffff";

			for (m in Markers) // make aircraft, radar markers non clockable
				Markers[m].clickable = false;
			for (radar in Radars)
	    		Radars[radar].marker.clickable = false; 
			
			measureCoordinates = [];

			measure_click_handler = google.maps.event.addListener(Map, 'click', function(e) {

			measureCoordinates.push(e.latLng);

			if (!measurePolyline)
			measurePolyline = new google.maps.Polyline({
				map: Map,
				geodesic: true,
				strokeColor: "#ff0000"
			});

			measurePolyline.setPath(measureCoordinates);

	  		measureMarkers.push(new MarkerWithLabel({
	    		position: e.latLng,
			map: Map,
			icon: "img/climb.png",
			labelContent: (measureMarkers.length ? 'd=' + 
				Number(dist(measureCoordinates[measureMarkers.length-1], measureCoordinates[measureMarkers.length])).toFixed(3) + 'km<br>' +
				'A=' + Number(azimuth(measureCoordinates[measureMarkers.length-1], measureCoordinates[measureMarkers.length])).toFixed(0) + '&deg;<br>'
				: '') +
				(measureMarkers.length > 1 ? 'S=' + Number(pathLength(measureCoordinates)).toFixed(3) + 'km<br>': '') + 
				Number(e.latLng.lat()).toFixed(4) + '&deg;<br>' + Number(e.latLng.lng()).toFixed(4) + '&deg;',
			labelAnchor: new google.maps.Point(-2,-2),
			labelClass: "measure_marker"
			}));

			});
		} else {
			document.getElementById('measure_btn').style.backgroundColor = null;

			for (m in Markers) // aircraft, radar markers are clickable now
				Markers[m].clickable = true;
			for (radar in Radars)
	    		Radars[radar].marker.clickable = true;
			
			measurePolyline.setMap(null);
			measurePolyline = null;
			
			for (m in measureMarkers) {
				measureMarkers[m].setMap(null);
			}
			while (measureMarkers.length > 0) measureMarkers.pop();
			google.maps.event.removeListener(measure_click_handler);
		}
	}

	// fill flight data on Info window
	function getFlightHTML() {
	  if (Selected_icao) {
		if (Flight_list[Selected_icao]['c4'] == "") // no callsign
			document.getElementById("info_head_text").innerHTML =
				("000000" + Number(Flight_list[Selected_icao]['c0']).toString(16)).substr(-6).toUpperCase();
	    else
	      	document.getElementById("info_head_text").innerHTML = Flight_list[Selected_icao]['c4'];

	    document.getElementById("info_country").innerHTML = Flight_list[Selected_icao]['c15'];
	    if (Flight_list[Selected_icao]['c16']) {
	    	document.getElementById("info_airline_bar").style.display = 'table-row';
	    	document.getElementById("info_airline").innerHTML = Flight_list[Selected_icao]['c16'];
	    	document.getElementById("info_airline_country").innerHTML = Flight_list[Selected_icao]['c17'];
		} else
			document.getElementById("info_airline_bar").style.display = 'none';

		document.getElementById("info_icao").innerHTML =
			("000000" + Number(Flight_list[Selected_icao]['c0']).toString(16)).substr(-6).toUpperCase();
		document.getElementById("info_squawk").innerHTML = Flight_list[Selected_icao]['c8'];

		if (!isNaN(Flight_list[Selected_icao]['c3']))
			document.getElementById("info_alt").innerHTML = Flight_list[Selected_icao]['c3'] + '&nbsp;ft';
		else
			document.getElementById("info_alt").innerHTML = '';

		if (!isNaN(Flight_list[Selected_icao]['c11']))
			document.getElementById("info_vspeed").innerHTML = Flight_list[Selected_icao]['c6'] + '&nbsp;ft/min';
		else
			document.getElementById("info_vspeed").innerHTML = '';
		if (Flight_list[Selected_icao]['c11'] == 1) // vspeed is indirect
			document.getElementById("info_vspeed_cell").style.background = 'yellow';
		else
			document.getElementById("info_vspeed_cell").style.background = '';

		if (!isNaN(Flight_list[Selected_icao]['c5']))
			document.getElementById("info_speed").innerHTML = Flight_list[Selected_icao]['c5'] + '&nbsp;kts';
		else
			document.getElementById("info_speed").innerHTML = '';
		if (Flight_list[Selected_icao]['c13'] == 1) // speed is indirect
			document.getElementById("info_speed_cell").style.background = 'yellow';
		else
			document.getElementById("info_speed_cell").style.background = '';

		if (Flight_list[Selected_icao]['c12'] == 1) // heading is indirect
			document.getElementById("info_heading_cell").style.background = 'yellow';
		else
			document.getElementById("info_heading_cell").style.background = '';
		if (!isNaN(Flight_list[Selected_icao]['c7']))
			document.getElementById("info_heading").innerHTML = Flight_list[Selected_icao]['c7'] + '&deg;';
		else
			document.getElementById("info_heading").innerHTML = '';

		document.getElementById("info_source").innerHTML = Flight_list[Selected_icao]['c10'];
		
		if (Flight_list[Selected_icao]['c14'] == 1) {// latlon is indirect
			document.getElementById("info_lat_cell").style.background = 'yellow';
			document.getElementById("info_lon_cell").style.background = 'yellow';
		} else {
			document.getElementById("info_lat_cell").style.background = '';
			document.getElementById("info_lon_cell").style.background = '';
		}
		document.getElementById("info_lat").innerHTML = Number(Flight_list[Selected_icao]['c1']).toFixed(3) + '&deg;';
		document.getElementById("info_lon").innerHTML = Number(Flight_list[Selected_icao]['c2']).toFixed(3) + '&deg;';
	  }	
	}

	function getSettingsLabelsHTML() {
	  var HTML="";
	  HTML = '<span><label><input id="ac_label" type="checkbox" onChange="writeCookie(\'ac_label\', this.checked)">&nbsp;Aircraft labels</label></span><br>';
	  for (select in settings_ac_label_rows) {
	    HTML += '&nbsp;' + settings_ac_label_rows[select] + '<select id="' + select + '" onChange="writeCookie(\'' + select + '\',this.options[this.selectedIndex].value)">';
	    HTML += '<option value="none">None</option>';
	    HTML += '<option value="icao">IACO hex</option>';
	    HTML += '<option value="callsign">Callsign</option>';
	    HTML += '<option value="alt">Altitude</option>';
	    HTML += '<option value="gspeed">Gnd.speed</option>';
	    HTML += '<option value="vspeed">Vert.speed</option>';
	    HTML += '<option value="heading">Heading</option>';
	    HTML += '<option value="squawk">Squawk</option>';
	    HTML += '</select><br>';
	  }
	  return HTML;
	}

	function getLabelHTML(flight) {
	  var m = Markers[flight['c0']];
	  var msg = Object(m).msg;
	  var now = new Date();
	  var popup_opacity = 1 - (now.getTime() - Object(msg).date)/60000;
	  var label_opacity;
	
	if (readCookie('label_opacity') == 'true')
		label_opacity = 0.75 - 0.75 * flight['c9'] / 60;
	else
		label_opacity = 0.75;

	var label_background = (flight['c14']==1 ? 'yellow' : 'white'); // mark mlat labels as yellow

	var HTML = '<div id="label_container">';
	HTML += '<div class="popup_message" style="opacity:' + (isNaN(popup_opacity)?0:popup_opacity) +
		';visibility:' + (readCookie('show_events')=="true"?"visible":"hidden") + '">' + (Object(msg).txt?Object(msg).txt:'') +'</div><br>';
	  HTML += '<div id="label_content" style="opacity:' + label_opacity +'; background:' + label_background + ';">';
	  
	  if (readCookie('ac_label')=='true')
	    for (select in settings_ac_label_rows) {
	      if (document.getElementById(select).value=='icao')
		HTML += ("000000" + Number(flight['c0']).toString(16)).substr(-6).toUpperCase() + '<br>';
	      if ((document.getElementById(select).value=='callsign') && (flight['c4']!=''))
		HTML += flight['c4'] + '<br>';
	      if (document.getElementById(select).value=='alt')
		HTML += flight['c3'] + ' ft<br>';
	      if ((document.getElementById(select).value=='gspeed') && (!isNaN(flight['c5'])))
		HTML += flight['c5'] + ' kts<br>';
	      if ((document.getElementById(select).value=='vspeed') && (!isNaN(flight['c5'])))
		HTML += flight['c6'] + ' ft/min<br>';
	      if ((document.getElementById(select).value=='heading') && (!isNaN(flight['c7']))) // not null
		HTML += flight['c7'] + '&deg;<br>';
	      if ((document.getElementById(select).value=='squawk') && (flight['c8']!=''))
		HTML += flight['c8'] + '<br>';
	    }
	  HTML += '</div></div>';
	  return HTML;
	}

	function getRadars() {
		query_db('SELECT ID, TXT, LAT, LON, NAME FROM V_SOURCES WHERE LAT<>0 AND LON<>0', function(data){
			var json_obj = JSON.parse(data);
			var c = 0;
			
			for (i in json_obj.sqlite_rows) {
				var r = json_obj.sqlite_rows[i];
				c++;
				if (c > 1) {
					Radars[r['c0']] = { // radar id as key
					id: r['c0'],
					txt: r['c1'],
					location: new google.maps.LatLng(r['c2'], r['c3']),
					ranges: new Array(),
					path: new Array(),
					marker: createRadarMarker(new google.maps.LatLng(r['c2'], r['c3']), r['c4'], r['c0']),
					areaPolygon: new google.maps.Polygon({
						strokeColor: '#00FF00',
						strokeOpacity: 0.6,
						strokeWeight: 1,
						fillColor: '#00FF00',
						fillOpacity: 0.2,
						clickable: false
					})
					};
					// populate ranges array
					for (var i=0; i<5; i++) {
			          var rangeOptions = {
			          strokeColor: '#000000',
			          strokeOpacity: 0.6,
			          strokeWeight: 1,
			          fillColor: '#00FF00',
			          fillOpacity: 0.0,
			          map: null,
			          center: Radars[r['c0']].location,
			          radius: i * 100000,
			          clickable: false
			          }
			          Radars[r['c0']].ranges.push(new google.maps.Circle(rangeOptions));
			        }
				}
			}
			showRadars((readCookie('show_radar')=='true'));
		});
	}

	function showRadars(show) {
		for (var radar in Radars) {
			if (show) {
				Radars[radar].marker.setMap(Map);

			    if (readCookie('radar_ranges')=='true') {
			    	// show radar ranges
			    	for (var i=0; i<5; i++)
			          if (Radars[radar].ranges[i])
			            Radars[radar].ranges[i].setMap(Map);
			    } else { // turn off ranges and radar area
			    	for (var i=0; i<5; i++)
			          if (Radars[radar].ranges[i])
			            Radars[radar].ranges[i].setMap(null);
			        Radars[radar].areaPolygon.setMap(null);
			    }
			}
			else {
				Radars[radar].marker.setMap(null);
				for (var i=0; i<5; i++)
			        if (Radars[radar].ranges[i])
			            Radars[radar].ranges[i].setMap(null);
			    Radars[radar].areaPolygon.setMap(null);
			}
		} 
	}

	function showRadarRanges(show) {
		for (var radar in Radars) {
			if (show) {
				for (var i=0; i<5; i++)
			        if ((Radars[radar].ranges[i]) && (Radars[radar].marker.map != null))
			            Radars[radar].ranges[i].setMap(Map);
			} else {
				for (var i=0; i<5; i++)
			        if (Radars[radar].ranges[i])
			            Radars[radar].ranges[i].setMap(null);	
			}
		}
	}

	function showRadarArea(radar) {
		query_db('SELECT ID, LAT, LON FROM RADAR_RANGE WHERE LAT<>0 AND LON<>0 AND ID=' + radar.id, function(data) {
		    var json_obj = JSON.parse(data);
		    for (i in json_obj.sqlite_rows) {
		    	var r = json_obj.sqlite_rows[i];
		        if (i > 0) {
		        	radar.path[i-1] = new google.maps.LatLng(r['c1'],r['c2']);
		        }
			}
			radar.areaPolygon.setMap(Map);
			radar.areaPolygon.setPath(radar.path);
		});
	}

	// Get all airport markers in zoom area
	function getAirports(show) {
		if (show && (Map.getZoom() > 7)) {
			var bounds = Map.getBounds();
			var ne = bounds.getNorthEast();
			var sw = bounds.getSouthWest();
			var sql;

			if ((sw.lng() > 0) && (ne.lng() < 0)) // New Zeland fix
				sql = 'SELECT ID, IATA, LAT, LON, NAME, ICAO, CITY, COUNTRY FROM EXTERNAL.AIRPORTS WHERE LAT BETWEEN ' + 
			sw.lat() + ' AND ' + ne.lat() + ' AND (F.LON BETWEEN ' + sw.lng() + 
			' AND 180 OR LON BETWEEN -180 AND ' + ne.lng() + ')';
			else
				sql = 'SELECT ID, IATA, LAT, LON, NAME, ICAO, CITY, COUNTRY FROM EXTERNAL.AIRPORTS WHERE LAT BETWEEN ' + 
			sw.lat() + ' AND ' + ne.lat() + ' AND LON BETWEEN ' + sw.lng() + ' AND ' + ne.lng();

			query_db(sql, function(data){
				var json_obj = JSON.parse(data);
				var c = 0;

				for (a in Airports) { // clear airports list first
					Airports[a].marker.setMap(null);
				}
		  		while (Airports.length > 0) {
		  			Airports.pop();
		  		}
				
				for (i in json_obj.sqlite_rows) {
					var r = json_obj.sqlite_rows[i];
					c++;
					if (c > 1) {
						Airports[r['c5']] = { // airport icao as key
						id: r['c0'],
						icao: r['c5'],
						iata: r['c1'],
						name: r['c4'],
						city: r['c6'],
						country: r['c7'],
						location: new google.maps.LatLng(r['c2'], r['c3']),
						marker: createAirportMarker(new google.maps.LatLng(r['c2'], r['c3']), r['c1'], r['c4'], r['c5']),
						};
					}
				}
			});
		} else {
			for (a in Airports) { // clear airports
					Airports[a].marker.setMap(null);
				}
		  		while (Airports.length > 0) {
		  			Airports.pop();
		  		}
		}
	}

	// Get METAR info for selected airport
	function getMetar(icao) {
		var req = new XMLHttpRequest();
		// clear all entires
		document.getElementById("table_metar").style.display = 'none';
		
		req.open('GET', 'proxy=http://avwx.rest/api/metar.php?station=' + icao + '&format=JSON', true);
		req.onreadystatechange = function() {
			if (req.readyState === 4) {
				if (req.status >= 200 && req.status < 400) {	
					var json_obj = JSON.parse(req.responseText);
					if (!json_obj['Error']) {
					document.getElementById("metar_qnh").innerHTML = json_obj['Altimeter'] + '&nbsp;' + '&nbsp;' + json_obj['Units']['Altimeter'];
					document.getElementById("metar_visibility").innerHTML = json_obj['Visibility'] + '&nbsp;' + json_obj['Units']['Visibility'];
					document.getElementById("metar_temperature").innerHTML = json_obj['Temperature'] + '&deg;' + json_obj['Units']['Temperature'];
					document.getElementById("metar_dewpoint").innerHTML = json_obj['Dewpoint'] + '&deg;' + json_obj['Units']['Temperature'];
					document.getElementById("metar_wind_speed").innerHTML = json_obj['Wind-Speed'] + '&nbsp;' + json_obj['Units']['Wind-Speed'];
					document.getElementById("metar_wind_direction").innerHTML = json_obj['Wind-Direction'] + '&deg;';
					
					var wind_dir = document.getElementById("wind_direction_arrow").getAttribute("wind_direction");
					document.getElementById("wind_direction_arrow").setAttribute("transform", "rotate(" + (-1)*wind_dir + " 8 8)");
					wind_dir = Number(json_obj['Wind-Direction']) + 180;
					document.getElementById("wind_direction_arrow").setAttribute("transform", "rotate(" + wind_dir + " 8 8)");
					document.getElementById("wind_direction_arrow").setAttribute("wind_direction", wind_dir);

					document.getElementById("metar_raw_report").innerHTML = json_obj['Raw-Report'];
					document.getElementById("table_metar").style.display = 'block';
					}
				} else
					console.log('Error getting METAR info ' + req.status);
			}
		};
		req.send();
	}

	function toggleAirportInfo(icao) {
		var info = document.getElementById("airport");
		
		if (icao == 0) // hide immediate
			$("#airport").animate({opacity: 'toggle'}, 300, function() {info.style.visibility="hidden"});

		if (Airports[icao]) {
			document.getElementById("airport_head_text").innerHTML = Airports[icao].icao + ' airport';
			document.getElementById("info_airport_name").innerHTML = 
				Airports[icao].name + (Airports[icao].iata==''?'':' (' + Airports[icao].iata +')');
			document.getElementById("info_airport_city").innerHTML = Airports[icao].city;
			getMetar(icao);
		}

		if (info.style.visibility == "visible") {
			if (info.icao != icao) {
				info.icao = icao;
				return;
			}
			$("#airport").animate({opacity: 'toggle'}, 300, function() {info.style.visibility="hidden"});
		} else {
			info.style.visibility="visible";
			info.icao = icao;
			$("#airport").animate({opacity: 'toggle'}, 300);
			}
	}

	// smooth marker movement
	function updateMarker() {
		if (readCookie('smooth_mv')=='true') {
			for (f in Flight_list) {
				if ((Markers[Flight_list[f]['c0']]) && (!isNaN(Flight_list[f]['c5'])) && (!isNaN(Flight_list[f]['c7']))) {
					var d = 1.852 * Flight_list[f]['c5'] * 500 / 3600000; // 500 ms timer period
					latlng = Markers[Flight_list[f]['c0']].getPosition();
					Markers[Flight_list[f]['c0']].setPosition(latlon(latlng.lat(), latlng.lng(), d, Flight_list[f]['c7']));
				}				
			}
			if (Map.getZoom() > 8)
				UpdateMarkerTimer = setTimeout(function(){updateMarker();},500);
			else
				clearTimeout(UpdateMarkerTimer);
		} else
			clearTimeout(UpdateMarkerTimer);
	}

	function setColorScheme(name) {
		if (map_styles[name]) 
			Map.setOptions({styles: map_styles[name]});
		else
			console.log('No such color scheme - ' + name);
	}

	function zoomIn() {
		Map.setZoom(Map.getZoom() + 1);
	}

	function zoomOut() {
		Map.setZoom(Map.getZoom() - 1);
	}

	function initializeGmap() {
	  var map_center_lat = readCookie("map_center_lat");
	  var map_center_lon = readCookie("map_center_lon");
	  if (map_center_lat && map_center_lon ) 
	  	var latLng = new google.maps.LatLng(map_center_lat, map_center_lon);
	   else
	 	var latLng = new google.maps.LatLng("60", "30");
	 
	  var map_zoom = readCookie("map_zoom");
	  if (!map_zoom) map_zoom = 6;

	  var mapOptions = {
		zoom: parseInt(map_zoom),
		center: latLng,
		mapTypeId: google.maps.MapTypeId.ROADMAP,
		streetViewControl: false,
		disableDefaultUI: true
	  }

	  Map = new google.maps.Map(document.getElementById('mapCanvas'), mapOptions);

	  if (readCookie("color_scheme")) 
	  	Map.setOptions({styles: map_styles[readCookie("color_scheme")]});
	  else
	  	Map.setOptions({styles: map_styles['light']});

	  google.maps.event.addListener(Map, 'center_changed', function() {
		writeCookie("map_center_lat", Map.getCenter().lat());
		writeCookie("map_center_lon", Map.getCenter().lng());
	  });

	  google.maps.event.addListener(Map, 'zoom_changed', function() {
	  	writeCookie("map_zoom", Map.getZoom());
	  	clearTimeout(UpdateMarkerTimer);
		if (Map.getZoom() > 8)
			UpdateMarkerTimer = setTimeout(function(){updateMarker();},500);
		else
			clearTimeout(UpdateMarkerTimer);
	  });

	  // TODO massive function execute on map move
	  google.maps.event.addListener(Map, 'bounds_changed', function() {
		// draw airports on map
		if (readCookie('show_airports')=='true') 
	  		getAirports(true);
	  	else
	  		getAirports(false);
	  });

	  if (document.getElementById("settings_labels"))
	  	document.getElementById("settings_labels").innerHTML = getSettingsLabelsHTML();

	  //put radars on map
	  getRadars();
	  
	  var radio_color_scheme = document.getElementsByName("color_scheme");
	  for (var i = 0; i < radio_color_scheme.length; i++) {
		if (radio_color_scheme[i].value == readCookie("color_scheme"))
			radio_color_scheme[i].checked = true;
		else
			radio_color_scheme[i].checked = false;
	  }

	  for (select in settings_ac_label_rows) {
	    if (readCookie(select) != null)
	      document.getElementById(select).value=readCookie(select);
	  }
	  
	  document.getElementById('ac_label').checked=(readCookie('ac_label')=='true');
	  document.getElementById('label_opacity').checked=(readCookie('label_opacity')=='true');
	  document.getElementById('show_events').checked=(readCookie('show_events')=='true');
	  document.getElementById('smooth_mv').checked=(readCookie('smooth_mv')=='true');
	  document.getElementById('show_hyperline').checked=(readCookie('show_hyperline')=='true');
	  document.getElementById('show_airports').checked=(readCookie('show_airports')=='true');
	  document.getElementById('show_radar').checked=(readCookie('show_radar')=='true');
	  document.getElementById('radar_ranges').checked=(readCookie('radar_ranges')=='true');

	  HyperlineLeft = new google.maps.Polyline({
					strokeColor: "blue",
					strokeWeight: 1
				});
	  HyperlineRight = new google.maps.Polyline({
					strokeColor: "purple",
					strokeWeight: 1
				});
	  
	  // delete these markers possible on map
	  for (f in Flight_list) {
		Markers[Flight_list[f]['c0']].setMap(null);
		delete Markers[Flight_list[f]['c0']]; // delete these inactive
		delete Flight_list[f];	
	  }
	  
	  GMDataTimer = setInterval(function(){getData_();},3000);
	  
	  if (Map.getZoom() > 8)
			UpdateMarkerTimer = setTimeout(function(){updateMarker();},500);
		else
			clearTimeout(UpdateMarkerTimer);
	}

	function finalizeGmap() {
		clearInterval(GMDataTimer);
		clearInterval(UpdateMarkerTimer);
		hideInfo();
	}
