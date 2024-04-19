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

p2txt-all: p2txt p2t-test1 p2t-test0

p2txt: p2txt.o

p2t-test0: p2txt
	./p2txt -z hitch-h.p > test/hitch-h-p2txt-z.bas
	./p2txt -r hitch-h.p > test/hitch-h-p2txt-r.bas

p2t-test1: TEST1-p2txt-r TEST1-p2txt-1 TEST1-p2txt-z TEST1-p2txt-2

# TEST1.bas -> zmakebas -> TEST1.p -> p2txt -z -> TEST1-p2txt-z.txt -> zmakebas
# -> TEST1-p2txt-z.p (compare to TEST1.p)

# TEST2.bas -> zmakebas -p -> TEST2.p -> p2speccy -> TEST2-p2speccy.txt ->
# zmakebas -> TEST2-p2speccy.tap

TEST1-p2txt-r: p2txt TEST1.p
	./p2txt -r TEST1.p > test/TEST1-p2txt-r.txt
	git diff test/TEST1-p2txt-r.txt

TEST1-p2txt-1: p2txt TEST1.p
	./p2txt -1 TEST1.p > test/TEST1-p2txt-1.txt

TEST1-p2txt-z: p2txt TEST1.p
	./p2txt -z TEST1.p > test/TEST1-p2txt-z.txt
	zmakebas -p -o test/TEST1-p2txt-z.p test/TEST1-p2txt-z.txt
	diff TEST1.p test/TEST1-p2txt-z.p

TEST1-p2txt-2: p2txt TEST1.p
	./p2txt -2 TEST1.p > test/TEST1-p2txt-2.txt

p2speccy-all: p2speccy p2s-test1 p2s-test2

p2speccy: p2speccy.o

p2s-test1: TEST1-p2speccy-r TEST1-p2speccy-z TEST1-p2speccy-tap

p2s-test2: TEST2.p

TEST1-p2speccy-z: p2speccy TEST1.p
	./p2speccy -z TEST1.p > test/TEST1-p2speccy-z.txt

TEST1-p2speccy-r: p2speccy TEST1.p
	./p2speccy -r TEST1.p > test/TEST1-p2speccy-r.txt

TEST1-p2speccy-tap: TEST1-p2speccy-z
	zmakebas -n TEST1 -o test/TEST1-p2speccy.tap test/TEST1-p2speccy-z.txt

hex2rem-all: hex2rem hex2rem-test1

hex2rem: hex2rem.o

hex2rem-test1: hex2rem
	./hex2rem -h hex2rem.txt > test/hex2rem.bas

rem2bin-all: rem2bin rem2bin-test

rem2bin: rem2bin.o

rem2bin-test: rem2bin TEST1.p
	./rem2bin -h -o test/rem2bin.txt TEST1.p
	./rem2bin -b -o test/rem2bin.bin TEST1.p

hex2tap-all: hex2tap pictest.tap

hex2tap: hex2tap.o

pic.tap: hex2tap
	./hex2tap -b -a SCR -n pic -o test/pic.tap pic.scr

pictest.tap: test/pic.tap
	cat loadpic.tap test/pic.tap > test/pictest.tap

pictest-demo:
	fuse --auto-load test/pictest.tap &

tap0auto-all: tap0auto

tap0auto: tap0auto.o

p2ts1510-all: p2ts1510

p2ts1510: p2ts1510.o

p2ts1510-loader: p2ts1510_loader.bin

p2ts1510-loader-tape: p2ts1510_loader-tape.bin

p2ts1510-test: p2ts1510 hello.p
	./p2ts1510 hello.p
	./p2ts1510 -t -o test/hello-p2ts1510-t.rom hello.p
	./p2ts1510 -o test/hello-p2ts1510-s.rom hello.p

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
