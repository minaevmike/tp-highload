CFLAGS=-Wall -g -std=c++11
LDFLAGS=-levent -lpthread -std=c++11
BINS=http
CC=g++
FILE=main.cc
all: ${BINS}

http: clean
	${CC} ${FILE} ${LDFLAGS} -o ${BINS}

clean:
		/bin/rm -rf ${BINS} *.o core *.core
