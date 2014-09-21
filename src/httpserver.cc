#include "httpserver.h"

contentType_t getContentType(char* path) {
	contentType_t type;
	char buf[256] = {'\0'};
	char *pch = strrchr(path, '.');
	if (pch != NULL) {
		memcpy(buf, pch + 1, strlen(path) - (pch - path) - 1);
		if (!strcmp(buf, "html"))
			type = HTML;
		else if (!strcmp(buf, "png"))
			type = PNG;
		else if (!strcmp(buf, "jpg"))
			type = JPG;
		else if (!strcmp(buf, "jpeg"))
			type = JPEG;
		else if (!strcmp(buf, "css"))
			type = CSS;
		else if (!strcmp(buf, "js"))
			type = JS;
		else if (!strcmp(buf, "gif"))
			type = GIF;
		else if (!strcmp(buf, "swf"))
			type = SWF;
		else 
			type = UNKNOWN_T;
	}
	else {
		type = UNKNOWN_T;
	}
	return type;
}

void urlDecode(char *src)
{
    char d[MAX_PATH_SIZE];
    char *pch = strrchr(src, '?');
    if (pch != NULL) {
        src[pch - src] = '\0';
    }
    char *dst = d, *s = src;
    char a, b;
    while (*src) {
        if ((*src == '%') &&
                ((a = src[1]) && (b = src[2])) &&
                (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
    strcpy(s, d);
}

int getDepth(char *path) {
	int depth = 0;
	char *ch = strtok(path, "/");
	while(ch != NULL) {
		if(!strcmp(ch, ".."))
			--depth;
		else
			++depth;
		if(depth < 0)
			return -1;
		ch = strtok(NULL, "/");
	}
	return depth;
}
void addServerHeader(struct evbuffer *buf) {
		char timeStr[64] = {'\0'};
		time_t now = time(NULL);
		strftime(timeStr, 64, "%Y-%m-%d %H:%M:%S", localtime(&now));
		//print date
		evbuffer_add_printf(buf, "Date: %s\r\n", timeStr);
		//print server
		evbuffer_add_printf(buf, "Server: BESTEUSERVER\r\n");
}

void addHeader(httpHeader_t *h, struct evbuffer *buf) {
	if(h->status == OK && (h->command == GET || h->command == HEAD)) {
		//print response
		evbuffer_add_printf(buf, "HTTP/1.1 200 OK\r\n");
        addServerHeader(buf);
		//print content type
        evbuffer_add_printf(buf, "Content-Type: ");
        switch(h->type) {
            case HTML:
                evbuffer_add_printf(buf, "text/html");
                break;
            case JPEG:
            case JPG:
                evbuffer_add_printf(buf, "image/jpeg");
                break;
            case PNG:
                evbuffer_add_printf(buf, "image/png");
                break;
            case CSS:
                evbuffer_add_printf(buf, "text/css");
                break;
            case JS:
                evbuffer_add_printf(buf, "application/x-javascript");
                break;
            case GIF:
                evbuffer_add_printf(buf, "image/gif");
                break;
            case SWF:
                evbuffer_add_printf(buf, "application/x-shockwave-flash");
                break;
            default:
                evbuffer_add_printf(buf, "text/html");
        }
        evbuffer_add_printf(buf, "\r\n");
        //print content length
        evbuffer_add_printf(buf, "Content-Length: %lu\r\n", h->length);
        //print connection close
        evbuffer_add_printf(buf, "Connection: close\r\n\r\n");
	}
    else if (h->status == NOT_FOUND) {
        evbuffer_add_printf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
        addServerHeader(buf);
        evbuffer_add_printf(buf, "\r\n");
		evbuffer_add_printf(buf, "404 NOT FOUND");
    } else if (h->status == BAD_REQUEST || h->command == UNKNOWN_C) { 
        evbuffer_add_printf(buf, "HTTP/1.1 405 BAD REQUEST\r\n");
        addServerHeader(buf);
        evbuffer_add_printf(buf, "\r\n");
		evbuffer_add_printf(buf, "405 BAD REQUEST");
    } else if (h->status == FORBIDDEN) {
        evbuffer_add_printf(buf, "HTTP/1.1 403 FORBIDDEN\r\n");
        addServerHeader(buf);
        evbuffer_add_printf(buf, "\r\n");
        evbuffer_add_printf(buf, "FORBIDDEN");
    }
}

void writeData(struct bufferevent *bev) {
	char *line;
	size_t n;
    char isIndex = 0;
	struct evbuffer *input =  bufferevent_get_input(bev);
    struct evbuffer *output = bufferevent_get_output(bev);
	line = evbuffer_readln(input, &n, EVBUFFER_EOL_CRLF);
	char cmd[MAX_OTHER_SIZE] = {'\0'}, protocol[MAX_OTHER_SIZE] = {'\0'}, path[MAX_PATH_SIZE] = {'\0'}, depthCheck[MAX_PATH_SIZE] = {'\0'};
	httpHeader_t httpHeader;
	httpHeader.command = UNKNOWN_C;
	httpHeader.status = OK;
    httpHeader.length = -1;
	if (n != 0) {
        /*printf("Line: %s\n", line);
        cmd = strtok(line, " ");
		path = strtok(NULL, " ");
        protocol = strtok(NULL, "\0");*/
        int scaned = sscanf(line, "%s %s %s\n", cmd, path, protocol);
		memcpy(depthCheck, path, MAX_PATH_SIZE);
        if (scaned == 3) {
			if (!strcmp(cmd, "GET")) {
				httpHeader.command = GET;
			}
			else if (!strcmp(cmd, "HEAD")) {
				httpHeader.command = HEAD;
			}
			else { 
				httpHeader.command = UNKNOWN_C;
			}
		/*	if (strcmp(protocol, "HTTP/1.1")) {
				printf("BAD PROTOCOL%s\n", protocol);
				httpHeader.status = BAD_REQUEST;
			}*/
			if (path[0] != '/') {
				httpHeader.status = BAD_REQUEST;
			}
            if (path[strlen(path) - 1] == '/') {
                strcat(path, "index.html");
                isIndex = 1;
            }
			urlDecode(path);
			httpHeader.type = getContentType(path);
			if (getDepth(depthCheck) == -1) {
				httpHeader.status = FORBIDDEN;
			}
		}
		else {
			httpHeader.status = BAD_REQUEST;
		}
	}
	else {
		httpHeader.status = BAD_REQUEST;
    }
    /*switch (httpHeader.status) {
        case BAD_REQUEST:
            printf("Bad request\n");
            break;
        case OK:
            printf("OK\n");
            break;
        case NOT_FOUND:
            printf("NOt found\n");
            break;
    }
    switch (httpHeader.command) {
        case UNKNOWN_C:
            printf("UNKNOWS\n");
            break;
        case GET:
            printf("GET\n");
            break;
        case HEAD:
            printf("HEAD\n");
            break;
    }*/
    char fullPath[2048] = {'\0'};
    strcpy(fullPath, ROOT_PATH);
    strcat(fullPath, path);
    int fd = -1;
    if (httpHeader.status == OK) {
        fd = open(fullPath, O_RDONLY);
        if (fd < 0) {
            if (isIndex == 0)
                httpHeader.status = NOT_FOUND;
            else
                httpHeader.status = FORBIDDEN;
        } else {
            struct stat st;
            httpHeader.length = lseek(fd, 0, SEEK_END);
            if (httpHeader.length == -1 || lseek(fd, 0, SEEK_SET) == -1) {
                httpHeader.status = BAD_REQUEST;
            }
            if (fstat(fd, &st) < 0) {
                perror("fstat");
            }
        }
    }
    addHeader(&httpHeader, output);
    if (fd != -1 && httpHeader.status == OK && httpHeader.command == GET) {
        evbuffer_set_flags(output, EVBUFFER_FLAG_DRAINS_TO_FD);
        if(evbuffer_add_file(output, fd, 0, httpHeader.length) != 0) {
            perror("add file");
        }
    }
    free(line);
    bufferevent_write_buffer(bev, output); 
}
