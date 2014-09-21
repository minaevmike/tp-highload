#include "httpserver.h"


#define PORT 8080

void listener_cb(struct evconnlistener *, evutil_socket_t, struct sockaddr *, int socklen, void *);
void conn_writecb(struct bufferevent *, void *);
void conn_readcb(struct bufferevent *, void *);
void conn_eventcb(struct bufferevent *, short, void *);
void accept_error_cb(struct evconnlistener *listener, void *ctx);

int main()
{
    struct event_base *base;
    struct evconnlistener *listener;
    struct sockaddr_in sin;

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_port = htons(PORT);
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

    evconnlistener_free(listener);
    event_base_free(base);

    printf("done\n");
    return 0;
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
    struct event_base *base = (event_base*)user_data;
    struct bufferevent *bev;

    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }
    bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, NULL);
    int res = bufferevent_enable(bev, EV_READ);
}

void conn_readcb(struct bufferevent *pBufferEvent, void *user_data)
{
    writeData(pBufferEvent);

    //Setup write callback
    bufferevent_setcb(pBufferEvent, NULL, conn_writecb, conn_eventcb, NULL);
    bufferevent_enable(pBufferEvent, EV_WRITE);
}

void conn_writecb(struct bufferevent *bev, void *user_data)
{
    struct evbuffer *output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0 && BEV_EVENT_EOF ) {
        printf("flushed answer \n");
        bufferevent_free(bev);
    }
}

void conn_eventcb(struct bufferevent *bev, short events, void *user_data)
{
	printf("event callback\n");
    if (events & BEV_EVENT_ERROR)
            perror("Error from bufferevent");
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
            bufferevent_free(bev);
    }
}
