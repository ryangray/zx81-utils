/* tapnoauto
 * By Ryan Gray
 * December 2023
 * 
 * This takes a .tap file and changes each program contained within to not 
 * autorun when loaded (the old autorun line number is printed for reference).
 * Currenty, all programs in the tap file are changed.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define VERSION "1.0.0"

#ifdef __MSDOS__
#define STRCMPI strcmpi
#else
#define STRCMPI strcasecmp
#endif

char *infile  = "";
char *outfile = ""; /* Base name if multiple segments are selected */
char *outname; /* Full output filename including a version number */
char header[19]; /* For reading tap header blocks (not including two block length bytes) */
FILE *in, *out;

void usageHelp()
{
    printf("tapnoauto v%s - by Ryan Gray\n\n", VERSION);

    printf("Usage: tapnoauto input_file output_file\n");
}


void parseOptions(int argc, char *argv[])
{
    while ((argc > 1) && (argv[1][0] == '-'))
        {
        switch (argv[1][1])
            {
            case 'h':
            case '?':
                usageHelp();
                exit(0);
                break;
            default:
                if (strcmp(infile,"") == 0)
                    {
                    infile = argv[1];
                    }
                else if (strcmp(outfile,"") == 0)
                    outfile = argv[1];
                else
                    {
                    usageHelp();
                    exit(1);
                    }
                break;
            }
	    ++argv;
	    --argc;
        }
    if (strcmp(infile,"") == 0)
        {
        if (argc <= 1)
            {
            usageHelp();
            exit(EXIT_FAILURE);
            }
        else
            {
            infile = argv[1];
            ++argv;
            --argc;
            }
        }
    if (strcmp(outfile,"") == 0)
        {
        if (argc <= 1)
            {
            usageHelp();
            exit(EXIT_FAILURE);
            }
        else
            {
            outfile = argv[1];
            }
        }
}


void unexpectedEOF ()
{
    fprintf(stderr, "Unexpected end of file.\n");
    exit(1);
}


int readHeader ()
{
    /* Read the 18 bytes of a tap header block after the two bytes of block
    length and 1 byte of block type into header[]. This includes the check byte */

    int b;
    int n = 0;

    while (!feof(in) && n < 18)
        {
        if ((b = fgetc(in)) == EOF)
            {
            unexpectedEOF();
            }
        else
            {
            header[n] = b;
            n++;
            }
        }
    return n;
}


int main (int argc, char *argv[])
{
    int f, chk, autoline;
    int l, h, c, blen, bnum, tnum = 0;
    char fname[11] = "          ";

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

    /* Read through tap file segments */

    while ((l = fgetc(in)) != EOF)
        {
        if ((h = fgetc(in)) == EOF)
            unexpectedEOF();
        blen = l + 256 * h; /* Includes block type byte and check byte */
        bnum = tnum*2; /* Current tap block number */
        tnum++; /* Curent tap segment number */
        if (blen != 19)
            {
            fprintf(stderr,"Expected a 19 byte block %d for a header block\n", bnum);
            exit(1);
            }
        /* Block type */
        if ((c = fgetc(in)) == EOF)
            unexpectedEOF();
        if (c != 0x00)
            {
            fprintf(stderr,"Block %d doesn't appear to be a header block\n", bnum);
            exit(1);
            }
        readHeader(); /* Includes check byte but not block length (2 bytes and block type (1 byte) */
        /* Grab file name */
        memcpy(fname, header+1, 10);

        if (header[0] == 0)
            {
            /* Program file, check for autorun and adjust */
            autoline = header[13] + 256 * header[14];
            if (autoline != 0)
                {
                /* Set to 32768 to turn off autorun */
                header[13] = 0x00;
                header[14] = 0x80;
                printf("Block: %d, Program: '%s', autorun line: %d\n", bnum, fname, autoline);
                }
            }
        /* write header block */

        fprintf(out, "%c%c%c", 19, 0, 0); /* Header block length is 19 (19,0), 0=header block */
        fputc(header[0], out); /* File type */
        chk = header[0]; /* Re-compute check byte in case autorun was turned off */
        for (f = 1; f < 17; f++)
            {
            fputc(header[f], out);
            chk ^= header[f];
            }
        fputc(chk, out); /* Check byte */

        /* pass-through the data block */

        /* Read block length */
        bnum++;
        if ((l = fgetc(in)) == EOF)
            unexpectedEOF();
        if ((h = fgetc(in)) == EOF)
            unexpectedEOF();
        blen = l + 256 * h; /* Includes block type byte and check byte */
        /* Block type should be a data block */
        if ((c = fgetc(in)) == EOF)
            unexpectedEOF();
        if (c != 0xFF)
            {
            fprintf(stderr, "Expected block %d to be a data block for '%s'\n", bnum, fname);
            exit(1);
            }
        
        /* Length lo/hi, 0xFF=data*/
        fprintf(out, "%c%c%c", blen & 255, blen >> 8, 0xFF);
        for (f = 0; f < blen - 1; f++)
            {
            if ((c = fgetc(in)) == EOF)
                unexpectedEOF();
            fputc(c, out);
            }
        }

    if (out != stdout)
    	fclose(out);
    if (in != stdin)
        fclose(in);

    return 0;
}
