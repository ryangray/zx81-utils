/* tapauto
 * By Ryan Gray
 * June 2024
 * 
 * This takes a .tap file and shows or changes the autorun value of a program
 * file contained within.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define VERSION "1.1"

#ifdef __MSDOS__
#define STRCMPI strcmpi
#else
#define STRCMPI strcasecmp
#endif

typedef unsigned char  BYTE;     /* Holds a one-byte value natively */
typedef unsigned short ADDR;     /* Holds a two-byte value natively (may not be little-endian) */

/* TAP file block and file types */
#define TAP_HEADER 0x00
#define TAP_BODY   0xFF
#define TAP_PROG   0x00
#define TAP_NUM    0x01
#define TAP_CHAR   0x02
#define TAP_CODE   0x03

char *infile  = "";
char *outfile = "";
unsigned char header[19];   /* For reading tap header blocks */
FILE *in, *out;
int autorun = -1;
int infoOnly = 0;
int blockNum = -1;


void printUsage()
{
    printf("tapauto %s - by Ryan Gray\n", VERSION);

    printf("Usage: tapauto [-i] [-a num] [-b num | -f num] input_file output_file\n");
    printf("       tapauto -?           Print this help.\n");
    printf("Options: -i                 Only print info about the autostart\n");
    printf("         -a line_number     Set the autostart line number (-1=none, the default)\n");
    printf("         -b block_number    Block number to modify (>=0, default=1st prog).\n");
    printf("         -f file_number     File number to modify (>=1, default=1st prog).\n");
    printf("Only one program will be modified if a block or file is specified or if autorun\n");
    printf("is being turned on. If a block or file is not specified and autorun is being\n");
    printf("turned off, then it will be turned off for all program files. File numbers start\n");
    printf("at 1, each composed of two blocks, which start at 0 with even values as the\n");
    printf("headers. The input and output files name can be given as - to use standard I/O.\n");
}


void parseOptions(int argc, char *argv[])
{
    char *aptr;

    while ((argc > 1) && (argv[1][0] == '-'))
        {
        switch (argv[1][1])
            {
            case '?':
                printUsage();
                exit(EXIT_SUCCESS);
            case 'a':
                aptr = argv[2] + strlen(argv[2]) - 1;
                autorun = (unsigned int)strtoul(argv[2], &aptr, 0);
                ++argv;
                --argc;
                break;
            case 'b':
            case 'f':
                aptr = argv[2] + strlen(argv[2]) - 1;
                blockNum = (unsigned int)strtoul(argv[2], &aptr, 0);
                if (argv[1][1] == 'f')
                    blockNum = 2 * (blockNum - 1);
                if (blockNum % 2)
                    blockNum--; /* Move odd block numbers down one to point at their header */
                ++argv;
                --argc;
                break;
            case 'i':
                infoOnly = 1;
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
                    printUsage();
                    fprintf(stderr, "unknown option: %c\n", argv[1][1]);
                    exit(EXIT_FAILURE);
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
            printUsage();
            exit(EXIT_FAILURE);
            }
        else
            {
            infile = argv[1];
            ++argv;
            --argc;
            }
        }
    if (strcmp(outfile,"") == 0 && !infoOnly)
        {
        if (argc <= 1)
            {
            printUsage();
            exit(EXIT_FAILURE);
            }
        else
            {
            outfile = argv[1];
            }
        }
}


void cleanup ()
{
    if (out && out != stdout)
    	fclose(out);
    if (in && in != stdin)
        fclose(in);
}


void errorExit ()
{
    cleanup();
    exit(EXIT_FAILURE);
}


void unexpectedEOF (int bnum)
{
    fprintf(stderr, "Unexpected end of file. Block %d\n", bnum);
    errorExit();
}


void setupInputOutput ()
{
    if ( strcmp(infile,"-") == 0 )

        in = stdin;

    else
        {
        in = fopen(infile, "rb");
        if ( in == NULL )
            {
            fprintf(stderr, "Error: couldn't open file '%s'\n", infile);
            exit(EXIT_FAILURE);
            }
        }

    if (strcmp(outfile, "-") == 0 || strcmp(outfile,"") == 0 || infoOnly)
        {
        out = stdout;
        }
    else
        {
        out = fopen(outfile, "wb");
        if (out == NULL)
            {
            fprintf(stderr, "Couldn't open output file.\n");
            if (in != stdin)
                fclose(in);
            exit(EXIT_FAILURE);
            }
        }
}


void processFile ()
{
    int f, chk, autoline, done = 0, gotProg = 0;
    int l, h, c, blen, bnum = -1, tnum = 0;
    char fname[11] = "          ";

    /* Read through tap file segments */

    while ((l = fgetc(in)) != EOF)
        {
        if ((h = fgetc(in)) == EOF)
            unexpectedEOF(bnum);
        blen = l + 256 * h; /* Includes block type byte and check byte */
        bnum = tnum*2; /* Current tap block number */
        tnum++; /* Curent tap segment number */
        if (blen != 19)
            {
            fprintf(stderr,"Expected a 19 byte block %d for a header block\n", bnum);
            errorExit(EXIT_FAILURE);
            }
        /* Block type */
        if ((c = fgetc(in)) == EOF)
            unexpectedEOF(bnum);
        if (c != 0x00)
            {
            fprintf(stderr,"Block %d doesn't appear to be a header block\n", bnum);
            errorExit(EXIT_FAILURE);
            if (c != 0xFF)
                {
                fprintf(stderr,"Block %d doesn't appear to be a body block\n", bnum);
                errorExit(EXIT_FAILURE);
                }
            }
        /* Load rest of the header */
        for (f = 0; !feof(in) && f < 18; f++)
            {
            if ((c = fgetc(in)) == EOF)
                unexpectedEOF(bnum);
            header[f] = c;
            }
        /* Grab file name */
        memcpy(fname, header+1, 10);

        if ((blockNum < 0 || blockNum == bnum) &&   /* A block we are interested in */
            (infoOnly || !done))                    /* Printing info or not already done our mod */
            {
            if (header[0] != TAP_PROG)              /* Not a program file */
                {
                if (blockNum >= 0 && blockNum == bnum)
                    {
                    fprintf(stderr, "Specified block %d is not a program block.\n", blockNum);
                    errorExit();
                    }
                }
            else                                    /* A program file */
                {
                /* Check for autorun and adjust */
                autoline = header[13] + 256 * header[14];
                printf("Blocks: %d, %d  Program: '%s'  Autorun ", bnum, bnum+1, fname);
                if (infoOnly)
                    printf("is ");
                else
                    printf("was ");

                if (autoline & 0x8000)
                    printf("disabled.\n");
                else
                    printf("line: %d\n", autoline);
                
                if (!infoOnly)
                    {
                    if (autorun == -1)
                        {
                        /* Set to 32768 to turn off autorun */
                        header[13] = 0x00;
                        header[14] = 0x80;
                        printf("Blocks: %d, %d  Program: '%s'  Autorun now disabled.\n", bnum, bnum+1, fname);
                        }
                    else
                        {
                        header[13] = autorun & 255;
                        header[14] = autorun >> 8;
                        printf("Blocks: %d, %d  Program: '%s'  Autorun now line: %d\n", bnum, bnum+1, fname, autorun);
                        }
                    }
                gotProg = 1; /* Got at least one program file */
                if ((blockNum >= 0 && blockNum == bnum) /* Only mod specific program */
                    || (autorun >= 0)) /* Set only the first if line given but no block or file specified. */
                    done = 1; /* Setting -a -1 with no block or file specified will disable autorun on all. */
                }
            }
        if (!infoOnly)
            {
            /* Write (possibly modified) header block */
            fprintf(out, "%c%c%c", 19, 0, 0); /* Header block length is 19 (19,0), 0=header block */
            fputc(header[0], out); /* File type */
            chk = header[0]; /* Re-compute check byte in case autorun was turned off */
            for (f = 1; f < 17; f++)
                {
                fputc(header[f], out);
                chk ^= header[f];
                }
            fputc(chk, out); /* Check byte */
            }

        /* Pass-through the data block */

        /* Read block length */
        bnum++;
        if ((l = fgetc(in)) == EOF)
            unexpectedEOF(bnum);
        if ((h = fgetc(in)) == EOF)
            unexpectedEOF(bnum);
        blen = l + 256 * h; /* Includes block type byte and check byte */
        /* Block type should be a data block */
        if ((c = fgetc(in)) == EOF)
            unexpectedEOF(bnum);
        if (c != 0xFF)
            {
            fprintf(stderr, "Expected block %d to be a data block for '%s'\n", bnum, fname);
            errorExit(EXIT_FAILURE);
            }
        
        /* Length lo/hi, 0xFF=data*/
        if (!infoOnly)
            fprintf(out, "%c%c%c", blen & 255, blen >> 8, 0xFF);
        for (f = 0; f < blen - 1; f++)
            {
            if ((c = fgetc(in)) == EOF)
                unexpectedEOF(bnum);
            if (!infoOnly)
                fputc(c, out);
            }
        }
    
    if (blockNum > bnum) /* If a block num was specified and not found */
        {
        fprintf(stderr, "Block %d was not found\n", blockNum);
        errorExit(EXIT_FAILURE);
        }
    if (!gotProg)
        {
        fprintf(stderr, "A program file was not found.\n");
        errorExit(EXIT_FAILURE);
        }
}


int main (int argc, char *argv[])
{
    parseOptions(argc, argv);
    setupInputOutput();
    processFile();
    cleanup();

    return 0;
}
