# ZX81-UTILS

This is forked from [Mike Ralphson's zx81-utils][ralphson] on GitHub and 
modified by [Ryan Gray][zx81-utils]. It changes the readable output of 
[`p2txt`](#p2txt) and adds a [zmakebas][] compatible output option. The 
[`p2spectrum`](#p2spectrum) utility has also been added.

[ralphson]: https://github.com/MikeRalphson/zx81-utils
[zx81-utils]: https://github.com/ryangray/zx81-utils
[zmakebas]: https://github.com/ryangray/zmakebas

# p2txt

Extracts the ZX81 BASIC listing from a .p file. It can give a version compatible
with [zmakebas][] or a more readable style. Output is to standard output, so you
can redirect to a file if you wish:

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

For the readable style (which is the default):

    p2txt -r filename.p

This will show the pound symbol as £, the quote image as "", the block graphics 
characters as block graphics, and inverse characters as enclosed in square 
brackets. The block graphics with half grey parts show as \"" for upper half,
\,, for lower half, and their inverses are [""] and [,,]. Other non-printable
characters print as a hash symbol, #.

For the zmakebas style:

    p2txt -z filename.p

This uses the conventions of zmakebas so that the output can be run through it
to reconstruct the .p file. It uses lowercase letters for inverse letters with a 
backslash before most other inverse characters. The quote image is a backtick, 
the pound symbol is a double backslash, and its inverse is \@. 

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


# p2spectrum

A utility to convert a ZX81 (or Timex Sinclair 1000) BASIC program in a .p file 
to a ZX Spectrum (or Timex Sinclair 2068) BASIC program in a text file. You can
choose either a [zmakebas][] compatible or a readable output style. A zmakebas 
output can be run through that utility to make a .tap file for a Spectrum 
emulator which can also be converted to a .wav file for loading on a real 
machine.

## Usage

    p2spectrum -r filename.p > filename.txt
    p2spectrum -z filename.p > filename.txt

The `-r` option give a more readable output form with block graphics characters 
(for most), and brackets around inverse characters. The `-z` output option (the 
default) is a [zmakebas][] compatible output so you can use that to make a 
`.tap` file of the result.

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
    inverse characters are shown each enclosed in square brackets. Letters are 
    uppercase. For the `-z` zmakebas output option, the characters are made 
    inverse by inserting the inverse video and true video attribute characters
    before and after the character.

* Miscelaneous

    - The ZX81 raise to power character `**` is converted to the Spectrum one 
      `^` except in `REM` statements and strings where it is rendered as two
      asterisks since those contexts would be using the ZX81 visual 
      representation.