#ifndef __HTTPSERVER_
#define __HTTPSERVER_
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <time.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#define MAX_PATH_SIZE 1024
#define MAX_OTHER_SIZE 256
#define ROOT_PATH "/home/minaev/"
typedef enum {
	GET,
	HEAD,
	UNKNOWN_C
} command_t;

typedef enum {
	OK,
	NOT_FOUND,
	BAD_REQUEST,
    FORBIDDEN
} status_t;

typedef enum {
	HTML,
	JPEG,
	JPG,
	PNG,
	CSS,
	JS,
	GIF,
	SWF,
	UNKNOWN_T
} contentType_t;

typedef struct httpHeader {
	command_t command;
	status_t status;
	long length;
	contentType_t type;
} httpHeader_t;

contentType_t getContentType(char* path);
void urlDecode(char *str);
int getDepth(char *path);
void addHeader(httpHeader_t *h, struct evbuffer *buf);
void writeData(struct bufferevent *bev);
#endif
