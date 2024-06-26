/* p2txt - list the BASIC program in an xtender/Z81 format .p file
 * PD by RJM 1993
 * cleaned up and ansified 960504
 * Forked by ryangray Jan2023
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1.0.2"

#define QUOTE_code 11
#define NUM_code 126
#define REM_code 234

unsigned char linebuf[32768]; /* BIG buffer for those lovely m/c REMs */
char *infile;
enum outstyle {OUT_READABLE, OUT_ZMAKEBAS, OUT_ZXTEXT2P};
enum outstyle style = OUT_READABLE;
int inFirstLineREM; /* 1=First line is a REM and we are on the first line */
int onlyFirstLineREM = 0; /* 1=Only preserve codes in a first line REM, 0=Preserve codes everywhere */

/* Character mapping ZX81 character set to ASCII
 *
 * We have 3 different mappings for optional output styles: readable, Zmakebas,
 * and ZXText2P
 */

/* Definitions for a "readable" character mapping in charset_read that uses
   Unicode characters when possible (upper ASCII for DOS) and square-bracketed
   letters for inverse characters.
*/

#define NAK "#" /* Not A Kharacter */

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

char *charset_read[] =
{
/*            spc \'  \ ' \'' \.  \:  \.' \:' \!:  \!. */
/* 000-009 */ " ",BUL,BUR,TOP,BLL,BLF,BUP,ILR,BGY,"\\,,",
/* 010-019 */ "\\~~","\"",POUND,"$",":","?","(",")",">","<",
/* 020-029 */ "=","+","-","*","/",";",",",".","0","1",
/* 030-039 */ "2","3","4","5","6","7","8","9","A","B",
/* 040-049 */ "C","D","E","F","G","H","I","J","K","L",
/* 050-059 */ "M","N","O","P","Q","R","S","T","U","V",
/* 060-069 */ "W","X","Y","Z","RND","INKEY$ ","PI",NAK,NAK,NAK,
/* 070-079 */ NAK,NAK,NAK,NAK,NAK, NAK,NAK,NAK,NAK,NAK,
/* 080-089 */ NAK,NAK,NAK,NAK,NAK, NAK,NAK,NAK,NAK,NAK,
/* 090-099 */ NAK,NAK,NAK,NAK,NAK, NAK,NAK,NAK,NAK,NAK,
/* 100-109 */ NAK,NAK,NAK,NAK,NAK, NAK,NAK,NAK,NAK,NAK,
/* 110-119 */ NAK,NAK,NAK,NAK,NAK, NAK,NAK,NAK,NAK,NAK,
/* 120-129 */ NAK,NAK,NAK,NAK,NAK, NAK,NAK,NAK,BLK,IUL, /* \:: \.: */
/*            \:. \.. \': \ : \'.  \ .     \|: */
/* 130-139 */ IUR,BTM,ILL,BRT,BDN, BLR,"[" BGY "]","[,,]","[~~]","[\"]",
/* 140-149 */ "[" POUND "]","[$]","[:]","[?]","[(]","[)]","[>]","[<]","[=]","[+]",
/* 150-159 */ "[-]","[*]","[/]","[;]","[,]","[.]","[0]","[1]","[2]","[3]",
/* 160-169 */ "[4]","[5]","[6]","[7]","[8]","[9]","[A]","[B]","[C]","[D]",
/* 170-179 */ "[E]","[F]","[G]","[H]","[I]","[J]","[K]","[L]","[M]","[N]",
/* 180-189 */ "[O]","[P]","[Q]","[R]","[S]","[T]","[U]","[V]","[W]","[X]",
/* 190-199 */ "[Y]","[Z]","\"\"","AT ","TAB ",NAK,"CODE ","VAL ","LEN ","SIN ",
/* 200-209 */ "COS ","TAN ","ASN ","ACS ","ATN ","LN ","EXP ",
		"INT ","SQR ","SGN ",
/* 210-219 */ "ABS ","PEEK ","USR ","STR$ ","CHR$ ","NOT ",
		"**"," OR "," AND ","<=",
/* 220-229 */ ">=","<>"," THEN"," TO "," STEP "," LPRINT ",
		" LLIST "," STOP"," SLOW"," FAST",
/* 230-239 */ " NEW"," SCROLL"," CONT "," DIM "," REM "," FOR "," GOTO ",
		" GOSUB "," INPUT "," LOAD ",
/* 240-249 */ " LIST "," LET "," PAUSE "," NEXT "," POKE ",
		" PRINT "," PLOT "," RUN "," SAVE ",
		" RAND ",
/* 250-255 */ " IF "," CLS"," UNPLOT "," CLEAR"," RETURN"," COPY"
};

/* Definitions for a character mapping compatible with Zmakebas in charset_zmb 
   that uses the escape sequences that zmakebas uses for non-ASCII characters
   and lowercase letters for inverse letters with a backslash before other
   inverse characters.
*/

/* Block Graphics escapes used by Zmakebas and listbasic:
 * 
 * "\' "  001 Top-left corner
 * "\ '"  002 Top-right corner
 * "\''"  003 Top half
 * "\. "  004 Bottom-left corner
 * "\: "  005 Left half
 * "\.'"  006 Bottom-left, top-right corners
 * "\:'"  007 Bottom-right empty
 * "\!:"  008 "Gray" solid
 * "\!."  009 "Gray" bottom half
 * "\!'"  010 "Gray top half
 * "\::"  128 Solid (inverse space)
 * "\.:"  129 Top-left bank (inverse of "\' ")
 * "\:."  130 Top-right blank (inverse of "\ '")
 * "\.."  131 Bottom half (inverse of "\''")
 * "\':"  132 Bottom-left blank (inverse of "\. ")
 * "\ :"  133 Right half (inverse of "\: ")
 * "\'."  134 Top-left, bottom-right (inverse of "\.'")
 * "\ ."  135 Bottom-right corner (inverse of "\:'")
 * "\|:"  136 Inverse "gray" (inverse pixels of "\!:")
 * "\|."  137 Black top, gray bottom (inverse of "\!.")
 * "\|'"  138 Black bottom, gray top (inverse of "\!'")
 * 
 * Special Characters:
 * 
 * "\\"   012 Pound Sterling symbol (its not # since that is a comment char)
 * "\@"   140 Inverse Pound Sterling symbol
 * "`"    192 Backtick for the Quote Image "" character (has no inverse)
 * Lowercase letters for inverse letters
 * Backslash before other characters for their inverse versions
 *  
 */

char *charset_zmb[] =
{
/* 000-009 */ " ","\\' ","\\ '","\\''","\\. ","\\: ","\\.'","\\:'","\\!:","\\!.",
/* 010-019 */ "\\!'","\"","\\\\","$",":","?","(",")",">","<",
/* 020-029 */ "=","+","-","*","/",";",",",".","0","1",
/* 030-039 */ "2","3","4","5","6","7","8","9","A","B",
/* 040-049 */ "C","D","E","F","G","H","I","J","K","L",
/* 050-059 */ "M","N","O","P","Q","R","S","T","U","V",
/* 060-069 */ "W","X","Y","Z","RND","INKEY$ ","PI",NAK,NAK,NAK,
/* 070-079 */ NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,
/* 080-089 */ NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,
/* 090-099 */ NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,
/* 100-109 */ NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,
/* 110-119 */ NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,
/* 120-129 */ NAK,NAK,NAK,NAK,NAK,NAK,NAK,NAK,"\\::","\\.:",
/* 130-139 */ "\\:.","\\..","\\':","\\ :","\\'.", "\\ .","\\|:","\\|.","\\|'","\\\"",
/* 140-149 */ "\\@","\\$","\\:","\\?","\\(","\\)","\\>","\\<","\\=","\\+",
/* 150-159 */ "\\-","\\*","\\/","\\;","\\,","\\.","\\0","\\1","\\2","\\3",
/* 160-169 */ "\\4","\\5","\\6","\\7","\\8","\\9","a","b","c","d",
/* 170-179 */ "e","f","g","h","i","j","k","l","m","n",
/* 180-189 */ "o","p","q","r","s","t","u","v","w","x",
/* 190-199 */ "y","z","`","AT ","TAB ",NAK,"CODE ","VAL ","LEN ","SIN ",
/* 200-209 */ "COS ","TAN ","ASN ","ACS ","ATN ","LN ","EXP ",
		"INT ","SQR ","SGN ",
/* 210-219 */ "ABS ","PEEK ","USR ","STR$ ","CHR$ ","NOT ",
		"**"," OR "," AND ","<=",
/* 220-229 */ ">=","<>"," THEN"," TO "," STEP "," LPRINT ",
		" LLIST "," STOP"," SLOW"," FAST",
/* 230-239 */ " NEW"," SCROLL"," CONT "," DIM "," REM "," FOR "," GOTO ",
		" GOSUB "," INPUT "," LOAD ",
/* 240-249 */ " LIST "," LET "," PAUSE "," NEXT "," POKE ",
		" PRINT "," PLOT "," RUN "," SAVE ",
		" RAND ",
/* 250-255 */ " IF "," CLS"," UNPLOT "," CLEAR"," RETURN"," COPY"
};

/* Definitions for a character mapping compatible with the ZXText2P utility in 
   charset_zxtext2p that uses the escape sequences that ZXText2P uses for non-
   ASCII characters and a percent sign before inverse characters.
*/

#define NAK2 "!" /* Not A Kharacter for ZXText2P */

char *charset_zxtext2p[] =
{
/* 000-009 */ " ","\\' ","\\ '","\\''","\\. ","\\: ","\\.'","\\:'","\\##","\\,,",
/* 010-019 */ "\\~~","\"","#","$",":","?","(",")",">","<",
/* 020-029 */ "=","+","-","*","/",";",",",".","0","1",
/* 030-039 */ "2","3","4","5","6","7","8","9","A","B",
/* 040-049 */ "C","D","E","F","G","H","I","J","K","L",
/* 050-059 */ "M","N","O","P","Q","R","S","T","U","V",
/* 060-069 */ "W","X","Y","Z","RND","INKEY$ ","PI",NAK2,NAK2,NAK2,
/* 070-079 */ NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,
/* 080-089 */ NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,
/* 090-099 */ NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,
/* 100-109 */ NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,
/* 110-119 */ NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,
/* 120-129 */ NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,NAK2,"\\::","\\.:",
/* 130-139 */ "\\:.","\\..","\\':","\\ :","\\'.", "\\ .","\\@@","\\;;","\\!!","%\"",
/* 140-149 */ "%#","%$","%:","%?","%(","%)","%>","%<","%=","%+",
/* 150-159 */ "%-","%*","%/","%;","%,","%.","%0","%1","%2","%3",
/* 160-169 */ "%4","%5","%6","%7","%8","%9","%A","%B","%C","%D",
/* 170-179 */ "%E","%F","%G","%H","%I","%J","%K","%L","%M","%N",
/* 180-189 */ "%O","%P","%Q","%R","%S","%T","%U","%V","%W","%X",
/* 190-199 */ "%Y","%Z","\\\"","AT ","TAB ",NAK2,"CODE ","VAL ","LEN ","SIN ",
/* 200-209 */ "COS ","TAN ","ASN ","ACS ","ATN ","LN ","EXP ",
		"INT ","SQR ","SGN ",
/* 210-219 */ "ABS ","PEEK ","USR ","STR$ ","CHR$ ","NOT ",
		"**"," OR "," AND ","<=",
/* 220-229 */ ">=","<>"," THEN"," TO "," STEP "," LPRINT ",
		" LLIST "," STOP"," SLOW"," FAST",
/* 230-239 */ " NEW"," SCROLL"," CONT "," DIM "," REM "," FOR "," GOTO ",
		" GOSUB "," INPUT "," LOAD ",
/* 240-249 */ " LIST "," LET "," PAUSE "," NEXT "," POKE ",
		" PRINT "," PLOT "," RUN "," SAVE ",
		" RAND ",
/* 250-255 */ " IF "," CLS"," UNPLOT "," CLEAR"," RETURN"," COPY"
};


char **charset = charset_read;

/* get a single ZX81 program line & line number & length */

void zxgetline(FILE *in, int *linenum, int *linelen, int *t)
{
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


/* translate line into keywords using the charset array */

void xlatline(int linelen)
{
int f, inQuotes = 0;
unsigned char c, keyword = linebuf[0];
char *x;

for (f = 0; f < linelen - 1; f++)
    {
    c = linebuf[f]; /* Character code  */
    x = charset[c]; /* Translated code */

    if ( (keyword != REM_code) && (c == QUOTE_code) )
        inQuotes = !inQuotes;

    if ( (keyword != REM_code) && (c == NUM_code) )
        f += 5;  /* avoid inline FP numbers - but ok for REMs */

    else if ( (keyword == REM_code && f > 0) || inQuotes)
        {
        if ( style == OUT_ZMAKEBAS && (!onlyFirstLineREM || inFirstLineREM) &&
                ((strcmp(x, NAK) == 0) || ((strlen(x) > 1) && (x[0] != '\\') && (x[0]!='`'))) )

            printf("\\{%d}", c); /* Print escaped as char code */

        else if ( style == OUT_ZXTEXT2P && inFirstLineREM && ((c>63 && c<128) || c>191) )

            printf("\\%02X", c); /* Print as backslash-escaped hex code */

        else
            printf("%s", x); /* Print translated char */
        }
    else
        printf("%s", x); /* Print translated char */
    }
printf("\n");
}


/* process (opened) .P file to stdout */

void thrashfile (FILE *in)
{
int b1, b2;
int f, linelen, linenum, d_file, total;

for (f = 1; f <= 3; f++)
    fgetc(in);   /* ignore sys vars */
b1 = fgetc(in); b2=fgetc(in);           /* except d_file */
d_file = b1 + 256 * b2;
for (f = 1; f <= 111; f++)
    fgetc(in); /* ignore more sys vars */

total = d_file - 16509; /* if d_file is after, prog is 16509 to d_file-1 right? */

/* run through 'total' bytes, interpreting them as ZX81 program lines */
zxgetline(in, &linenum, &linelen, &total);
inFirstLineREM = (linebuf[0] == REM_code);

while (total >= 0)
    {
    printf("%4d", linenum);
    xlatline(linelen);
    inFirstLineREM = 0;
    zxgetline(in, &linenum, &linelen, &total);
    }
}

void printUsage ()
  {
  printf("p2txt %s by Ryan Gray, from original by Russell Marks \n", VERSION);
  printf("    for improbabledesigns.\n");
  printf("Lists ZX81 .P files to stdout.\n");
  printf("Usage:  p2txt [options] infile.p > outfile.txt\n");
  printf("Options are:\n");
  printf("  -z  Output Zmakebas compatible markup\n");
  printf("  -r  Output a more readable markup (default).\n");
  printf("      Inverse characters in square brackets, most block graphics.\n");
  printf("  -1  Output Zmakebas markup but only use codes in a first line that is a REM.\n");
  printf("  -2  Output ZXText2P compatible markup\n");
  printf("  -?  Print this help.\n");
  printf("The Zmakebas output will use \\{xxx} codes in REMs and quotes to preserve\n");
  printf("the non-printable and token character codes, whereas in readable mode, these\n");
  printf("will give a hash (#) character. Zmakebas mode also inserts inverse and true\n");
  printf("video codes where inverse characters appear in REMs and strings.\n");
  }

void parse_options(int argc, char *argv[])
{
    while ((argc > 1) && (argv[1][0] == '-'))
        {
        switch (argv[1][1])
            {
            case 'z':
                style = OUT_ZMAKEBAS;
                charset = charset_zmb;
                onlyFirstLineREM = 0;
                break;
            case 'r':
                style = OUT_READABLE;
                charset = charset_read;
                onlyFirstLineREM = 0;
                break;
            case '1':
                style = OUT_ZMAKEBAS;
                charset = charset_zmb;
                onlyFirstLineREM = 1;
                break;
            case '2':
                style = OUT_ZXTEXT2P;
                charset = charset_zxtext2p;
                onlyFirstLineREM = 0;
                break;
            case '?':
                printUsage();
                exit(EXIT_SUCCESS);
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
    infile = argv[argc-1];
}


int main(int argc, char *argv[])
{
FILE *in;

parse_options(argc, argv);

if ( (in = fopen(infile, "rb")) == NULL )
    {
    fprintf(stderr, "Error: couldn't open file '%s'\n", infile);
    exit(1);
    }

thrashfile(in);      /* process it */
fclose(in);

exit(0);
}
