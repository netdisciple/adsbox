/*
 *	This is a part of adsbox - ADS-B decoder software
 * 	(c) atty 2011-15 romulmsg@mail.ru
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include "../sqlite3/sqlite3.h"
#include "misc.h"
#include "adsb.h"
#include "db.h"

#define CPR_EXPIRE 15 // seconds to odd/even CPR pair expire
#define AIRCRAFT_EXPIRE 60 // seconds to track expire
#define LOG_EXPIRE 600 // seconds to log expire
#define COUNTRIES 189 // countries assigned to adresses by ICAO

extern sqlite3 *db;

extern void azimuth(double *lat, double *lon, double *lat1, double *lon1,
		double *a);
extern double dist(double lat, double lon, double lat1, double lon1);

char *stmts[] =
		{
				"CREATE TABLE FLIGHTS (ICAO INTEGER PRIMARY KEY ASC, DF_MASK INTEGER DEFAULT 0, COUNTRY TEXT DEFAULT '', \
                    LAT0 INTEGER DEFAULT 0, LON0 INTEGER DEFAULT 0, LAT1 INTEGER DEFAULT 0, \
                    LON1 INTEGER DEFAULT 0, LAT REAL DEFAULT NULL, LON REAL DEFAULT NULL, \
		    LATLON_INDIRECT INTEGER DEFAULT NULL, LATLON_VALID INTEGER DEFAULT 0, \
                    LATLON_TIMESTAMP INTEGER DEFAULT ((julianday('now')-2440587.5)*86400), GND INTEGER DEFAULT NULL, \
                    ALT INTEGER DEFAULT NULL, ALT_TIMESTAMP INTEGER DEFAULT ((julianday('now')-2440587.5)*86400), \
                    CALLSIGN TEXT DEFAULT '', AIRLINE TEXT DEFAULT '', AIRLINE_COUNTRY TEXT DEFAULT '', CAT TEXT DEFAULT '', \
                    SPEED REAL DEFAULT NULL, SPEED_INDIRECT INTEGER DEFAULT NULL, \
                    VSPEED DEFAULT NULL, VSPEED_INDIRECT DEFAULT NULL, \
                    HEADING REAL DEFAULT NULL, HEADING_INDIRECT INTEGER DEFAULT NULL, HEADING_TIMESTAMP REAL DEFAULT NULL,\
                    SQUAWK INTEGER DEFAULT NULL, SEEN_TIME INTEGER DEFAULT ((julianday('now')-2440587.5)*86400), \
		    APP_TIME REAL DEFAULT ((julianday('now') - 2440587.5)*86400), \
		    PARITY INTEGER DEFAULT 0, ID_SOURCE DEFAULT 0, \
		    ALT_TYPE INTEGER DEFAULT NULL, TGT_ALT INTEGER DEFAULT NULL, BARO FLOAT DEFAULT NULL, \
		    TGT_HDG FLOAT DEFAULT NULL, AP INTEGER DEFAULT NULL, VNAV INTEGER DEFAULT NULL, \
		    AHOLD INTEGER DEFAULT NULL, APPR INTEGER DEFAULT NULL, TCAS INTEGER DEFAULT NULL)",
				"CREATE INDEX IX_FLIGHTS ON FLIGHTS (ICAO)",
				"CREATE INDEX IX_FLIGHTS_ID ON FLIGHTS (ID_SOURCE)",
				"CREATE TRIGGER ON_INSERT_FLIGHTS AFTER INSERT ON FLIGHTS BEGIN \
                    INSERT INTO LOG (ICAO, MSG) VALUES (NEW.ICAO, \"New contact with \" || inttohex(NEW.ICAO)); END",

				"CREATE TRIGGER ON_DELETE_FLIGHTS AFTER DELETE ON FLIGHTS BEGIN \
                    DELETE FROM TRACKS WHERE ICAO=OLD.ICAO; INSERT INTO LOG (ICAO, MSG) VALUES \
                    (OLD.ICAO, \"Lost contact with \" || inttohex(OLD.ICAO) || \" (\" || \
                    CAST((((julianday('now') - 2440587.5)*86400) - OLD.APP_TIME)/60 AS INTEGER) || \
                    \" min)\");END",

				"CREATE TRIGGER ON_UPDATE_FLIGHTS AFTER UPDATE ON FLIGHTS BEGIN \
                    UPDATE FLIGHTS SET SEEN_TIME = (julianday('now') - 2440587.5)*86400 \
		    WHERE ICAO=OLD.ICAO; END;",


				"CREATE TABLE RADAR_RANGE (ID INTEGER DEFAULT 0, AZIMUTH REAL DEFAULT 0, \
                    LAT DEFAULT 0, LON DEFAULT 0, DIST REAL DEFAULT 0)",
				"CREATE INDEX IX_RADAR_RANGE ON RADAR_RANGE (ID)",
				"CREATE TABLE TRACKS (ICAO INTEGER, \
                    LAT REAL DEFAULT 0, LON REAL DEFAULT 0, ALT INTEGER DEFAULT 0, \
                    SEEN_TIME INTEGER DEFAULT ((julianday('now') - 2440587.5)*86400) )",
				"CREATE INDEX IX_TRACKS ON TRACKS (ICAO)",

				"CREATE TABLE SOURCE_TCP(ID INTEGER PRIMARY KEY ASC, IP TEXT DEFAULT '', \
                    PORT INTEGER DEFAULT 30004, HZ INTEGER DEFAULT NULL, \
                    LAT REAL DEFAULT 0, LON REAL DEFAULT 0, PKTS INTEGER DEFAULT 0, BYTES INTEGER DEFAULT 0, NAME TXT DEFAULT '')",
				"CREATE INDEX IX_SOURCE_TCP ON SOURCE_TCP (ID)",
				"CREATE TABLE SOURCE_TCP_SBS3(ID INTEGER PRIMARY KEY ASC, IP TEXT DEFAULT '', \
				   PORT INTEGER DEFAULT 10001, \
				   LAT REAL DEFAULT 0, LON REAL DEFAULT 0, PKTS INTEGER DEFAULT 0, BYTES INTEGER DEFAULT 0, NAME TXT DEFAULT '')",
				"CREATE INDEX IX_SOURCE_TCP_SBS3 ON SOURCE_TCP_SBS3 (ID)",
				"CREATE TABLE SOURCE_TCP_BEAST(ID INTEGER PRIMARY KEY ASC, IP TEXT DEFAULT '', \
				    PORT INTEGER DEFAULT 10005, \
				    LAT REAL DEFAULT 0, LON REAL DEFAULT 0, PKTS INTEGER DEFAULT 0, BYTES INTEGER DEFAULT 0, NAME TXT DEFAULT '')",
				"CREATE INDEX IX_SOURCE_TCP_BEAST ON SOURCE_TCP_BEAST (ID)",
				"CREATE TABLE SOURCE_FILE(ID INTEGER PRIMARY KEY ASC, FILENAME TEXT DEFAULT '', \
		    PLAYRATE INTEGER DEFAULT 1, DELAY DEFAULT 0, \
                    LAT REAL DEFAULT 0, LON REAL DEFAULT 0, PKTS INTEGER DEFAULT 0, BYTES INTEGER DEFAULT 0, NAME TXT DEFAULT '')",
				"CREATE INDEX IX_SOURCE_FILE ON SOURCE_FILE (ID)",
				"CREATE TABLE SOURCE_SERIAL(ID INTEGER PRIMARY KEY ASC, DEVICE TEXT DEFAULT '', \
		    BAUD INTEGER DEFAULT 0, BITS DEFAULT 0, \
                    LAT REAL DEFAULT 0, LON REAL DEFAULT 0, HZ INTEGER DEFAULT NULL, \
		    PKTS INTEGER DEFAULT 0, BYTES INTEGER DEFAULT 0, NAME TXT DEFAULT '')",
				"CREATE INDEX IX_SOURCE_SERIAL ON SOURCE_SERIAL (ID)",
				"CREATE TABLE SOURCE_RTLSDR(ID INTEGER PRIMARY KEY ASC, DEVICE NUMBER DEFAULT 0, \
		    GAIN REAL DEFAULT -1, AGC INTEGER DEFAULT 0, \
                    LAT REAL DEFAULT 0, LON REAL DEFAULT 0, PKTS INTEGER DEFAULT 0, BYTES INTEGER DEFAULT 0, NAME TXT DEFAULT '')",
				"CREATE INDEX IX_SOURCE_RTLSDR ON SOURCE_RTLSDR (ID)",
				"CREATE TABLE SOURCE_UDP(ID INTEGER PRIMARY KEY ASC, PORT NUMBER DEFAULT 30005, \
					LAT REAL DEFAULT 0, LON REAL DEFAULT 0, PKTS INTEGER DEFAULT 0, BYTES INTEGER DEFAULT 0, NAME TXT DEFAULT '')",
				"CREATE INDEX IX_SOURCE_UDP ON SOURCE_UDP (ID)",

				"CREATE TABLE LOG(ICAO INTEGER DEFAULT NULL, MSG TXT, DATE REAL DEFAULT ((julianday('now') - 2440587.5)*86400))",
				"CREATE VIEW V_SOURCES AS \
		    SELECT ID, LAT, LON, PKTS, BYTES, NAME, NULL HZ, 'sbs-3: ' || IP || ':' || PORT TXT FROM SOURCE_TCP_SBS3 UNION \
		    SELECT ID, LAT, LON, PKTS, BYTES, NAME, HZ, 'serial: ' || DEVICE TXT FROM SOURCE_SERIAL UNION \
		    SELECT ID, LAT, LON, PKTS, BYTES, NAME, NULL HZ, 'file: ' || FILENAME TXT FROM SOURCE_FILE UNION \
		    SELECT ID, LAT, LON, PKTS, BYTES, NAME, HZ, 'tcp_avr: ' || IP || ':' || PORT TXT FROM SOURCE_TCP UNION \
		    SELECT ID, LAT, LON, PKTS, BYTES, NAME, NULL HZ, 'tcp_beast: ' || IP || ':' || PORT TXT FROM SOURCE_TCP_BEAST UNION \
		    SELECT ID, LAT, LON, PKTS, BYTES, NAME, NULL HZ, 'rtlsdr: device ' || DEVICE TXT FROM SOURCE_RTLSDR UNION \
		    SELECT ID, LAT, LON, PKTS, BYTES, NAME, NULL HZ, 'udp_avr: port ' || PORT TXT FROM SOURCE_UDP",
		    "PRAGMA SYNCHRONOUS = OFF", "PRAGMA JOURNAL_MODE = OFF", "PRAGMA AUTO_VACUUM = FULL",
		    "PRAGMA mmap_size=100000000",
		    "CREATE TABLE THREADS(TID INTEGER, CLOCKID INTEGER, TIME_START REAL \
		    	DEFAULT ((julianday('now') - 2440587.5)*86400), \
		    	TIMESTAMP REAL DEFAULT 0, TIMESTAMP_CPU REAL DEFAULT 0,\
		    	CPU_LOAD INTEGER, DESCRIPTION TXT)",
				NULL};
char *stmts_indirect_trg[] =
		{
		// indirect calc triggers
				"CREATE TRIGGER ON_UPDATE_FLIGHTS_HDG BEFORE UPDATE OF LAT ON FLIGHTS \
		    WHEN OLD.LAT IS NOT NULL AND \
		    (OLD.HEADING_INDIRECT=1 OR OLD.HEADING_INDIRECT IS NULL) BEGIN \
		    UPDATE FLIGHTS SET HEADING=AZIMUTH(OLD.LAT, OLD.LON, NEW.LAT, NEW.LON), \
		    HEADING_INDIRECT=1, HEADING_TIMESTAMP=(julianday('now') - 2440587.5)*86400 WHERE ICAO=OLD.ICAO; END;",

				"CREATE TRIGGER ON_UPDATE_FLIGHTS_SPEED AFTER UPDATE OF LAT ON FLIGHTS \
		    WHEN OLD.LAT IS NOT NULL \
		    AND DISTANCE(OLD.LAT, OLD.LON, NEW.LAT, NEW.LON) > 0.1 \
		    AND (OLD.SPEED_INDIRECT=1 OR OLD.SPEED_INDIRECT IS NULL) BEGIN \
		    UPDATE FLIGHTS SET SPEED=DISTANCE(OLD.LAT, OLD.LON, NEW.LAT, NEW.LON) / \
		    ((julianday('now') - 2440587.5)*86400 - OLD.LATLON_TIMESTAMP) * 3600 / 1.852, \
		    SPEED_INDIRECT=1 WHERE ICAO=OLD.ICAO; END;",

				"CREATE TRIGGER ON_UPDATE_FLIGHTS_VSPEED AFTER UPDATE OF ALT ON FLIGHTS \
		    WHEN OLD.ALT IS NOT NULL \
		    AND ((julianday('now') - 2440587.5)*86400) - OLD.ALT_TIMESTAMP > 5 \
		    AND (OLD.VSPEED_INDIRECT=1 OR OLD.VSPEED_INDIRECT IS NULL) BEGIN \
		    UPDATE FLIGHTS SET VSPEED= \
		    CAST(ROUND((NEW.ALT-OLD.ALT) * 60 / (((julianday('now') - 2440587.5)*86400) - OLD.ALT_TIMESTAMP)) AS INTEGER), \
		    ALT_TIMESTAMP=(julianday('now') - 2440587.5)*86400, \
		    VSPEED_INDIRECT=1 WHERE ICAO=OLD.ICAO; END;",
			NULL};
char *stmts_evt_trg[] =
		{
		// events triggers
		"CREATE TRIGGER ON_UPDATE_FLIGHTS_SQUAWK AFTER UPDATE OF SQUAWK ON FLIGHTS \
		    WHEN NEW.SQUAWK != OLD.SQUAWK AND OLD.SQUAWK IS NOT NULL BEGIN \
		    INSERT INTO LOG(ICAO, MSG) VALUES (NEW.ICAO, \"squawk \" || inttooct(NEW.SQUAWK)); END;",
				"CREATE TRIGGER ON_UPDATE_FLIGHTS_GND0 AFTER UPDATE OF GND ON FLIGHTS WHEN OLD.GND=0 AND  NEW.GND=-1 BEGIN \
		    INSERT INTO LOG(ICAO, MSG) VALUES (NEW.ICAO, \"landing now\"); END;",
				"CREATE TRIGGER ON_UPDATE_FLIGHTS_GND1 AFTER UPDATE OF GND ON FLIGHTS WHEN OLD.GND=-1 AND NEW.GND=0 BEGIN \
		    INSERT INTO LOG(ICAO, MSG) VALUES (NEW.ICAO, \"just airborn\"); END;",
				"CREATE TRIGGER ON_UPDATE_FLIGHTS_TGT_ALT AFTER UPDATE OF TGT_ALT ON FLIGHTS \
		    WHEN NEW.TGT_ALT != OLD.TGT_ALT AND OLD.TGT_ALT IS NOT NULL BEGIN \
		    INSERT INTO LOG(ICAO, MSG) VALUES (NEW.ICAO, \"maintain \" || NEW.TGT_ALT || \" ft\"); END;",
				"CREATE TRIGGER ON_UPDATE_FLIGHTS_AP0 AFTER UPDATE OF AP ON FLIGHTS \
		    WHEN OLD.AP=1 AND NEW.AP=0 BEGIN \
		    INSERT INTO LOG(ICAO, MSG) VALUES (NEW.ICAO, \"AP off\"); END;",
				"CREATE TRIGGER ON_UPDATE_FLIGHTS_AP1 AFTER UPDATE OF AP ON FLIGHTS \
		    WHEN OLD.AP=0 AND NEW.AP=1 BEGIN \
		    INSERT INTO LOG(ICAO, MSG) VALUES (NEW.ICAO, \"AP on\"); END;",
			NULL};

	
char *countries[COUNTRIES] = 
{"Afghanistan", "Albania", "Algeria", "Angola", "Antigua and Barbuda", "Argentina", "Armenia", 
"Australia", "Austria", "Azerbaijan", "Bahamas", "Bahrain", "Bangladesh", "Barbados", "Belarus", 
"Belgium", "Belize", "Benin", "Bhutan", "Bolivia", "Bosnia and Herzegovina", "Botswana", "Brazil", 
"Brunei Darussalam", "Bulgaria", "Burkina Faso", "Burundi", "Cambodia", "Cameroon", "Canada", 
"Cape Verde", "Central African Republic", "Chad", "Chile", "China", "Colombia", "Comoros", "Congo", 
"Cook Islands", "Costa Rica", "Cote d Ivoire", "Croatia", "Cuba", "Cyprus", "Czech Republic", 
"Democratic People's Republic of Korea", "Democratic Republic of the Congo", "Denmark", "Djibouti", 
"Dominican Republic", "Ecuador", "Egypt", "El Salvador", "Equatorial Guinea", "Eritrea", 
"Estonia", "Ethiopia", "Fiji", "Finland", "France", "Gabon", "Gambia", "Georgia", "Germany", 
"Ghana", "Greece", "Grenada", "Guatemala", "Guinea", "Guinea-Bissau", "Guyana", "Haiti", 
"Honduras", "Hungary", "Iceland", "India", "Indonesia", "Iran, Islamic Republic of", "Iraq", 
"Ireland", "Israel", "Italy", "Jamaica", "Japan", "Jordan", "Kazakhstan", "Kenya", "Kiribati", 
"Kuwait", "Kyrgyzstan", "Lao People's Democratic Republic", "Latvia", "Lebanon", "Lesotho", 
"Liberia", "Libyan Arab Jamahiriya", "Lithuania", "Luxembourg", "Madagascar", "Malawi", 
"Malaysia", "Maldives", "Mali", "Malta", "Marshall Islands", "Mauritania", "Mauritius", 
"Mexico", "Micronesia, Federated States of", "Monaco", "Mongolia", "Morocco", "Mozambique", 
"Myanmar", "Namibia", "Nauru", "Nepal", "Netherlands, Kingdom of the", "New Zealand", 
"Nicaragua", "Niger", "Nigeria", "Norway", "Oman", "Pakistan", "Palau", "Panama", 
"Papua New Guinea", "Paraguay", "Peru", "Philippines", "Poland", "Portugal", "Qatar", 
"Republic of Korea", "Republic of Moldova", "Romania", "Russian Federation", "Rwanda", 
"Saint Lucia", "Saint Vincent and the Grenadines", "Samoa", "San Marino", "Sao Tome and Principe", 
"Saudi Arabia", "Senegal", "Seychelles", "Sierra Leone", "Singapore", "Slovakia", "Slovenia", 
"Solomon Islands", "Somalia", "South Africa", "Spain", "Sri Lanka", "Sudan", "Suriname", 
"Swaziland", "Sweden", "Switzerland", "Syrian Arab Republic", "Tajikistan", "Thailand", 
"The former Yugoslav Republic of Macedonia", "Togo", "Tonga", "Trinidad and Tobago", "Tunisia", 
"Turkey", "Turkmenistan", "Uganda", "Ukraine", "United Arab Emirates", "United Kingdom", 
"United Republic of Tanzania", "United States", "Uruguay", "Uzbekistan", "Vanuatu", "Venezuela", 
"Viet Nam", "Yemen", "Zambia", "Zimbabwe", "Yugoslavia", "ICAO (1)", "ICAO (2)", "ICAO (2)"};

unsigned int country_addr[COUNTRIES][2] = {
{0xFFF000, 0x700000}, {0xFFFC00, 0x501000}, {0xFF8000, 0x0A0000}, {0xFFF000, 0x090000}, 
{0xFFFC00, 0x0CA000}, {0xFC0000, 0xE00000}, {0xFFFC00, 0x600000}, {0xFC0000, 0x7C0000}, 
{0xFF8000, 0x440000}, {0xFFFC00, 0x600800}, {0xFFF000, 0x0A8000}, {0xFFF000, 0x894000}, 
{0xFFF000, 0x702000}, {0xFFFC00, 0x0AA000}, {0xFFFC00, 0x510000}, {0xFF8000, 0x448000}, 
{0xFFFC00, 0x0AB000}, {0xFFFC00, 0x094000}, {0xFFFC00, 0x680000}, {0xFFF000, 0xE94000}, 
{0xFFFC00, 0x513000}, {0xFFFC00, 0x030000}, {0xFC0000, 0xE40000}, {0xFFFC00, 0x895000}, 
{0xFF8000, 0x450000}, {0xFFF000, 0x09C000}, {0xFFF000, 0x032000}, {0xFFF000, 0x70E000}, 
{0xFFF000, 0x034000}, {0xFC0000, 0xC00000}, {0xFFFC00, 0x096000}, {0xFFF000, 0x06C000}, 
{0xFFF000, 0x084000}, {0xFFF000, 0xE80000}, {0xFC0000, 0x780000}, {0xFFF000, 0x0AC000}, 
{0xFFFC00, 0x035000}, {0xFFF000, 0x036000}, {0xFFFC00, 0x901000}, {0xFFF000, 0x0AE000}, 
{0xFFF000, 0x038000}, {0xFFFC00, 0x501C00}, {0xFFF000, 0x0B0000}, {0xFFFC00, 0x4C8000}, 
{0xFF8000, 0x498000}, {0xFF8000, 0x720000}, {0xFFF000, 0x08C000}, {0xFF8000, 0x458000}, 
{0xFFFC00, 0x098000}, {0xFFF000, 0x0C4000}, {0xFFF000, 0xE84000}, {0xFF8000, 0x010000}, 
{0xFFF000, 0x0B2000}, {0xFFF000, 0x042000}, {0xFFFC00, 0x202000}, {0xFFFC00, 0x511000}, 
{0xFFF000, 0x040000}, {0xFFF000, 0xC88000}, {0xFF8000, 0x460000}, {0xFC0000, 0x380000}, 
{0xFFF000, 0x03E000}, {0xFFF000, 0x09A000}, {0xFFFC00, 0x514000}, {0xFC0000, 0x3C0000}, 
{0xFFF000, 0x044000}, {0xFF8000, 0x468000}, {0xFFFC00, 0x0CC000}, {0xFFF000, 0x0B4000}, 
{0xFFF000, 0x046000}, {0xFFFC00, 0x048000}, {0xFFF000, 0x0B6000}, {0xFFF000, 0x0B8000}, 
{0xFFF000, 0x0BA000}, {0xFF8000, 0x470000}, {0xFFF000, 0x4CC000}, {0xFC0000, 0x800000}, 
{0xFF8000, 0x8A0000}, {0xFF8000, 0x730000}, {0xFF8000, 0x728000}, {0xFFF000, 0x4CA000}, 
{0xFF8000, 0x738000}, {0xFC0000, 0x300000}, {0xFFF000, 0x0BE000}, {0xFC0000, 0x840000}, 
{0xFF8000, 0x740000}, {0xFFFC00, 0x683000}, {0xFFF000, 0x04C000}, {0xFFFC00, 0xC8E000}, 
{0xFFF000, 0x706000}, {0xFFFC00, 0x601000}, {0xFFF000, 0x708000}, {0xFFFC00, 0x502C00}, 
{0xFF8000, 0x748000}, {0xFFFC00, 0x04A000}, {0xFFF000, 0x050000}, {0xFF8000, 0x018000}, 
{0xFFFC00, 0x503C00}, {0xFFFC00, 0x4D0000}, {0xFFF000, 0x054000}, {0xFFF000, 0x058000}, 
{0xFF8000, 0x750000}, {0xFFFC00, 0x05A000}, {0xFFF000, 0x05C000}, {0xFFFC00, 0x4D2000}, 
{0xFFFC00, 0x900000}, {0xFFFC00, 0x05E000}, {0xFFFC00, 0x060000}, {0xFF8000, 0x0D0000}, 
{0xFFFC00, 0x681000}, {0xFFFC00, 0x4D4000}, {0xFFFC00, 0x682000}, {0xFF8000, 0x020000}, 
{0xFFF000, 0x006000}, {0xFFF000, 0x704000}, {0xFFFC00, 0x201000}, {0xFFFC00, 0xC8A000}, 
{0xFFF000, 0x70A000}, {0xFF8000, 0x480000}, {0xFF8000, 0xC80000}, {0xFFF000, 0x0C0000}, 
{0xFFF000, 0x062000}, {0xFFF000, 0x064000}, {0xFF8000, 0x478000}, {0xFFFC00, 0x70C000}, 
{0xFF8000, 0x760000}, {0xFFFC00, 0x684000}, {0xFFF000, 0x0C2000}, {0xFFF000, 0x898000}, 
{0xFFF000, 0xE88000}, {0xFFF000, 0xE8C000}, {0xFF8000, 0x758000}, {0xFF8000, 0x488000}, 
{0xFF8000, 0x490000}, {0xFFFC00, 0x06A000}, {0xFF8000, 0x718000}, {0xFFFC00, 0x504C00}, 
{0xFF8000, 0x4A0000}, {0xF00000, 0x100000}, {0xFFF000, 0x06E000}, {0xFFFC00, 0xC8C000}, 
{0xFFFC00, 0x0BC000}, {0xFFFC00, 0x902000}, {0xFFFC00, 0x500000}, {0xFFFC00, 0x09E000}, 
{0xFF8000, 0x710000}, {0xFFF000, 0x070000}, {0xFFFC00, 0x074000}, {0xFFFC00, 0x076000}, 
{0xFF8000, 0x768000}, {0xFFFC00, 0x505C00}, {0xFFFC00, 0x506C00}, {0xFFFC00, 0x897000}, 
{0xFFF000, 0x078000}, {0xFF8000, 0x008000}, {0xFC0000, 0x340000}, {0xFF8000, 0x770000}, 
{0xFFF000, 0x07C000}, {0xFFF000, 0x0C8000}, {0xFFFC00, 0x07A000}, {0xFF8000, 0x4A8000}, 
{0xFF8000, 0x4B0000}, {0xFF8000, 0x778000}, {0xFFFC00, 0x515000}, {0xFF8000, 0x880000}, 
{0xFFFC00, 0x512000}, {0xFFF000, 0x088000}, {0xFFFC00, 0xC8D000}, {0xFFF000, 0x0C6000}, 
{0xFF8000, 0x028000}, {0xFF8000, 0x4B8000}, {0xFFFC00, 0x601800}, {0xFFF000, 0x068000}, 
{0xFF8000, 0x508000}, {0xFFF000, 0x896000}, {0xFC0000, 0x400000}, {0xFFF000, 0x080000}, 
{0xF00000, 0xA00000}, {0xFFF000, 0xE90000}, {0xFFFC00, 0x507C00}, {0xFFFC00, 0xC90000}, 
{0xFF8000, 0x0D8000}, {0xFF8000, 0x888000}, {0xFFF000, 0x890000}, {0xFFF000, 0x08A000}, 
{0xFFFC00, 0x004000}, {0xFF8000, 0x4C0000}, {0xFF8000, 0xF00000}, {0xFFFC00, 0x899000}, 
{0xFFFC00, 0xF09000}};

int db_delete_expired() {
	/*
	 *  Delete expired flight
	 */
	if(db_exec_sql("DELETE FROM FLIGHTS \
	    WHERE (julianday('now') - 2440587.5)*86400 - SEEN_TIME > %i",
			AIRCRAFT_EXPIRE)) return 1;
	return 0;
}

int db_delete_expired_log() {
	if(db_exec_sql("DELETE FROM LOG \
            WHERE (julianday('now') - 2440587.5)*86400 - DATE > %i",
			LOG_EXPIRE)) return 1;
	return 0;
}

// DF11 only
int db_update_icao(unsigned int *icao, adsb_data *data) {
	if (db_exec_sql("UPDATE FLIGHTS SET ICAO=%i, ID_SOURCE=%i, DF_MASK=DF_MASK|(1<<11) WHERE ICAO=%i",
			*icao, data->source_id, *icao)) return 1;
	if (!sqlite3_changes(db)) {
		if (db_exec_sql("INSERT INTO FLIGHTS (ICAO, DF_MASK, COUNTRY, ID_SOURCE) VALUES (%i, 1<<11,  country(%i), %i)",
				*icao, *icao, data->source_id)) return 1;
	}
	return 0;
}

int db_update_position(unsigned int *icao, double *lat, double *lon,
		adsb_data *data) {
	double a, d;

	// Create a phantom aircraft for mlat accuracy test
	//if (icao_hex == 0x424288)
	//		icao_hex = icao_hex & 0x00ffff;

	/*
	 *  Update record in flights table
	 */
	if (db_exec_sql("UPDATE FLIGHTS SET LAT=%f, LON=%f, ID_SOURCE=%i, LATLON_INDIRECT=0, \
LATLON_TIMESTAMP=(julianday('now') - 2440587.5)*86400 WHERE ICAO=%i",
			*lat, *lon, data->source_id, *icao)) return 1;

	/*
	 * If nothing was updated create the record
	 */
	if (!sqlite3_changes(db)) {
		if (db_exec_sql("INSERT INTO FLIGHTS (ICAO, COUNTRY, LAT, LON, ID_SOURCE, LATLON_INDIRECT, LATLON_TIMESTAMP) \
VALUES (%i,country(%i),%f,%f,%i,0,(julianday('now') - 2440587.5)*86400)",
			*icao, *icao, *lat, *lon, data->source_id)) return 1;
	}
	/*
	 * Record the track point
	 */
	if (db_exec_sql("INSERT INTO TRACKS (ICAO, LAT, LON, ALT) \
    	    SELECT %i, %f, %f, ALT FROM FLIGHTS \
    	    WHERE FLIGHTS.ICAO=%i AND FLIGHTS.ALT>=0",
			*icao, *lat, *lon, *icao)) return 1;
	/*
	 *  Update radar coverage area
	 */
	if ((data->lat != 0) && (data->lon != 0)) {
		azimuth(&data->lat, &data->lon, lat, lon, &a);
		d = dist(data->lat, data->lon, *lat, *lon);
		if (db_exec_sql("UPDATE RADAR_RANGE SET \
        	    LAT=%f, LON=%f, DIST=%f WHERE ID=%u AND ABS(AZIMUTH - %f) < 1 AND DIST<%f",
				*lat, *lon, d, data->source_id, a, d)) return 1;
	}
	return 0;
}

int db_update_alt(unsigned int *icao, int *alt, int *gnd, adsb_data *data, unsigned char *df) {
	//altitude is in feets units
	if (gnd) {
		if (db_exec_sql("UPDATE FLIGHTS SET ALT=%i, GND=%i, ID_SOURCE=%i, DF_MASK=DF_MASK|(1<<%i) WHERE ICAO=%i",
				*alt, *gnd, data->source_id, *df, *icao)) return -1;
	} else {
		if (db_exec_sql("UPDATE FLIGHTS SET ALT=%i, ID_SOURCE=%i, DF_MASK=DF_MASK|(1<<%i) WHERE ICAO=%i",
				*alt, data->source_id, *df, *icao)) return -1;
	}
	return sqlite3_changes(db);
}

int db_update_speed_heading(unsigned int *icao, float *speed, float *heading,
		int *vr, adsb_data * data, unsigned char * df) {

	if (db_exec_sql("UPDATE FLIGHTS SET SPEED=%f, SPEED_INDIRECT=0, VSPEED=%i, VSPEED_INDIRECT=0, \
			HEADING_TIMESTAMP=(julianday('now')-2440587.5)*86400, \
			HEADING=%f, HEADING_INDIRECT=0, ID_SOURCE=%i, DF_MASK=DF_MASK|(1<<%i) WHERE ICAO=%i",
			*speed, *vr, *heading, data->source_id, *df, *icao)) return 1;

	if (!sqlite3_changes(db)) {
		if (db_exec_sql("INSERT INTO FLIGHTS (ICAO, DF_MASK, COUNTRY, SPEED, SPEED_INDIRECT, VSPEED, VSPEED_INDIRECT, \
			HEADING, HEADING_INDIRECT, ID_SOURCE, HEADING_TIMESTAMP) \
            VALUES (%i,(1<<%i),country(%i),%f,0,%i,0,%f,0,%i,(julianday('now')-2440587.5)*86400)",
				*icao, *df, *icao, *speed, *vr, *heading, data->source_id)) return 1;
	}
	return 0;
}

int db_update_squawk(unsigned int *icao, unsigned int *squawk, int *gnd,
		adsb_data *data, unsigned char *df) {

	if (gnd) {
		if (db_exec_sql("UPDATE FLIGHTS SET SQUAWK=%i, GND=%i, ID_SOURCE=%i, DF_MASK=DF_MASK|(1<<%i) WHERE ICAO=%i",
				*squawk, *gnd, data->source_id, *df, *icao)) return -1;
	} else {
		if (db_exec_sql("UPDATE FLIGHTS SET SQUAWK=%i, ID_SOURCE=%i, DF_MASK=DF_MASK|(1<<%i) WHERE ICAO=%i",
				*squawk, data->source_id, *df, *icao)) return -1;
	}
	return sqlite3_changes(db);
}

int db_update_callsign(unsigned int *icao, char *callsign, char *cat,
		adsb_data *data, unsigned char *df) {

	if (db_exec_sql("UPDATE FLIGHTS SET CALLSIGN=TRIM('%s'), CAT='%s', ID_SOURCE=%i, \
AIRLINE=(SELECT A.NAME FROM EXTERNAL.AIRLINES A WHERE INSTR(\'%s\',A.ICAO)=1), \
AIRLINE_COUNTRY=(SELECT A.COUNTRY FROM EXTERNAL.AIRLINES A WHERE INSTR(\'%s\',A.ICAO)=1), \
DF_MASK=DF_MASK|(1<<%i) WHERE ICAO=%i",
			(char *) callsign, (char *) cat, data->source_id, (char *) callsign, (char *) callsign, *df, *icao))
		return 1;

	if (!sqlite3_changes(db)) {
		if (db_exec_sql("INSERT INTO FLIGHTS (ICAO, DF_MASK, COUNTRY, CALLSIGN, CAT, ID_SOURCE) \
            VALUES (%i,(1<<%i),country(%i),TRIM('%s'),'%s',%i)",
				*icao,*df,*icao, (char *) callsign, (char*) cat, data->source_id)) return 1;
	}
	return 0;
}

cpr_info db_update_cpr(unsigned int *icao, unsigned char *f, int *lat, int *lon,
		int *id_source) {
	int rc;
	char sql[256];
	sqlite3_stmt *stmt;
	cpr_info ci;
	long int seen_time;
	time_t t = time(NULL);

	// TODO Make CPR table for each data source.

	ci.lat0 = 0;
	ci.lon0 = 0;
	ci.lat1 = 0;
	ci.lon1 = 0;
	ci.parity = 0;
	ci.lat = 0;
	ci.lon = 0;
	ci.id_source = 0;
	seen_time = t;

	sprintf(sql,
			"SELECT LAT0, LON0, LAT1, LON1, PARITY, SEEN_TIME, LAT, LON, ID_SOURCE \
        FROM FLIGHTS WHERE ICAO=%i",
			*icao);
	rc = sqlite3_prepare(db, sql, -1, &stmt, 0);
	if (rc != SQLITE_OK) {
		DEBUG("SQLite error: could not prepare.");
		sqlite3_finalize(stmt);
		return ci;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ci.lat0 = sqlite3_column_int(stmt, 0);
		ci.lon0 = sqlite3_column_int(stmt, 1);
		ci.lat1 = sqlite3_column_int(stmt, 2);
		ci.lon1 = sqlite3_column_int(stmt, 3);
		ci.parity = sqlite3_column_int(stmt, 4);
		seen_time = sqlite3_column_int(stmt, 5);
		ci.lat = sqlite3_column_double(stmt, 6);
		ci.lon = sqlite3_column_double(stmt, 7);
		ci.id_source = sqlite3_column_int(stmt, 8);
	}

	if (*f == 1) {
		ci.lat1 = *lat;
		ci.lon1 = *lon;
		if ((t - seen_time > CPR_EXPIRE) || (ci.id_source != *id_source)) {
			ci.lat0 = 0;
			ci.lon0 = 0;
		}
	} else {
		ci.lat0 = *lat;
		ci.lon0 = *lon;
		if ((t - seen_time > CPR_EXPIRE) || (ci.id_source != *id_source)) {
			ci.lat1 = 0;
			ci.lon1 = 0;
		}
	}

	if (db_exec_sql("UPDATE FLIGHTS SET LAT0=%i, LON0=%i, LAT1=%i, LON1=%i, \
    	    PARITY=%i, id_source=%i WHERE ICAO=%i",
			ci.lat0, ci.lon0, ci.lat1, ci.lon1, *f, *id_source, *icao)) return ci;

	if (!sqlite3_changes(db)) {
		if (db_exec_sql("INSERT INTO FLIGHTS (ICAO, COUNTRY, LAT0, LON0, LAT1, LON1, PARITY, ID_SOURCE) VALUES \
            (%i, country(%i), %i, %i, %i, %i, %i, %i)",
			*icao, *icao, ci.lat0, ci.lon0, ci.lat1, ci.lon1, *f, *id_source)) return ci;
	}

	sqlite3_finalize(stmt);
	return ci;
}

int db_source_name(char *name, unsigned int *id) {
	if (db_exec_sql(
			"UPDATE SOURCE_TCP_SBS3 SET NAME=\"%s\" WHERE ID=%i; \
			UPDATE SOURCE_SERIAL SET NAME=\"%s\" WHERE ID=%i; \
			UPDATE SOURCE_FILE SET NAME=\"%s\" WHERE ID=%i; \
			UPDATE SOURCE_TCP SET NAME=\"%s\" WHERE ID=%i; \
			UPDATE SOURCE_TCP_BEAST SET NAME=\"%s\" WHERE ID=%i; \
			UPDATE SOURCE_RTLSDR SET NAME=\"%s\" WHERE ID=%i; \
			UPDATE SOURCE_UDP SET NAME=\"%s\" WHERE ID=%i;",
			name, *id,
			name, *id,
			name, *id,
			name, *id,
			name, *id,
			name, *id,
			name, *id)) return -1;
	return sqlite3_changes(db);
}

int db_source_lat(double lat, unsigned int *id) {
	if (db_exec_sql(
			"UPDATE SOURCE_TCP_SBS3 SET LAT=%f WHERE ID=%i; \
			UPDATE SOURCE_SERIAL SET LAT=%f WHERE ID=%i; \
			UPDATE SOURCE_FILE SET LAT=%f WHERE ID=%i; \
			UPDATE SOURCE_TCP SET LAT=%f WHERE ID=%i; \
			UPDATE SOURCE_TCP_BEAST SET LAT=%f WHERE ID=%i; \
			UPDATE SOURCE_RTLSDR SET LAT=%f WHERE ID=%i; \
			UPDATE SOURCE_UDP SET LAT=%f WHERE ID=%i;",
			lat, *id,
			lat, *id,
			lat, *id,
			lat, *id,
			lat, *id,
			lat, *id,
			lat, *id)) return -1;   
	return sqlite3_changes(db);
}

int db_source_lon(double lon, unsigned int *id) {
	if (db_exec_sql(
			"UPDATE SOURCE_TCP_SBS3 SET LON=%f WHERE ID=%i; \
			UPDATE SOURCE_SERIAL SET LON=%f WHERE ID=%i; \
			UPDATE SOURCE_FILE SET LON=%f WHERE ID=%i; \
			UPDATE SOURCE_TCP SET LON=%f WHERE ID=%i; \
			UPDATE SOURCE_TCP_BEAST SET LON=%f WHERE ID=%i; \
			UPDATE SOURCE_RTLSDR SET LON=%f WHERE ID=%i; \
			UPDATE SOURCE_UDP SET LON=%f WHERE ID=%i;",
			lon, *id,
			lon, *id,
			lon, *id,
			lon, *id,
			lon, *id,
			lon, *id,
			lon, *id)) return -1;   
	return sqlite3_changes(db);
}

int db_update_source_stat(adsb_data *data) {

	db_exec_sql(
			"UPDATE SOURCE_TCP_SBS3 SET PKTS=PKTS+1, BYTES=%i WHERE ID=%i; \
			UPDATE SOURCE_SERIAL SET PKTS=PKTS+1, BYTES=%i WHERE ID=%i; \
			UPDATE SOURCE_FILE SET PKTS=PKTS+1, BYTES=%i WHERE ID=%i; \
			UPDATE SOURCE_TCP SET PKTS=PKTS+1, BYTES=%i WHERE ID=%i; \
			UPDATE SOURCE_TCP_BEAST SET PKTS=PKTS+1, BYTES=%i WHERE ID=%i; \
			UPDATE SOURCE_RTLSDR SET PKTS=PKTS+1, BYTES=%i WHERE ID=%i; \
			UPDATE SOURCE_UDP SET PKTS=PKTS+1, BYTES=%i WHERE ID=%i;",
			data->source_bytes, data->source_id, data->source_bytes,
			data->source_id, data->source_bytes, data->source_id,
			data->source_bytes, data->source_id, data->source_bytes,
			data->source_id, data->source_bytes, data->source_id,
			data->source_bytes, data->source_id);

	return sqlite3_changes(db);
}

/*
 Target state and status infornation
 */
int db_update_status(unsigned int *icao, unsigned char *alt_type,
		uint16_t *tgt_alt, float *baro, float *tgt_hdg, unsigned char *ap,
		unsigned char *vnav, unsigned char *alt_hold, unsigned char *app_mode,
		unsigned char *tcas, adsb_data * data, unsigned char * df) {

	if (tgt_hdg) {
		if (db_exec_sql("UPDATE FLIGHTS SET ALT_TYPE=%i, TGT_ALT=%i, \
BARO=(CASE WHEN 0=%1.0f THEN NULL ELSE (%1.0f - 1) * 0.8 + 800 END), \
TGT_HDG=%f, AP=%i, VNAV=%i, AHOLD=%i, APPR=%i, TCAS=%i, ID_SOURCE=%i, \
DF_MASK=DF_MASK|(1<<%i) WHERE ICAO=%i",
				*alt_type, *tgt_alt, *baro, *baro, *tgt_hdg, *ap, *vnav,
				*alt_hold, *app_mode, *tcas, data->source_id, *df, *icao)) return -1;
	} else {
		if (db_exec_sql("UPDATE FLIGHTS SET ALT_TYPE=%i, TGT_ALT=%i, \
BARO=(CASE WHEN 0=%1.0f THEN NULL ELSE (%1.0f - 1) * 0.8 + 800 END), \
AP=%i, VNAV=%i, AHOLD=%i, APPR=%i, TCAS=%i, ID_SOURCE=%i, \
DF_MASK=DF_MASK|(1<<%i) WHERE ICAO=%i",
				*alt_type, *tgt_alt, *baro, *baro, *ap, *vnav, *alt_hold,
				*app_mode, *tcas, data->source_id, *df, *icao)) return -1;
	}
	return sqlite3_changes(db);
}

int db_update_hz(int id_source, unsigned int hz) {
	/*
	*  Update HZ in sources
	*/
	if (db_exec_sql("BEGIN; UPDATE SOURCE_TCP SET HZ=%u WHERE ID=%i; \
			UPDATE SOURCE_SERIAL SET HZ=%u WHERE ID=%i;END;",
			hz, id_source, hz, id_source)) return 1;
	return 0;
}

int db_insert_thread(unsigned int cid, char *desc) {

	if (db_exec_sql("INSERT INTO THREADS (CLOCKID, DESCRIPTION) VALUES ( \
	    %u, \"%s\")", cid, desc)) return 1;
	return 0;
}

int db_delete_thread(unsigned int cid) {

	if (db_exec_sql("DELETE FROM THREADS WHERE CLOCKID = %u", cid)) return 1 ;
	return 0;
}

int db_stat_thread() {
	int rc;
	char sql[256] = "SELECT CLOCKID, TIMESTAMP, TIMESTAMP_CPU FROM THREADS";
	sqlite3_stmt *stmt;
	double time, time_cpu, cpu_load;
	struct timespec ts, ts_cpu;
	
	rc = sqlite3_prepare(db, sql, -1, &stmt, 0);
	if (rc != SQLITE_OK) {
		DEBUG("SQLite error: cant prepare %s\n %s\n", sql, sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 1;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		clock_gettime(CLOCK_REALTIME, &ts);
		clock_gettime((clockid_t)sqlite3_column_int(stmt, 0), &ts_cpu);

		time = ts.tv_sec + ts.tv_nsec / 1000000000.;
		time_cpu = ts_cpu.tv_sec + ts_cpu.tv_nsec / 1000000000.;
		cpu_load = (time_cpu - sqlite3_column_double(stmt, 2)) * 100 / (time - sqlite3_column_double(stmt, 1));

		if (db_exec_sql("UPDATE THREADS SET CPU_LOAD = %f, TIMESTAMP=%f, TIMESTAMP_CPU=%f WHERE CLOCKID=%u",
			cpu_load, time, time_cpu, sqlite3_column_int(stmt, 0))) return 1;

	}
	return 0;
}

int init_radar_range() {
	int rc;
	char sql[] = "SELECT ID, LAT, LON FROM V_SOURCES";
	sqlite3_stmt *stmt;
	int i;

	/*
	 *  Fill with initial values
	 */

	rc = sqlite3_prepare(db, sql, -1, &stmt, 0);
	if (rc != SQLITE_OK) {
		DEBUG("SQLite error: cant prepare %s\n %s\n", sql, sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 1;
	}
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		for (i = 0; i < 360; i = i + 2) {
			if (db_exec_sql("INSERT INTO RADAR_RANGE(ID, AZIMUTH, LAT, LON) VALUES(%i, %i, %f, %f)",
					sqlite3_column_int(stmt, 0), i,
					sqlite3_column_double(stmt, 1),
					sqlite3_column_double(stmt, 2))) return 1;
		}
	}

	sqlite3_finalize(stmt);
	return 0;
}

int DEBUG_DB(char * format, ...) {
	char s[256];
	va_list args;

	va_start(args, format);
	vsnprintf(s, sizeof(s), format, args);
	va_end(args);
	db_exec_sql("INSERT INTO LOG (MSG) VALUES (\"%s\")", s);

	return 0;
}

int db_exec_sql(char * format, ...) {
	int rc;
	char *zErrMsg = 0;
	char *sql = malloc(512);
	va_list args;

	va_start(args, format);
	vsnprintf(sql, 512, format, args);
	va_end(args);

	rc = sqlite3_exec(db, sql, NULL, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		DEBUG("\ndb_exec_sql() error in\n%s\n%s\n", sql, zErrMsg);
		sqlite3_free(zErrMsg);
		return 1;
	}
	free(sql);
	return 0;
}

/*
 *	Twosome below are for compatibility only. Newer SQLite supports printf() function.
 */
static void f_inttohex(sqlite3_context *context, int argc, sqlite3_value **argv) {
	if (argc == 1) {
		int * t = (int *) sqlite3_value_int(argv[0]);
		char result[6];
		sprintf(result, "%06X", (unsigned int) t);
		sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
		return;
	}
	sqlite3_result_null(context);
}

static void f_inttooct(sqlite3_context *context, int argc, sqlite3_value **argv) {
	if (argc == 1) {
		int * t = (int *) sqlite3_value_int(argv[0]);
		char result[6];
		sprintf(result, "%04o", (unsigned int) t);
		sqlite3_result_text(context, result, -1, SQLITE_TRANSIENT);
		return;
	}
	sqlite3_result_null(context);
}

static void f_azimuth(sqlite3_context *context, int argc, sqlite3_value **argv) {
	if (argc == 4) {
		double lat1 = sqlite3_value_double(argv[0]);
		double lon1 = sqlite3_value_double(argv[1]);
		double lat2 = sqlite3_value_double(argv[2]);
		double lon2 = sqlite3_value_double(argv[3]);
		//double a;
		double a = azimuth2(lat1, lon1, lat2, lon2);
		sqlite3_result_double(context, a);
		return;
	}
	sqlite3_result_null(context);
}

static void f_distance(sqlite3_context *context, int argc, sqlite3_value **argv) {
	if (argc == 4) {
		double lat1 = sqlite3_value_double(argv[0]);
		double lon1 = sqlite3_value_double(argv[1]);
		double lat2 = sqlite3_value_double(argv[2]);
		double lon2 = sqlite3_value_double(argv[3]);
		double d;
		d = dist(lat1, lon1, lat2, lon2);
		sqlite3_result_double(context, d);
		return;
	}
	sqlite3_result_null(context);
}

// Get country by assigned ICAO address
static void f_country(sqlite3_context *context, int argc, sqlite3_value **argv) {
	int i;
	if (argc == 1) {
		unsigned int t = sqlite3_value_int(argv[0]);
		
		for (i = 0; i < COUNTRIES; i++)
			if ((t & country_addr[i][0]) == country_addr[i][1]) {
				sqlite3_result_text(context, countries[i], -1, SQLITE_TRANSIENT);
				return;
			}
	}
	sqlite3_result_null(context);
}

int db_exec(char **stmts, sqlite3 *db) {
	int rc;
	char *zErrMsg = 0;
	char **stmt = stmts;

	while (*stmt) {
		rc = sqlite3_exec(db, *stmt, NULL, 0, &zErrMsg);
		if (rc != SQLITE_OK) {
			DEBUG("SQLite error db_exec(): %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			return 1;
		}
		stmt++;
	}

	return 0;
}

int db_evt_trg() {
	if (db_exec(stmts_evt_trg, db)) return 1;
	return 0;
}

int db_indirect_trg() {
	if (db_exec(stmts_indirect_trg, db)) return 1;
	return 0;
}

int db_init(sqlite3 *db) {

	DEBUG("Init database... ");
	if (db_exec(stmts, db)) return 1;

	sqlite3_create_function(db, "inttohex", 1, SQLITE_UTF8, NULL, &f_inttohex,
	NULL, NULL);
	sqlite3_create_function(db, "inttooct", 1, SQLITE_UTF8, NULL, &f_inttooct,
	NULL, NULL);
	sqlite3_create_function(db, "azimuth", 4, SQLITE_UTF8, NULL, &f_azimuth,
	NULL, NULL);
	sqlite3_create_function(db, "distance", 4, SQLITE_UTF8, NULL, &f_distance,
	NULL, NULL);
	sqlite3_create_function(db, "country", 1, SQLITE_UTF8, NULL, &f_country,
	NULL, NULL);
	DEBUG("done.\n");

	return 0;
}

int db_attach(char * file, char * schema) {

	if (db_exec_sql("ATTACH DATABASE '%s' AS %s",
		file, schema)) return 1;
	else
		DEBUG("%s database attached.\n", file);

	return 0;
}
