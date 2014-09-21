CFLAGS=-Wall -g -O3
LDFLAGS=-levent -lbsd 
BINS=http
CC=g++
FILE=test.cc httpserver.cc 
all: ${BINS}

http: clean
	${CC} ${FILE} ${LDFLAGS} -o ${BINS}

debug: clean
	${CC} ${FILE} -DDEBUG ${LDFLAGS} -o ${BINS}

clean:
		/bin/rm -rf ${BINS} *.o core *.core
