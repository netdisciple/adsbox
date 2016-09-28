/*
 *	This is a part of adsbox - ADS-B decoder software
 * 	(c) atty 2011-15 romulmsg@mail.ru
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <zlib.h>
#include "http.h"
#include "misc.h"
#include "db.h"

#define REQBUFLEN 2048
#define KEEPALIVE_TIMEOUT 10

extern sqlite3 *db;

void http_header(int *s, int status, char *title, char *mime, char *encoding,
		int length) {
	char buf[128];
	char *str = NULL;

	sprintf(buf, "HTTP/1.0 %i %s\r\n", status, title);
	str = realloc(NULL, strlen(buf) + 1);
	str = strcpy(str, buf);
	sprintf(buf, "Server: ADSBox ver.%s\r\n", VERSION);
	str = realloc(str, strlen(str) + strlen(buf) + 1);
	str = strncat(str, buf, strlen(buf));
	sprintf(buf, "Content-Type: %s\r\n", mime);
	str = realloc(str, strlen(str) + strlen(buf) + 1);
	str = strncat(str, buf, strlen(buf));
	if (!strstr(mime, "image")) { // images are cached
		sprintf(buf,"Cache-Control: no-cache, no-store, must-revalidate\r\nPragma: no-cache\r\nExpires: 0\r\n");
		str = realloc(str, strlen(str) + strlen(buf) + 1);
		str = strncat(str, buf, strlen(buf));
	}
	if (length > -1) {
		sprintf(buf, "Content-Length: %i\r\n", length);
		str = realloc(str, strlen(str) + strlen(buf) + 1);
		str = strncat(str, buf, strlen(buf));
	}
	if (encoding) {
		sprintf(buf, "Content-Encoding: %s\r\n", encoding);
		str = realloc(str, strlen(str) + strlen(buf) + 1);
		str = strncat(str, buf, strlen(buf));
	}
	sprintf(buf, "Connection: keep-alive\r\n\r\n");
	str = realloc(str, strlen(str) + strlen(buf) + 1);
	str = strncat(str, buf, strlen(buf));

	send(*s, str, strlen(str), 0);
	free(str);
}

void http_error(int *s, int status, char *title, char *info) {
	char buf[128];

	http_header((int *) s, status, title, "text/html", NULL, -1);

	sprintf(buf, "<html>\n<head><title>%i %s</title></head>\n<body>\n", status,
			title);
	send(*s, buf, strlen(buf), 0);
	snprintf(buf, 128, "<h3>%i %s</h3><br>%s", status, title, info);
	send(*s, buf, strlen(buf), 0);
	sprintf(buf, "<hr><i>ADSBox ver.%s http server.</i>\n</body>\n</html>\n",
	VERSION);
	send(*s, buf, strlen(buf), 0);
}

int execute_sql(int *s, char * sql) {
	char buf[REQBUFLEN];
	char * str = NULL;
	char * cstr = NULL;
	char ** results;
	int rc, rows, cols, i, j, k;
	char *zErrMsg = 0;

	rc = sqlite3_get_table(db, sql, &results, &rows, &cols, &zErrMsg);
	if (rc != SQLITE_OK)
		DEBUG("\nexecute_sql() error: %s in\n\"%s\"", zErrMsg, sql);

	if (zErrMsg) {
		int l = 0;
		while (zErrMsg[l] != 0) {
			if (zErrMsg[l] == '\t') zErrMsg[l] = ' ';
			if (zErrMsg[l] == '\"') zErrMsg[l] = '\'';
			l++;
		}
	}

	sprintf(buf, "{\"timestamp\":\"%u\",\"err\":\"%s\",\"sqlite_rows\":[",
			(unsigned)time(NULL), (rc == SQLITE_OK ? "SQLITE_OK" : zErrMsg));
	str = realloc(NULL, strlen(buf) + 1);
	strcpy(str, buf);
	for (i = 0; i < (rows == 0 ? 0 : rows == 1 ? 2 : rows + 1); i++) {
		sprintf(buf, "{");
		str = realloc(str, strlen(str) + strlen(buf) + 1);
		str = strncat(str, buf, strlen(buf));
		for (j = 0; j < cols; j++) {
			if (results[i * cols + j]) // null entries are possible
				for (k = 0; results[i * cols + j][k] != '\0'; k++) {
					if (results[i * cols + j][k] == '\t') // tabs are not allowed in json
						results[i * cols + j][k] = ' ';
					if (results[i * cols + j][k] == '\"') // quotes are not allowed in json
						results[i * cols + j][k] = '\'';
				}
			sprintf(buf, "\"c%u\":\"%s\"%s", j, results[i * cols + j],
					(j == (cols - 1) ? "" : ","));
			str = realloc(str, strlen(str) + strlen(buf) + 1);
			str = strncat(str, buf, strlen(buf));
		}
		sprintf(buf, "}%s", (i == rows ? "" : ","));
		str = realloc(str, strlen(str) + strlen(buf) + 1);
		str = strncat(str, buf, strlen(buf));
	}
	sprintf(buf, "]}");
	str = realloc(str, strlen(str) + strlen(buf) + 1);
	str = strncat(str, buf, strlen(buf));

	cstr = malloc(strlen(str) + 4);
	bzero(cstr, strlen(str) + 4);

	// deflate compression of http data
	z_stream defstream;
	defstream.zalloc = Z_NULL;
	defstream.zfree = Z_NULL;
	defstream.opaque = Z_NULL;

	defstream.avail_in = (uint32_t) strlen(str);
	defstream.next_in = (unsigned char *) str;
	defstream.avail_out = (uint32_t) strlen(str) + 4;
	defstream.next_out = (unsigned char *) cstr;

	deflateInit(&defstream, Z_BEST_COMPRESSION);
	deflate(&defstream, Z_FINISH);
	deflateEnd(&defstream);

	http_header(s, 200, "OK", "application/json", "deflate",
			defstream.total_out);
	send(*s, cstr, defstream.total_out, 0);

	sqlite3_free_table(results);
	free(str);
	free(cstr);

	return 0;
}

int make_kml(int *s) {
	char buf[1024];
	char sql[256], sql_f[256], sql_r[256]; // flights
	sqlite3_stmt *stmt, *stmt_f, *stmt_r;
	int rc;

	http_header(s, 200, "OK", "text/application/vnd.google-earth.kml+xml", NULL,
			-1);

	strcpy(buf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	send(*s, buf, strlen(buf), 0);
	strcpy(buf, "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n<Document>\n");
	send(*s, buf, strlen(buf), 0);
	sprintf(buf,
			"<Style id=\"steady\"><IconStyle><Icon><href>/img/steady.png</href></Icon></IconStyle></Style>\n");
	send(*s, buf, strlen(buf), 0);
	sprintf(buf,
			"<Style id=\"climb\"><IconStyle><Icon><href>/img/climb.png</href></Icon></IconStyle></Style>\n");
	send(*s, buf, strlen(buf), 0);
	sprintf(buf,
			"<Style id=\"desc\"><IconStyle><Icon><href>/img/desc.png</href></Icon></IconStyle></Style>\n");
	send(*s, buf, strlen(buf), 0);
	sprintf(buf,
			"<Style id=\"track\"><LineStyle><color>9f7fffff</color><width>1</width></LineStyle>");
	send(*s, buf, strlen(buf), 0);
	sprintf(buf, "<PolyStyle><color>#afD20F0F</color></PolyStyle></Style>");
	send(*s, buf, strlen(buf), 0);

	sprintf(buf,
			"<Style id=\"radar\"><LineStyle><color>9f7fffff</color><width>1</width></LineStyle>");
	send(*s, buf, strlen(buf), 0);
	sprintf(buf, "<PolyStyle><color>#5f0fff0F</color></PolyStyle></Style>");
	send(*s, buf, strlen(buf), 0);

	sprintf(sql_f,
			"SELECT ICAO, LAT, LON, ALT * 0.3048, CALLSIGN, SPEED, HEADING, SQUAWK, VSPEED FROM FLIGHTS \
			WHERE ALT<>0 AND LAT<>0 AND LON<>0");
	sqlite3_prepare(db, sql_f, -1, &stmt_f, 0);
	while (sqlite3_step(stmt_f) == SQLITE_ROW) {

		sprintf(buf, "<Placemark id=\"%06X\">\n",
				sqlite3_column_int(stmt_f, 0));
		send(*s, buf, strlen(buf), 0);
		if (!strlen((char *) sqlite3_column_text(stmt_f, 4)))
			sprintf(buf, "<name>%06X</name>\n", sqlite3_column_int(stmt_f, 0));
		else
			sprintf(buf, "<name>%s</name>\n", sqlite3_column_text(stmt_f, 4));
		send(*s, buf, strlen(buf), 0);
		sprintf(buf,
				"<description><![CDATA[\
							Flight: %s<br>\n\
							ICAO hex: %06X<br>\n\
							Altitude: %s m<br>\n\
							Heading: %1.0f&deg;<br>\n\
							Speed: %1s<br>\n\
							Squawk: %6o<br>\n\
							Lat: %1.2f Lon: %1.2f<br>\n\
							]]></description>\n",
				sqlite3_column_text(stmt_f, 4), sqlite3_column_int(stmt_f, 0),
				sqlite3_column_text(stmt_f, 3),
				sqlite3_column_double(stmt_f, 6),
				sqlite3_column_text(stmt_f, 5), sqlite3_column_int(stmt_f, 7),
				sqlite3_column_double(stmt_f, 1),
				sqlite3_column_double(stmt_f, 2));
		send(*s, buf, strlen(buf), 0);

		// colour icons
		if (sqlite3_column_int(stmt_f, 8) > 128)
			sprintf(buf, "<styleUrl>#climb</styleUrl>\n");
		else if (sqlite3_column_int(stmt_f, 8) < -128)
			sprintf(buf, "<styleUrl>#desc</styleUrl>\n");
		else
			sprintf(buf, "<styleUrl>#steady</styleUrl>\n");
		send(*s, buf, strlen(buf), 0);

		strcpy(buf, "<Point>\n");
		send(*s, buf, strlen(buf), 0);
		strcpy(buf, "<extrude>1</extrude>\n");
		send(*s, buf, strlen(buf), 0);
		strcpy(buf, "<altitudeMode>relativeToGround</altitudeMode>\n");
		send(*s, buf, strlen(buf), 0);
		sprintf(buf, "<coordinates>%s,%s,%s</coordinates>\n",
				sqlite3_column_text(stmt_f, 2), sqlite3_column_text(stmt_f, 1),
				sqlite3_column_text(stmt_f, 3));
		send(*s, buf, strlen(buf), 0);
		strcpy(buf, "</Point>\n</Placemark>\n");
		send(*s, buf, strlen(buf), 0);

		strcpy(buf,
				"<Placemark>\n<styleUrl>#track</styleUrl>\n<LineString>\n<extrude>1</extrude>\n<tessellate>1</tessellate>\n");
		send(*s, buf, strlen(buf), 0);
		strcpy(buf, "<altitudeMode>absolute</altitudeMode>\n<coordinates>\n");
		send(*s, buf, strlen(buf), 0);

		sprintf(sql,
				"SELECT LON, LAT, ALT * 0.3048 FROM TRACKS WHERE ICAO=%s ORDER BY SEEN_TIME DESC",
				sqlite3_column_text(stmt_f, 0));
		sqlite3_prepare(db, sql, -1, &stmt, 0);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			sprintf(buf, "%s,%s,%s\n", sqlite3_column_text(stmt, 0),
					sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2));
			send(*s, buf, strlen(buf), 0);
		}
		sqlite3_finalize(stmt);

		strcpy(buf, "</coordinates>\n</LineString>\n");
		send(*s, buf, strlen(buf), 0);
		strcpy(buf, "</Placemark>\n");
		send(*s, buf, strlen(buf), 0);
	}
	sqlite3_finalize(stmt_f);


	sprintf(sql_r,
			"SELECT ID, LAT, LON FROM V_SOURCES WHERE LAT <> 0 AND LON <> 0");

	rc = sqlite3_prepare(db, sql_r, -1, &stmt_r, 0);
	if (rc != SQLITE_OK) {
		DEBUG("SQLite error: cant prepare %s\n %s\n", sql_r, sqlite3_errmsg(db));
		sqlite3_finalize(stmt_r);
		return 1;
	}
	while (sqlite3_step(stmt_r) == SQLITE_ROW) {

		sprintf(buf, "<Placemark id=\"0001\">\n<name>Source id %i</name>\n",
				sqlite3_column_int(stmt_r, 0));
		send(*s, buf, strlen(buf), 0);
		sprintf(buf, "<description>Data source location.</description>\n");
		send(*s, buf, strlen(buf), 0);
		sprintf(buf, "<Point><coordinates>%f,%f,0</coordinates></Point>\n",
				sqlite3_column_double(stmt_r, 2),
				sqlite3_column_double(stmt_r, 1));
		send(*s, buf, strlen(buf), 0);
		sprintf(buf, "</Placemark>\n");
		send(*s, buf, strlen(buf), 0);

		/*
		 *  Radar area
		 */

		sprintf(buf, "<Placemark>\n<name>Your radar coverage area.</name>\n");
		send(*s, buf, strlen(buf), 0);
		strcpy(buf,
				"<styleUrl>#radar</styleUrl>\n<Polygon>\n<outerBoundaryIs>\n<LinearRing id=\"radar_range\">");
		send(*s, buf, strlen(buf), 0);
		strcpy(buf, "<extrude>0</extrude>\n<tessellate>0</tessellate>\n");
		send(*s, buf, strlen(buf), 0);
		strcpy(buf,
				"<altitudeMode>clampToGround</altitudeMode>\n<coordinates>\n");
		send(*s, buf, strlen(buf), 0);

		sprintf(sql,
				"SELECT LON, LAT FROM RADAR_RANGE \
			WHERE ID = %i ORDER BY AZIMUTH",
				sqlite3_column_int(stmt_r, 0));
		sqlite3_prepare(db, sql, -1, &stmt_f, 0);
		while (sqlite3_step(stmt_f) == SQLITE_ROW) {
			sprintf(buf, "%f,%f,0\n", sqlite3_column_double(stmt_f, 0),
					sqlite3_column_double(stmt_f, 1));
			send(*s, buf, strlen(buf), 0);
		}

		strcpy(buf,
				"</coordinates>\n</LinearRing>\n</outerBoundaryIs>\n</Polygon>\n</Placemark>\n");
		send(*s, buf, strlen(buf), 0);
	}
	sqlite3_finalize(stmt_r);

	strcpy(buf, "</Document></kml>\n");
	send(*s, buf, strlen(buf), 0);

	return 0;
}

char * get_mime(char *path) { // mime type by file extension
	char *ext = strchr(path, '.');

	if (!ext)
		return NULL;
	if (!strcmp(ext, ".html") || !strcmp(ext, ".htm"))
		return "text/html";
	if (!strcmp(ext, ".jpeg") || !strcmp(ext, ".jpg"))
		return "image/jpeg";
	if (!strcmp(ext, ".png"))
		return "image/png";
	if (!strcmp(ext, ".gif"))
		return "image/gif";
	if (!strcmp(ext, ".svg"))
		return "image/svg+xml";
	if (!strcmp(ext, ".css"))
		return "text/css";
	if (!strcmp(ext, ".kml"))
		return "application/vnd.google-earth.kml+xml";
	if (!strcmp(ext, ".js"))
		return "application/javascript";
	return "text/plain";
}

void http_send_file(int *s, char *path, int length) {
	char * buf = NULL;

	FILE * f = fopen(path, "r");
	if (!f) {
		http_error((int *) s, 403, "Forbidden", "Access denied.");
		return;
	}

	http_header((int *) s, 200, "OK", get_mime(path), NULL, length);
	buf = malloc(length);

	if (fread(buf, 1, length, f) > 0)
		send(*s, buf, length, 0);

	fclose(f);
	free(buf);
}

int http_proxy(int *s, char *url) { // http proxy server
	int sockfd;
	struct hostent *server;
	struct sockaddr_in serveraddr;
	char * hostname;
	char * hosturl;
	char buf[512];
	/*char *metar = "{\"Altimeter\": \"3004\", \"Cloud-List\": [[\"BKN\", \"044\"], [\"OVC\", \"070\"]], \"Dewpoint\": \"M01\", \"Flight-Rules\": \"VFR\", \"Other-List\": [\"-RA\"], \"Raw-Report\": \"KJFK 050651Z 01013G21KT 10SM -RA BKN044 OVC070 05/M01 A3004 RMK AO2 SLP172 P0000 T00501011 $\", \"Remarks\": \"RMK AO2 SLP172 P0000 T00501011 $\", \"Remarks-Info\": {\"Dew-Decimal\": \"-1.1\", \"Temp-Decimal\": \"5.0\"}, \"Runway-Vis-List\": [], \"Station\": \"KJFK\", \"Temperature\": \"05\", \"Time\": \"050651Z\", \"Units\": {\"Altimeter\": \"inHg\", \"Altitude\": \"ft\", \"Temperature\": \"C\", \"Visibility\": \"sm\", \"Wind-Speed\": \"kt\"}, \"Visibility\": \"10\", \"Wind-Direction\": \"010\", \"Wind-Gust\": \"21\", \"Wind-Speed\": \"13\", \"Wind-Variable-Dir\": []}\0";
	char send[512];

	// metar data stub
	if (strstr(url, "avwx.rest")) {
		sprintf(send, "HTTP/1.0 200 OK\r\nContent-Type: application-json\r\nContent-Length: %u\r\n\r\n", strlen(metar));
		write(*s, send, strlen(send));
		write(*s, metar, strlen(metar)); // send this data back to client	
		goto proxy_exit;
	}*/

	if (strstr(url, "http://")) {
		hostname = strtok(url + 7, "/");
		//DEBUG("\nHost is %s", hostname);
	} else {
		DEBUG("\nNo host in request");
		return 0;
	}

	hosturl = strstr(url, hostname) + strlen(hostname) + 1;

	server = gethostbyname(hostname);
	if (server == NULL) {
		DEBUG("\nHost not found: %s", hostname);
		return 0;
	}

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
	serveraddr.sin_port = htons(80); // TODO parse port

	//DEBUG("\nip %s", inet_ntoa(serveraddr.sin_addr));

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		DEBUG("\nError opening socket.");
		return 0;
	}

	struct timeval timeout;
	timeout.tv_sec = 8;
	timeout.tv_usec = 0;

	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0) {
		DEBUG("\nsetsockopt() error");
		return 0;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0) {
		DEBUG("\nsetsockopt() error");
		return 0;
	}

	if (connect(sockfd, &serveraddr, sizeof(serveraddr)) < 0) {
		DEBUG("\nUnable to connect to %s", hostname);
		return 0;
	}

	bzero(buf, sizeof(buf));
	sprintf(buf,
			"GET /%s HTTP/1.1\r\n\
Host: %s\r\n\
Connection: close\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n\
User-Agent: ADSBox/%s\r\n\
Accept-Encoding: identity\r\n\
Accept-Language: ru-RU,ru;q=0.8,en-US;q=0.6,en;q=0.4\r\n\r\n",
			hosturl, hostname, VERSION);
	if (write(sockfd, buf, strlen(buf)) < 0) {
		DEBUG("\nUnable write to server socket.");
		return 0;
	}

	bzero(buf, sizeof(buf));
	while (read(sockfd, buf, sizeof(buf) - 1) > 0) {
		write(*s, buf, strlen(buf)); // send this data back to client
		bzero(buf, sizeof(buf));
	}
//proxy_exit:
	close(sockfd);
	return 1;
}

void *http(void *arg) /* process client http connection thread */
{
	char * buf = malloc(REQBUFLEN);
	int *s = (int *) arg;
	char info[128];
	char path[128];
	struct stat statbuf;
	char * method;
	char * url;
	char * post_data;
	int en = 1;
	struct timeval timeout;
	timeout.tv_sec = KEEPALIVE_TIMEOUT;
	timeout.tv_usec = 0;
	struct sockaddr_in peer;
	unsigned int peer_len = sizeof(peer);
	clockid_t cid;

	pthread_detach(pthread_self());

	if (getpeername(*s, &peer, &peer_len) == -1)
		DEBUG("\ngetpeername() error");
	pthread_getcpuclockid(pthread_self(), &cid);
	sprintf(info, "http connection from %s:%u", inet_ntoa(peer.sin_addr),
			(int) ntohs(peer.sin_port));
	db_insert_thread(cid, info);
	//DEBUG("\nHTTP connect from %s:%u", inet_ntoa(peer.sin_addr), (int)ntohs(peer.sin_port));

	if (setsockopt(*s, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0)
		panic("http setsockopt()");
	if (setsockopt(*s, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0)
		panic("http setsockopt()");
	if (setsockopt(*s, SOL_SOCKET, SO_KEEPALIVE, &en, sizeof(en)) != 0)
		panic("http setsockopt()");
	if (setsockopt(*s, IPPROTO_TCP, TCP_NODELAY, &en, sizeof(en)) != 0)
		panic("http setsockopt()");

	bzero(buf, REQBUFLEN);
	bzero(info, sizeof(info));
	bzero(path, sizeof(path));

	while (1) {

		if (recv(*s, buf, REQBUFLEN, 0) < 0) {
			//DEBUG("\nerrno is %i", errno);
			break;
		}

		if ((post_data = strstr(buf, "\r\n\r\n")))
			post_data += 4;
		method = strtok(buf, " ");
		url = strtok(NULL, " ");

		if (!method || !url) {
			http_error((int *) s, 400, "Bad Request.", NULL);
			goto exit;
		}

		//DEBUG("%s %s\n", method, url);

		if (!strcmp(method, "GET")) {

			if (!strcmp(url, "/"))
				sprintf(url, "%s", "/index.html"); // default page

			bzero(path, sizeof(path));
			sprintf(path, "%s/htdocs%s", get_current_dir_name(), url);

			if (!strcmp(url, "/adsbox.kml")) {
				make_kml((int *) s);
			} else if (strstr(url, "/proxy=")) {
				if (!http_proxy((int *) s, url + 7))
					http_error((int *) s, 502, "Bad gateway", "Bad gateway.");
			} else if (stat(path, &statbuf) < 0) {
				snprintf(info, 128, "URL not found: %s", url);
				http_error((int *) s, 404, "Not Found", info);
				goto exit;
			} else if (S_ISDIR(statbuf.st_mode)) {
				http_error((int *) s, 403, "Forbidden", "Access denied."); // dir listing not supported
				goto exit;
			} else {
				http_send_file((int *) s, path, statbuf.st_size);
			}
		} else if (!strcmp(method, "POST")) {
			if (!strcmp(url, "/execute"))
				execute_sql((int*) s, post_data);
			else {
				snprintf(info, 128, "URL not found: %s", url);
				http_error((int *) s, 404, "Not Found", info);
				goto exit;
			}
		} else {
			snprintf(info, 128, "Method %s is not supported.", method);
			http_error((int *) s, 501, "Not Supported.", info);
			goto exit;
		}
		bzero(buf, REQBUFLEN);
	}

	exit: close(*s);
	free(s);
	free(buf);
	db_delete_thread(cid);
	//DEBUG ("\nHTTP thread exit.");
	pthread_exit(0);
}

void *http_server(void * arg) {
	http_info * hi = (http_info *) arg;
	int hd, en = 1;
	struct sockaddr_in http_addr;
	pthread_t child;
	struct timeval timeout;

	timeout.tv_sec = KEEPALIVE_TIMEOUT;
	timeout.tv_usec = 0;

	hd = socket(AF_INET, SOCK_STREAM, 0);
	if (hd < 0)
		panic("http socket()");
	if (setsockopt(hd, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en)) != 0)
		panic("http setsockopt()");
	if (setsockopt(hd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0)
		panic("http setsockopt()");
	if (setsockopt(hd, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
			sizeof(timeout)) < 0)
		panic("http setsockopt()");
	if (setsockopt(hd, SOL_SOCKET, SO_KEEPALIVE, &en, sizeof(en)) != 0)
		panic("http setsockopt()");

	memset(&http_addr, 0, sizeof(http_addr));
	http_addr.sin_family = AF_INET;
	http_addr.sin_port = htons(hi->port);
	http_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(hd, (struct sockaddr*) &http_addr, sizeof(http_addr)) != 0)
		panic("http bind()");
	if (listen(hd, 10) != 0)
		panic("http listen()")
	DEBUG("Run HTTP server on port %i\n", hi->port);

	while (1) {
		int *hd1 = (int*) malloc(sizeof(int));
		if ((*hd1 = accept(hd, 0, 0)) > -1) {
			if (pthread_create(&child, NULL, &http, (void *)hd1)) {
				DEBUG("pthread_create http server error\n");
				free(hd1);
			}
		} else {
			//DEBUG("\nhttp server accept() error.");
			free(hd1);
		}
	}
	// never reach
	free(hi);
	close (hd);
	pthread_exit(0);
}
