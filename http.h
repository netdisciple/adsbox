#ifndef __HTTP_H__
#define __HTTP_H__

typedef struct {
	int port;
} http_info;

void *http (void *);
void *http_server(void *);

#endif
