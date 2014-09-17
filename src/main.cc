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

#include <memory>
#include <cstdint>
#include <iostream>
#include <evhttp.h>
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

#define SERVER_PORT 5555

void setAttr(evkeyvalq *outHeader, std::string key, std::string value);
void setResponseFile(evbuffer *buf, std::string const &fileName);
const evhttp_uri* getUri(evhttp_request *req);
void requestThread();
std::string getPath(evhttp_request *req);
void onReq(evhttp_request *req, void*);
std::mutex m, tm;
std::map <std::thread::id, int> threadId;
std::exception_ptr initExcept;
bool volatile isRun = true;
evutil_socket_t soc = -1;
static struct event_base *evbase_accept;

void setAttr(evkeyvalq *outHeader, std::string key, std::string value) {
    evhttp_add_header(outHeader, key.c_str(), value.c_str());
}

typedef struct client {
    /* The client's socket. */
    int fd;

    /* The event_base for this client. */
    struct event_base *evbase;

    /* The bufferedevent for this client. */
    struct bufferevent *buf_ev;

    /* The output buffer for this client. */
    struct evbuffer *output_buffer;

    /* Here you can add your own application-specific
     * attributes which
     *      * are connection-specific. */
} client_t;

void setResponseFile(evbuffer *buf, std::string const &fileName) {           
	auto FileDeleter = [] (int *f) { if (*f != -1) close(*f); delete f; };
	std::lock_guard<std::mutex> lock(m);
	std::unique_ptr<int, decltype(FileDeleter)> file(new int(open(fileName.c_str(), 0)), FileDeleter);
	if (*file == -1) {
		std::cout << "Could not find content for uri: " << fileName << std::endl;
		return;
	}
	ev_off_t Length = lseek(*file, 0, SEEK_END);
	if (Length == -1 || lseek(*file, 0, SEEK_SET) == -1) {
		std::cout << "Failed to calc file size " << std::endl;
		return;
	}
	if (evbuffer_add_file(buf, *file, 0, Length) == -1) {
		std::cout << "Failed to make response." << std::endl;
		return;
	}
	*file.get() = -1;
}

const evhttp_uri* getUri(evhttp_request *req){
	return evhttp_request_get_evhttp_uri(req);
}
void requestThread(){
	try
	{
		std::unique_ptr<event_base, decltype(&event_base_free)> eventBase(event_base_new(), &event_base_free);
		if (!eventBase)
			throw std::runtime_error("Failed to create new base_event.");
        struct event *ev_accept;
		/*std::unique_ptr<evhttp, decltype(&evhttp_free)> evHttp(evhttp_new(eventBase.get()), &evhttp_free);
		if (!evHttp)
			throw std::runtime_error("Failed to create new evhttp.");
		evhttp_set_gencb(evHttp.get(), onReq, nullptr);
		if (soc == -1)
		{
			auto *boundSock = evhttp_bind_socket_with_handle(evHttp.get(), addr.c_str(), port);
			if (!boundSock)
				throw std::runtime_error("Failed to bind server socket.");
			if ((soc = evhttp_bound_socket_get_fd(boundSock)) == -1)
				throw std::runtime_error("Failed to get server socket for next instance.");
		}
		else
		{
			if (evhttp_accept_socket(evHttp.get(), soc) == -1)
				throw std::runtime_error("Failed to bind server socket for new instance.");
		}*/
		for ( ; isRun ; )
		{
			event_base_loop(eventBase.get(), EVLOOP_NONBLOCK);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	catch (...)
	{
		initExcept = std::current_exception();
	}
}

std::string getPath(evhttp_request *req){
	//std::cout << "URI: " << getUri(req) << std::endl;
	const char *path = evhttp_uri_get_path(getUri(req));
	return path==NULL ? std::string("") : std::string(path);
}

void onReq(evhttp_request *req, void*) {
	tm.lock();
	auto it = threadId.find(std::this_thread::get_id());
	if(it == threadId.end())
		threadId[std::this_thread::get_id()] = 0;
	else 
		threadId[std::this_thread::get_id()]++;
	//std::cout << std::this_thread::get_id() << std::endl;
    tm.unlock();
    auto *outBuf = evhttp_request_get_output_buffer(req);
    evbuffer_set_flags(outBuf, EVBUFFER_FLAG_DRAINS_TO_FD);
    if (!outBuf)
        return;
    auto *outHeader = evhttp_request_get_output_headers(req);
    setAttr(outHeader, std::string("Content-Type"), std::string("text/html"));
    setResponseFile(outBuf, documentRoot + getPath(req));
    //evbuffer_add_printf(OutBuf, "<html><body><center><h1>Hello Wotld!</h1></center></body></html>");
    evhttp_send_reply(req, HTTP_OK, "", outBuf);
}
void buffered_on_read(struct bufferevent *bef, void*arg) {
    printf("On read\n");
}
void buffered_on_write(struct bufferevent *bev, void *arg) {
    printf("On write\n");
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

    /* Set the client socket to non-blocking
     * mode. */
    if (evutil_make_socket_nonblocking(client_fd) < 0) {
        printf("failed to set client socket to non-blocking\n");
        close(client_fd);
        return;
    }

    /* Create a client object. */
    if ((client =(client_t*) malloc(sizeof(*client))) == NULL) {
        printf("failed to allocate memory for client state\n");
        close(client_fd);
        return;
    }
    memset(client, 0, sizeof(*client));
    client->fd = client_fd;

    /* Add any custom code
     * anywhere from here to
     * the end of this function
     *      * to initialize
     *      your
     *      application-specific
     *      attributes in the
     *      client struct.
     *           */

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
    event_base_dispatch(client->evbase);

}
static void sighandler(int signal) {
    fprintf(stdout, "Received signal %d: %s.  Shutting down.\n", signal,
            strsignal(signal));
    exit(1);
}
int main()
{
    evutil_socket_t listenfd;
    struct sockaddr_in listen_addr;
    struct event *ev_accept;
    int reuseaddr_on;

    /* Set signal handlers */
    sigset_t sigset;
    sigemptyset(&sigset);
    struct sigaction siginfo;
    siginfo.sa_handler = sighandler;
    siginfo.sa_mask = sigset;
    siginfo.sa_flags = SA_RESTART;
    sigaction(SIGINT, &siginfo, NULL);
    sigaction(SIGTERM, &siginfo, NULL);

    /* Create our listening socket. */
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

    /* Set
     * the
     * socket
     * to
     * non-blocking,
     * this
     * is
     * essential
     * in
     * event
     *      *
     *      based
     *      programming
     *      with
     *      libevent.
     *      */
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
    event_base_dispatch(evbase_accept);

    event_base_free(evbase_accept);
    evbase_accept = NULL;

    close(listenfd);

    printf("Server shutdown.\n");
}
