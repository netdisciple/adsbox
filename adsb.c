/*
 *	This is a part of adsbox - ADS-B decoder software
 * 	(c) atty 2011-16 romulmsg@mail.ru
 *	Alexander Heidrich - DF4, Flight Status decode
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "misc.h"
#include "db.h"
#include "adsb.h"

#ifdef MLAT
#include <mlat.h>
#endif

#ifndef round
double round(double value) {
	if (value < 0)
		return -(floor(-value + 0.5));
	else
		return floor(value + 0.5);
}
#endif

#ifdef MLAT
extern _Bool with_mlat;
#endif

#define MAXDIST 450.0 // reject too far position
#define MAXSTEP 20.0 // reject strange position
char cs_tbl[] = { '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
		'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y',
		'Z', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '0', '1', '2', '3', '4', '5',
		'6', '7', '8', '9', ' ', ' ', ' ', ' ', ' ', ' ' };
unsigned int crc_table[256] = { 0x000000, 0xfff409, 0x001c1b, 0xffe812,
		0x003836, 0xffcc3f, 0x00242d, 0xffd024, 0x00706c, 0xff8465, 0x006c77,
		0xff987e, 0x00485a, 0xffbc53, 0x005441, 0xffa048, 0x00e0d8, 0xff14d1,
		0x00fcc3, 0xff08ca, 0x00d8ee, 0xff2ce7, 0x00c4f5, 0xff30fc, 0x0090b4,
		0xff64bd, 0x008caf, 0xff78a6, 0x00a882, 0xff5c8b, 0x00b499, 0xff4090,
		0x01c1b0, 0xfe35b9, 0x01ddab, 0xfe29a2, 0x01f986, 0xfe0d8f, 0x01e59d,
		0xfe1194, 0x01b1dc, 0xfe45d5, 0x01adc7, 0xfe59ce, 0x0189ea, 0xfe7de3,
		0x0195f1, 0xfe61f8, 0x012168, 0xfed561, 0x013d73, 0xfec97a, 0x01195e,
		0xfeed57, 0x010545, 0xfef14c, 0x015104, 0xfea50d, 0x014d1f, 0xfeb916,
		0x016932, 0xfe9d3b, 0x017529, 0xfe8120, 0x038360, 0xfc7769, 0x039f7b,
		0xfc6b72, 0x03bb56, 0xfc4f5f, 0x03a74d, 0xfc5344, 0x03f30c, 0xfc0705,
		0x03ef17, 0xfc1b1e, 0x03cb3a, 0xfc3f33, 0x03d721, 0xfc2328, 0x0363b8,
		0xfc97b1, 0x037fa3, 0xfc8baa, 0x035b8e, 0xfcaf87, 0x034795, 0xfcb39c,
		0x0313d4, 0xfce7dd, 0x030fcf, 0xfcfbc6, 0x032be2, 0xfcdfeb, 0x0337f9,
		0xfcc3f0, 0x0242d0, 0xfdb6d9, 0x025ecb, 0xfdaac2, 0x027ae6, 0xfd8eef,
		0x0266fd, 0xfd92f4, 0x0232bc, 0xfdc6b5, 0x022ea7, 0xfddaae, 0x020a8a,
		0xfdfe83, 0x021691, 0xfde298, 0x02a208, 0xfd5601, 0x02be13, 0xfd4a1a,
		0x029a3e, 0xfd6e37, 0x028625, 0xfd722c, 0x02d264, 0xfd266d, 0x02ce7f,
		0xfd3a76, 0x02ea52, 0xfd1e5b, 0x02f649, 0xfd0240, 0x0706c0, 0xf8f2c9,
		0x071adb, 0xf8eed2, 0x073ef6, 0xf8caff, 0x0722ed, 0xf8d6e4, 0x0776ac,
		0xf882a5, 0x076ab7, 0xf89ebe, 0x074e9a, 0xf8ba93, 0x075281, 0xf8a688,
		0x07e618, 0xf81211, 0x07fa03, 0xf80e0a, 0x07de2e, 0xf82a27, 0x07c235,
		0xf8363c, 0x079674, 0xf8627d, 0x078a6f, 0xf87e66, 0x07ae42, 0xf85a4b,
		0x07b259, 0xf84650, 0x06c770, 0xf93379, 0x06db6b, 0xf92f62, 0x06ff46,
		0xf90b4f, 0x06e35d, 0xf91754, 0x06b71c, 0xf94315, 0x06ab07, 0xf95f0e,
		0x068f2a, 0xf97b23, 0x069331, 0xf96738, 0x0627a8, 0xf9d3a1, 0x063bb3,
		0xf9cfba, 0x061f9e, 0xf9eb97, 0x060385, 0xf9f78c, 0x0657c4, 0xf9a3cd,
		0x064bdf, 0xf9bfd6, 0x066ff2, 0xf99bfb, 0x0673e9, 0xf987e0, 0x0485a0,
		0xfb71a9, 0x0499bb, 0xfb6db2, 0x04bd96, 0xfb499f, 0x04a18d, 0xfb5584,
		0x04f5cc, 0xfb01c5, 0x04e9d7, 0xfb1dde, 0x04cdfa, 0xfb39f3, 0x04d1e1,
		0xfb25e8, 0x046578, 0xfb9171, 0x047963, 0xfb8d6a, 0x045d4e, 0xfba947,
		0x044155, 0xfbb55c, 0x041514, 0xfbe11d, 0x04090f, 0xfbfd06, 0x042d22,
		0xfbd92b, 0x043139, 0xfbc530, 0x054410, 0xfab019, 0x05580b, 0xfaac02,
		0x057c26, 0xfa882f, 0x05603d, 0xfa9434, 0x05347c, 0xfac075, 0x052867,
		0xfadc6e, 0x050c4a, 0xfaf843, 0x051051, 0xfae458, 0x05a4c8, 0xfa50c1,
		0x05b8d3, 0xfa4cda, 0x059cfe, 0xfa68f7, 0x0580e5, 0xfa74ec, 0x05d4a4,
		0xfa20ad, 0x05c8bf, 0xfa3cb6, 0x05ec92, 0xfa189b, 0x05f089, 0xfa0480 };

double dist(double lat, double lon, double lat1, double lon1) {
	return 6371
			* acos(
					sin(lat * M_PI / 180.) * sin(lat1 * M_PI / 180.)
							+ cos(lat * M_PI / 180.) * cos(lat1 * M_PI / 180.)
									* cos((lon - lon1) * M_PI / 180.));
}

void azimuth(double *rlat1, double *rlon1, double *rlat2, double *rlon2,
		double *a) {
	double result = 0.;

	double lat1 = *rlat1 * M_PI / 180;
	double lon1 = *rlon1 * M_PI / 180;
	double lat2 = *rlat2 * M_PI / 180;
	double lon2 = *rlon2 * M_PI / 180;

	double b = acos(
			sin(lat2) * sin(lat1) + cos(lat2) * cos(lat1) * cos(lon2 - lon1));
	if (sin(b) == 0) {
		if (lon1 < lon2)
			result = M_PI;
		else
			result = 2 * M_PI;
	} else {
		double k = cos(lat2) * sin(lon2 - lon1) / sin(b);
		(k > 1 ? k = 1 : (k < -1 ? k = -1 : k)); // TODO why k is out of (-1;1) ?
		result = asin(k);

		if ((lat2 > lat1) && (lon2 > lon1)) {
		} else if ((lat2 < lat1) && (lon2 < lon1)) {
			result = M_PI - result;
		} else if ((lat2 < lat1) && (lon2 > lon1)) {
			result = M_PI - result;
		} else if ((lat2 > lat1) && (lon2 < lon1)) {
			result = result + 2 * M_PI;
		}
	}
	result = result * 180 / M_PI;
	*a = result;
}

double mod2(double y, double x) {
	return y - x*floor(y/x);
}

double azimuth2 (double rlat1, double rlon1, double rlat2, double rlon2) {
	double tc;
	double lat1 = rlat1 * M_PI / 180;
	double lon1 = rlon1 * M_PI / 180;
	double lat2 = rlat2 * M_PI / 180;
	double lon2 = rlon2 * M_PI / 180;

	tc = mod2(atan2(sin(lon1-lon2)*cos(lat2),
		  cos(lat1)*sin(lat2)-sin(lat1)*cos(lat2)*cos(lon1-lon2)), 2*M_PI);
	if (tc < 0)
		return -tc * 180 / M_PI;
	else
		return 360 - tc * 180 / M_PI;
}

int NL(double lat) {
	lat = fabs(lat);
	if (lat < 10.47047130)
		return 59;
	if (lat < 14.82817437)
		return 58;
	if (lat < 18.18626357)
		return 57;
	if (lat < 21.02939493)
		return 56;
	if (lat < 23.54504487)
		return 55;
	if (lat < 25.82924707)
		return 54;
	if (lat < 27.93898710)
		return 53;
	if (lat < 29.91135686)
		return 52;
	if (lat < 31.77209708)
		return 51;
	if (lat < 33.53993436)
		return 50;
	if (lat < 35.22899598)
		return 49;
	if (lat < 36.85025108)
		return 48;
	if (lat < 38.41241892)
		return 47;
	if (lat < 39.92256684)
		return 46;
	if (lat < 41.38651832)
		return 45;
	if (lat < 42.80914012)
		return 44;
	if (lat < 44.19454951)
		return 43;
	if (lat < 45.54626723)
		return 42;
	if (lat < 46.86733252)
		return 41;
	if (lat < 48.16039128)
		return 40;
	if (lat < 49.42776439)
		return 39;
	if (lat < 50.67150166)
		return 38;
	if (lat < 51.89342469)
		return 37;
	if (lat < 53.09516153)
		return 36;
	if (lat < 54.27817472)
		return 35;
	if (lat < 55.44378444)
		return 34;
	if (lat < 56.59318756)
		return 33;
	if (lat < 57.72747354)
		return 32;
	if (lat < 58.84763776)
		return 31;
	if (lat < 59.95459277)
		return 30;
	if (lat < 61.04917774)
		return 29;
	if (lat < 62.13216659)
		return 28;
	if (lat < 63.20427479)
		return 27;
	if (lat < 64.26616523)
		return 26;
	if (lat < 65.31845310)
		return 25;
	if (lat < 66.36171008)
		return 24;
	if (lat < 67.39646774)
		return 23;
	if (lat < 68.42322022)
		return 22;
	if (lat < 69.44242631)
		return 21;
	if (lat < 70.45451075)
		return 20;
	if (lat < 71.45986473)
		return 19;
	if (lat < 72.45884545)
		return 18;
	if (lat < 73.45177442)
		return 17;
	if (lat < 74.43893416)
		return 16;
	if (lat < 75.42056257)
		return 15;
	if (lat < 76.39684391)
		return 14;
	if (lat < 77.36789461)
		return 13;
	if (lat < 78.33374083)
		return 12;
	if (lat < 79.29428225)
		return 11;
	if (lat < 80.24923213)
		return 10;
	if (lat < 81.19801349)
		return 9;
	if (lat < 82.13956981)
		return 8;
	if (lat < 83.07199445)
		return 7;
	if (lat < 83.99173563)
		return 6;
	if (lat < 84.89166191)
		return 5;
	if (lat < 85.75541621)
		return 4;
	if (lat < 86.53536998)
		return 3;
	if (lat < 87.00000000)
		return 2;
	return 1;
}

/*double modulo(double x, double y) {
 double r;
 r = x - y * floor(x / y);
 return r;
 }*/

int modulo(int x, int y) {
	int r;
	r = x % y;
	if (r < 0)
		r += y;
	return r;
}

unsigned int crc_data(char * buf, int length) {
	unsigned int crc = 0;
	int i;

	if ((length != 11) && (length != 4)) {
		DEBUG("crc_data(): wrong data length %i\n", length);
		return 0;
	}

	for (i = 0; i < length; i++) {
		crc = crc_table[((crc >> 16) ^ buf[i]) & 0xff] ^ (crc << 8);
	}
	return crc & 0xffffff;
}

void flight_status(unsigned char *data, int *ground, int *alert, int *spi) {
	char fs = *data & 0x07;
	//DEBUG("FS:%d\n",fs);
	if (fs == 0) {
		*ground = 0;
		*alert = 0;
		*spi = 0;
	} // airborne
	else if (fs == 1) {
		*ground = -1;
		*alert = 0;
		*spi = 0;
	} // ground
	else if (fs == 2) {
		*ground = 0;
		*alert = -1;
		*spi = 0;
	} // airborne + squawk change
	else if (fs == 3) {
		*ground = -1;
		*alert = -1;
		*spi = 0;
	} // ground + squawk change
	else if (fs == 4) {
		ground = NULL;
		*alert = -1;
		*spi = -1;
	} else if (fs == 5) {
		ground = NULL;
		*alert = 0;
		*spi = -1;
	} // airborne or ground, spi (see fs == 4)
}

unsigned int find_squawk(unsigned char *data2, unsigned char *data3,
		int *emergency) {
	int squawk = (((*data3 >> 7) & 0x01) << 11) | (((*data2 >> 1) & 0x01) << 10)
			| (((*data2 >> 3) & 0x01) << 9) |

			(((*data3 >> 1) & 0x01) << 8) | (((*data3 >> 3) & 0x01) << 7)
			| (((*data3 >> 5) & 0x01) << 6) |

			((*data2 & 0x01) << 5) | (((*data2 >> 2) & 0x01) << 4)
			| (((*data2 >> 4) & 0x01) << 3) |

			((*data3 & 0x01) << 2) | (((*data3 >> 2) & 0x01) << 1)
			| (((*data3 >> 4) & 0x01));

	if (squawk == 07500 || squawk == 07600 || squawk == 07700) {
		*emergency = -1;
	} else {
		*emergency = 0;
	}
	return squawk;
}

/* The following two functions are submitted by kstewart
 * at http://www.ccsinfo.com/forum/viewtopic.php?p=77544
 */

unsigned int GrayToBinary(unsigned int num) {
	unsigned int temp;

	temp = num ^ (num >> 8);
	temp ^= (temp >> 4);
	temp ^= (temp >> 2);
	temp ^= (temp >> 1);
	return temp;
}

int GillhamToAltitude(int GillhamValue) {
// Data must be in following order (MSB to LSB)
// D1 D2 D4 A1 A2 A4 B1 B2 B4 C1 C2 C4
	int Result;
	int FiveHundreds;
	int OneHundreds;

	// Convert Gillham value using gray code to binary conversion algorithm.
	// Get rid of Hundreds (lower 3 bits).
	FiveHundreds = GillhamValue >> 3;

	// Strip off Five Hundreds leaving lower 3 bits.
	OneHundreds = GillhamValue & 0x07;

	FiveHundreds = GrayToBinary(FiveHundreds);
	OneHundreds = GrayToBinary(OneHundreds);

	// Check for invalid codes.
	if (OneHundreds == 5 || OneHundreds == 6 || OneHundreds == 0) {
		Result = 0;
		return Result;
	}

	// Remove 7s from OneHundreds.
	if (OneHundreds == 7)
		OneHundreds = 5;

	// Correct order of OneHundreds.
	if (FiveHundreds % 2)
		OneHundreds = 6 - OneHundreds;

	// Convert to feet and apply altitude datum offset.
	Result = (int) ((FiveHundreds * 500) + (OneHundreds * 100)) - 1300;

	return Result;
}

int ac_alt_decode(unsigned char *data2, unsigned char *data3) {
	// port of Spruts code from Delphi to C
	unsigned char a1, a2, a4, b1, b2, b4, c1, c2, c4, d2, d4, m1, q1;
	int alt;

	a4 = ((*data3 >> 7) & 0x01);
	a2 = ((*data2 >> 1) & 0x01);
	a1 = ((*data2 >> 3) & 0x01);

	b4 = ((*data3 >> 1) & 0x01);
	b2 = ((*data3 >> 3) & 0x01);
	b1 = ((*data3 >> 5) & 0x01);

	c4 = (*data2 & 0x01);
	c2 = ((*data2 >> 2) & 0x01);
	c1 = ((*data2 >> 4) & 0x01);

	d4 = (*data3 & 0x01);
	d2 = ((*data3 >> 2) & 0x01);

	//DEBUG("A4:%d, A2:%d, A1:%d, B4:%d, B2:%d, B1:%d, C4:%d, C2:%d, C1:%d, D4:%d, D2:%d\n", a4, a2, a1, b4, b2, b1, c4,c2,c1, d4,d2);

	m1 = ((*data3 >> 6) & 0x01);
	q1 = ((*data3 >> 4) & 0x01);

	if (q1) {
		alt = (c1 * 1024 + a1 * 512 + c2 * 256 + a2 * 128 + c4 * 64 + a4 * 32
				+ b1 * 16 + b2 * 8 + d2 * 4 + b4 * 2 + d4) * 25 - 1000;
		if (m1) {
			alt = round((alt + 1000.0) / 25.0);
			alt = alt * 10 - 300;
			alt = round(alt * 3.281);
		}
	} else {
		int alt_code = 1024 * d2 + 512 * d4 + 256 * a1 + 128 * a2 + 64 * a4
				+ 32 * b1 + 16 * b2 + 8 * b4 + 4 * c1 + 2 * c2 + c4;
		// in feets
		alt = GillhamToAltitude(alt_code);
	}
	return alt;
}

int decode_message56(adsb_data * data, char * msg) {
	unsigned char * rawdata = (unsigned char *) data->data;
	unsigned char df;
	int emergency = 0;
	int spi = 0;
	int alert = 0;
	int ground = 0;
	unsigned int icao;
	struct timespec ts;
	struct tm *now;
	char date_gen[32];
	char time_gen[32];

	clock_gettime(CLOCK_REALTIME, &ts);
	now = localtime(&ts.tv_sec);
	sprintf(date_gen, "%02d/%02d/%02d", now->tm_year + 1900, now->tm_mon + 1,
			now->tm_mday);
	sprintf(time_gen, "%02d:%02d:%02d.%03lu", now->tm_hour, now->tm_min,
			now->tm_sec, ts.tv_nsec / 1000000);

	df = (rawdata[0] >> 3) & 0x1F;
	//DEBUG("\nDF:%u\t", df);

	switch (df) {
	// Short air-air survaillance (ACAS)
	case 0: {
		icao = crc_data((char *) data->data, 4)
				^ ((rawdata[4] << 16) | (rawdata[5] << 8) | rawdata[6]);
		int alt = ac_alt_decode(&rawdata[2], &rawdata[3]);
		if (db_update_alt(&icao, &alt, NULL, data, &df) == 1) {
			sprintf(msg, "MSG,5,,,%06X,,%s,%s,%s,%s,,%d,,,,,,,,,,\n", icao,
					date_gen, time_gen, date_gen, time_gen, alt);
			DEBUG("\nDF0\tICAO:%06x Alt:%d", icao, alt);
		} else
			return 0;
		break;
	}
	// Altitude reply
	case 4: {
		icao = crc_data((char *) data->data, 4)
				^ ((rawdata[4] << 16) | (rawdata[5] << 8) | rawdata[6]);
		int alt = ac_alt_decode(&rawdata[2], &rawdata[3]);
		flight_status(&rawdata[0], &ground, &alert, &spi);
		if (db_update_alt(&icao, &alt, &ground, data, &df) == 1) {
			sprintf(msg, "MSG,5,,,%06X,,%s,%s,%s,%s,,%d,,,,,,,%d,%d,%d,%d\n",
					icao, date_gen, time_gen, date_gen, time_gen, alt, alert,
					emergency, spi, ground);
			DEBUG("\nDF4\tICAO:%06x Alt:%d Alert:%d Emerg.:%d SPI:%d Gnd.:%d",
					icao, alt, alert, emergency, spi, ground);
		} else
			return 0;
		break;
	}
	// Identify reply
	case 5: {
		icao = crc_data((char *) data->data, 4)
				^ ((rawdata[4] << 16) | (rawdata[5] << 8) | rawdata[6]);
		unsigned int squawk = find_squawk(&rawdata[2], &rawdata[3], &emergency);
		flight_status(&rawdata[0], &ground, &alert, &spi);
		if (db_update_squawk(&icao, &squawk, &ground, data, &df) == 1) {
			sprintf(msg, "MSG,6,,,%06X,,%s,%s,%s,%s,,,,,,,,%04o,%d,%d,%d,%d\n",
					icao, date_gen, time_gen, date_gen, time_gen, squawk, alert,
					emergency, spi, ground);
			DEBUG(
					"\nDF5\tICAO:%06x Squawk:%04o Alert:%d Emerg.:%d SPI:%d Gnd.:%d",
					icao, squawk, alert, emergency, spi, ground);
		} else
			return 0;
		break;
	}
	// All-call reply
	case 11: {
		/* Thanks to Andrew Hall and ModeSBeast@yahoogroups.com with "Bad CRC!"
		 The lower six bits (interrogator ID) are not checked for DF11, discussion is here
		 http://groups.yahoo.com/group/ModeSBeast/messages/1227?threaded=1&m=e&var=1&tidx=1
		 */
		int c = crc_data((char *) data->data, 4);
		if ((c & 0xFFFFC0) != ((rawdata[4] << 16) | (rawdata[5] << 8) | (rawdata[6] & 0xC0))) {
			//DEBUG("Bad CRC !");
			return 0;
		}
		icao = (rawdata[1] << 16) | (rawdata[2] << 8) | rawdata[3];
		if (db_update_icao(&icao, data) == 0) {
			sprintf(msg, "MSG,8,,,%06X,,%s,%s,%s,%s,,,,,,,,,,,,\n", icao,
					date_gen, time_gen, date_gen, time_gen);
			DEBUG("\nDF11\tICAO:%06x", icao);
		} else
			return 0;
		break;
	}
	default:
		if (data->type == TYPE_RTLSDR) // supress noise on SDR dongle
			return 0;
		DEBUG("\nDF%u not implemented yet (56 bit message).", df);
	}
#ifdef MLAT
	if ((with_mlat) && strlen(msg) && (data->lat != 0) && (data->lon != 0))
		data_mlat(&data->source_id, &icao, (unsigned char*)&data->avr_data, &data->timestamp);
#endif
	return 1;
}

int decode_message(adsb_data *data, char *msg) {
	unsigned char * rawdata = (unsigned char *) data->data;
	unsigned char df = 0;
	unsigned int icao;
	struct timespec ts;
	struct tm *now;
	char date_gen[32];
	char time_gen[32];

	clock_gettime(CLOCK_REALTIME, &ts);
	now = localtime(&ts.tv_sec);
	sprintf(date_gen, "%02d/%02d/%02d", now->tm_year + 1900, now->tm_mon + 1,
			now->tm_mday);
	sprintf(time_gen, "%02d:%02d:%02d.%03lu", now->tm_hour, now->tm_min,
			now->tm_sec, ts.tv_nsec / 1000000);

	df = (rawdata[0] >> 3) & 0x1F;

	//DEBUG("\nDF:%u\t", df);

	switch (df) {
	// Long air-air survaillance (ACAS)
	case 16: {
		icao = crc_data((char *) data->data, 11)
				^ ((rawdata[11] << 16) | (rawdata[12] << 8) | rawdata[13]);
		int alt = ac_alt_decode(&rawdata[2], &rawdata[3]);
		if (db_update_alt(&icao, &alt, NULL, data, &df) == 1) {
			sprintf(msg, "MSG,5,,,%06X,,%s,%s,%s,%s,,%d,,,,,,,,,,\n", icao,
					date_gen, time_gen, date_gen, time_gen, alt);
			DEBUG("\nDF16\tICAO:%06x Alt:%d", icao, alt);
		} else
			return 0;
		break;
	}
	// Extended squitter
	case 17:
	case 18: {
		int c = crc_data((char *) data->data, 11);
		if (c != ((rawdata[11] << 16) | (rawdata[12] << 8) | rawdata[13])) { // TODO FEC
			//DEBUG("Bad CRC !");
			return 0;
		}
		icao = (rawdata[1] << 16) | (rawdata[2] << 8) | rawdata[3];
		unsigned char tc = (rawdata[4] >> 3) & 0x1f;
		
		if ((tc > 0) && (tc < 5)) {	// aircraft ID, and category
			char callsign[9];
			char cat[3];
			char *t = "DCBA";
			sprintf(cat, "%c%u", t[tc - 1], rawdata[4] & 0x07);
			bzero(&callsign, 9);
			unsigned int cs = (rawdata[5] << 16) | (rawdata[6] << 8)
					| (rawdata[7]);
			callsign[0] = cs_tbl[(cs >> 18) & 0x3f];
			callsign[1] = cs_tbl[(cs >> 12) & 0x3f];
			callsign[2] = cs_tbl[(cs >> 6) & 0x3f];
			callsign[3] = cs_tbl[cs & 0x3f];
			cs = (rawdata[8] << 16) | (rawdata[9] << 8) | rawdata[10];
			callsign[4] = cs_tbl[(cs >> 18) & 0x3f];
			callsign[5] = cs_tbl[(cs >> 12) & 0x3f];
			callsign[6] = cs_tbl[(cs >> 6) & 0x3f];
			callsign[7] = cs_tbl[cs & 0x3f];

			db_update_callsign(&icao, callsign, cat, data, &df);
			sprintf(msg, "MSG,1,,,%06X,,%s,%s,%s,%s,%s,,,,,,,,0,0,0,0\n", icao,
					date_gen, time_gen, date_gen, time_gen, callsign);
			DEBUG("\nDF%u.%u\tICAO:%06x Callsign:%s", df, tc, icao, callsign);
		} else if ((tc > 8) && (tc < 19)) { // aircraft air position
			double d = 0;
			unsigned char f = (rawdata[6] >> 2) & 0x01;
			int lat = ((rawdata[8] >> 1) & 0x7f) | (rawdata[7] << 7)
					| ((rawdata[6] & 0x03) << 15);
			int lon = rawdata[10] | (rawdata[9] << 8)
					| ((rawdata[8] & 0x01) << 16);
			int alt = 25
					* (((rawdata[6] >> 4) & 0x0f)
							| (((rawdata[5] >> 1) & 0x7f) << 4)) - 1000;
			int ground = 0; // we know we're in air

			cpr_info ci = db_update_cpr(&icao, &f, &lat, &lon,
					(int *) &data->source_id);

			if ((ci.lat0 == 0) || (ci.lat1 == 0))
				return 0; //no odd and even both

			int j = floor((59. * ci.lat0 - 60. * ci.lat1) / 131072. + 0.5);

			double rlat0 = 6. * (modulo(j, 60.) + ci.lat0 / 131072.);
			double rlat1 = 6.101694915254237288
					* (modulo(j, 59.) + ci.lat1 / 131072.);

			rlat0 = (rlat0 < 270 ? rlat0 : rlat0 - 360.);
			rlat1 = (rlat1 < 270 ? rlat1 : rlat1 - 360.);

			int NL0 = NL(rlat0);
			int NL1 = NL(rlat1);

			if (NL0 != NL1)
				return 0;

			int m = floor(
					(ci.lon0 * (NL0 - 1) - ci.lon1 * NL1) / 131072. + 0.5);

			double dlon, rlat, rlon;

			if (f == 0) {
				int ni = (NL0 > 1 ? NL0 : 1);
				dlon = 360. / (double) ni;
				rlat = rlat0;
				rlon = dlon * (modulo(m, ni) + (double) ci.lon0 / 131072.);
			} else {
				int ni = ((NL1 - 1) > 1 ? (NL1 - 1) : 1);
				dlon = 360. / (double) ni;
				rlat = rlat1;
				rlon = dlon * (modulo(m, ni) + (double) ci.lon1 / 131072.);
			}

			rlon = (rlon > 180 ? rlon - 360. : rlon); // western long.

			if ((data->lat != 0) && (data->lon != 0)) {
				d = dist(rlat, rlon, data->lat, data->lon);
				if (d > MAXDIST) {
					DEBUG(
							"\nDF%u.%u\tICAO:%06x Drop air position (too far...) %f %f radar at %f %f",
							df, tc, icao, rlat, rlon, data->lat, data->lon);
					return 0; //reject too far position
				}
			}
			if ((ci.lat != 0) && (ci.lon != 0)
					&& (dist(rlat, rlon, ci.lat, ci.lon) > MAXSTEP))
				return 0; // reject too large track step

			db_update_position(&icao, &rlat, &rlon, data);
			db_update_alt(&icao, &alt, &ground, data, &df);
			sprintf(msg,
					"MSG,3,,,%06X,,%s,%s,%s,%s,,%d,,,%1.5f,%1.5f,,,0,0,0,0\n",
					icao, date_gen, time_gen, date_gen, time_gen, alt, rlat,
					rlon);
			DEBUG("\nDF%u.%u\tICAO:%06x Alt:%d Lat:%1.4f Lon:%1.4f", df, tc,
					icao, alt, rlat, rlon);
			if (d) DEBUG(" Distance:%.1f km", d);
		} else if ((tc > 4) && (tc < 9)) { // aircraft surface position
			double d = 0;
			if ((data->lat == 0) && (data->lon == 0)) {
				DEBUG(
						"\nDF%u.%u\tICAO:%06x Surface position is unavailable. Set the receiver location (--lat --lon options).",
						df, tc, icao);
				return 0; // can't find surface position
			}

			unsigned char f = (rawdata[6] >> 2) & 0x01;
			int lat = ((rawdata[8] >> 1) & 0x7f) | (rawdata[7] << 7)
					| ((rawdata[6] & 0x03) << 15);
			int lon = rawdata[10] | (rawdata[9] << 8)
					| ((rawdata[8] & 0x01) << 16);
			int alt = 0;
			int ground = -1; // we know we're on ground

			int gnd_trk = 0, gnd_move = 0;
			float gnd_trk_val = 0;

			if (rawdata[5] & 0x08) { // ground track is valid
				gnd_trk = ((rawdata[6] >> 4) & 0x0f)
						| ((rawdata[5] & 0x07) << 4);
				gnd_trk_val = (gnd_trk & 0x01) * 360. / 128.
						+ ((gnd_trk >> 1) & 0x01) * 360. / 64.
						+ ((gnd_trk >> 2) & 0x01) * 360. / 32.
						+ ((gnd_trk >> 3) & 0x01) * 360. / 16.
						+ ((gnd_trk >> 4) & 0x01) * 360. / 8.
						+ ((gnd_trk >> 5) & 0x01) * 360. / 4.
						+ ((gnd_trk >> 6) & 0x01) * 360. / 2.;
			}

			float gnd_spd = 0;

			gnd_move = ((rawdata[5] >> 4) & 0x0f) | ((rawdata[4] & 0x07) << 4);
			if (gnd_move < 2)
				gnd_spd = 0;
			else if ((gnd_move >= 2) && (gnd_move <= 8))
				gnd_spd = 0.125 + (gnd_move - 2) * 0.125;
			else if ((gnd_move >= 9) && (gnd_move <= 12))
				gnd_spd = 1 + (gnd_move - 9) * 0.25;
			else if ((gnd_move >= 13) && (gnd_move <= 38))
				gnd_spd = 2 + (gnd_move - 13) * 0.5;
			else if ((gnd_move >= 39) && (gnd_move <= 93))
				gnd_spd = 15 + (gnd_move - 39);
			else if ((gnd_move >= 94) && (gnd_move <= 108))
				gnd_spd = 70 + (gnd_move - 94) * 2;
			else if ((gnd_move >= 109) && (gnd_move <= 123))
				gnd_spd = 100 + (gnd_move - 109) * 5;
			else
				gnd_spd = 175;
			gnd_spd *= 1.852; // in km/h units

			int vr = 0;
			db_update_speed_heading(&icao, &gnd_spd, &gnd_trk_val, &vr, data, &df);
			cpr_info ci = db_update_cpr(&icao, &f, &lat, &lon,
					(int *) &data->source_id);

			if ((ci.lat0 == 0) || (ci.lat1 == 0))
				return 0; //no odd and even both

			int j = floor((59. * ci.lat0 - 60. * ci.lat1) / 131072. + 0.5);

			double rlat0 = 1.5 * (modulo(j, 60) + ci.lat0 / 131072.);
			double rlat1 = 1.525423728813559322
					* (modulo(j, 59) + ci.lat1 / 131072.);

			//rlat0 = (rlat0 < 270 ? rlat0 : rlat0 - 360.);
			//rlat1 = (rlat1 < 270 ? rlat1 : rlat1 - 360.);

			int NL0 = NL(rlat0);
			int NL1 = NL(rlat1);

			if (NL0 != NL1)
				return 0;

			int m = floor(
					(ci.lon0 * (NL0 - 1) - ci.lon1 * NL1) / 131072. + 0.5);

			double dlon, rlat, rlon;

			if (f == 0) {
				int ni = (NL0 > 1 ? NL0 : 1);
				dlon = 90. / (double) ni;
				rlat = rlat0;
				rlon = dlon * (modulo(m, ni) + (double) ci.lon0 / 131072.);
			} else {
				int ni = ((NL1 - 1) > 1 ? (NL1 - 1) : 1);
				dlon = 90. / (double) ni;
				rlat = rlat1;
				rlon = dlon * (modulo(m, ni) + (double) ci.lon1 / 131072.);
			}

			// find real position based on receiver one
			int vcrd;
			double dcrd = 180.;
			double rcrd = 0.;
			for (vcrd = -180; vcrd < 270; vcrd += 90) {
				if (dcrd > fabs(vcrd + rlon - data->lon))
					rcrd = rlon + vcrd;
				dcrd = fabs(vcrd + rlon - data->lon);
			}
			rlon = rcrd;

			dcrd = 90.;
			for (vcrd = -90; vcrd < 90; vcrd += 90) {
				if (dcrd > fabs(vcrd + rlat - data->lat))
					rcrd = rlat + vcrd;
				dcrd = fabs(vcrd + rlat - data->lat);
			}
			rlat = rcrd;

			//DEBUG("\n******** Surface position %f %f", rlat, rlon);

			if ((data->lat != 0) && (data->lon != 0)) {
				d = dist(rlat, rlon, data->lat, data->lon);
				if (d > MAXDIST) {
					DEBUG(
							"\nDF%u.%u\tICAO:%06x Drop air position (too far...) %f %f radar at %f %f",
							df, tc, icao, rlat, rlon, data->lat, data->lon);
					return 0; //reject too far position
				}
			}
			if ((ci.lat != 0) && (ci.lon != 0)
					&& (dist(rlat, rlon, ci.lat, ci.lon) > MAXSTEP))
				return 0; // reject too large track step

			db_update_position(&icao, &rlat, &rlon, data);
			db_update_alt(&icao, &alt, &ground, data, &df);

			sprintf(msg,
					"MSG,2,,,%06X,,%s,%s,%s,%s,,,,%1.1f,%1.5f,%1.5f,,,0,0,0,0\n",
					icao, date_gen, time_gen, date_gen, time_gen, gnd_trk_val,
					rlat, rlon);
			DEBUG("\nDF%u.%u\tICAO:%06x Track:%1.1f Lat:%1.4f Lon:%1.4f", df,
					tc, icao, gnd_trk_val, rlat, rlon);
			if (d) DEBUG(" Distance:%.1f km", d);
		}

		else if (tc == 19) { // air speed
			unsigned char subtype = rawdata[4] & 0x07;
			if (subtype == 1) { // ground non supersonic speed
				double ew_spd = rawdata[6] | ((rawdata[5] & 0x03) << 8); // -1 TODO
				double ns_spd = ((rawdata[8] >> 5) & 0x07)
						| ((rawdata[7] & 0x7f) << 3);
				if ((ew_spd == 0) && (ns_spd == 0))
					return 0;

				unsigned char ew_dir = (rawdata[5] >> 2) & 0x01;
				unsigned char ns_dir = (rawdata[7] >> 7) & 0x01;
				float spd = floor(sqrt(ew_spd * ew_spd + ns_spd * ns_spd));

				float trk;

				if ((!ew_dir) && (!ns_dir))
					trk = (ew_spd == 0 ?
							0 : 90 - 180. / M_PI * atan(ns_spd / ew_spd));
				if ((!ew_dir) && (ns_dir))
					trk = (ew_spd == 0 ?
							180 : 90 + 180. / M_PI * atan(ns_spd / ew_spd));
				if ((ew_dir) && (ns_dir))
					trk = (ew_spd == 0 ?
							180 : 270 - 180. / M_PI * atan(ns_spd / ew_spd));
				if ((ew_dir) && (!ns_dir))
					trk = (ew_spd == 0 ?
							0 : 270 + 180. / M_PI * atan(ns_spd / ew_spd));
				int vr = ((rawdata[8] & 0x08) == 0 ? 1 : -1)
						* ((((rawdata[9] >> 2) & 0x3f)
								| (rawdata[8] & 0x07) << 6) - 1) * 64;
				db_update_speed_heading(&icao, &spd, &trk, &vr, data, &df);
				sprintf(msg,
						"MSG,4,,,%06X,,%s,%s,%s,%s,,,%1.1f,%1.1f,,,%i,,0,0,0,0\n",
						icao, date_gen, time_gen, date_gen, time_gen, spd, trk,
						vr);
				DEBUG("\nDF%u.19\tICAO:%06x GS:%1.0f Track:%1.0f %s VRate:%i",
						df, icao, spd, trk,
						((rawdata[8] & 0x10) ? "Baro" : "GNSS"), vr);
			} else if (subtype == 3) { // air non supersonic speed
				float trk = (rawdata[6] | ((rawdata[5] & 0x03) << 8)) * 360.0
						/ 1024.0;
				unsigned char as_type = (rawdata[7] >> 7) & 0x01; // 0=IAS, 1=TAS
				float spd = ((rawdata[8] >> 5) & 0x07)
						| ((rawdata[7] & 0x7f) << 3);
				if (spd == 0)
					return 0;
				int vr = ((rawdata[8] & 0x08) == 0 ? 1 : -1)
						* ((((rawdata[9] >> 2) & 0x3f)
								| (rawdata[8] & 0x07) << 6) - 1) * 64;
				db_update_speed_heading(&icao, &spd, &trk, &vr, data, &df);
				sprintf(msg,
						"MSG,4,,,%06X,,%s,%s,%s,%s,,,%1.1f,%1.1f,,,%i,,0,0,0,0\n",
						icao, date_gen, time_gen, date_gen, time_gen, spd, trk,
						vr);
				DEBUG("\nDF%u.19\tICAO:%06x %s:%1.0f Track:%1.0f Vertical:%i",
						df, icao, (as_type ? "TAS" : "IAS"), spd, trk, vr);
			}
		} else if (tc == 28) {
			unsigned char subtype = rawdata[4] & 0x07;
			if (subtype == 1) { // emergency status and Mode A (squawk)
				unsigned int squawk = (((rawdata[6] >> 7) & 0x01) << 11)
						| (((rawdata[5] >> 1) & 0x01) << 10)
						| (((rawdata[5] >> 3) & 0x01) << 9)
						| (((rawdata[6] >> 1) & 0x01) << 8)
						| (((rawdata[6] >> 3) & 0x01) << 7)
						| (((rawdata[6] >> 5) & 0x01) << 6)
						| ((rawdata[5] & 0x01) << 5)
						| (((rawdata[5] >> 2) & 0x01) << 4)
						| (((rawdata[5] >> 4) & 0x01) << 3)
						| ((rawdata[6] & 0x01) << 2)
						| (((rawdata[6] >> 2) & 0x01) << 1)
						| ((rawdata[6] >> 4) & 0x01);
				db_update_squawk(&icao, &squawk, NULL, data, &df);
				DEBUG("\nDF%u.28\tICAO:%06x Squawk:%04o Emerg.:%u", df, icao,
						squawk, (rawdata[5] >> 5) & 0x07);
			} else
			DEBUG("\nDF%u.28\tICAO:%06x Subtype %u not implemented yet.", df,
					icao, subtype);
		} else if (tc == 29) {
			unsigned char subtype = (rawdata[4] >> 1) & 0x03;
			if (subtype == 1) {	// Target state and status information as defined in DO-260B
				unsigned char alt_type = (rawdata[5] >> 7) & 0x01; // 0=MCP/FCU, 1=FMS
				uint16_t tgt_alt = ((((rawdata[6] >> 4) & 0x0f)
						| ((rawdata[5] & 0x7f) << 4)) - 1) * 32;
				float baro = (((rawdata[7] >> 3) & 0x1f)
						| ((rawdata[6] & 0x0f) << 5));
				unsigned char status = (rawdata[7] >> 2) & 0x01; // 1 - tgt_hdg is valid
				unsigned char sign = (rawdata[7] >> 1) & 0x01;
				float tgt_hdg = (((rawdata[8] >> 1) & 0x7f)
						| ((rawdata[7] & 0x01) << 7)) * 0.703125;
				tgt_hdg = (sign == 0 ? tgt_hdg : 360. - tgt_hdg);
				unsigned char ap = rawdata[9] & 0x01;
				unsigned char vnav = (rawdata[10] >> 7) & 0x01;
				unsigned char alt_hold = (rawdata[10] >> 6) & 0x01;
				unsigned char app_mode = (rawdata[10] >> 4) & 0x01;
				unsigned char tcas = (rawdata[10] >> 3) & 0x01;
				if (status)
					db_update_status(&icao, &alt_type, &tgt_alt, &baro,
							&tgt_hdg, &ap, &vnav, &alt_hold, &app_mode, &tcas,
							data, &df);
				else
					db_update_status(&icao, &alt_type, &tgt_alt, &baro, NULL,
							&ap, &vnav, &alt_hold, &app_mode, &tcas, data, &df);
				DEBUG(
						"\nDF%u.29\tICAO:%06x %s alt:%u Baro:%1.1f AP:%u (more info)",
						df, icao, (alt_type ? "FMS" : "MCP/FCU"), tgt_alt,
						(baro ? (baro - 1) * 0.8 + 800 : 0), ap);
			} else // subtype 0 - TODO fing good description
			DEBUG("\nDF%u.29\tICAO:%06x Subtype:%u not implemented yet.", df,
					icao, subtype);
		} else if (tc == 31) {
			unsigned char subtype = rawdata[4] & 0x07;
			DEBUG("\nDF%u.31\tICAO:%06x Subtype:%u", df, icao, subtype);
		} else {
			DEBUG("\nDF%u.%u\tICAO:%06x TC:%u not implemented yet.", df, tc,
					icao, tc);
		}
		break;
	}
	// COMM-B altitude reply
	case 20: {
		icao = crc_data((char *) data->data, 11)
				^ ((rawdata[11] << 16) | (rawdata[12] << 8) | rawdata[13]);
		int alt = ac_alt_decode(&rawdata[2], &rawdata[3]);
		if (db_update_alt(&icao, &alt, NULL, data, &df) == 1) {
			sprintf(msg, "MSG,5,,,%06X,,%s,%s,%s,%s,,%d,,,,,,,,,,\n", icao,
					date_gen, time_gen, date_gen, time_gen, alt);
			DEBUG("\nDF20\tICAO:%06x Alt:%d", icao, alt);
		} else
			return 0;
		break;
	}
	// COMM-B identify reply
	case 21: {
		int emergency = 0;
		int spi = 0;
		int alert = 0;
		int ground = 0;

		icao = crc_data((char *) data->data, 11)
				^ ((rawdata[11] << 16) | (rawdata[12] << 8) | rawdata[13]);
		unsigned int squawk = find_squawk(&rawdata[2], &rawdata[3], &alert);
		flight_status(&rawdata[0], &ground, &alert, &spi);
		if (db_update_squawk(&icao, &squawk, &ground, data, &df) == 1) {
			sprintf(msg, "MSG,6,,,%06X,,%s,%s,%s,%s,,,,,,,,%04o,%d,%d,%d,%d\n",
					icao, date_gen, time_gen, date_gen, time_gen, squawk, alert,
					emergency, spi, ground);
			DEBUG(
					"\nDF21\tICAO:%06x Squawk:%04o Alert:%d Emerg.:%d SPI:%d Gnd.:%d",
					icao, squawk, alert, emergency, spi, ground);
		} else
			return 0;
		break;
	}
	default:
		if (data->type == TYPE_RTLSDR) // supress noise on SDR dongle
			return 0;
		DEBUG("\nDF%u not implemented yet (112 bit message).", df);
	}
#ifdef MLAT
	if ((with_mlat) && strlen(msg) && (data->lat != 0) && (data->lon != 0))
		data_mlat(&data->source_id, &icao, (unsigned char*)&data->avr_data, &data->timestamp);
#endif
	return 1;
}
