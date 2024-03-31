IDIR=.
CC=gcc

# Compiler flags

# make mode=debug
ifeq ($(mode),debug)
	CFLAGS = -g -Wall -I$(IDIR)
else
	mode = release
	CFLAGS = -Wall -I$(IDIR)
endif

all: p2txt-all p2speccy-all hex2rem-all rem2bin-all hex2tap-all tap0auto-all p2ts1510-all

.PHONY: all

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.p: %.bas
	zmakebas -p -n $* -o $@ $<

%.p: %.txt
	zmakebas -p -n $* -o $@ $<

%.bin: %.a80
	z80asm --list=$*.txt -o $@ $<

p2txt-all: p2txt p2t-test1

p2txt: p2txt.o

p2t-test1: TEST1-p2txt-r.txt TEST1-p2txt-1.txt TEST1-p2txt-z.txt TEST1-p2txt-2.txt

TEST1-p2txt-r.txt: p2txt TEST1.p
	./p2txt -r TEST1.p > TEST1-p2txt-r.txt

TEST1-p2txt-1.txt: p2txt TEST1.p
	./p2txt -1 TEST1.p > TEST1-p2txt-1.txt

TEST1-p2txt-z.txt: p2txt TEST1.p
	./p2txt -z TEST1.p > TEST1-p2txt-z.txt

TEST1-p2txt-2.txt: p2txt TEST1.p
	./p2txt -2 TEST1.p > TEST1-p2txt-2.txt

p2speccy-all: p2speccy p2s-test1

p2speccy: p2speccy.o

p2s-test1: TEST1-p2speccy-r.txt TEST1-p2speccy-z.txt TEST1-p2speccy.tap

TEST1-p2speccy-z.txt: p2speccy TEST1.p
	./p2speccy -z TEST1.p > TEST1-p2speccy-z.txt

TEST1-p2speccy-r.txt: p2speccy TEST1.p
	./p2speccy -r TEST1.p > TEST1-p2speccy-r.txt

TEST1-p2speccy.tap: TEST1-p2speccy-z.txt
	zmakebas -n TEST1 -o TEST1-p2speccy.tap TEST1-p2speccy-z.txt

hex2rem-all: hex2rem hex2rem-test1

hex2rem: hex2rem.o

hex2rem-test1: hex2rem.bas hex2rem.p

hex2rem.bas: hex2rem hex2rem.txt
	./hex2rem -h hex2rem.txt > hex2rem.bas

rem2bin-all: rem2bin rem2bin-test

rem2bin: rem2bin.o

rem2bin-test: rem2bin.bin rem2bin.txt

rem2bin.txt: rem2bin TEST1.p
	./rem2bin -h -o rem2bin.txt TEST1.p

rem2bin.bin: rem2bin TEST1.p
	./rem2bin -b -o rem2bin.bin TEST1.p

hex2tap-all: hex2tap pictest.tap

hex2tap: hex2tap.o

pic.tap: hex2tap pic.scr
	./hex2tap -b -a SCR -n pic -o pic.tap pic.scr

pictest.tap: pic.tap loadpic.tap
	cat loadpic.tap pic.tap > pictest.tap

tap0auto-all: tap0auto

tap0auto: tap0auto.o

p2ts1510-all: p2ts1510

p2ts1510: p2ts1510.o

p2ts1510-loader: p2ts1510_loader.bin

p2ts1510-loaderP: p2ts1510_loaderP.bin

.PHONY: clean

clean:
	rm -f core
	rm *.o
	rm p2txt
	rm p2speccy
	rm hex2rem
	rm rem2bin
	rm hex2tap
	rm p2ts1510

install-home:
	cp p2txt ~/bin
	cp p2speccy ~/bin
	cp hex2rem ~/bin
	cp rem2bin ~/bin
	cp hex2tap ~/bin
	cp p2ts1510 ~/bin
