#include "httpserver.h"
#include <thread>
#include <iostream>
#include <event2/thread.h>
#include <boost/lockfree/queue.hpp>
#define PORT 8080


void listener_cb(struct evconnlistener *, evutil_socket_t, struct sockaddr *, int socklen, void *);
void conn_writecb(struct bufferevent *, void *);
void conn_readcb(struct bufferevent *, void *);
void conn_eventcb(struct bufferevent *, short, void *);
void accept_error_cb(struct evconnlistener *listener, void *ctx);
static void theadFunc1();
boost::lockfree::queue<int> q(1024);

void listener_cb_1(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *sa, int socklen, void *user_data)
{
	q.push(fd);
}

static void threadFunc1() {
	event_base *base = event_base_new();
	if (!base) {
		perror("Base new");
	}
	int fd;
	while(1) {
		event_base_loop(base, EVLOOP_ONCE);
		if(q.pop(fd)){
			listener_cb(NULL, fd, NULL, 0, (void*)base);
		}
	}
}
volatile bool isRun = true;
int main()
{
    struct event_base *base;
    struct evconnlistener *listener;
    struct sockaddr_in sin;
	int numThreads = 2 * std::thread::hardware_concurrency();
    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_port = htons(PORT);
    sin.sin_family = AF_INET;

    listener = evconnlistener_new_bind(base, listener_cb_1, (void *)base,
        LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
        (struct sockaddr*)&sin,
        sizeof(sin));
	printf("Starting server on localhost:%d\nNumber of working threads: %d\n", PORT, numThreads);
	for(int i = 0; i < numThreads; ++i) {
		std::thread (threadFunc1).detach();
	}
    if (!listener) {
        fprintf(stderr, "Could not create a listener!\n");
        return 1;
    }
    evconnlistener_set_error_cb(listener, accept_error_cb);

    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_base_free(base);
    printf("done\n");
    return 0;
}

void threadFunc(int fd, struct event_base *base) {
    struct bufferevent *bev;
    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }
    bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, NULL);
    bufferevent_enable(bev, EV_READ);
    //bufferevent_setcb(ev, conn_readcb, NULL, conn_eventcb, NULL);
}
void accept_error_cb(struct evconnlistener *listener, void *ctx)
{
    struct event_base *base = evconnlistener_get_base(listener);
    int err = EVUTIL_SOCKET_ERROR();
    fprintf(stderr, "Got an error %d (%s) on the listener. "
            "Shutting down.\n", err, evutil_socket_error_to_string(err));

    event_base_loopexit(base, NULL);
}

void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *sa, int socklen, void *user_data)
{
	event_base *base = (event_base*)user_data;
	bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
	if(!bev) {
		perror("Bev");
	}
	bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, NULL);
	if (bufferevent_enable(bev, EV_READ) == -1){
		perror("Read");
	}
}

void conn_readcb(struct bufferevent *pBufferEvent, void *user_data)
{
    writeData(pBufferEvent);
    bufferevent_setcb(pBufferEvent, NULL, conn_writecb, conn_eventcb, NULL);
    bufferevent_enable(pBufferEvent, EV_WRITE);
}

void conn_writecb(struct bufferevent *bev, void *user_data)
{
    struct evbuffer *output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0 && BEV_EVENT_EOF ) {
        bufferevent_free(bev);
    }
}

void conn_eventcb(struct bufferevent *bev, short events, void *user_data)
{
    if (events & BEV_EVENT_ERROR)
            perror("Error from bufferevent");
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
            bufferevent_free(bev);
    }
}
