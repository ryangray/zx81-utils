# ZX81-UTILS

This is forked from [Mike Ralphson's zx81-utils](https://github.com/MikeRalphson/zx81-utils)
on GitHub and modified by [Ryan Gray](https://github.com/ryangray/zx81-utils).
It changes the readable output of `p2txt` and adds a [zmakebas](https://github.com/ryangray/zmakebas)
compatible output option.

# p2txt

Extracts the ZX81 BASIC listing from a .p file. It can give a version compatible
with [zmakebas]() or a more readable style. Output is to standard output, so you
can redirect to a file if you wish:

    p2txt filename.p > filename.txt

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
