
#CFLAGS += -I.. -I../.. -I../../../src -DDEBUG -g
CFLAGS += -I.. -I../.. -I../../../src -g

DEPS = ../../serial.o ../../../src/libtrivdl-libc.o

all: stream

stream: stream.o $(DEPS) ../stream.h
	${CC} stream.o ${DEPS} -o stream

clean:
	rm -f stream stream.o ../../serial.o

