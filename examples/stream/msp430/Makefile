# makefile configuration
NAME            = stream
OBJECTS         = stream.o ../../../src/libtrivdl-msp430.o
CPU             = msp430g2553
DRIVER			= rf2500

#CFLAGS          = -mmcu=${CPU} -O0 -Wall -g
CFLAGS          = -mmcu=${CPU} -O2 -Wall

CFLAGS			+= -I.. -I../../../src -DDEBUG
#CFLAGS			+= -I.. -I../../../src

#switch the compiler (for the internal make rules)
CC              = msp430-gcc

.PHONY: all FORCE clean download download-jtag download-bsl dist

#all should be the first target. it's built when make is run without args
all: ${NAME}.elf ${NAME}.a43 ${NAME}.lst

#confgigure the next line if you want to use the serial download
download: download-jtag
#download: download-bsl

#additional rules for files
${NAME}.elf: ${OBJECTS}
	${CC} -mmcu=${CPU} -o $@ ${OBJECTS}

${NAME}.a43: ${NAME}.elf
	msp430-objcopy -O ihex $^ $@

${NAME}.lst: ${NAME}.elf
	msp430-objdump -dSt $^ >$@

download-jtag: all
	msp430-jtag -e ${NAME}.elf

download-bsl: all
	msp430-bsl -e ${NAME}.elf

clean:
	rm -f ${NAME}.elf ${NAME}.a43 ${NAME}.lst ${OBJECTS}

#backup archive
dist:
	tar czf dist.tgz *.c *.h *.txt makefile

upload: ${NAME}.elf
	mspdebug ${DRIVER} "prog ${NAME}.elf"

#dummy target as dependecy if something has to be build everytime
FORCE:

# project dependencies
# if implicit are not enough
stream.o: stream.c ../../../src/libtrivdl.h ../stream.h

