var ge;

function init() {
	google.earth.createInstance('gearth', initCB, failureCB);
}

function initializeGE() {
	if (!ge)
		google.load("earth", "1", {"callback":init});
}

function finalizeGE(){
}

function addKml(kmlUrl) {
	var link = ge.createLink('');
	link.setHref(kmlUrl);

	var networkLink = ge.createNetworkLink('');
	link.setRefreshMode(ge.REFRESH_ON_INTERVAL);
	link.setRefreshInterval(5);
	networkLink.set(link, true, false);

	ge.getFeatures().appendChild(networkLink);
}

function initCB(instance) {
	ge = instance;
	ge.getWindow().setVisibility(true);
	ge.getNavigationControl().setVisibility(ge.VISIBILITY_AUTO);
	ge.getLayerRoot().enableLayerById(ge.LAYER_BORDERS, true); // city names on
      
	var lookAt = ge.createLookAt('');
	var ge_center_lat = readCookie("ge_center_lat");
	var ge_center_lon = readCookie("ge_center_lon");
	var ge_tilt = readCookie("ge_tilt");
	var ge_range = readCookie("ge_range");
	var ge_hdg = readCookie("ge_hdg");
	var ge_alt = readCookie("ge_alt");
	  
	if (ge_center_lat && ge_center_lon && ge_tilt && ge_range && ge_hdg && ge_alt) {
		// lat, lon, alt, altmode, heading, tilt, range
		lookAt.set(parseFloat(ge_center_lat), parseFloat(ge_center_lon),
			parseFloat(ge_alt), ge.ALTITUDE_RELATIVE_TO_GROUND,
			parseFloat(ge_hdg),parseFloat(ge_tilt), parseFloat(ge_range));
	} else
		lookAt.set(0,0,16000000,ge.ALTITUDE_RELATIVE_TO_GROUND,0,0,0);
  
	ge.getView().setAbstractView(lookAt);

	google.earth.addEventListener(ge.getView(), "viewchangeend", viewChangeEndListener);
      
	addKml('http://' + window.location.host + '/adsbox.kml');
}

function failureCB(errorCode) {
	console.log('GE failure code ' + errCode);
}

function viewChangeEndListener() {
		var lookAt = ge.getView().copyAsLookAt(ge.ALTITUDE_RELATIVE_TO_GROUND);
		writeCookie("ge_center_lat", lookAt.getLatitude());
		writeCookie("ge_center_lon", lookAt.getLongitude());
		writeCookie("ge_tilt", lookAt.getTilt());
		writeCookie("ge_range", lookAt.getRange());
		writeCookie("ge_alt", lookAt.getAltitude());
		writeCookie("ge_hdg", lookAt.getHeading());
}