/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * main.cc
 * Copyright (C) 2014 ubuntu 14.04 <mike@ubuntu>
	 * 
 * http is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
	 * 
 * http is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <boost/lockfree/queue.hpp>
#include <memory>
#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <fstream>
#include "const.h"
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
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <signal.h>
#include <queue>

#define ROOT_PATH "/home/mike/"
#define SERVER_PORT 5555
#define MAX_PATH_SIZE 1024

typedef enum {
    GET,
    HEAD,
    UNKNOWN_C
} command_t;

typedef enum {
    OK,
    NOT_FOUND,
    BAD_REQUEST
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
std::mutex m, tm;
std::map <std::thread::id, int> threadId;
std::exception_ptr initExcept;
bool volatile isRun = true;
evutil_socket_t soc = -1;
struct client_t {
    int fd;
    int openedFd;
    struct event_base *evbase;
    struct bufferevent *buf_ev;
    struct evbuffer *output_buffer;
    ~client_t(){
#ifdef DEBUG
        std::cout << "Close" << std::endl;
#endif
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
        if (openedFd >= 0) {
            close(openedFd);
            openedFd = -1;
        }
        if (buf_ev != NULL) {
            bufferevent_free(buf_ev);
            buf_ev = NULL;
        }
        if (evbase != NULL) {
            event_base_free(evbase);
            evbase = NULL;
        }
        if (output_buffer != NULL) {
            evbuffer_free(output_buffer);
            output_buffer = NULL;
        }
    }
    client_t() {
        fd = -1;
        openedFd = -1;
    }
    client_t(const client_t&cl) {
        fd = cl.fd;
        openedFd = cl.openedFd;
        evbase = cl.evbase;
        buf_ev = cl.buf_ev;
        output_buffer = cl.output_buffer;
    }
    client_t &operator=(const client_t &other) {
        if(this != &other) {
            fd = other.fd;
            openedFd = other.openedFd;
            evbase = other.evbase;
            buf_ev = other.buf_ev;
            output_buffer = other.output_buffer;
        }
        return *this;
    }
};

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

void urlDecode(char *str) {
    char tmp[MAX_PATH_SIZE] = {0};
    char *pch = strrchr(str, '?');
    unsigned int i;
    if (pch != NULL)
        str[pch - str] = '\0';
    pch = tmp;
    for(i = 0; i < strlen(str); i++) {
        if (str[i] != '%') {
            *pch++ = str[i];
            continue;
        }
        if (!isdigit(str[i+1]) || !isdigit(str[i+2])){
            *pch++ = str[i];
            continue;
        }
        *pch++ = ((str[i+1] - '0') << 4) | (str[i+2] - '0');
        i+=2;
    }
    *pch = '\0';
    strcpy(str, tmp);
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
void addHeader(httpHeader_t *h, struct evbuffer *buf) {
    if(h->status == OK) {
        //print response
        evbuffer_add_printf(buf, "HTTP/1.1 200 OK\n");
        char timeStr[64] = {'\0'};
        time_t now = time(NULL);
        strftime(timeStr, 64, "%Y-%m-%d %H:%M:%S", localtime(&now));
        evbuffer_add_printf(buf, "Date: %s\n", timeStr);
            evbuffer_add_printf(buf, "Server: BESTEUSERVER\n");
            evbuffer_add_printf(buf, "Content-Type: ");
        switch(h->type)
        {
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
        evbuffer_add_printf(buf, "\n");
        evbuffer_add_printf(buf, "Content-Length: %lu\n", h->length);
        evbuffer_add_printf(buf,"Connection: close\n\n");
    }
}
static struct event_base *evbase_accept;
/*typedef struct client{
        int fd;
        int openedFd;
        struct event_base *evbase;
        struct bufferevent *buf_ev;
        struct evbuffer *output_buffer;
} client_t;*/
/*class client {
    public:
        int fd;
        int openedFd;
        struct event_base *evbase;
        struct bufferevent *buf_ev;
        struct evbuffer *output_buffer;
        ~client();
        client(const client& cl) {
            fd = cl.fd;
            openedFd = cl.openedFd;
            evbase = cl.evbase;
            buf_ev = 

};*/

//std::queue<client_t*> q;
boost::lockfree::queue<client_t*> lq(128);

static void closeAll(client_t *client) {
#ifdef DEBUG
    std::cout << "Close" << std::endl;
#endif
    if (client != NULL) {
        if (client->fd >= 0) {
            close(client->fd);
            client->fd = -1;
        }
        if (client->openedFd >= 0) {
            close(client->openedFd);
            client->openedFd = -1;
        }
        if (client->buf_ev != NULL) {
            bufferevent_free(client->buf_ev);
            client->buf_ev = NULL;
        }
        if (client->evbase != NULL) {
            event_base_free(client->evbase);
            client->evbase = NULL;
        }
        if (client->output_buffer != NULL) {
            evbuffer_free(client->output_buffer);
            client->output_buffer = NULL;
        }
        free(client);
    }
}

void buffered_on_read(struct bufferevent *bev, void*arg) {
#ifdef DEBUG
    printf("On read\n");
#endif
    client_t *client = (client_t*) arg;
    char *line;
    size_t n;
    struct evbuffer *input =  bufferevent_get_input(bev);
    line = evbuffer_readln(input, &n, EVBUFFER_EOL_CRLF);
    char cmd[256], protocol[256], path[MAX_PATH_SIZE];
    httpHeader_t httpHeader;
    httpHeader.command = UNKNOWN_C;
    httpHeader.status = OK;
    if (n != 0) {
        int scaned = sscanf(line, "%s %s %s\n", cmd, path, protocol);
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
            if (path[0] != '/') {
                printf("BAD INPUtT\n");
                httpHeader.status = BAD_REQUEST;
            }
            urlDecode(path);
            httpHeader.type = getContentType(path);
            if (getDepth(path) == -1) {
                printf("BAD DEPTH\n");
                httpHeader.status = BAD_REQUEST;
            }
        }
        else {
            printf("Bad scanned\n");
            httpHeader.status = BAD_REQUEST;
        }
    }
    else {
        printf("OOO BAD N\n");
        httpHeader.status = BAD_REQUEST;
    }
    switch (httpHeader.status) {
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
    }
    printf("%s\n", path);
    free(line);
    if (httpHeader.status != BAD_REQUEST) {
        char fullPath[2048] = {'\0'};
        strcpy(fullPath, ROOT_PATH);
        strcat(fullPath, path);
        int fd = open(fullPath, O_RDONLY);
        if (fd < 0) {
            httpHeader.status = NOT_FOUND;
            printf("Can't open %s", fullPath);
        }
        client->openedFd = -1;
        struct stat st;
        httpHeader.length = lseek(fd, 0, SEEK_END);
        if (httpHeader.length == -1 || lseek(fd, 0, SEEK_SET) == -1) {
            httpHeader.status = BAD_REQUEST;
            printf("Cant seek\n");
        }
        addHeader(&httpHeader, client->output_buffer);
        if (fstat(fd, &st) < 0) {
            perror("fstat");
        }
        if (fd != -1 && httpHeader.status == OK && httpHeader.command == GET) {
            evbuffer_set_flags(client->output_buffer, EVBUFFER_FLAG_DRAINS_TO_FD);
            if(evbuffer_add_file(client->output_buffer, fd, 0, httpHeader.length) != 0) {
                perror("add file");
            }
        }
        if (bufferevent_write_buffer(bev, client->output_buffer)) {
            printf("Error sending data to client on fd %d\n", client->fd);
            closeAll(client);
        }
    }
}

void buffered_on_write(struct bufferevent *bev, void *arg) {
#ifdef DEBUG
    printf("On write\n");
    printf("exiting\n");
#endif
    client_t *cl =(client_t*) arg;
    event_base_loopbreak(cl->evbase);
    //closeAll(cl);
}
void on_accept(evutil_socket_t fd, short ev, void *arg) {
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    client_t *client;

    client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        warn("accept failed");
        return;
    }

    if (evutil_make_socket_nonblocking(client_fd) < 0) {
        printf("failed to set client socket to non-blocking\n");
        close(client_fd);
        return;
    }

    /*if ((client =(client_t*) malloc(sizeof(*client))) == NULL) {
        printf("failed to allocate memory for client state\n");
        close(client_fd);
        return;
    }*/
    client = new client_t();
    //memset(client, 0, sizeof(*client));
    client->fd = client_fd;

    if ((client->output_buffer = evbuffer_new()) == NULL) {
        printf("client output buffer allocation failed\n");
        return;
    }

    if ((client->evbase = event_base_new()) == NULL) {
        printf("client event_base creation failed\n");
        return;
    }

    client->buf_ev = bufferevent_socket_new(client->evbase, client_fd, BEV_OPT_CLOSE_ON_FREE);
    if ((client->buf_ev) == NULL) {
        return;
    }
    bufferevent_setcb(client->buf_ev, buffered_on_read, buffered_on_write, NULL, client);
    bufferevent_enable(client->buf_ev, EV_READ);
    client->openedFd = -1;
    event_base_dispatch(client->evbase);
    closeAll(client);
    //lq.push(client);
    //m.lock();
    //aqq.push_back(client);
    //m.unlock();
    //lq.push(fd);
    //event_base_dispatch(client->evbase);

}
static void sighandler(int signal) {
    fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal,
            strsignal(signal));
    isRun = false;
    exit(1);
}

void threadF(){
#ifdef DEBUG
    std::cout << "Thread " << std::this_thread::get_id() << " started" << std::endl;
#endif
    client_t *client;
    bool fl = false;
    int fd;
    while(isRun) {
        //m.lock();
       // client = aqq.pop_front();'
        if(lq.pop(client)) {
#ifdef DEBUG
            std::cout << "Poped element in " << std::this_thread::get_id() <<  std::endl;
#endif
            event_base_dispatch(client->evbase);
            closeAll(client);
        }
        //m.unlock();
    /*    if(lq.pop(fd)) {
            int client_fd;
            std::cout << fd << std::endl;
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            client_t *client;

            client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd < 0) {
                warn("accept failed");
                continue;
            }

            if (evutil_make_socket_nonblocking(client_fd) < 0) {
                printf("failed to set client socket to non-blocking\n");
                close(client_fd);
                continue;
            }

            if ((client =(client_t*) malloc(sizeof(*client))) == NULL) {
                printf("failed to allocate memory for client state\n");
                close(client_fd);
                continue;
            }
            memset(client, 0, sizeof(*client));
            client->fd = client_fd;

            if ((client->output_buffer = evbuffer_new()) == NULL) {
                printf("client output buffer allocation failed\n");
                continue;
            }

            if ((client->evbase = event_base_new()) == NULL) {
                printf("client event_base creation failed\n");
                continue;
            }

            client->buf_ev = bufferevent_socket_new(client->evbase, client_fd, BEV_OPT_CLOSE_ON_FREE);
            if ((client->buf_ev) == NULL) {
                continue;
            }
            bufferevent_setcb(client->buf_ev, buffered_on_read, buffered_on_write, NULL, client);
            bufferevent_enable(client->buf_ev, EV_READ);
            client->openedFd = -1;
            event_base_dispatch(client->evbase);
            std::cout << "FINISED" << std::endl;
            closeAll(client);
        }*/
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#ifdef DEBUG
    std::cout << "Exiting from thread: " << std::this_thread::get_id() << std::endl;
#endif
}
int main()
{
    /*evutil_socket_t listenfd;
    struct sockaddr_in listen_addr;
    struct event *ev_accept;
    int reuseaddr_on;

    sigset_t sigset;
    sigemptyset(&sigset);
    struct sigaction siginfo;
    siginfo.sa_handler = sighandler;
    siginfo.sa_mask = sigset;
    siginfo.sa_flags = SA_RESTART;
    sigaction(SIGINT, &siginfo, NULL);
    sigaction(SIGTERM, &siginfo, NULL);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(1, "listen failed");
        }

        memset(&listen_addr, 0, sizeof(listen_addr));
        listen_addr.sin_family = AF_INET;
        listen_addr.sin_addr.s_addr = INADDR_ANY;
        listen_addr.sin_port = htons(SERVER_PORT);
        if (bind(listenfd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) 
        < 0) {
        err(1, "bind failed");
        }
        if (listen(listenfd, 8) < 0) {
        err(1, "listen failed");
        }
        reuseaddr_on = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on,
        sizeof(reuseaddr_on));
        if (evutil_make_socket_nonblocking(listenfd) < 0) {
        err(1, "failed to set server socket to non-blocking");
        }

        if ((evbase_accept = event_base_new()) == NULL) {
        perror("Unable to create socket accept event base");
        close(listenfd);
        return 1;
        }
        ev_accept = event_new(evbase_accept, listenfd, EV_READ|EV_PERSIST, on_accept, NULL);
        event_add(ev_accept, NULL);
        for(int i = 0;i < 4; i++) {
    //    std::thread (threadF).detach();
    }
    event_base_dispatch(evbase_accept);
    */
    struct event_base *base;
    struct evconnlistener *listener;
    struct sockaddr_in sin;

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_port = htons(5555);
    sin.sin_family = AF_INET;

    listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
            LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
            (struct sockaddr*)&sin,
            sizeof(sin));

    if (!listener) {
        fprintf(stderr, "Could not create a listener!\n");
        return 1;
    }

    evconnlistener_set_error_cb(listener, accept_error_cb);

    event_base_dispatch(base);

    event_base_free(evbase_accept);
    evbase_accept = NULL;

    close(listenfd);
    
    printf("Server shutdown.\n");
}
