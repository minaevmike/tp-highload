CFLAGS=-Wall -g -O3
LDFLAGS=-levent -lbsd -lboost_system -std=c++11 -g -levent_pthreads -lpthread
BINS=http
CC=g++
FILE=test.cc httpserver.cc threadpool.cpp 
all: ${BINS}

http: clean
	${CC} ${FILE} ${LDFLAGS} -o ${BINS}

debug: clean
	${CC} ${FILE} -DDEBUG ${LDFLAGS} -o ${BINS}

clean:
		/bin/rm -rf ${BINS} *.o core *.core
