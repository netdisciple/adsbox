/*
 *	adsbox - ADS-B decoder software
 *	(c) atty 2011-15 romulmsg@mail.ru
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "tcp_avr.h"
#include "misc.h"
#include "adsb.h"
#include "db.h"

extern sqlite3 *db;

int db_insert_source_tcp(unsigned int *id, char *ip) {

	if (db_exec_sql("INSERT INTO SOURCE_TCP (ID, IP) VALUES (%i, \"%s\")", *id, ip))
	    return 1;

	return 0;
}

int db_update_source_tcp_port(unsigned int *id, unsigned int port) {

	if (db_exec_sql("UPDATE SOURCE_TCP SET PORT=%i WHERE ID=%i", port, *id))
	    return 1;

	if (!sqlite3_changes(db)) {
		DEBUG("Can not set TCP port for source id %u\n", *id);
		return 1;
	}

	return 0;
}

int run_source_tcp() {
	char ** results;
	int rows, cols, i;
	char *zErrMsg = 0;
	char sql[] = "SELECT ID, IP, PORT, LAT, LON FROM SOURCE_TCP ORDER BY ID";
	pthread_t child;

	if (sqlite3_get_table(db, sql, &results, &rows, &cols, &zErrMsg) != SQLITE_OK) {
		DEBUG("SQLite error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_free_table(results);
		return 1;
	}

	for (i = 1; i <= rows; i++) {
		tcp_info *ti = malloc(sizeof(tcp_info));
		ti->id = atoi(results[i * cols]);
		strcpy(ti->addr, results[i * cols + 1]);
		ti->port = atoi(results[i * cols + 2]);
		ti->lat = atof(results[i * cols + 3]);
		ti->lon = atof(results[i * cols + 4]);
		pthread_create(&child, NULL, &tcp_client, ti); /* start tcp client thread */
		pthread_detach(child);
	}

	sqlite3_free_table(results);
	return 0;
}

void *tcp_client(void *arg) { /* Data client thread */
	tcp_info *ti = (tcp_info*) arg;
	char rawbuf[BUFLEN];
	char d[2];
	struct sockaddr_in addr;
	adsb_data *data = malloc(sizeof(adsb_data));
	int fd, en = 1;
	clockid_t cid;

	pthread_getcpuclockid(pthread_self(), &cid);

	data->source_id = ti->id;
	data->type = TYPE_TCP_AVR; // type of source
	data->lat = ti->lat;
	data->lon = ti->lon;
	data->source_bytes = 0;
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		panic("tcp_client socket");
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &en, sizeof(en)) != 0)
		panic("tcp_client setsockopt()");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(ti->port);
	addr.sin_addr.s_addr = inet_addr(ti->addr);

	sprintf(rawbuf, "AVR TCP data source %s:%i", ti->addr, ti->port);
	db_insert_thread(cid, rawbuf);

	DEBUG("Connecting to AVR data server %s:%i...\n", ti->addr, ti->port);
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		DEBUG("Could not connect to %s:%i\n",ti->addr,
				ti->port);
		goto exit;
	}
	DEBUG("Connected to %s:%i\n",ti->addr,
			ti->port);

	while (1) {
		bzero(rawbuf, BUFLEN);
		while (strstr((char *) rawbuf, ";") == NULL) {
			read(fd, d, 1);
			if (d[0] == '*' || d[0] == '@' || d[0] == '#')
				bzero(rawbuf, BUFLEN);
			strncat((char *) rawbuf, d, 1);
		}

		if (!avr_data(data, rawbuf))
			enqueue_data(data);
	}

	exit:
	free(ti);
	free(data);
	db_delete_thread(cid);
	DEBUG("\ntcp_client exit");
	pthread_exit(0);
}
