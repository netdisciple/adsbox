#ifndef __ADSB_H__
#define __ABSB_H__

double mod2(double, double);
double azimuth2(double, double, double, double);
double dist(double, double, double, double);
int decode_message(adsb_data*, char*);
int decode_message56(adsb_data*, char*);
unsigned int crc_data(char*, int);

#endif
