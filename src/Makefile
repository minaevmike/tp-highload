DEBUG=-Wall -g -DDEBUG
LDFLAGS=-levent -std=c++11 -g -levent_pthreads -lpthread
BINS=http
CC=g++
FILE=test.cc httpserver.cc 
all: ${BINS}

http: clean
	${CC} ${FILE} ${LDFLAGS} -o ${BINS}

debug: clean
	${CC} ${FILE} ${DEBUG} ${LDFLAGS} -o ${BINS}

clean:
		/bin/rm -rf ${BINS} *.o core *.core
