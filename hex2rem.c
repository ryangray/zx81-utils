/* hex2zmbrem - Convert a file of hex codes to a zmakebas text file of one REM statement
 * By Ryan Gray April 2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define VERSION "1.0.0"

char *infile = "";
enum input_style {IN_HEX, IN_BINARY};
enum input_style in_fmt = IN_HEX;


void print_usage ()
  {
  printf("hex2zmbrem %s by Ryan Gray\n", VERSION);
  printf("Make a one-line REM statement from hex codes for zmakebas input.\n");
  printf("Usage:  hex2zmbrem [options] infile > outfile\n");
  printf("Options are:\n");
  printf("  -h  Input is ASCII hex codes (default)\n");
  printf("  -b  Input is a binary file.\n");
  printf("The Zmakebas output will use \\{xxx} codes in the REM to preserve\n");
  printf("the byte codes.\n");
  exit(1);
  }


void parse_options(int argc, char *argv[])
{
    int opt = 0;

    while ((opt = getopt(argc, argv, "hb")) != -1)
        {
        switch (opt)
            {
            case 'h':
                in_fmt = IN_HEX;
                break;
            case 'b':
                in_fmt = IN_BINARY;
                break;
            default:
                print_usage();
                exit(EXIT_FAILURE);
            }
        }
    if (argc <= 1)
        {
        print_usage();
        exit(EXIT_FAILURE);
        }
    if (optind < argc)
        infile = argv[optind];
}

#define BUFFLEN 32768
#define WRAP 10

int main(int argc,char *argv[])
{
FILE *in;
int b, h1, h2, l, n;
char buff[BUFFLEN], *ptr;

parse_options(argc, argv);

if ( strcmp(infile,".") == 0 )
    in = stdin;

else
    {
    if ( in_fmt == IN_BINARY )
        in = fopen(infile, "rb");
    else
        in = fopen(infile, "rt");

    if ( in == NULL )
        {
        fprintf(stderr, "Error: couldn't open file '%s'\n", infile);
        exit(EXIT_FAILURE);
        }
    }

printf(" 1 REM ");
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
                printf("\\\n"); /* zmakebas continue line */
                }
            printf("\\{0x%02X}", b);
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
                    printf("\nInvalid hex code: %c%c\n", h1, h2);
                    exit(EXIT_FAILURE);
                    }
                if (n >= WRAP)
                    {
                    n = 0;
                    printf("\\\n"); /* zmakebas continue line */
                    }
                printf("\\{0x%c%c}", h1, h2);
                n++;
                }
            }
        }
    }
printf("\n");
fclose(in);

exit(0);
}
