/*
 *	This is a part of adsbox - ADS-B decoder software
 * 	(c) atty 2011-15 romulmsg@mail.ru
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <strings.h>
#include <string.h>
#include <pthread.h>
#include "misc.h"
#include "adsb.h"
#include "rtlsdr.h"
#include <libusb-1.0/libusb.h>
#include "rtl-sdr.h"
#include "db.h"

// uncomment if async mode read
#define ASYNC_READ 1

#define MODES_PREAMBLE_LEN 16
#define MODES_LONG_LEN 112
#define ADSB_FREQ 1090000000
#define ADSB_RATE 2000000
#define DEFAULT_BUF_LENGTH (16 * 16384)
#define DEFAULT_ASYNC_BUF_NUMBER 12

extern sqlite3 *db;
uint16_t mag_table[256][256];
static rtlsdr_dev_t *dev = NULL;
unsigned char *buffer;
uint16_t *mag_buf;

int db_insert_source_rtlsdr(unsigned int *id, int device) {

	if (db_exec_sql("INSERT INTO SOURCE_RTLSDR (ID, DEVICE) VALUES (%i, %i)", *id,
			device)) return 1;
	return 0;
}

int db_update_source_rtlsdr_gain(unsigned int *id, double gain) {

	if (db_exec_sql("UPDATE SOURCE_RTLSDR SET GAIN=%f WHERE ID=%i", gain, *id))
	    return 1;

	if (!sqlite3_changes(db)) {
		DEBUG("Can not set gain for source id %u\n", *id);
		return 1;
	}

	return 0;
}

int db_update_source_rtlsdr_agc(unsigned int *id, int agc) {

	if (db_exec_sql("UPDATE SOURCE_RTLSDR SET AGC=%i WHERE ID=%i", agc, *id))
	    return 1;

	if (!sqlite3_changes(db)) {
		DEBUG("Can not set agc for source id %u\n", *id);
		return 1;
	}

	return 0;
}

int db_update_source_rtlsdr_freq_correct(unsigned int *id, int fc) {

	if (db_exec_sql("UPDATE SOURCE_RTLSDR SET FREQ_CORR=%i WHERE ID=%i", fc, *id))
	    return 1;

	if (!sqlite3_changes(db)) {
		DEBUG("Can not frequency correction for source id %u\n", *id);
		return 1;
	}

	return 0;
}

int run_source_rtlsdr() {
	char ** results;
	int rows, cols, i;
	char *zErrMsg = 0;
	char sql[] = "SELECT ID, DEVICE, GAIN, AGC, FREQ_CORR, LAT, LON FROM SOURCE_RTLSDR ORDER BY ID";
	pthread_t child;

	if (sqlite3_get_table(db, sql, &results, &rows, &cols, &zErrMsg) != SQLITE_OK) {
		DEBUG("SQLite error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_free_table(results);
		return 1;
	}

	for (i = 1; i <= rows; i++) {
		rtlsdr_info *ri = malloc(sizeof(rtlsdr_info));
		ri->id = atoi(results[i * cols]);
		ri->device = atoi(results[i * cols + 1]);
		ri->gain = atof(results[i * cols + 2]);
		ri->agc = atoi(results[i * cols + 3]);
		ri->freq_correct = atoi(results[i * cols + 4]);
		ri->lat = atof(results[i * cols + 5]);
		ri->lon = atof(results[i * cols + 6]);
		pthread_create(&child, NULL, &rtlsdr_thread, ri); /* start rtlsdr thread */
		pthread_detach(child);
	}

	sqlite3_free_table(results);
	return 0;
}

void decode_mag_buf(uint16_t *buf, unsigned int len, adsb_data *data) {
	int i, j, k;
	uint16_t avg_high;
	unsigned char bits[MODES_LONG_LEN];
	unsigned char msg[14];
	int msg_len;

	//xdraw_buffer(data->window);
	
	for (i = 0; i < len - MODES_PREAMBLE_LEN - MODES_LONG_LEN * 2; i++) {
		//DEBUG("%u\n", buf[i]);
/*
Find this pattern
0 --
1 -----------
2 --
3 -----------
4 --
5 --
6 --
7 --
8 -----------
9 --
10-----------
11--
12--
13--
14--
15--
16--
*/
		if (!(buf[i] < buf[i + 1]/2 && buf[i + 1]/2 > buf[i + 2] &&
			buf[i + 2] < buf[i + 3]/2 && buf[i + 4] < buf[i + 3]/2 &&
			buf[i + 5] < buf[i + 3]/2 && buf[i + 6] < buf[i + 3]/2 &&
			buf[i + 7] < buf[i + 3]/2 && buf[i + 8]/2 > buf[i + 9] &&
			buf[i + 10] > buf[i + 11]))
			continue;

			avg_high = buf[i + 1]/2;

		if (buf[i + 12] > avg_high ||
			buf[i + 13] > avg_high || buf[i + 14] > avg_high ||
			buf[i + 15] > avg_high || buf[i + 16] > avg_high)
			continue;

		/*printf("preamble found as %u %u %u %u %u %u %u %u %u %u\n",
		buf[i], buf[i+1], buf[i+2], buf[i+3],
		buf[i+4], buf[i+5], buf[i+6], buf[i+7],
		buf[i+8], buf[i+9]);
		*/

		i += MODES_PREAMBLE_LEN + 1;

		/* decode bits */
		j = 0;
		while (j < MODES_LONG_LEN) {

			if (buf[i] < buf[i + 1])
				bits[j] = 0;
			else
				bits[j] = 1;

			if ((j == 55) && (buf[i + 1] < avg_high) && (buf[i + 2] < avg_high))
				break;
			// short message ?

			j++;
			i += 2;
		}
		msg_len = j + 1;

		/* copy bits into message */
		for (j = 0; j < msg_len; j += 8) {
			msg[j / 8] = bits[j] << 7 | bits[j + 1] << 6 | bits[j + 2] << 5
					| bits[j + 3] << 4 | bits[j + 4] << 3 | bits[j + 5] << 2
					| bits[j + 6] << 1 | bits[j + 7];
		}

		/* compose AVR format */
		char modes_msg[256];
		bzero(modes_msg, 256);
		char s[3];

		strcat(modes_msg, "*");
		for (k = 0; k < msg_len / 8; k++) {
			bzero(s, 3);
			sprintf(s, "%02X", msg[k]);
			strcat(modes_msg, s);
		}
		strcat(modes_msg, ";\0");

		//DEBUG("decoding %s %i\n", modes_msg, msg_len / 8);
		data->data_len = msg_len / 8;
		strncpy((char *) data->data, (char *) msg, sizeof(data->data));
		bzero(data->avr_data, sizeof(data->avr_data));
		memcpy(data->avr_data, modes_msg, strlen(modes_msg));
		enqueue_data(data);
	}
}

void prepare_mag_buf(unsigned char *iq_buf, unsigned int iq_buf_len,
		uint16_t *mag_buf) {
	int k = 0;

	while (k < iq_buf_len) {
		mag_buf[k / 2] = mag_table[iq_buf[k]][iq_buf[k + 1]];
		//DEBUG("i=%i q=%i m=%i\n", iq_buf[k], iq_buf[k+1], mag_buf[k / 2]);
		k = k + 2;
	}
}

void prepare_mag_table() {
	int i, q;
	for (i = 0; i < 256; i++)
		for (q = 0; q < 256; q++)
			mag_table[i][q] = (i - 127) * (i - 127) + (q - 127) * (q - 127);
}

#ifdef ASYNC_READ
void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx) {
	adsb_data *data = (adsb_data *) ctx;
	prepare_mag_buf(buf, len, mag_buf);
	decode_mag_buf(mag_buf, len / 2, data);
}
#endif

void *rtlsdr_thread(void *arg) {
	rtlsdr_info *ri = (rtlsdr_info*) arg;
	int device_count;
	unsigned int dev_index = ri->device;
	int gain = ri->gain;
	char vendor[256], product[256], serial[256];
	int i, r;
#ifndef ASYNC_READ
	int n_read;
#endif
	clockid_t cid;
	pthread_getcpuclockid(pthread_self(), &cid);

	adsb_data *data = malloc(sizeof(adsb_data));

	data->source_id = ri->id;
	data->type = TYPE_RTLSDR; // type of source
	data->lat = ri->lat;
	data->lon = ri->lon;
	data->source_bytes = 0;

	prepare_mag_table();

	buffer = malloc(DEFAULT_BUF_LENGTH * sizeof(unsigned char));
	mag_buf = malloc(DEFAULT_BUF_LENGTH / 2 * sizeof(uint16_t));

	device_count = rtlsdr_get_device_count();
	if (!device_count) {
		DEBUG("No supported devices found\n");
		goto exit;
	}
	DEBUG("Found %d devices:\n", device_count);
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		DEBUG("\t%d: %s, %s, SN: %s\n", i, vendor, product, serial);
	}

	DEBUG("Using device %d: %s\n", dev_index,
			rtlsdr_get_device_name(dev_index));

	r = rtlsdr_open(&dev, dev_index);
	if (r < 0) {
		DEBUG("Failed to open device #%d\n", dev_index);
		goto exit;
	}

	sprintf(product, "RTLSDR data source device %u", ri->device);
	db_insert_thread(cid, product);

	r = rtlsdr_set_tuner_gain_mode(dev, 1); // 0 - autogain
	if (r != 0)
		DEBUG("Failed to set tuner gain mode\n");

	int ngains;
	int gains[100];

	ngains = rtlsdr_get_tuner_gains(dev, gains);
	DEBUG ("Supported gains are:");
	for (i = 0; i < ngains; i++)
		DEBUG(" %.2f", gains[i] / 10.0);
	DEBUG("\n");

	if (gain == -1) {
		gain = gains[ngains - 1];
		rtlsdr_set_tuner_gain(dev, gain);
		DEBUG("Set max available gain %.2f.\n", (float)rtlsdr_get_tuner_gain(dev)/10);
	} else if (gain == -2) {
		rtlsdr_set_tuner_gain_mode(dev, 0);
		DEBUG("Set autogain.\n");
	} else {
		gain = ri->gain * 10;
		rtlsdr_set_tuner_gain(dev, gain);
		DEBUG("\nSet gain to %.2f\n", (float)rtlsdr_get_tuner_gain(dev)/10);
	}

	if (ri->agc == 1) {
	    r = rtlsdr_set_agc_mode(dev, ri->agc);
	    if (r < 0) {
			DEBUG("Failed to ret AGC mode.\n");
			goto exit;
	    }
	    DEBUG("Using AGC.\n");
	}
	
	/* Tune to freq. */
	r = rtlsdr_set_center_freq(dev, ADSB_FREQ);
	if (r < 0) {
		DEBUG("Failed to center freq. %u", ADSB_FREQ);
		goto exit;
	} else
	DEBUG("Tuned to %u\n", ADSB_FREQ);

	/* Frequency correction */
	if (ri->freq_correct) {
		r = rtlsdr_set_freq_correction(dev, ri->freq_correct);
		if (r < 0) {
			DEBUG("Failed to set freq. correction to %i ppm\n", ri->freq_correct);
			goto exit;
		} else
		DEBUG("Set frequency correction %i ppm\n", ri->freq_correct);
	}

	/* Sample rate */
	r = rtlsdr_set_sample_rate(dev, ADSB_RATE);
	if (r < 0) {
		DEBUG("Failed to sample rate %u", ADSB_RATE);
		goto exit;
	}

	r = rtlsdr_reset_buffer(dev);
	if (r < 0) {
		DEBUG("Failed to reset buffer.\n");
		goto exit;
	}

	rtlsdr_read_sync(dev, NULL, 4096, NULL);
#ifdef ASYNC_READ
	rtlsdr_read_async(dev, rtlsdr_callback, (void *)(data),
			DEFAULT_ASYNC_BUF_NUMBER, DEFAULT_BUF_LENGTH);
#else
	while (1) {
		r = rtlsdr_read_sync(dev, buffer, DEFAULT_BUF_LENGTH, &n_read);
		if (r < 0) {
			DEBUG("sync_read failed.");
		}

		//DEBUG("read %u bytes\n", n_read);
		prepare_mag_buf(buffer, n_read, mag_buf);
		decode_mag_buf(mag_buf, n_read / 2, edata);
	}
#endif	

	exit:
	rtlsdr_close(dev);
	free(ri);
	free(data);
	free(buffer);
	free(mag_buf);
	db_delete_thread(cid);
	pthread_exit(0);
}
