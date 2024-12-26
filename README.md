# ZX81-UTILS

This is forked from [Mike Ralphson's zx81-utils][ralphson] on GitHub and 
modified by [Ryan Gray][zx81-utils]. It changes the readable output of 
[`p2txt`](#p2txt) and adds a [zmakebas][] and [ZXText2P][] compatible output 
options. Other utilities have been added.

* [`p2txt`](#p2txt) - Extract listing from .p file
* [`p2speccy`](#p2speccy) - Convert program in .p file to ZX Spectrum BASIC
* [`hex2rem`](#hex2rem) - Convert hex or binary file to a line 1 REM zmakebas text
* [`rem2bin`](#rem2bin) - Extract machine code in line 1 REM to a file
* [`hex2tap`](#hex2tap) - Convert hex or binary file to a Spectrum CODE block in
  a tap file.
* [`tapauto`](#tapauto) - Disable BASIC program auto run in a tap file
* [`p2ts1510`](#p2ts1510) - Convert a program in a P file to a ROM file for a
  TS1510 cartridge adapter.

[ralphson]: https://github.com/MikeRalphson/zx81-utils
[zx81-utils]: https://github.com/ryangray/zx81-utils
[zmakebas]: https://github.com/ryangray/zmakebas
[ZXText2P]: http://freestuff.grok.co.uk/zxtext2p/index.html

# p2txt

Extracts the ZX81 BASIC listing from a .p file. It can give a version compatible
with [zmakebas][], [ZXText2P][], or a more readable style. Output is to 
standard output, so you can redirect to a file if you wish:

    p2txt filename.p > filename.txt

The zmakebas output can be run back through that utility to create a .p file 
again, allowing you to edit the file on your computer and take it back into the
emulator. There are also utilities out there to convert the .p file to a .wav 
file for loading onto a real machine.

This utility is similar to the `listbasic` utility from the [FUSE emulator][fuse] 
utilities [`fuse-utils`][fuse-utils] and `tzxcat` from [tzxtools][] which produce
Spectrum BASIC listings. The [`p2spectrum`](#p2spectrum) utility in this package
can convert the ZX81 program in a .p file to a Spectrum compatible BASIC program
in a text file that can also be used with Zmakebas or ZXText2P.

[fuse]: https://fuse-emulator.sourceforge.net/
[fuse-utils]: https://sourceforge.net/p/fuse-emulator/fuse-utils/ci/master/tree/
[tzxtools]: https://github.com/shred/tzxtools

## Usage

    p2txt [options] infile.p [> outfile.txt]

Options:

* `-z` : Output Zmakebas compatible markup
* `-r` : Output a more readable markup (default).
  Inverse characters in square brackets, most block graphics.
* `-1` : Output Zmakebas markup but only use codes in a first line that is a REM.
* `-2` : Output ZXText2P compatible markup
* `-?` : Print this help.

The Zmakebas output will use `\{xxx}` codes in REMs and quotes to preserve
the non-printable and token character codes, whereas in readable mode, these
will give a hash (#) character. Zmakebas mode also inserts inverse and true
video codes where inverse characters appear in REMs and strings.

### Readable Style

For the readable style (which is the default):

    p2txt -r filename.p

This will show the pound symbol as `£`, the quote image as `""`, the block  
graphics characters as block graphics, and inverse characters as enclosed in  
square brackets. The block graphics with half grey parts show as `\~~` for the 
upper half, `\,,` for lower half, and their inverses are `[~~]` and `[,,]`. 
Other non-printable characters print as a hash symbol, `#`.

### ZMakeBas Style

For the zmakebas style:

    p2txt -z filename.p

This uses the conventions of zmakebas so that the output can be run through it
to make a .p file. It uses lowercase letters for inverse letters with a 
backslash before most other inverse characters. The quote image is a backtick 
(\`), the pound symbol is a double backslash (\\\\), and its inverse is `\@`. 

The block graphics are a backslash before two symbols depicting the graphic shape:

         |▘|  |▝|  |▀|  |▖|  |▌|  |▞|  |▛|  |▒|
         \'   \ '  \''  \.   \:   \.'  \:'  \!:

    |█|  |▟|  |▙|  |▄|  |▜|  |▐|  |▚|  |▗|  |▒|
    \::  \.:  \:.  \..  \':  \ :  \'.  \ .  \|:

    Grey top-half: \!'      Inverse: \|'
    Grey bot-half: \!.      Inverse: \|'

See the example `TEST1.p` with its two output versions, `TEST1.txt`, the 
"readable" version, and `TEST1.bas`, the zmakebas version.

An alternative to `-z` is `-1` which is the same except that it only uses 
character codes in a first line that is a `REM` statement in order to preserve
machine code there but elsewhere expand to the normal token text. If you run
this back through zmakebas, the code will have regular text in those places 
rather than character codes. This may be undesirable if, for example, tokens 
were used in a comparison to an `INKEY$` result:

    100 LET K$=INKEY$
    110 IF K$=" STOP " THEN STOP

You would need to preserve tokens, manually insert the token code escape, or 
change the comparison to test the `CODE` of the string instead.

### ZXText2P Style

For the ZXText2P style:

    p2txt -2 filename.p

This is mostly the same as ZMakeBas style, but changes the grey graphics blocks:

    Grey solid:    \##      Inverse: \@@
    Grey top-half: \~~      Inverse: \!!
    Grey bot-half: \,,      Inverse: \;;

The pound Sterling symbol is just `#`, so the non-printable character changes
from `#` to `!`. The inverse characters are all just `%` in front of the normal 
character.

It appears it can't do graphics escapes in REMs. It says there's a character it
can't translate, so it's not interpreting the backslash there. Looking at the 
code, it doesn't interpret `%` for inverse characters either. I don't know why 
it qualifies either of these to not be in a REM since the neither the `%` nor 
the `\` are ZX81 characters that you would encounter in a REM anyway. 

There seems to be a version of ZXText2P format that will show codes in the line 
1 REM as backslash with 2-digit hex code. I've added that but only for non-
printable characters and tokens in the first line REM rather than printing 
expanded tokens or `!` for a non-printable character.


# p2speccy

A utility to convert a ZX81 (or Timex Sinclair 1000) BASIC program in a .p file 
to a ZX Spectrum (or Timex Sinclair 2068) BASIC program in a text file. You can
choose either a [zmakebas][] compatible or a readable output style. A zmakebas 
output can be run through that utility to make a .tap file for a Spectrum 
emulator which can also be converted to a .wav file for loading on a real 
machine.

## Usage

    p2speccy [options] infile.p > outfile
    p2speccy p2speccy [options] -o outfile infile.p

Options:

* `-z` : Output Zmakebas compatible markup
* `-r` : Output a more readable markup (default).
  Inverse characters in square brackets, most block graphics.
* `-o outfile` : Give the name of an output file rather than using stdout.
* `-?` or `--help` : Print this usage.

The Zmakebas output will use `\{xxx}` codes in REMs and quotes to preserve
the non-printable and token character codes, whereas in readable mode, these
will give a hash (#) character. Zmakebas mode also inserts inverse and true
video codes where inverse characters appear in REMs and strings.

The `-r` option gives a more readable output form with block graphics characters 
(for most), and brackets around inverse characters. The `-z` output option (the 
default) is a [zmakebas][] compatible output so you can use that to make a 
`.tap` file of the result. The `-o` option can be used instead of sandard output 
redirection to name the output file. The name is given after `-o`.

The differences in ZX81 and Spectrum BASIC are handled by:

* Block graphics

    The block graphic characters, except for the 6 that have "grey" parts, are
    shown as Unicode block characters for the `-r` option , and as zmakebas
    escapes for the `-z` option (as shown in the [`p2txt`](#p2txt) section).
    
* "Grey" block graphic characters
  
    If any of the 6 "grey" block graphics characters are used, which the Spectrum
    doesn't have, then a subroutine is added that programs them on UDG characters
    A through F and makes the character substitutions using zmakebas notation of
    `\a` through `\f`.

* `UNPLOT` command

    The `UNPLOT` command is converted to `PLOT INVERSE 1;`.

* `PLOT` resolution

    The `PLOT` and `UNPLOT` commands have their x,y coordinated multiplied by 4,
    and subroutine calls added after each to draw a magnified pixel that is 4x 
    the Spectrum's. This is meant for compatibility, and you can then go in and
    upscale the resolution of the program.

* `SCROLL` command

    The Spectrum doesn't have this command since it has "automatic" scrolling,
    but that usually prompts the user. The `SCROLL` command is replaced with:

        POKE 23692,255: PRINT AT 21,0'': REM SCROLL

    The POKE resets the scroll prompt countdown so it won't prompt you when a 
    print causes a scroll, then we print at the last line and two new lines.

* `FAST` and `SLOW`

    The Spectrum did away with the need for these commands, so they are simply
    left out of the output. However, there are ZX81 programs that used these in
    rapid succession and in patterns to make music and sounds, but that wouldn't
    work on a Spectrum, and you'd have to change the sound generation to use the
    `BEEP` command or the sound chip if your Spectrum variant has one.

* `POKE` and `PEEK`

    Any `POKE` statement is disabled as a REMark, and a warning REM is appended.
    Any line with a `PEEK` call has a warning `REM` appended to it to warn you 
    that the address won't be compatible.

* `USR` call to machine code

    Since it is very unikely that any practical machine code is compatible, any
    `USR` call is converted to `INT INT` to not attempt to call any code. The
    utility also doesn't use escape codes in any `REM` to try to preserve the
    codes (like `p2txt` does) since it disables these calls.

* `CHR$` and `CODE`

    Lines that use these will have warning REMarks appended to alert you that
    you may need to modify the codes these use or are compared to since the 
    codes differ between the two systems.

* `INKEY$`

    Any line that uses `INKEY$` will get a warning REMark appended alerting you 
    to check characters the result is compared to and that you will likely want 
    to change letters to lowercase since that's what it will return for letters
    without the shift key.

* Autosave

    A `SAVE` line with an inverse character at the end of the name is converted
    to `SAVE` with `LINE` appended and the line number plus one to have the 
    save auto run on the next line. You would need to use the `-a` option on 
    `zmakebas` to have a .tap file you made of it to autorun.

* Inverse characters

    These are shown in two ways. For the `-r` option for more readable output,
    inverse characters are shown enclosed in square brackets, with letters in 
    uppercase. For the `-z` zmakebas output option, the characters are made 
    inverse by inserting the inverse video and true video attribute characters
    before and after the characters. Groups of consecutive inverse characters
    are enclosed in a single pair of square brackets (`-r`) or inverse/true 
    video codes. 

* Miscelaneous

    - The ZX81 raise to power character `**` is converted to the Spectrum one 
      `^` except in `REM` statements and strings where it is rendered as two
      asterisks since those contexts would be using the ZX81 visual 
      representation.


# hex2rem

This lets you generate a REM line in a text file from hex codes or a binary file
that you can run through [zmakebas][] to create a line of (presumably) machine
code. This utility can also be used for ZX Spectrum and TS2068 code since it
creates a zmakebas input file with nothing particular to either machine, you 
would just use the zmakebas `-p` option when making a `.p` file for the ZX81 or
TS1000.

For example, you could use a Z80 assembler to assemble code designed for
an origin of 16514 for the ZX81, assemble that to a binary file, then use 
`hex2rem` to turn it into a REM line. Then add any other BASIC code to the 
file and run through `zmakebas`.

## Usage

    hex2rem [-?] [-h | -b] [-l nnnn] [infile [outfile]]

* `-h` : Input are hex values in a text file . These can be on multiple lines,
  and whitespace is ignored between hex digit pairs. This is the default.
* `-b` : Input is a binary file.
* `-l nnnn` : Specify line number of REM to be nnnn (default is 1, max is 9999)
* `-?` : Print the help summary

Note that `input_file` can be "-" to use standard input.

The Zmakebas output will use `\{xxx}` codes in the REM to preserve
the byte codes. Input and output files default to standard in/out.
    
The output is written as one `REM` line with continuation breaks every 10 values
(a backsash at the ends of the continuing lines). Each value is written with the
zmakebas literal code escape notation using a hex value:

    \{0xab}


# rem2bin

This is the opposite of `hex2rem` but with the default of binary output. It 
extracts the first line REM statement's content as binary code. You can opt for
hexadecimal output instead. The REM statement number doesn't have to be `1`, but
it does need to be the first line of the program. This supports both .p  and 
.tap files, so it works for both ZX81/TS1000 and ZX Spectrim/TS2068.

You could use this to extract the machine code in a BASIC rem statement into a
binary file, then use a Z80 disassembler on the code. The utility `tzxcat` from
the [tzxtools][] can disassemble the machine code in a Spectrum CODE block of a
tape file, but not from the rem line. I think you can pull the tape file into a
disassembler and give it the correct offset to start at (116 for .p, 24 for 
.tap).

## Usage

    rem2bin [-b|-h] [-p|-t] input_file > output_file
    rem2bin [-b|-h] [-p|-t] -o output_file input_file

Options:

* `-b` : Output is binary (the default).
* `-h` : Output is hex values as text, written 16 to a line.
* `-p` : Input file is .p format
* `-t` : Input file is .tap format (only reads the first program in the file)
* `-?` : Print the help summary

Note that `input_file` can be "-" to use standard input. The `-p` and `-t` 
options aren't necessary if the type can be inferred fron the input file name
extension.

For example, if the program in a .p file has machine code in the line 1 REM 
statement, you could extract that to a file and disassemble it:

    rem2bin -o game.bin game.p
    z80dasm -g 16514 game.bin

TODO: Keep going as long as the first lines are REMs since some programs split
the MC up between a few REMs. However, some might have comment REMs after the
MC REMs though.


# hex2tap

Like `hex2rem`, this takes a hex or binary file as input, but it outputs a 
ZX Spectrum/TS2068 .tap file with a CODE block containing the bytes. Yes, this
is actualy a Spectrum utility.

The bytes are not translated, so this could be Z80 machine code suitable for 
running on the Spectrum or similar model, but it could also just be bytes for
some useer-defined graphics characters.

You can specify the code block starting address as well as the tape block name.
The default output is to standard output. 

## Usage

    hex2tap [-h|-b] [-n speccy_filename] [-a address] [-o output_file] input_file
    
* `-h` : Input are hex values in a text file. These can be on multiple lines,
  and whitespace is ignored between hex digit pairs. This is the default.

* `-b` : Input is a binary file.

* `-n speccy_filename` : The filename in the .tap file as the Spectrum sees it.

* `-a address` : The address the code block is tagged with to load into by
  default. The length is set by the input length.

    - Use `-a UDG` as an alias for `-a 65368` (USR "a")
    - Use `-a SCR` as an alias for `-a 16384` (SCREEN$)

* `-?` : Print the help summary

Note that `input_file` can be "-" to use standard input. `output_file` should
customarily have a file extension of ".tap", which is not automatically added.

## Test case

The test for `hex2tap` is converting `pic.scr` (a raw Spectrum screen memory
file) to a code block in a .tap file, `pic.tap`. This is done with:

    hex2tap -b -a SCR -n pic -o pic.tap pic.scr

The `Makefile` has a target `pic.tap` to do this. It also has a target of 
`pictest.tap` which will make `pictest.tap`, composed of `loadpic.tap` and 
`pic.tap` using:

    cat loadpic.tap pic.tap > pictest.tap

`loadpic.tap` contains just a one line BASIC program that auto starts to do a
`LOAD "" SCREEN$` command. This is put with `pic.tap` that contains the SCREEN$
code block together in `pictest.tap`. You can load this into an emulator to
easily load the image. You could also convert the file to a .wav file to play 
into a real machine.

For code testing, `pic.tap` will be re-made if `hex2tap` changes and `pic.tap`
is made, which is also done if `hex2tap-all` is made. Then check if it differs 
from the repository version.

You could make a script `scr2tap` to specifically convert .SCR files to .TAP:
```bash
#!/bin/bash
hex2tap -b -a SCR $*
```

Then you can just use: `scr2tap -n pic -o pic.tap pic.scr`


# tapauto

Yet another ZX Spectrum/TS2068 utility. This will show or set the autorun for
programs in a .tap file.

## Usage

    tapauto [-i] [-a num] [-b num | -f num] input_file output_file
    tapauto -?                  Print this help.
    Options: -i                 Only print info about the autostart
             -a line_number     Set the autostart line number (-1=none, the default)
             -b block_number    Block number to modify (>=0, default=1st prog).
             -f file_number     File number to modify (>=1, default=1st prog).

Only one program will be modified if a block or file is specified or if autorun
is being turned on. If a block or file is not specified and autorun is being
turned off, then it will be turned off for all program files. File numbers start
at 1, each composed of two blocks, which start at 0 with even values as the
headers. The input and output files name can be given as - to use standard I/O.

This only works on .tap files, so if you have a .tzx file, you need to convert
it to a .tap file. `tzxtap` from the [tzxtools][] can be used for this.

`input_file` and/or `output_file` can be given as `-` to use standard input and
standard output, respectively. So, you could pipe the output of `tzxtap` to
process a .tzx file with:

    tzxtap foo.tzx | tapauto - foo.tap


# p2ts1510

This will comvert a ZX81/TS1000/TS1500 program contained in a .p file format to
a ROM file intended to be used with the [Timex Sinclair 1510 Command Cartridge
Player][TS1510] or [equivalent][TS1510clone].

[TS1510]: https://timexsinclair.com/product/ts-1510-cartridge-player/

[TS1510clone]: https://www.sinclairzxworld.com/viewtopic.php?f=7&t=192&sid=cff3eefa733588c77ae157fa5bcd25e8

In order to be general, this creates a more extensive loader program than we see
in the existing 4 cartridge ROMs. These can be burned to EPROMs tu be used in a
compatible device or used in the [EightyOne][] emulator which allows you load 
custom 1510 ROMs.

[EightyOne]: https://sourceforge.net/projects/eightyone-sinclair-emulator/

# Usage

    p2ts1510 [options] [input_file]

The input file name can be given as `-` to use standard input but is not
necessary. If the input is stdin, then the output will default to stdout
unless the `-o` option is given.

Options:

* `-?` - Show the usage help.

* `-o output_file` - Name the output file. The default uses the basename of 
  the input file. You can use `-` to explicitly send to standard output. If
  the output is stdout, then it is the equivalent of using the `-1` option.

* `-v` - The default is to include the variables from the P file. This option
  will cause the variables data to not be included. This only applies to the 
  prog+vars loader (-p) and not the tape-like loader (-t) currently. A future
  release might add the option for it to apply to the tape-like loader, but that
  could cause some programs to not work if they either need the variables, or if
  they are like VU-CALC and do a checksum of the system variables.

* `-a line_number` - Force the auto run line number. The default is to use
  whatever the P file has, which could be no auto run. You can use `-1` to force
  no auto run.

* `-s` - Write a "short" ROM file that is not padded out to a length multiple 
  of 8K. If the ROM is more than 8K, then the first block is a full 8K, and the
  second block is short.

* `-2` - For large programs that require more than one 8K ROM, two 8K ROM
  segments are made. By default, these are concatenated into one file. This is
  compatible with the [EightyOne][] emulator. If you want two separate ROM 
  files, use the `-2` option, and the two files are written with "_A" and "_B" 
  added to the output name. 
  
* `-1` - This option puts both 8K blocks into one file. It is the default.

* `-i` - Print the P file and ROM block info without writing any ROMs. This can
  be useful to see if there is any variable data in the P file and what the auto
  start line number is. If there are variables, you could use `-v` to include
  them, or you could opt to not to if you are able to find that the line(s)
  after the auto start include a `RUN` which clears the variables anyway. This
  will also show you the output file name(s) that would be used.

  This output is written to the standard error output, so it does not redirect
  with the standard output. To redirect it, use `p2ts1510 ... 2>file`.

* `-t` - Use the tape-like loader which will load all of the P file contents,
  which includes most all of the system variables (116 bytes), the program, the 
  display file (793 bytes full, 25 collapsed), and the variables. This loader 
  ignores the `-v` option and is the default loader. 

  This loader should be extremely compatible with most any program, and works on
  those that do things like checksum the system variables on load like VU-CALC
  does. The loader is only 73 bytes as opposed to the standard loader of 207 
  bytes. However, the system variables and display file make it use 659 bytes 
  more overall. In some cases, this could make the difference in needing a 16K 
  ROM versus just an 8K ROM, so you could try the standard loader.

  Note that this loader also skips clearing the memory since it writes the 
  system variables and over all the other memory we care about, and this seems
  to be fine even on a TS1500 that doesn't do this init before loading a 
  cartridge ROM. If a program doesn't seem to work with this, you can try the
  prog+vars loader with option -p.

* `-p` - Use the prog+vars loader which is a generalized version of the original
  Timex ROM cartridge loaders. It only stores the program and optionally the 
  variables in the ROM (-v switch). It clears RAM and generates a display file
  when loading the program. This doesn't work for a number of programs that rely
  on certain system variables. The total size with this loader is slightly 
  smaller by 659 bytes, so if the program is near the max size, you might have 
  to use this loader to fit in 16K. However, if the P file has variables you
  don't need, you can use this loader with the -v option to leave out the 
  variables data to have a smaller ROM size.

The cartridge ROM will autorun on startup on a TS1500, but on a ZX81 or 
TS1000, you will have to give the command `RAND USR 8192` to start the ROM
loader.
