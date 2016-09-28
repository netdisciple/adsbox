#ifndef _RTLSDR_H_
#define _RTLSDR_H_

typedef struct {
	int id;
	int device;
	int gain;
	int agc;
	int freq_correct;
	double lat;
	double lon;
} rtlsdr_info;

void *rtlsdr_thread(void *);
int db_insert_source_rtlsdr(unsigned int*, int);
int db_update_source_rtlsdr_gain(unsigned int*, double);
int db_update_source_rtlsdr_agc(unsigned int*, int);
int db_update_source_rtlsdr_freq_correct(unsigned int*, int);
int run_source_rtlsdr();

#endif
