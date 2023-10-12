# ZX81-UTILS

This is forked from [Mike Ralphson's zx81-utils][ralphson] on GitHub and 
modified by [Ryan Gray][zx81-utils]. It changes the readable output of 
[`p2txt`](#p2txt) and adds a [zmakebas][] and [ZXText2P][] compatible output 
options. Other utilities have been added.

* [`p2txt`](#p2txt) - Extract listing from .p file
* [`p2spectrum`](#p2spectrum) - Convert program in .p file to ZX Spectrum BASIC
* [`hex2rem`](#hex2rem) - Convert hex or binary file to a line 1 REM zmakebas text
* [`rem2bin`](#rem2bin) - Extract machine code in line 1 REM to a file

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
emulator. There are also utilities to convert the .p file to a .wav file for 
loading onto a real machine.

This utility is similar to the `listbasic` utility from the [FUSE emulator][fuse] 
utilities [`fuse-utils`][fuse-utils] and `tzxcat` from [tzxtools][] which produce
Spectrum BASIC listings. The [`p2spectrum`](#p2spectrum) utility in this package
can convert the ZX81 program in a .p file to a Spectrum compatible BASIC program
in a text file.

[fuse]: https://fuse-emulator.sourceforge.net/
[fuse-utils]: https://sourceforge.net/p/fuse-emulator/fuse-utils/ci/master/tree/
[tzxtools]: https://github.com/shred/tzxtools

## Usage

### Readable Style

For the readable style (which is the default):

    p2txt -r filename.p

This will show the pound symbol as £, the quote image as "", the block graphics 
characters as block graphics, and inverse characters as enclosed in square 
brackets. The block graphics with half grey parts show as \~~ for upper half,
\,, for lower half, and their inverses are [~~] and [,,]. Other non-printable
characters print as a hash symbol, #.

### ZMakeBas Style

For the zmakebas style:

    p2txt -z filename.p

This uses the conventions of zmakebas so that the output can be run through it
to reconstruct the .p file. It uses lowercase letters for inverse letters with a 
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

    p2speccy -r infile.p > outfile
    p2speccy -z infile.p > outfile
    p2speccy -r -o outfile infile.p

The `-r` option give a more readable output form with block graphics characters 
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

    hex2rem [-h|-b] input_file > output_file
    hex2rem [-h|-b] -o output_file input_file
    
* `-h` : Input are hex values in a text file. These can be on multiple lines,
    and whitespace is ignored between hex digit pairs. This is the default.

* `-b` : Input is a binary file.

Note that `input_file` can be "." to use standard input.

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

* `-a address` : The address the code block is tagged with to load into by default.

Note that `input_file` can be "-" to use standard input. `output_file` should
custromarily have a file extension of ".tap", which is not automatically added.
