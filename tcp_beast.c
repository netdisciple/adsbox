/*
 *	adsbox - ADS-B decoder software
 *	(c) atty 2011-14 romulmsg@mail.ru
 *
 *	Nikolay Zlatev code rework
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "tcp_beast.h"
#include "misc.h"
#include "adsb.h"
#include "db.h"

extern sqlite3 *db;

int db_insert_source_tcp_beast(unsigned int *id, char *ip) {

	if (db_exec_sql("INSERT INTO SOURCE_TCP_BEAST (ID, IP) VALUES (%i, \"%s\")",
			*id, ip)) return 1;

	return 0;
}

int db_update_source_tcp_beast_port(unsigned int *id, unsigned int port) {

	if (db_exec_sql("UPDATE SOURCE_TCP_BEAST SET PORT=%i WHERE ID=%i", port, *id))
	    return 1;

	if (!sqlite3_changes(db)) {
		DEBUG("Can not set TCP port for source id %u\n", *id);
		return 1;
	}

	return 0;
}

int run_source_tcp_beast() {
	char ** results;
	int rows, cols, i;
	char *zErrMsg = 0;
	char sql[] = "SELECT ID, IP, PORT, LAT, LON FROM SOURCE_TCP_BEAST ORDER BY ID";
	pthread_t child;

	if (sqlite3_get_table(db, sql, &results, &rows, &cols, &zErrMsg) != SQLITE_OK) {
		DEBUG("SQLite error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_free_table(results);
		return 1;
	}

	for (i = 1; i <= rows; i++) {
		tcp_beast_info *ti = malloc(sizeof(tcp_beast_info));
		ti->id = atoi(results[i * cols]);
		strcpy(ti->addr, results[i * cols + 1]);
		ti->port = atoi(results[i * cols + 2]);
		ti->lat = atof(results[i * cols + 3]);
		ti->lon = atof(results[i * cols + 4]);
		pthread_create(&child, NULL, &tcp_beast_client, ti); /* start tcp client thread */
		pthread_detach(child);
	}

	sqlite3_free_table(results);
	return 0;
}

void *tcp_beast_client(void *arg) {
	tcp_beast_info *ti = (tcp_beast_info*) arg;
	unsigned char beast_data[23];
	unsigned char idx = 0, c = 0, c_prev = 0;
	struct sockaddr_in addr;
	adsb_data *data = malloc(sizeof(adsb_data));
	int fd;

	data->source_id = ti->id;
	data->type = TYPE_TCP_BEAST; // type of source
	data->lat = ti->lat;
	data->lon = ti->lon;
	data->source_bytes = 0;
	data->data_len = 0;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		panic("socket");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(ti->port);
	addr.sin_addr.s_addr = inet_addr(ti->addr);

	DEBUG("Connecting to beast binary data server %s:%i...\n", ti->addr,
			ti->port);
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		DEBUG("Could not connect to %s:%i\n", ti->addr, ti->port);
		goto exit;
	}
	DEBUG("Connected to %s:%i\n", ti->addr, ti->port);
	while (read(fd, &c, 1)) {
		if (((c != ESC) && (c_prev == ESC)) || (idx == sizeof(beast_data))) { // esc byte received - start of frame
			memcpy(data->data, (char *) &beast_data + idx - data->data_len - 1,
					data->data_len);
			data->timestamp = beast_data[6] + (beast_data[5] << 8) +
						(beast_data[4] << 16) + (beast_data[3] << 24);
			data->source_bytes += data->data_len;
			enqueue_data(data);
			idx = 0;
		}
		if ((c_prev == ESC) && (c == '2') && (idx==0))
			data->data_len = 7;
		if ((c_prev == ESC) && (c == '3') && (idx==0))
			data->data_len = 14;
		if ((c_prev == ESC) && (c == ESC)) { // ESC stuffing
			beast_data[idx] = ESC;
			idx--;
			c = 0;
		} else
			beast_data[idx] = c;

		idx++;
		c_prev = c;
	}

	exit:
	free(ti);
	free(data);
	DEBUG("\ntcp_beast_client exit");
	pthread_exit(0);
}
