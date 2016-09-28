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
#include "tcp_sbs3.h"
#include "misc.h"
#include "adsb.h"
#include "db.h"

extern sqlite3 *db;

int db_insert_source_tcp_sbs3(unsigned int *id, char *ip) {

	if (db_exec_sql("INSERT INTO SOURCE_TCP_SBS3 (ID, IP) VALUES (%i, \"%s\")",
			*id, ip)) return 1;

	return 0;
}

int db_update_source_tcp_sbs3_port(unsigned int *id, unsigned int port) {

	if (db_exec_sql("UPDATE SOURCE_TCP_SBS3 SET PORT=%i WHERE ID=%i", port, *id))
	    return 1;

	if (!sqlite3_changes(db)) {
		DEBUG("Can not set TCP port for source id %u\n", *id);
		return 1;
	}

	return 0;
}

int run_source_tcp_sbs3() {
	char ** results;
	int rc, rows, cols, i;
	char *zErrMsg = 0;
	char sql[] = "SELECT ID, IP, PORT, LAT, LON FROM SOURCE_TCP_SBS3 ORDER BY ID";
	pthread_t child;

	rc = sqlite3_get_table(db, sql, &results, &rows, &cols, &zErrMsg);
	if (rc != SQLITE_OK) {
		DEBUG("SQLite error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_free_table(results);
		return 1;
	}

	for (i = 1; i <= rows; i++) {
		tcp_sbs3_info *ti = malloc(sizeof(tcp_sbs3_info));
		ti->id = atoi(results[i * cols]);
		strcpy(ti->addr, results[i * cols + 1]);
		ti->port = atoi(results[i * cols + 2]);
		ti->lat = atof(results[i * cols + 3]);
		ti->lon = atof(results[i * cols + 4]);
		pthread_create(&child, NULL, &tcp_sbs3_client, ti); /* start tcp client thread */
		pthread_detach(child);
	}

	sqlite3_free_table(results);
	return 0;
}

void *tcp_sbs3_client(void *arg) { /* Data client thread */
	tcp_sbs3_info *ti = (tcp_sbs3_info*) arg;
	struct sockaddr_in addr;
	int fd;
	unsigned char c, crc1, crc2;
	unsigned char sbs3_data[64];
	unsigned char state = STATE_WAIT, old_state = STATE_WAIT, mode =
			MODE_UNKNOWN;
	unsigned int count = 0;
	adsb_data *data = malloc(sizeof(adsb_data));
	int checksum = 0;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		panic("socket");

	data->source_id = ti->id;
	data->type = TYPE_TCP_SBS3; // type of source
	data->lat = ti->lat;
	data->lon = ti->lon;
	data->source_bytes = 0;
	data->timestamp = 0;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(ti->port);
	addr.sin_addr.s_addr = inet_addr(ti->addr);

	DEBUG("Connecting to SBS-3 data server %s:%i...\n", ti->addr, ti->port);
	if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		DEBUG("Could not connect to %s:%i\n", ti->addr, ti->port);
		goto exit;
	}
	DEBUG("Connected to %s:%i\n", ti->addr, ti->port);

	/* (1)
	 * DLE stuffing (KineticAPIDoc_104.pdf)
	 * Any bytes in the data having a value of 10 hex will be preceded by a second 10 hex byte
	 * (DLE character). DLE stuffing also applies to the two CRC bytes.
	 *
	 * M  S  T       00 01 02 03     04 05 06 07 08 09 10 11 12 13 14 15 16 17      M  E  C1 C2
	 * ========================================================================================
	 * 10 02 01      00 11 2f ca     8d 3c 60 d4 60 bf 07 47 70 64 83 00 00 00      10 03 86 8b
	 * 10 02 01      00 11 7a 3f     8d 3c 60 d4 60 bf 03 bc 9e 8a ed 00 00 00      10 03 f6 f9
	 * 10 02 01      00 11 a1 28     8d 4b a8 7a 99 05 57 24 20 05 00 00 00 00      10 03 d9 67
	 * 10 02 01      00 11 db d9     8d 89 60 7b 58 b9 73 d4 1c e5 88 00 00 00      10 03 a8 bc
	 * 10 02 01      00 12 52 6e     8d 50 00 5b 99 45 3d 26 20 04 42 00 00 00      10 03 68 a3
	 * 10 02 01      00 12 83 40     8d 4b a9 45 60 a5 80 5a 8e 90 5a 00 00 00      10 03 31 42
	 * 10 02 01      00 12 ba 87     8d 50 81 ee 99 41 6d a3 c0 04 42 00 00 00      10 03 89 07
	 * 10 02 01      00 12 e0 5e     8d 3c 60 c8 60 bf 03 05 66 e4 cc 00 00 00      10 03 d0 95
	 * 10 02 01      00 13 36 94     8d 4b aa 02 58 b7 f7 93 2a 4a 10 10 00 00      00 10 03 6f 9b // DLE stuffing
	 *                                                                                             // Problem with original method
	 *                                                                                             // (read fixed number of characters)
	 *
	 * 10 02 05      00 fe 09 60     a0 00 18 38 20 50 86 76 cf 2e 60 4b a8 7a      10 03 42 e0
	 * 10 02 05      00 fe 11 ec     aa 00 16 9d c8 48 00 30 aa 00 00 1f 16 f3      10 03 9c fb
	 * 10 02 05      00 fe 8f 75     a0 00 17 b0 c8 48 00 30 a8 00 00 50 81 ee      10 03 92 85
	 * 10 02 05      00 fe 93 79     a0 00 19 10 10 20 50 86 79 58 88 20 4b a8      c7 10 03 87 c2 // DLE stuffing
	 *
	 * 10 02 07      00 00 05 93     02 e1 97 b0 3c 60 d4                           10 03 59 24
	 * 10 02 07      00 00 57 4b     02 e1 99 10 10 4b a8                           c7 10 03 7e b2 // DLE stuffing
	 * 10 02 07      00 00 ce 3a     5d 4b aa 0a 00 1c 18                           10 03 34 9e
	 * 10 02 07      00 00 e0 7b     4d 01 45 9c fe 0a ed                           10 03 db b0
	 * 10 02 07      00 00 eb 73     2a 00 1a 0d 1d 54 ad                           10 03 72 da
	 * 10 02 07      00 00 f3 84     5d 39 46 ea 00 00 4c                           10 03 1d 9d
	 * 10 02 07      00 00 fa 7a     00 61 89 00 16 6e b4                           10 03 7c 67
	 */
	while (read(fd, &c, 1)) {
		switch (state) {
		case STATE_WAIT:
			if (c == DLE) {
				count = 0;
				mode = MODE_UNKNOWN;
				bzero(sbs3_data, sizeof(sbs3_data));
				state = STATE_DLE;
			}
			break;
		case STATE_DLE:
			if (c == STX) {
				state = STATE_STX;
			} else if (c == ETX) {
				state = STATE_ETX;
			} else if (c == DLE) { // DLE stuffing
				state = old_state;
				if (state == STATE_COLLECT && mode != MODE_UNKNOWN) {
					sbs3_data[count++] = c;
				}
			} else {
				state = STATE_WAIT;
			}
			break;
		case STATE_STX:
			if (c == KAL_PKT_MODES_ADSB || c == KAL_PKT_MODES_LONG) {
				mode = MODE_LONG;
			} else if (c == KAL_PKT_MODES_SHORT) {
				mode = MODE_SHORT;
			} else {
				mode = MODE_UNKNOWN;
			}
			state = STATE_COLLECT;
			break;
		case STATE_COLLECT:
			if (c != DLE) {
				if (mode != MODE_UNKNOWN) {
					sbs3_data[count++] = c;
				}
			} else {
				state = STATE_DLE;
			}
			break;
		case STATE_ETX:
			crc1 = c;
			read(fd, &crc2, 1);
			if (crc1 == DLE && crc2 == DLE) {
				read(fd, &crc2, 1);
			}
			if ((mode == MODE_SHORT && count == 11)
					|| (mode == MODE_LONG && count == 18)) {
				bzero(data->avr_data, sizeof(data->avr_data));
				/* (2)
				 * Checksum must be calculated always, not only when last 3 bytes are 0.
				 * When last 3 bytes != 0 they must be xored with calculated checksum(else decode_message fail).
				 * sprintf is slow function it will be better to write a new function(s) bin2hex and hex2bin.
				 */
				if (mode == MODE_SHORT) {
					checksum = crc_data((char *)sbs3_data + 4, 4);

					if (!sbs3_data[8] && !sbs3_data[9] && !sbs3_data[10]) {
						sbs3_data[8] = checksum >> 16;
						sbs3_data[9] = checksum >> 8;
						sbs3_data[10] = checksum;
					} else {
						sbs3_data[8] ^= checksum >> 16;
						sbs3_data[9] ^= checksum >> 8;
						sbs3_data[10] ^= checksum;
					}
					sprintf(data->avr_data, "*%02X%02X%02X%02X%02X%02X%02X;",
							sbs3_data[4], sbs3_data[5], sbs3_data[6],
							sbs3_data[7], sbs3_data[8], sbs3_data[9],
							sbs3_data[10]);
					data->data_len = 7;
					memcpy(data->data, sbs3_data + 4, 7);
					data->source_bytes += 18;
				} else if (mode == MODE_LONG) {
					checksum = crc_data((char *)sbs3_data + 4, 11);

					if (!sbs3_data[15] && !sbs3_data[16] && !sbs3_data[17]) {
						sbs3_data[15] = checksum >> 16;
						sbs3_data[16] = checksum >> 8;
						sbs3_data[17] = checksum;
					} else {
						sbs3_data[15] ^= checksum >> 16;
						sbs3_data[16] ^= checksum >> 8;
						sbs3_data[17] ^= checksum;
					}

					sprintf(data->avr_data,
							"*%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X;",
							sbs3_data[4], sbs3_data[5], sbs3_data[6],
							sbs3_data[7], sbs3_data[8], sbs3_data[9],
							sbs3_data[10], sbs3_data[11], sbs3_data[12],
							sbs3_data[13], sbs3_data[14], sbs3_data[15],
							sbs3_data[16], sbs3_data[17]);
					data->data_len = 14;
					memcpy(data->data, sbs3_data + 4, 14);
					data->source_bytes += 25;
				}
				enqueue_data(data);
			}
			state = STATE_WAIT;
			break;
		}
		if (state != STATE_DLE)
			old_state = state;
	}

	exit:
	free(ti);
	free(data);
	DEBUG("\ntcp_sbs3_client exit\n");
	pthread_exit(0);
}
