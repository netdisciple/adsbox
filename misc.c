/*
 *	adsbox - ADS-B decoder software
 *	(c) atty 2011-15 romulmsg@mail.ru
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "misc.h"
#include "db.h"

#ifdef MLAT
#include <mlat.h>
#endif

unsigned int avr_data (adsb_data *data, char *b) {
	char timestamp[9];
	bzero(data->avr_data, sizeof(data->avr_data));
	strncpy(data->avr_data, b, sizeof(data->avr_data));
	data->source_bytes += strlen(data->avr_data);

	//DEBUG("data is %s len is %u\n", data->avr_data, strlen(b));

	data->timestamp = 0;
	// strip time mark if present. Thanks Petr Kouda
	if (strlen(b) == 28 || strlen(b) == 42) {
		bzero(timestamp, sizeof(timestamp));
		strncpy(timestamp, b + 5, 8);
		data->timestamp = strtoll(timestamp, NULL, 16); // extract timestamp here
		sprintf(data->avr_data, "*%s", data->avr_data + 13);
	}
	
	if (strlen(data->avr_data) == 30) {
		data->data_len = 14;
		sscanf(data->avr_data,
			"*%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
			&data->data[0], &data->data[1], &data->data[2], &data->data[3], &data->data[4],
			&data->data[5], &data->data[6], &data->data[7], &data->data[8], &data->data[9],
			&data->data[10], &data->data[11], &data->data[12], &data->data[13]);
		return 0;
	}
	if (strlen(data->avr_data) == 16) {
		data->data_len = 7;
		sscanf(data->avr_data,
			"*%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
			&data->data[0], &data->data[1], &data->data[2], &data->data[3], &data->data[4],
			&data->data[5], &data->data[6]);
		return 0;
	}

	// measure frequency packet (ADSBox AVR protocol extension)
	if (strlen(data->avr_data) == 10) {
		data->data_len = 4;
		sscanf(data->avr_data,
			"#%02hhx%02hhx%02hhx%02hhx",
			&data->data[0], &data->data[1], &data->data[2], &data->data[3]);
		static unsigned int freq;
		if (!sscanf(data->avr_data, "#%08X", &freq)) {
			DEBUG("\nError convert frequency.");
		} else {
			db_update_hz(data->source_id, freq);
		}
		return 0;
	}

	return 1;
}
