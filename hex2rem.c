/* hex2rem - Convert a file of hex codes to a zmakebas text file of one REM statement
 * By Ryan Gray April 2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define VERSION "1.1"

char *infile = NULL;
char *outfile = NULL;
enum input_style {IN_HEX, IN_BINARY};
enum input_style in_fmt = IN_HEX;
int lineno = 1;

void print_usage ()
  {
  printf("hex2rem %s by Ryan Gray\n", VERSION);
  printf("Make a one-line REM statement from hex codes for zmakebas input.\n");
  printf("Usage:  hex2rem [-?] [-h | -b] [-l nnnn] [infile [outfile]]\n");
  printf("Options are:\n\n");
  
  printf("  -?        Print this help\n");
  printf("  -h        Input is ASCII hex codes (default)\n");
  printf("  -b        Input is a binary file.\n");
  printf("  -l nnnn   Specify line number of REM (default is 1)\n");
  printf("The Zmakebas output will use \\{xxx} codes in the REM to preserve\n");
  printf("the byte codes. Input and output files default to standard in/out.\n");
  exit(1);
  }


void parse_options(int argc, char *argv[])
{
    char *aptr;

    while (argc > 1)
        {
        if (argv[1][0] == '-')
            {
            switch (argv[1][1])
                {
                case 'h':
                    in_fmt = IN_HEX;
                    break;
                case 'b':
                    in_fmt = IN_BINARY;
                    break;
                case 'l':
                    if (argc < 3)
                        {
                        print_usage();
                        exit(EXIT_FAILURE);
                        }
                    aptr = argv[2] + strlen(argv[2]) - 1;
                    lineno = (unsigned int)strtoul(argv[2], &aptr, 0);
                    ++argv;
                    --argc;
                    break;
                case '\0':
                    infile = argv[1];
                    break;
                default:
                    print_usage();
                    exit(EXIT_FAILURE);
                }
            }
        else if (!infile)
            {
            infile = argv[1];
            }
        else if (!outfile)
            {
            outfile = argv[1];
            }
        else
            {
            print_usage();
            exit(EXIT_FAILURE);
            }
	    ++argv;
	    --argc;
        }
}

#define BUFFLEN 16384 /* Above this causes problems on DOS w/Turbo C++ 3  and is only */
#define WRAP 10 /* How many codes per line */

int main(int argc,char *argv[])
{
FILE *in, *out;
int b, h1, h2, l, n;
char buff[BUFFLEN], *ptr;

parse_options(argc, argv);

if (!infile || strcmp(infile,"-") == 0 )
    {
    in = stdin;
    }
else
    {
    if ( in_fmt == IN_BINARY )
        in = fopen(infile, "rb");
    else
        in = fopen(infile, "rt");

    if ( in == NULL )
        {
        fprintf(stderr, "Error: couldn't open input file '%s'\n", infile);
        exit(EXIT_FAILURE);
        }
    }

if (!outfile || strcmp(outfile,"-") == 0 )
    {
    out = stdout;
    }
else
    {
    out = fopen(outfile, "wt");

    if ( out == NULL )
        {
        fprintf(stderr, "Error: couldn't open output file '%s'\n", outfile);
        exit(EXIT_FAILURE);
        }
    }

fprintf(out, "%d REM ", lineno);
n = 0;

while (!feof(in))
    {
    if (in_fmt == IN_BINARY)
        {
        b = fgetc(in);
        if ( b != -1 )
            {
            if (n >= WRAP)
                {
                n = 0;
                fprintf(out, "\\\n"); /* zmakebas continue line */
                }
            fprintf(out, "\\{0x%02X}", b);
            n++;
            }
        }
    else if ( fgets(buff, BUFFLEN, in) != NULL )
        {
        l = strlen(buff);
        for (ptr = buff; ptr < buff+l-1; ptr++)
            {
            h1 = toupper(*ptr);
            if (h1 != ' ' && h1 != '\t')
                {
                ptr++;
                h2 = toupper(*ptr);
                if (h1 < '0' || h1 > 'F' || h2 < '0' || h2 > 'F')
                    {
                    fprintf(stderr, "\nInvalid hex code: %c%c\n", h1, h2);
                    exit(EXIT_FAILURE);
                    }
                if (n >= WRAP)
                    {
                    n = 0;
                    fprintf(out, "\\\n"); /* zmakebas continue line */
                    }
                fprintf(out, "\\{0x%c%c}", h1, h2);
                n++;
                }
            }
        }
    }
fprintf(out, "\n");
fclose(in);
fclose(out);

exit(EXIT_SUCCESS);
}
