/* rem2bin - Extract the machine code from the 1st line REM in a ZX81 P file
 * By Ryan Gray April 2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define VERSION "1.0.1"
#define REM_code 234

char *infile = "", *outfile = "";
enum output_style {OUT_HEX, OUT_BINARY};
enum output_style out_fmt = OUT_BINARY;


void print_usage ()
  {
  printf("rem2bin %s by Ryan Gray\n", VERSION);
  printf("Extrtact codes for 1st line REM statement of a ZX81 P file program.\n");
  printf("Usage:  rem2bin [-h|-b] infile > outfile\n");
  printf("        rem2bin [-h|-b] -o outfile infile\n");
  printf("Options are:\n");
  printf("  -h  : Output is ASCII hex codes\n");
  printf("  -b  : Output is a binary file (default).\n");
  printf("  -o outputfile  : Give name of output file\n");
  exit(1);
  }


void parse_options(int argc, char *argv[])
{
    while ((argc > 1) && (argv[1][0] == '-'))
        {
        switch (argv[1][1])
            {
            case 'h':
                out_fmt = OUT_HEX;
                break;
            case 'b':
                out_fmt = OUT_BINARY;
                break;
            case 'o':
                outfile = argv[2];
                ++argv;
                --argc;
                break;
            default:
                print_usage();
                exit(EXIT_FAILURE);
            }
	    ++argv;
	    --argc;
        }
    if (argc <= 1)
        {
        print_usage();
        exit(EXIT_FAILURE);
        }
    infile = argv[argc-1];
}

#define WRAP 16 /* How many codes per line */

int main(int argc,char *argv[])
{
FILE *in, *out;
int b1, b2, l, n;

parse_options(argc, argv);

if ( strcmp(infile,".") == 0 )

    in = stdin;

else
    {
    in = fopen(infile, "rb");
    if ( in == NULL )
        {
        fprintf(stderr, "Error: couldn't open input file '%s'\n", infile);
        exit(EXIT_FAILURE);
        }
    }

if ( strcmp(outfile,"") == 0 )

    out = stdout;

else if ( out_fmt == OUT_BINARY )

    out = fopen(outfile, "wb");

else
    out = fopen(outfile, "wt");

if ( out == NULL )
    {
    fprintf(stderr, "Error: couldn't open output file '%s'\n", outfile);
    exit(EXIT_FAILURE);
    }

for (n = 1; n <= 116; n++) /* ignore sys vars */
    fgetc(in);

b1 = fgetc(in); /* First line number */
b2 = fgetc(in);
l = b1 * 256 + b2;

b1 = fgetc(in); /* Line length */
b2 = fgetc(in);
l = b1 + 256 * b2 - 2; /* Less a byte for newline and for REM keyword */

b1 = fgetc(in); /* Keyword of 1st line */
if (b1 != REM_code)
    {
    fprintf(stderr, "First line not a REM statement");
    exit(1);
    }
b2 = 0;
n = 0;

while (!feof(in) && b2 < l)
    {
    b1 = fgetc(in);
    if ( b1 == -1 )
        {
        fprintf(stderr,"File too short\n");
        exit(1);
        }
    b2++;
    if (out_fmt == OUT_BINARY)
        {
        fprintf(out, "%c", b1);
        }
    else
        {
        if (n >= WRAP)
            {
            n = 0;
            fprintf(out, "\n");
            }
        fprintf(out, "%02X", b1);
        n++;
        }
    }
if (out_fmt == OUT_HEX)
    fprintf(out, "\n");
fclose(in);
fclose(out);

exit(0);
}
