/* p2speccy - convert the BASIC program in a ZX81 format .p file to ZX Spectrum
 *            BASIC as text that is compatible with Zmakebas 
 * Author: Ryan Gray (based on p2txt)
 * 
 * I decided to not do the keyword and function translation in the character 
 * array since we don't need to do those inside a quote or REM, so keywords in 
 * REMs and quotes will appear as their original text in case that was the 
 * intent (sometimes tokens are used in REMs and strings for text using fewer 
 * bytes). We also don't bother outputting tokens as zmakebas codes in a REM  
 * since that is only useful for preserving machine code, which is unlikely to 
 * work. Regular characters and the quote image are translated with the 
 * character array, with the inverse characters mapped to their non-inverse 
 * versions which will get wrapped with [] or inverse/true video attributes 
 * depending on the output style. If the grey block graphics, plot, or unplot 
 * are used, then small helper subroutines are added at the locations found in 
 * the first pass by checkLine(). Uses of CODE, CHR$, INKEY$, and PEEK get 
 * warning REMs appended. Use of POKE is disabled with a REM and a REM note 
 * added. Use of USR is disabled by changing it to INT INT to make it notable
 * and a warning REM added. SCROLL is replaced with a POKE and PRINT that are 
 * equivalent and ": REM SCROLL" appended as a note. FAST and SLOW command lines
 * are removed. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1.0.2"

#ifdef __MSDOS__
#define STRCMPI strcmpi
#else
#define STRCMPI strcasecmp
#endif

/* Some ZX81 character codes we reference */
#define K_QUOTE      11
#define K_DOLLAR     13
#define K_LPAREN     16
#define K_RPAREN     17
#define K_COMMA      26
#define K_INKEY      65
#define K_NUMBER    126
#define K_CODE      196
#define K_PEEK      211
#define K_USR       212
#define K_CHR       214
#define K_POWER     216
#define K_STOP      227
#define K_SLOW      228
#define K_FAST      229
#define K_SCROLL    231
#define K_REM       234
#define K_GOTO      236
#define K_LET       241
#define K_POKE      244
#define K_PLOT      246
#define K_RUN       247
#define K_SAVE      248
#define K_UNPLOT    252
#define K_RETURN    254

unsigned char linebuf[32768]; /* Buffer to store a line of code */
char *infile = NULL;
char *outfile = "";
int usr_flag = 0;       /* Flags that function was used somewhere (set in 1st pass) */
int slow_flag = 0;
int fast_flag = 0;
int chr_flag = 0;
int poke_flag = 0;
int peek_flag = 0;
int scroll_flag = 0;
int inkey_flag = 0;
int udg_flag = 0;       /* Were grey block chars used? We need to define them as UDGs */
int udg_sub = 0;        /* Line num for UDG routine */
int udg_sub_w = 0;      /* UDG sub has been written */
int udg_sub_length = 3;
int udg_call = 0;       /* Line where we will put the call to the UDG routine */
int udg_call_w = 0;     /* Call line has been written */
int plot_flag = 0;      /* Was a PLOT command used? We need to emit the big pixel subroutine */
int plot_sub = 0;       /* The line number of the big-pixel PLOT subroutine */
int plot_sub_w = 0;     /* Plot rouine written */
int unplot_flag = 0;    /* The same for UNPLOT */
int unplot_sub = 0;
int unplot_sub_w = 0;
int addStop = 0;        /* If > 0, line number of STOP to add at end of program before subroutines s*/
long fp = 0;            /* Save file position after header is read */
int prog_size = 0;
int prev_k_branch = 0;  /* Was the command code of the previous line a branch or stop? */
int prev_line = 0;      /* The previous line number */

/* Define character strings for DOS Code Page 437 */
#ifdef __MSDOS__
#define POUND "\x9C"
#define BLK "\xDB"
#define BTM "\xDC"
#define BUL "\\' "
#define BUR "\\ '"
#define TOP "\xDF"
#define BLF "\xDD"
#define BRT "\xDE"
#define BGY "\xB1"
#define BUP "\\.'"
#define BDN "\\'."
#define BLL "\\. "
#define BLR "\\ ."
#define ILR "\\:'"
#define ILL "\\':"
#define IUL "\\.:"
#define IUR "\\:."
#else
#define POUND "£"
#define BLK "█"
#define BTM "▄"
#define BUL "▘"
#define BUR "▝"
#define TOP "▀"
#define BLF "▌"
#define BRT "▐"
#define BGY "▒"
#define BUP "▞"
#define BDN "▚"
#define BLL "▖"
#define BLR "▗"
#define ILR "▛"
#define ILL "▜"
#define IUL "▟"
#define IUR "▙"
#endif

/* Block Graphics escapes used by Zmakebas and listbasic:
 * 
 * "\' "  001 Top-left corner
 * "\ '"  002 Top-right corner
 * "\''"  003 Top half
 * "\. "  004 Bottom-left corner
 * "\: "  005 Left half
 * "\.'"  006 Bottom-left, top-right corners
 * "\:'"  007 Bottom-right empty
 * "\!:"  008 "Gray" solid -> Becomes UDG \a
 * "\!."  009 "Gray" bottom half -> Becomes UDG \b
 * "\!'"  010 "Gray top half -> Becomes UDG \c
 * "\::"  128 Solid (inverse space)
 * "\.:"  129 Top-left bank (inverse of "\' ")
 * "\:."  130 Top-right blank (inverse of "\ '")
 * "\.."  131 Bottom half (inverse of "\''")
 * "\':"  132 Bottom-left blank (inverse of "\. ")
 * "\ :"  133 Right half (inverse of "\: ")
 * "\'."  134 Top-left, bottom-right (inverse of "\.'")
 * "\ ."  135 Bottom-right corner (inverse of "\:'")
 * "\|:"  136 Inverse "gray" (inverse pixels of "\!:") -> Becomes UDG \d
 * "\|."  137 Black top, "gray" bottom (inverse of "\!.") -> Becomes UDG \e
 * "\|'"  138 Black bottom, "gray" top (inverse of "\!'") -> Becomes UDG \f
 * 
 * If "grey" block graphics are used, then a routine to program UDG characters
 * a-f with their patterns is added and the chafacters are mapped to them.
 * 
 * Special Characters:
 * 
 * `     012 Pound Sterling symbol (its not # since that is a zmakebas comment char)
 * ""    192 Two double quote characters for the Quote Image "" character
 * ^     216 Caret for raise to power (becomes up arrow)
 * Inverse letters become normal and wrapped with Inverse Video attribute or []
 * depending on the output style.
 */

/* Character for codes not mapped. Probably will only see in machine code REMs. */
/* nak = 'not a kharacter', of course! :-) */
#define NAK "#"

char *charset_zmb[] =
{
/* 000-009 */ " ",     "\\' ",   "\\ '",      "\\''",  "\\. ",   "\\: ",    "\\.'",   "\\:'",    "\\a",    "\\b",
/* 010-019 */ "\\c",   "\"",     "`",         "$",     ":",      "?",       "(",      ")",       ">",      "<",
/* 020-029 */ "=",     "+",      "-",         "*",     "/",      ";",       ",",      ".",       "0",      "1",
/* 030-039 */ "2",     "3",      "4",         "5",     "6",      "7",       "8",      "9",       "A",      "B",
/* 040-049 */ "C",     "D",      "E",         "F",     "G",      "H",       "I",      "J",       "K",      "L",
/* 050-059 */ "M",     "N",      "O",         "P",     "Q",      "R",       "S",      "T",       "U",      "V",
/* 060-069 */ "W",     "X",      "Y",         "Z",     "RND",    "INKEY$ ", "PI",     NAK,       NAK,      NAK,
/* 070-079 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 080-089 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 090-099 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 100-109 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 110-119 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 120-129 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       "\\::",   "\\.:",
/* 130-139 */ "\\:.",  "\\..",   "\\':",      "\\ :",  "\\'.",   "\\ .",    "\\d",    "\\e",     "\\f",    "\"",
/* 140-149 */ "`",     "$",      ":",         "?",     "(",      ")",       ">",      "<",       "=",      "+",
/* 150-159 */ "-",     "*",      "/",         ";",     ",",      ".",       "0",      "1",       "2",      "3",
/* 160-169 */ "4",     "5",      "6",         "7",     "8",      "9",       "A",      "B",       "C",      "D",
/* 170-179 */ "E",     "F",      "G",         "H",     "I",      "J",       "K",      "L",       "M",      "N",
/* 180-189 */ "O",     "P",      "Q",         "R",     "S",      "T",       "U",      "V",       "W",      "X",
/* 190-199 */ "Y",     "Z",      "\"\"",      "AT ",   "TAB ",   NAK,       "CODE ",  "VAL ",    "LEN ",   "SIN ",
/* 200-209 */ "COS ",  "TAN ",   "ASN ",      "ACS ",  "ATN ",   "LN ",     "EXP ",   "INT ",    "SQR ",   "SGN ",
/* 210-219 */ "ABS ",  "PEEK ",  "USR ",      "STR$ ", "CHR$ ",  "NOT ",    "^",      " OR ",    " AND ",  "<=",
/* 220-229 */ ">=",    "<>",     " THEN",     " TO ",  " STEP ", " LPRINT "," LLIST "," STOP",   " SLOW",  " FAST",
/* 230-239 */ " NEW",  " SCROLL"," CONTINUE "," DIM ", " REM ",  " FOR ",   " GO TO "," GO SUB "," INPUT "," LOAD ",
/* 240-249 */ " LIST "," LET ",  " PAUSE ",   " NEXT "," POKE ", " PRINT ", " PLOT ", " RUN ",   " SAVE ", " RANDOMISE ",
/* 250-255 */ " IF ",  " CLS",   " UNPLOT ",  " CLEAR"," RETURN"," COPY"
};

char *charset_read[] =
{
/* 000-009 */ " ",     BUL,      BUR,         TOP,     BLL,      BLF,       BUP,      ILR,       BGY,      "\\,,",
/* 010-019 */ "\\\"\"","\"",     POUND,       "$",     ":",      "?",       "(",      ")",       ">",      "<",
/* 020-029 */ "=",     "+",      "-",         "*",     "/",      ";",       ",",      ".",       "0",      "1",
/* 030-039 */ "2",     "3",      "4",         "5",     "6",      "7",       "8",      "9",       "A",      "B",
/* 040-049 */ "C",     "D",      "E",         "F",     "G",      "H",       "I",      "J",       "K",      "L",
/* 050-059 */ "M",     "N",      "O",         "P",     "Q",      "R",       "S",      "T",       "U",      "V",
/* 060-069 */ "W",     "X",      "Y",         "Z",     "RND",    "INKEY$ ", "PI",     NAK,       NAK,      NAK,
/* 070-079 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 080-089 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 090-099 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 100-109 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 110-119 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       NAK,      NAK,
/* 120-129 */ NAK,     NAK,      NAK,         NAK,     NAK,      NAK,       NAK,      NAK,       BLK,      IUL,
/* 130-139 */ IUR,     BTM,      ILL,         BRT,     BDN,      BLR,       BGY,      ",,",      "\"\"",   "\"",
/* 140-149 */ POUND,   "$",      ":",         "?",     "(",      ")",       ">",      "<",       "=",      "+",
/* 150-159 */ "-",     "*",      "/",         ";",     ",",      ".",       "0",      "1",       "2",      "3",
/* 160-169 */ "4",     "5",      "6",         "7",     "8",      "9",       "A",      "B",       "C",      "D",
/* 170-179 */ "E",     "F",      "G",         "H",     "I",      "J",       "K",      "L",       "M",      "N",
/* 180-189 */ "O",     "P",      "Q",         "R",     "S",      "T",       "U",      "V",       "W",      "X",
/* 190-199 */ "Y",     "Z",      "\"\"",      "AT ",   "TAB ",   NAK,       "CODE ",  "VAL ",    "LEN ",   "SIN ",
/* 200-209 */ "COS ",  "TAN ",   "ASN ",      "ACS ",  "ATN ",   "LN ",     "EXP ",   "INT ",    "SQR ",   "SGN ",
/* 210-219 */ "ABS ",  "PEEK ",  "USR ",      "STR$ ", "CHR$ ",  "NOT ",    "^",      " OR ",    " AND ",  "<=",
/* 220-229 */ ">=",    "<>",     " THEN",     " TO ",  " STEP ", " LPRINT "," LLIST "," STOP",   " SLOW",  " FAST",
/* 230-239 */ " NEW",  " SCROLL"," CONTINUE "," DIM ", " REM ",  " FOR ",   " GO TO "," GO SUB "," INPUT "," LOAD ",
/* 240-249 */ " LIST "," LET ",  " PAUSE ",   " NEXT "," POKE ", " PRINT ", " PLOT ", " RUN ",   " SAVE ", " RANDOMISE ",
/* 250-255 */ " IF ",  " CLS",   " UNPLOT ",  " CLEAR"," RETURN"," COPY"
};

char **charset = charset_zmb;

/* Styles of output. Selectable by command line options. */
enum outstyle {OUT_READABLE, OUT_ZMAKEBAS};
enum outstyle style = OUT_ZMAKEBAS;

char warn_ZMB[] = "\\{20}\\{1}WARNING\\{20}\\{0}"; /* Inverse video attribs for zmakebas */
char warn_READ[] = "[WARNING]";
char *warn;

/************************* program starts here ****************************/

void getCodeLine (FILE *in, int *linenum, int *linelen, int *t)
{
    /* Get a single ZX81 program line.
     * Line number into linenum, length into linelen, and line into linebuf
     */

    int b1, b2, f;

    b1 = fgetc(in);
    b2 = fgetc(in);
    *linenum = b1 * 256 + b2;
    (*t) -= 2;

    b1 = fgetc(in);
    b2 = fgetc(in);
    *linelen = b1 + 256 * b2;
    (*t) -= 2;

    for (f = 0; f < *linelen; f++)
        {
        linebuf[f] = fgetc(in);
        (*t)--;
        }
}

void checkForSubs (int linenum)
{
    /* Check for places we can put the subroutines or calls we might need to insert */

    int addedAnything = 0;
    int next_line = prev_line + 1;

    if ( !udg_call && linenum > next_line )
        {
        udg_call = next_line++;
        }

    if ( linenum > 9999 && !prev_k_branch ) 
        {
        /* After end of program, and it didn't end with an unconditional branch,
         * so we might have to insert a stop to put routines after.
         */
        addStop = next_line++;
        prev_k_branch = 1; /* To allow the following checks to be done */
        }

    if ( prev_k_branch ) /* Previous line was an unconditional branch or stop */
        {
        /* We could safely put a subroutine here.
         * Check if any routines still need a location
         * We assume we need them even if we don't later
         */

        if ( !plot_sub && linenum > next_line )
            {
            plot_sub = next_line++;
            addedAnything = 1;
            }
        if ( !unplot_sub && linenum > next_line )
            {
            unplot_sub = next_line++; 
            addedAnything = 1;
            }
        if ( !udg_sub && linenum >= next_line + udg_sub_length )
            {
            udg_sub = next_line;
            next_line += udg_sub_length; 
            addedAnything = 1;
            }
        if (!addedAnything)
            addStop = 0;
        }
}

void writeSubs (FILE *out, int linenum)
{
    /* Check if we need to write any routines before the next line */

    if ( udg_flag && udg_call && !udg_call_w && linenum > udg_call )
        {
        fprintf(out, "%4d GO SUB %d: REM Grey UDGs\n", udg_call, udg_sub);
        udg_call_w = 1;
        }
    if ( addStop && linenum > addStop )
        {
        fprintf(out, "%4d STOP\n", addStop);
        addStop = 0;
        }
    if ( plot_flag )
        {
        if ( plot_sub && !plot_sub_w && linenum > plot_sub )
            {
            fprintf(out, "%4d DRAW 3,0: DRAW 0,3: DRAW -3,0: DRAW 0,-2: DRAW 2,0: DRAW 0,1: DRAW -1,0: RETURN: REM Plot 4x pixel\n", plot_sub);
            plot_sub_w = 1;
            }
        if ( unplot_sub && !unplot_sub_w && linenum > unplot_sub )
            {
            fprintf(out, "%4d DRAW INVERSE 1;3,0: DRAW INVERSE 1;0,3: DRAW INVERSE 1;-3,0: DRAW INVERSE 1;0,-2: DRAW INVERSE 1;2,0: DRAW INVERSE 1;0,1: DRAW INVERSE 1;-1,0: RETURN: REM Unplot 4x pixel\n", unplot_sub);
            unplot_sub_w = 1;
            }
        }
    if ( udg_flag && udg_sub && ! udg_sub_w && linenum > udg_sub )
        {
        fprintf(out, "%4d RESTORE %d: LET U=USR \"a\": REM Init grey UDGs\n", udg_sub, udg_sub+3);
        fprintf(out, "%4d FOR A=0 TO 47 STEP 4: READ B,C\n", udg_sub+1);
        fprintf(out, "%4d POKE U+A,B: POKE U+A+1,C: POKE U+A+2,B: POKE U+A+3,C: NEXT A: RETURN\n", udg_sub+2);
        fprintf(out, "%4d DATA 170,85,170,85,170,85,0,0,0,0,170,85,85,170,255,255,255,255,85,170,85,170,85,170\n", udg_sub+3);
        udg_sub_w = 1;
        }

}

void checkLine (int linelen, int linenum)
{
    /* Check a line for tokens of interest to set their presence flags */

    int f, inQuotes = 0;
    unsigned char c, keyword = linebuf[0];

    if ( keyword != K_REM )
        prev_k_branch = 0;

    switch (keyword)
        {
        case K_SLOW:    slow_flag = 1;  return;
        case K_FAST:    fast_flag = 1;  return;
        case K_PLOT:    plot_flag = 1;  return;
        case K_UNPLOT:  unplot_flag = 1;return;
        case K_SCROLL:  scroll_flag = 1;return;
        case K_POKE:    poke_flag = 1;  return;
        case K_STOP:
        case K_GOTO:
        case K_RETURN:
        case K_RUN:     prev_k_branch = 1; /* We can possibly insert a subroutine after this line */
                        return;
        default:        break;
        }

    /* Otherwise, check line for other function usage of note */

    for (f = 0; f < linelen - 1; f++)
        {
        c = linebuf[f]; /* Character code  */

        if ( c == K_NUMBER ) f += 5;  /* avoid inline FP numbers */

        if ( (keyword != K_REM) && (c == K_QUOTE) ) inQuotes = !inQuotes;

        if ( (c >= 8 && c <= 10) || ( c >= 136 && c <= 138) ) /* Grey block graphics used */
            {
            udg_flag = 1;
            }
        else if (keyword != K_REM && !inQuotes ) /* Only if these are not in REMs or quotes */
            {
            switch (c)
                {
                case K_USR:     usr_flag   = 1; break;
                case K_CHR:
                case K_CODE:    chr_flag   = 1; break;
                case K_INKEY:   inkey_flag = 1; break;
                case K_PEEK:    peek_flag  = 1; break;
                }
            }
        }
}

void translateLine (FILE *out, int linelen, int linenum)
{
    /* Translate line into words and characters using the charset array,
     * applying any translation transforms.
     */

    int f, inQuotes = 0, inInverse = 0;
    unsigned char c, keyword = linebuf[0];
    char *x;
    int parens   = 0; /* Track parens level */
    int comma    = 0; /* Handled comma between x,y of PLOT */
    int usr_p    = 0; /* Set flags for post note to print */
    int chr_p    = 0;
    int code_p   = 0;
    int peek_p   = 0;
    int inkey_p  = 0;
    int plot_p   = ( keyword == K_PLOT );
    int unplot_p = ( keyword == K_UNPLOT );

    if ( keyword == K_SLOW || keyword == K_FAST ) return; /* Just remove these */

    fprintf(out, "%4d", linenum);

    for (f = 0; f < linelen - 1; f++)
        {
        c = linebuf[f]; /* Character code  */
        x = charset[c]; /* Translated code */

        if ( c == K_NUMBER )
            {
            f += 5;  /* avoid inline FP numbers */
            inInverse = 0;
            }
        else if ( f == 0 ) /* On the keyword character */
            {
            /* Things to modify only when a command */
            switch (keyword)
                {
                case K_PLOT:
                    fprintf(out, " PLOT 4*(");
                    break;
                case K_UNPLOT:
                    fprintf(out, " PLOT INVERSE 1;4*(");
                    break;
                case K_SCROLL:
                    fprintf(out, " POKE 23692,255: PRINT AT 21,0'': REM SCROLL");
                    break;
                case K_POKE:
                    fprintf(out, " REM POKE ");
                    break;
                default:
                    fprintf(out, "%s", x); /* Print translated char */
                    break;
                }
            }
        else
            {
            /* Track in quotes and parens level */
            if ( (keyword != K_REM) && (c == K_QUOTE) ) inQuotes = !inQuotes;
            if ( !inQuotes && c == K_LPAREN ) parens++;
            if ( !inQuotes && c == K_RPAREN ) parens--;

            if (inInverse && ( c < (139-3*(style==OUT_READABLE)) || c > 191) )
                {
                /* Non-inverse character - discontinue inverse mode if on */
                inInverse = 0;
                if ( style == OUT_ZMAKEBAS )
                    fprintf(out, "\\{20}\\{0}");
                else
                    fprintf(out, "]");
                }

            if ( (plot_p || unplot_p) && !inQuotes && parens == 0 && !comma && c == K_COMMA )
                {
                /* The comma separating the x,y values in plot or unplot command */
                comma = 1;
                fprintf(out, "),4*(");
                }
            else if ( (c >= 8 && c <= 10) || ( style==OUT_ZMAKEBAS && c >= 136 && c <= 138) ) /* Grey block graphics character */
                {
                fprintf(out, "%s", x);
                }
            else if ( c >= (139-3*(style==OUT_READABLE)) && c <= 191) /* Inverse character */
                {
                if ( keyword == K_SAVE ) /* Don't switch to inverse mode */
                    {
                    fprintf(out, "%s", x);
                    }
                else if (inInverse)
                    fprintf(out, "%s", x); /* continue in inverse */
                else
                    {
                    /* Switch to inverse mode */
                    inInverse = 1;
                    if ( style == OUT_ZMAKEBAS )
                        fprintf(out, "\\{20}\\{1}%s", x);
                    else
                        fprintf(out, "[%s", x);
                    }
                }
            else if ( keyword != K_REM && !inQuotes ) /* Only if used */
                {
                if ( c == K_USR )
                    {
                    fprintf(out, "INT INT "); /* Disable by replacing with distinct pattern when a function */
                    usr_p = 1;
                    }
                else
                    {
                    fprintf(out, "%s", x); /* Print translated char */
                    if (c == K_PEEK)        peek_p  = 1;
                    else if (c == K_CHR)    chr_p   = 1;
                    else if (c == K_CODE)   code_p  = 1;
                    else if (c == K_INKEY)  inkey_p = 1;
                    }
                }
            else
                {
                if ( c == K_POWER && (keyword == K_REM || inQuotes) )
                    fprintf(out, "**"); /* Print stars instead of ^ */
                else /* Nothing special */
                    fprintf(out, "%s", x); /* Print translated char */
                }
            }
        }

    if (inInverse)
        {
        /* End of line - discontinue inverse mode if on */
        inInverse = 0;
        if ( style == OUT_ZMAKEBAS )
            fprintf(out, "\\{20}\\{0}");
        else
            fprintf(out, "]");
        }

    /* Append any post stuff needed */

    if ( keyword == K_SAVE ) /* Check for autorun SAVE */
        {
        if ( linebuf[linelen-2] == K_QUOTE ) /* Literal filename */
            {
            c = linebuf[linelen-3];
            if ( c >= 128 ) /* Inverted last char of filename = autosave */
                fprintf(out, " LINE %d", linenum+1);
            }
        }
    else if ( keyword == K_POKE )   fprintf(out, ": REM POKE disabled! << WARNING **");
    else if ( keyword == K_PLOT )   fprintf(out, "): GO SUB %d: REM PLOT 4x", plot_sub);
    else if ( keyword == K_UNPLOT ) fprintf(out, "): GO SUB %d: REM UNPLOT 4x", unplot_sub);

    if ( peek_p )   fprintf(out, ": REM PEEK used! << WARNING **");
    if ( usr_p )    fprintf(out, ": REM USR disabled as INT INT! << WARNING **");
    if ( chr_p )    fprintf(out, ": REM CHR$ used << WARNING **");
    if ( code_p )   fprintf(out, ": REM CODE used << WARNING **");
    if ( inkey_p )  
        {
        fprintf(out, ": REM  INKEY$ used << WARNING ** You may need to change key comparisons to lowercase");
        if ( keyword == K_LET && linebuf[2] == K_DOLLAR) /* Assigned to a string var */
            fprintf(out, " with %s$.", charset[linebuf[1]]);
        else
            fprintf(out, ".");
        }

    fprintf(out, "\n");
}

void checkFile (FILE *in, int *linenum)
{
    /* check (opened) .P file for needed extra routines */

    int b1, b2;
    int f, linelen, d_file, total;

    for (f = 1; f <= 3; f++)
        fgetc(in);   /* ignore sys vars */
    b1 = fgetc(in); b2=fgetc(in);           /* except d_file */
    d_file = b1 + 256 * b2;
    for (f = 1; f <= 111; f++)
        fgetc(in); /* ignore more sys vars */
    fp = ftell(in); /* Save where program starts in the file */
    prog_size = d_file - 16509; /* if d_file is after, prog is 16509 to d_file-1 right? */
    total = prog_size;

    /* First pass to scout for subroutine locations and what xforms the code needs */

    getCodeLine(in, linenum, &linelen, &total); /* Get first line */
    /* Check space before 1st line for UDG call */
    if ( *linenum > 1 ) udg_call = 1; /* Put it at line 1*/

    while (total >= 0) /* run through 'total' bytes */
        {
        checkForSubs(*linenum);         /* Can we put a subroutine before this line? */
        checkLine(linelen, *linenum);   /* Check line for issues */
        prev_line = *linenum;
        getCodeLine(in, linenum, &linelen, &total);   /* Get next line */
        }
    checkForSubs(20000); /* Any subroutines left unplaced can go after the last line */
}

/* process (opened) .P file to stdout */

void processFile (FILE *in, FILE *out)
{
    int linelen, linenum, total;

    /* Go back to start of program */
    fseek(in, fp, SEEK_SET);

    total = prog_size;

    /* run through 'total' bytes, interpreting them as ZX81 program lines */
    getCodeLine(in, &linenum, &linelen, &total);
    while (total >= 0)
        {
        writeSubs(out, linenum);
        /* Write the line */
        translateLine(out, linelen, linenum);
        prev_line = linenum;
        getCodeLine(in, &linenum, &linelen, &total);
        }
    writeSubs(out, 20000);
}

void printUsage ()
{
    printf("p2speccy %s by Ryan Gray\n", VERSION);
    printf("Translates a ZX81 .P file program to Spectrum BASIC text.\n");
    printf("Usage:  p2speccy [options] infile.p > outfile\n");
    printf("Usage:  p2speccy [options] -o  outfile infile.p\n");
    printf("Options are:\n");
    printf("  -z            Output Zmakebas compatible markup\n");
    printf("  -r            Output a more readable markup (default).\n");
    printf("                Inverse characters in square brackets, most block graphics.\n");
    printf("  -o outfile    Give the name of an output file rather than using stdout.\n");
    printf("  -? or --help  Print this usage.\n");
    printf("The Zmakebas output will use \\{xxx} codes in REMs and quotes to preserve\n");
    printf("the non-printable and token character codes, whereas in readable mode, these\n");
    printf("will give a hash (#) character. Zmakebas mode also inserts inverse and true\n");
    printf("video codes where inverse characters appear in REMs and strings.\n");
}

void parseOptions (int argc, char *argv[])
{
    while ((argc > 1) && (argv[1][0] == '-'))
        {
        switch (argv[1][1])
            {
            case 'z':
                style = OUT_ZMAKEBAS;
                charset = charset_zmb;
                break;
            case 'r':
                style = OUT_READABLE;
                charset = charset_read;
                break;
            case 'o':
                outfile = argv[2];
                ++argv;
                --argc;
                break;
            case '?':
                printUsage();
                exit(EXIT_SUCCESS);
            case '-':
                if (strcmp(argv[1],"-") == 0)
                    {
                    infile = argv[1];
                    }
                else if (STRCMPI(argv[1],"--help") == 0)
                    {
                    printUsage();
                    exit(EXIT_SUCCESS);
                    }
                else if (STRCMPI(argv[1],"--version") == 0)
                    {
                    printf("%s\n", VERSION);
                    }
                else
                    {
                    printUsage();
                    exit(EXIT_FAILURE);
                    }
            default:
                printUsage();
                fprintf(stderr, "unknown option: %c\n", argv[1][1]);
                exit(EXIT_FAILURE);
            }
	    ++argv;
	    --argc;
        }
    if (argc <= 1)
        {
        printUsage();
        exit(EXIT_FAILURE);
        }
    if (!infile)
        infile = argv[argc-1];
}


int main (int argc, char *argv[])
{
    FILE *in, *out;
    int linenum;

    if (argc < 2)
        {
        printUsage();
        exit(EXIT_FAILURE);
        }
    parseOptions(argc, argv);

    if ( strcmp(infile,".") == 0 )

        in = stdin;

    else
        {
        in = fopen(infile, "rb");
        if ( in == NULL )
            {
            fprintf(stderr, "Error: couldn't open file '%s'\n", infile);
            exit(1);
            }
        }

    if ( outfile[0] == '\0' )
        out = stdout;
    else
        {
        out = fopen(outfile, "wt");
        if ( out == NULL )
            {
            fprintf(stderr, "Error: couldn't write output file '%s'\n", outfile);
            exit(1);
            }
        }

    if ( style == OUT_READABLE)
        warn = warn_READ;
    else
        warn = warn_ZMB;

    checkFile(in, &linenum); /* 1st pass to check */
    processFile(in, out);    /* 2nd pass to process */
    fclose(in);
    fclose(out);

    exit(0);
}
