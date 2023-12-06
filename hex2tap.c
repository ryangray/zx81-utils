/* hex2tap
 * By Ryan Gray
 * October 2023
 * 
 * This takes a binary input file and puts it into a .tap file as a ZX Spectrum 
 * CODE block. The bytes are not translated, so this would be Z80 machine code
 * suitable for running on the Spectrum or similar model.
 * 
 * You can specify the code block starting address as well as the tape block
 * name. The default output is to standard out. 
 */

/* This should be hex2tap - outputs a .tap file with a CODE block
   The other should be hex2data, and we alreaday have hex2rem
 */

/* This is essentialy hex2rem but outputting to a CODE block in a Spectrum .tap 
 * file. If I combined them, you'd need an output format switch. If I
 * added it to zmakebas, you'd need an output switch as well. Also, hex2rem as a
 * name is not really specific as to ZX81 or Spectrum, and the result is a 
 * zmakebas text file that could work for either, assuming the source bytes do.
 * So, hex2rem lets you move machine code to a REM statement, and this lets you
 * move it to a .tap file as CODE. I could add an option for output to either a
 * zmakebas REM, a REM in a ZX81 .p file, a REM in a .tap file, or CODE in a 
 * .tap file. For the REM output, there can be a switch for the line number
 * since the .tap file version could be MERGEd into an existing program. That
 * could also make it easy to have a DATA statement output format where the 
 * bytes are written as BASIC DATA lines:
 *   8000 DATA 23,12,114,209,...
 * This could also be done for the ZX81 with a READ/DATA MC add-in that uses:
 *   8000 REM ,23,12,114,209,...
 * This would also need a line increment option, and a line length option.
 * To write this in .p or .tap format would need the system vars output, display
 * file output, and FP number output. At least the BASIC line code output would
 * be easy. Just emitting zmakebas output like hex2rem does would be easy.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define VERSION "1.0.1"

#ifdef __MSDOS__
#define STRCMPI strcmpi
#else
#define STRCMPI strcasecmp
#endif
#define BUFSIZE 49152
#define LINESIZE 8192

char *infile  = "";
char *outfile = "";
char *speccy_filename = "";
unsigned int length = 0;
unsigned int address = 0;
char filebuf[BUFSIZE];
FILE *in, *out;
enum input_style {IN_HEX, IN_BINARY};
enum input_style in_fmt = IN_BINARY;

void usageHelp()
{
    printf("hex2tap v%s - by Ryan Gray\n\n", VERSION);

    printf("Usage: hex2tap [-?] [-h | -b] -a address [-n speccy_filename]\n");
    printf("               [-o output_file] [input_file]\n\n");

    printf("    -?           Print this help\n");
    printf("    -b           Input is a binary file (default)\n");
    printf("    -h           Input is text file of hex codes\n");
    printf("    -a address   Start address of the code (use 0x prefix for hex)\n");
    printf("                 Use '-a UDG' as an alias for '-a 65368' (USR \"a\").\n");
    printf("                 Use '-a SCR' as an alias for '-a 16384' (SCREEN$).\n");
    printf("    -n name      Set Spectrum filename (default is blank or -o name)\n");
    printf("    -o filename  Specify output file name (default is stdout)\n");
    printf("\nDefault input is stdin or explicitly with input filename of '-'\n");
}


void parseOptions(int argc, char *argv[])
{
    char *aptr;

    while ((argc > 1) && (argv[1][0] == '-'))
        {
        switch (argv[1][1])
            {
            case 'o':
                outfile = argv[2];
                ++argv;
                --argc;
                break;
            case 'n':
                speccy_filename = argv[2];
                ++argv;
                --argc;
                break;
            case 'a':
                aptr = argv[2] + strlen(argv[2]) - 1;
                if (STRCMPI(argv[2],"UDG") == 0)
                    address = 65368;
                else if (STRCMPI(argv[2],"SCR") == 0)
                    address = 16384;
                else
                    address = (unsigned int)strtoul(argv[2], &aptr, 0);
                ++argv;
                --argc;
                break;
            case 'h':
                in_fmt = IN_HEX;
                break;
            case 'b':
                in_fmt = IN_BINARY;
                break;
            case '?':
                usageHelp();
                exit(0);
            default:
                usageHelp();
                exit(EXIT_FAILURE);
            }
	    ++argv;
	    --argc;
        }
    if (argc <= 1)
        {
        usageHelp();
        exit(EXIT_FAILURE);
        }
    infile = argv[argc-1];
}


void readInputBytes ()
{
    /* Read input as just a stream of bytes into filebuf */
    int b;

    length = 0;

    while ( !feof(in) && length < BUFSIZE )
        {
        b = fgetc(in);
        if ( b != -1 )
            {
            filebuf[length] = b;
            length++;
            }
        }
}

void readInputHex ()
{
    /* Read input as a text file of hex bytes, ignoring spaces and tabs between */

    unsigned int l, m;
    char buff[LINESIZE];
    char *ptr, h1, h2;
    
    while ( fgets(buff, LINESIZE, in) != NULL )
        {
        l = strlen(buff);
        for (ptr = buff; ptr < buff+l-1; ptr++)
            {
            h1 = toupper(*ptr);
            if (h1 != ' ' && h1 != '\t')
                {
                ptr++;
                h2 = toupper(*ptr);
                if (h1 >= '0' && h1 <= '9')
                    m = 16 * (h1-'0');
                else if (h1 >= 'A' && h1 <= 'F')
                    m = 16 * (h1-'A'+10);
                else
                    {
                    printf("\nInvalid hex code: '%c%c' at offset %d\n", h1, h2, length);
                    exit(EXIT_FAILURE);
                    }
                if (h2 >= '0' && h2 <= '9')
                    m = m + h2 - '0';
                else if (h2 >= 'A' && h2 <= 'F')
                    m = m + h2 - 'A' + 10;
                else
                    {
                    printf("\nInvalid hex code '%c%c' at offset %d\n", h1, h2, length);
                    exit(EXIT_FAILURE);
                    }
                filebuf[length] = m;
                length++;
                }
            }
        }
}


int main (int argc, char *argv[])
{
    int f, chk;
    char headerbuf[17];

    parseOptions(argc, argv);

    if (argc < 2)
        usageHelp();
    else
        parseOptions(argc, argv);

    if ( strcmp(infile,"-") == 0 )

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

    if (strcmp(outfile, "-") == 0 || strcmp(outfile,"") == 0)
    
        out = stdout;
        
    else
        {
        out = fopen(outfile, "wb");
        if (out == NULL)
            {
            fprintf(stderr, "Couldn't open output file.\n");
            exit(1);
            }
        }

    /* Read input */

    switch (in_fmt)
        {
        case IN_BINARY:
            readInputBytes();
            break;
        case IN_HEX:
            readInputHex();
            break;
        default:
            fprintf(stderr,"Bad input_style: %d", in_fmt);
            exit(1);
            break;
        }

    if (in != stdin)
        fclose(in);

    /* make header */

    headerbuf[0] = 3;   /* CODE file */
    /* Pad filename with spaces to 10 chars */
    strncpy(headerbuf + 1, speccy_filename, 10);
    for (f = strlen(speccy_filename); f < 10; f++)
        headerbuf[f+1] = 32;
    headerbuf[11] = (length & 255);    /* Length of data block */
    headerbuf[12] = (length / 256);
    headerbuf[13] = (address & 255);   /* Start address of CODE */
    headerbuf[14] = (address / 256);
    headerbuf[15] = 0;                 /* 32768 for a CODE block */
    headerbuf[16] = 0x80;

    /* write header block */

    fprintf(out, "%c%c%c", 19, 0, 0); /* Header block length is 19 (19,0), 0=header block */
    fputc(headerbuf[0], out);
    chk = headerbuf[0];
    for (f = 1; f < 17; f++)
        {
        fputc(headerbuf[f], out);
        chk ^= headerbuf[f];
        }
    fputc(chk, out); /* Check byte */
    //fprintf(stderr,"Header check = %02X\n",chk);

    /* write data block */
    
    /* Length lo/hi, 0xFF=data*/
    fprintf(out, "%c%c%c", (length + 2) & 255, (length + 2) >> 8, chk = 255);
    for (f = 0; f < length; f++)
        chk ^= filebuf[f];
    fwrite(filebuf, 1, length, out);
    fputc(chk, out);
    //fprintf(stderr,"Data check = %02X\n",chk);
    if (out != stdout)
    	fclose(out);

    return 0;
}
