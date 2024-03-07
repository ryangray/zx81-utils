/* 
 * Convert a ZX81/TS1000/TS1500 program in a P file to a TS1510 cartridge ROM
 * image (up to 8K program) or two 8K images for a 16K program.
 *
 * The general scheme is to store the BASIC program at offset $0100 in the image
 * and put a loader machine code at $0000 that initializes the BASIC system, 
 * copies the program to the usual location in RAM, sets up the screen and 
 * variables, then starts the BASIC program.
 * 
 * The 8K ROM is mapped to $2000-$3FFF, which is the "shadow ROM" location, so
 * the BASIC program is stored at $2100 and needs to get copied to 16509.
 * If the program is longer than 8K ($2000-$100=7936, or bytes past 24444), it 
 * is split into two 8K images with the second 8K getting mapped to $8000, but 
 * that doesn't need a 256 byte loader if we put that loader in with the first
 * loader. We certainly need to detect that it is a two ROM cart in order to
 * copy the second block.
 * 
 * Some Technical Info provided by Paul Farrow:
 * The TS1510 ROM cartridges can be either 8K, 16K or 24K.
 * - First 8K at $2000-$3FFF
 * - 8K-16K   at $8000-$9FFF
 * - 16K-24K  at $A000-$BFFF
 * Source: http://k1.spdns.de/Vintage/Sinclair/80/Timex%20Peripherals/TS1510%20Command%20Cartridge%20Player/
 *
 * 
 * The existing carts don't load screens, but they could be loaded since the 
 * screen data is in the P file. Maybe in a future version.
 * 
 * I have modified the loader slightly from what is used in the old carts. I've
 * moved the addresses and other data from the end of the first 8K to just
 * before $2100 since there was plenty of room. I also have it built to handle
 * including variables as well as splitting either the program or the variables
 * across two ROMs. I'm only handling two ROMS right now since I don't know how
 * 24K carts work other than where EightyOne says it maps the 3rd 8K.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1.0.0"

#define romSize 8192       /* 8K buffer for making the ROMs, even if a 16K one */
#define dataOffset  0x0100 /* Where data starts in the first ROM */
#define romReserved 0x2000 /* Where reserved bytes at end of 1st ROM start */

int prog_size = 0;
int const sizeLimit = romReserved - dataOffset; /* Space in ROM A for data */
char *infile = "";
char *outfile = "";
char *outfile_malloc = NULL;
char *outname = NULL;
char *outext;
char *outroot;
int includeVars = 0; /* Some programs need this */
int autorun = 99999; /* 99999=Default from P file, <0=disable, >=0=set*/
int shortRomFile = 0;
int oneRom = 0;
int thisRomSize = 0;
int prevRomSize = 0; /* Length of ROM written so far */
int infoOnly = 0;    /* Only printing P file and block info but no ROMs */

/* We will be writing 1 or two ROM images, and later perhaps a cart image file
*/
typedef unsigned char BYTE;

/* Memory map:
 *
 * $2000-$3FFF ROM A in memory
 * $2000-$20FF Loader program
 * $2100-$3FEB Program data to load
 * $3FEB-$3FFF Addresses & lengths of program & variables blocks, autostart line
 */

/* Memory address origins of ROMs */
#define ORGA 0x2000  // ROM A
#define ORGB 0x8000  // ROM B

/* Reserved location offsets at the end of the ROM image */
/* Add ORGA to these offsets to get a memory address */
#define PCDFLAG 0xEB // Store value of CDFLAG from P file
#define VARS2L 0xEC // Length of VARS block 2
#define VARS2S 0xEE // Source address of vars block 2
#define VARS1L 0xF0 // Length of VARS block 1
#define VARS1S 0xF2 // Source address of vars block 1
#define PROG2L 0xF4 // Second program block length (zero if no 2nd program block)
#define PROG2S 0xF6 // Second program block source address (likely $8000)
#define AUTOLN 0xF8 // Program line to start: F8=, F9=
#define PROG1L 0xFA // Length of program block 1
#define AUTOAD 0xFC // Auto start line address (need to figure)
#define PROG1S 0xFE // Source address of program block 1

#define RAMTOP  16388 // 0x4004
#define PPC     16391 // 0x4007
#define SYSSAVE 16393 // 0x4009
#define D_FILE  16396 // 0x400C
#define VARS    16400 // 0x4010
#define E_LINE  16404 // 0x4014
#define CH_ADD  16406 // 0x4016
#define STKBOT  16410 // 0x401A
#define STKEND  16412 // 0x401C
#define NXTLIN  16425 // 0x4029
#define CDFLAG  16443 // 0x403B

BYTE rom[romSize];
BYTE buff[16384];

BYTE ldr[] = {
    0x01, 0x00, 0x00,       // ld bc, $0000 (So byte 0 contains 0x01)
    0xd3, 0xfd,             // out (0fdh),a
    0xf3,                   // di
    // This appears to be clearing RAM from RAMTOP-1 down to $4000, which includes
    // the system variables, so we have to save RAMTOP in de register to put back after.
    0x2a, 0x04, 0x40,       // ld hl,(RAMTOP)
    0x54,                   // ld d,h  Copy RAMTOP to de
    0x5d,                   // ld e,l
    0x2b,                   // dec hl           hl = RAMTOP-1
    0x3e, 0x3f,             // ld a,0x3f        High-byte val to check on hl
    0x36, 0x00,             // ld (hl),0x00     Store $00 there
    0x2b,                   // dec hl
    0xbc,                   // cp h             Loop until hl=$3fff (h=$3f)
    0x20, 0xfa,             // jr nz -6
    0xeb,                   // ex de,hl         Get RAMTOP back from de
    0x22, 0x04, 0x40,       // ld (04004h),hl   Set RAMTOP back after clearing sys vars area
    // Set up stack
    0x2b,                   // dec hl           hl = RAMTOP-1
    0x36, 0x3e,             // ld (hl),0x3e     Put $3e at top of BASIC RAM
    0x2b,                   // dec hl
    0xf9,                   // ld sp,hl         Point sp just below that
    0x2b,                   // dec hl
    0x2b,                   // dec hl
    0x22, 0x02, 0x40,       // ld (ERR_SP),hl   Set address of first item on machine stack
    // Other setup
    0x3e, 0x1e,             // ld a,0x1e
    0xed, 0x47,             // ld i,a
    0xed, 0x56,             // im 1
    0xfd, 0x21, 0x00, 0x40, // ld iy,0x4000     Set index to start of RAM
    0x3a, 0xeb, 0x20,		// ld a,(ORGA+PCDFLAG); Get CDFLAG value stored at end of ROM 
    0xfd, 0x77, 0x3b,       // ld (iy+03bh),a   Set CDFLAG
    0x2a, 0xfe, 0x20,       // ld hl,(ORGA+PROG1S)   Get block source from end of rom
    0x11, 0x7d, 0x40,       // ld de,0x407d     Set block destination to start of program (16509)
    0xed, 0x4b, 0xfa, 0x20, // ld bc,(ORGA+PROG1L)   Get length of block to copy
    0xed, 0xb0,             // ldir             Copy first program block
    // Check for second prog block to copy and copy it
    0xed, 0x4b, 0xf4, 0x20, // ld bc,(ORGA+PROG2L)  Load bc with length of 2nd program block
    0x78,                   // ld a,b
    0xb1,                   // or c
    0x28, 0x05,             // jr z, +5         Skip copy for bc==0
    0x2a, 0xf6, 0x20,       // ld hl,(ORGA+PROG2S)
    0xed, 0xb0,             // ldir             Copy program block 2
    0xeb,                   // ex de,hl         hl = de, which is dest byte after program (D_FILE should start there)
    // Chess inserts a dec hl here, why? 
    //0x2b,                   // dec hl
    0x22, 0x0c, 0x40,       // ld (D_FILE),hl   Set D_FILE location to be after the program
    0x06, 0x19,             // ld b,0x19        Set it up as 25 newlines
    0x3e, 0x76,             // ld a,0x76
    0x77,                   // ld (hl),a        for a collapsed display file.
    0x23,                   // inc hl
    0x10, 0xfc,             // djnz -4
    0x22, 0x10, 0x40,       // ld (VARS),hl     Point VARS to just after display file
    0xcd, 0x9a, 0x14,       // call 0x149a  Do these init vars?
    0xcd, 0xad, 0x14,       // call 0x14ad  My guess is expand DFILE, which will move 
    0xcd, 0x07, 0x02,       // call 0x0207  VARS up afterward.
    0xcd, 0x2a, 0x0a,       // call 0x0a2a  There is supposed to be a 0x80 byte after variables, then E_LINE
    0xed, 0x4b, 0xf0, 0x20, // ld bc,(ORGA+VARS1L) Get variables block 1 length
    // If bc==0, no vars block, so skip vars loading
    0x78,                   // ld a,b
    0xb1,                   // or c
    0x28, 0x25,             // jr z, +0x25       Skip vars copy for bc==0
    0x2a, 0xec, 0x20,       // ld hl,(ORGA+VARS2L) Get variables block 2 length
    0x09,                   // add hl, bc  hl=hl+bc = Total vars size
    0x44,                   // ld b,h bc = hl = total vars length
    0x4d,                   // ld c,l
    0x2a, 0x14, 0x40,       // ld hl,(E_LINE)   Get new E_LINE
    0x2b,                   // dec hl           why? Point to the $80 at end of empty vars?
    0xcd, 0x9e, 0x09,       // call 0x099e      Making room for the vars block? We will have to add VARS1L and VARS2L
    0x23,                   // inc hl           hl must point to VARS-1 after?
    0xeb,                   // ex de,hl		    de=hl to set the destination (VARS) for the vars block
    0x2a, 0xf2, 0x20,       // ld hl,(ORGA+VARS1S) Get variables block 1 source address, $2080
    0xed, 0x4b, 0xf0, 0x20, // ld bc,(ORGA+VARS1L) Get variables block 1 length
    0xed, 0xb0,             // ldir             Copy the vars block 1
    0xed, 0x4b, 0xec, 0x20, // ld bc,(ORGA+VARS2L) Get varaibles block 2 length
    // If bc==0, no vars block 2, so skip loading
    0x78,                   // ld a,b
    0xb1,                   // or c
    0x28, 0x05,             // jr z, +5         Skip vars 2 copy for bc==0
    0x2a, 0xee, 0x20,       // ld hl,(ORGA+VARS2S) Get variables block 2 source address
    0xed, 0xb0,             // ldir             Copy the vars block 2
    // All done copying, set auto start (what values do we set if no auto start?
    // zero?)
    // Why can't these use ld hl,(NN) instead? 
    0xed, 0x4b, 0xf8, 0x20, // ld bc,(ORGA+AUTOLN) Get program line to start
    0xed, 0x5b, 0xfc, 0x20, // ld de,(ORGA+AUTOAD) Get program address to start **** This needs to be NXTLIN because we
    0x62,                   // ld h,d hl=de     For call to NEXT-LINE later     **** dec de to set CH_ADD with it
    0x6b,                   // ld l,e       
    0x1b,                   // dec de           CH_ADD points one less than you would think
    0xed, 0x53, 0x16, 0x40, // ld (CH_ADD),de   Set address of next char to be interpreted
    0xed, 0x43, 0x07, 0x40, // ld (PPC),bc      Set line number of statement being executed
    // Set STKEND and FLAGS
    0xfd, 0x36, 0x22, 0x02, // ld (iy+022h),0x02  Load DF_SZ with 2 lines for lower screen
    0xfd, 0x36, 0x01, 0x80, // ld (iy+001h),080h  Load FLAGS,$80
    // Start BASIC interpreter
    0x3e, 0xff,             // ld a,0xff
    0x32, 0x7c, 0x40,       // ld (16508),a     Why are we setting the unused byte before the program to $FF?
    0xc3, 0x6c, 0x06        // jp 0x066c        This sets NXTLIN to hl
};

int ldr_size = sizeof(ldr); // Currently $00b1

void cleanup ()
{
    if (outroot)
        free(outroot);
    if (outname)
        free(outname);
    if (outfile_malloc)
        free(outfile_malloc);
}

void printUsage ()
{
    printf("p2ts1510 %s by Ryan Gray\n", VERSION);
    printf("Converts a ZX81 .P file program to a TS1510 cartridge ROM image.\n");
    printf("Usage:  p2ts1510 [options] [infile]\n");
    printf("Options are:\n");
    printf("  -v          Will cause the variables saved in the P file to be included.\n");
    printf("  -o outfile  Give the name of the output file rather than using the default.\n");
    printf("  -a line     Will set the autorun line number. Negative to disable autorun.\n");
    printf("  -s          Output short ROM files that are not padded to 8K boundaries.\n");
    printf("  -1          Output only a single ROM file.\n");
    printf("  -i          Print the P file and block info but don't output the ROMs.\n");
    printf("  -h          Print this help.\n");
    printf("The default output file name is taken from the input file name.\n");
    printf("The input can be standard input or you can give '-' as the file name.\n");
    printf("The output can be standard input or you can give '-' as the file name.\n");
    printf("Programs requiring more than one 8K ROM will have '_A', etc. added to the name.\n");
    exit(EXIT_FAILURE);
}



void parseOptions (int argc, char *argv[])
{
    char *aptr;

    while ((argc > 1) && (argv[1][0] == '-'))
        {
        switch (argv[1][1])
            {
            case '1':
                oneRom = 1;
                break;
            case 's':
                shortRomFile = 1;
                break;
            case 'a':
                aptr = argv[2] + strlen(argv[2]) - 1;
                autorun = (unsigned int)strtoul(argv[2], &aptr, 0);
                ++argv;
                --argc;
                break;
            case 'v':
                includeVars = 1;
                break;
            case 'o':
                outfile = argv[2];
                ++argv;
                --argc;
                break;
            case 'h':
                printUsage();
                exit(EXIT_SUCCESS);
            case 'i':
                infoOnly = 1;
                break;
            default:
                fprintf(stderr, "unknown option: %c\n", argv[1][1]);
                printUsage();
                exit(EXIT_FAILURE);
            }
	    ++argv;
	    --argc;
        }
    if (argc > 1)
        {
        infile = argv[argc-1];
        }
}


int findFileExtension (char *str)
{
    /* Return offset of dot of file extension, return -1 if no extension */
    int i = strlen(str) - 1;
    while (i >= 0 && str[i] != '.')
        {
        i--;
        }
    return i;
}


void writeROM(FILE *out, int last)
{
    int f;
    if (shortRomFile && last)
        {
        if (!infoOnly)
            {
            for (f = 0; f < thisRomSize; f++)
                fputc(rom[f], out);
            }
        fprintf(stderr, "  %d bytes\n", prevRomSize + thisRomSize);
        }
    else
        {
        if (!infoOnly)
            {
            for (f = 0; f < romSize; f++)
                fputc(rom[f], out);
            }
        if (last)
            fprintf(stderr, "  %d bytes\n", prevRomSize + romSize);
        }
}


int findLine (int l)
{
    /* Search rom for the offset of a given line number l */
    /* Returns -1 if line l is not found */

    int ithis = 0, thisline = 256 * buff[0] + buff[1];
    int inext = 4 + buff[2] + 256 * buff[3];

    while (l > thisline && inext < prog_size)  {
        ithis = inext;
        thisline = 256 * buff[ithis] + buff[ithis+1];
        inext = ithis + 4 + buff[ithis+2] + 256 * buff[ithis+3];
        }
    if (l <= thisline)
        return ithis;
    else
        return -1;
}


int main (int argc, char *argv[])
{
    FILE *in = NULL, *out = NULL;
    int f, b1, b2, c;
    int dfile, vars, eline;
    int dfile_size, vars_size;
    int autoaddr, autoline, ch_add, nxtlin;
    int prog1len = 0;
    int vars1len = 0;
    int prog1offs = dataOffset;
    int prog1addr = 0;
    int vars1addr = 0;
    int prog2addr = 0;
    int vars2addr = 0;
    int vars1offs = 0;
    int prog2len = 0;
    int vars2len = 0;
    int prog2offs = 0;
    char R[] = "_A";


    parseOptions(argc, argv);
 
    if ( infile[0] == '\0' || strcmp(infile,"-") == 0 )
        {
        in = stdin;
        }
    else
        {
        in = fopen(infile, "r");
        if ( in == NULL )
            {
            fprintf(stderr, "Error: couldn't open file '%s'\n", infile);
            exit(1);
            }
        }

    if (outfile[0] == '\0')
        {
        if (in == stdin)
            {
            out = stdout;
            oneRom = 1;
            }
        else
            {
            /* Make name from input */
            f = findFileExtension(infile);
            if (f < 0)
                f = strlen(infile);
            outfile_malloc = malloc(f+4+1);
            outfile = outfile_malloc;
            strncpy(outfile,infile,f);
            outext = ".rom";
            strcat(outfile,outext);
            }
        }
    else if (strcmp(outfile,"-") == 0)
        {
        out = stdout;
        oneRom = 1;
        }

    if (!out)
        {
        /* Make output name */
        b1 = strlen(outfile);
        f = findFileExtension(outfile);
        if (f < 0)
            {
            outext = ".rom";
            b2 = 4;
            }
        else
            {
            outext = outfile + f;
            b2 = b1 - f;
            b1 = f;
            }
        outroot = malloc(b1+b2+2+1); /* Allow for "_A" and "_B" additions */
        strncpy(outroot, outfile, b1);
        outname = malloc(b1+b2+2+1); /* outname is what we use for fopen */
        }

    /* Copy the loader to the ROM image */
    memset(rom, 0xFF, romSize);
    memcpy(rom, ldr, ldr_size);

    /* Load the .P file system vars to get the program size */

    /* Skip to D_FILE variable */
    for (f = 0; f < D_FILE-SYSSAVE; f++)
        fgetc(in);
    b1 = fgetc(in); b2 = fgetc(in);
    dfile = b1 + 256 * b2;
    prog_size = dfile - 16509; /* The prog is from 16509 to dfile-1 in RAM */
    fprintf(stderr, "16509: Program, %d bytes\n", prog_size);

    /* Skip to VARS variable */
    for (f+=2; f < VARS-SYSSAVE; f++)
        fgetc(in);
    b1 = fgetc(in); b2 = fgetc(in);
    vars = b1 + 256 * b2;
    dfile_size = vars - dfile;
    fprintf(stderr, "%5d: D_FILE, %d bytes\n", dfile, dfile_size);

    /* Skip to E_LINE variable */
    for (f+=2; f < E_LINE-SYSSAVE; f++)
        fgetc(in);
    b1 = fgetc(in); b2 = fgetc(in);
    eline = b1 + 256 * b2;
    vars_size = eline - vars - 1;
    fprintf(stderr, "%5d: VARS, %d bytes\n", vars, vars_size);
    fprintf(stderr, "%5d: E_LINE\n", eline);

    /* Next is CH_ADD variable */
    f += 2;
    b1 = fgetc(in); b2 = fgetc(in);
    ch_add = b1 + 256 * b2;
    fprintf(stderr, "CH_ADD = %d\n", ch_add);
    // An auto run program in a P file will have this set to NXTLIN-1
    // A non-autorun program may not have this set as it was not running when
    // saved. We may have to check for this.

    /* Skip to NXTLIN variable */
    for (f+=2; f < NXTLIN-SYSSAVE; f++)
        fgetc(in);
    b1 = fgetc(in); b2 = fgetc(in);
    nxtlin = b1 + 256 * b2;
    fprintf(stderr, "NXTLIN = %d\n", nxtlin);
    /* Use this instead of ch_add */
    autoaddr = nxtlin;
    /* Set as default unless -a option overrides */
    rom[AUTOAD]   = b1;
    rom[AUTOAD+1] = b2;

    /* Skip to CDFLAG variable */
    for (f+=2; f < CDFLAG-SYSSAVE; f++)
        fgetc(in);
    rom[PCDFLAG] = fgetc(in);
    fprintf(stderr, "CDFLAG = %02x\n", rom[PCDFLAG]);

    /* Skip the remainder of the system vars to get to the program */
    for (f+=1; f < 116; f++)
        fgetc(in);
    
    /* If prog bigger than 8K, then set end point for first ROM */

    prog1offs = dataOffset;
    prog1addr = ORGA + dataOffset;

    if (prog_size > sizeLimit)
        {
        // Program is split
        prog1len = sizeLimit;
        prog2len = prog_size - prog1len;
        prog2addr = ORGB;
        prog2offs = 0;
        // Vars are not
        if (includeVars)
            {
            vars1len = vars_size;
            vars2len = 0;
            vars1offs = prog2offs + prog2len;
            vars1addr = prog2addr + prog2len;
            }
        if (!oneRom)
            strcat(outroot, R);
        }
    else
        {
        // Program is not split
        prog1len = prog_size;
        prog2len = 0;
        prog2offs = 0;
        prog2addr = 0;
        if (includeVars)
            {
            if (vars_size > sizeLimit - prog_size)
                {
                // Vars are split
                vars1len = sizeLimit - prog_size;
                vars2len = vars_size - vars1len;
                vars1offs = prog1offs + prog_size;
                vars1addr = prog1addr + prog_size;
                vars2addr = ORGB;
                if (!oneRom)
                    strcat(outroot, R);
                }
            else
                {
                // Vars are not
                vars1len = vars_size;
                vars1addr = prog1addr + prog1len;
                vars1offs = prog1offs + prog1len;
                vars2len = 0;
                }
            }
        }
    /* Set block info at end of ROM */

    /* Length of 2nd variables block */
    b2 = vars2len / 256;
    b1 = vars2len - b2*256;
    rom[VARS2L] = b1; // 0x1EC
    rom[VARS2L+1] = b2;
    /* Address of 2nd variables block */
    b2 = vars2addr / 256;
    b1 = vars2addr - b2 * 256;
    rom[VARS2S] = b1; // 0x1EE
    rom[VARS2S+1] = b2;
    /* Length of 1st variables block */
    b2 = vars1len / 256;
    b1 = vars1len - b2 * 256;
    rom[VARS1L] = b1; // 0x1F0
    rom[VARS1L+1] = b2;
    /* Address of 1st variables block */
    b2 = vars1addr / 256;
    b1 = vars1addr - b2 * 256;
    rom[VARS1S] = b1; // 0x1F2
    rom[VARS1S+1] = b2;
    /* Length of 2nd program block */
    b2 = prog2len / 256;
    b1 = prog2len - b2*256;
    rom[PROG2L] = b1; // 0x1F4
    rom[PROG2L+1] = b2;
    /* Address of 2nd program block */
    b2 = prog2addr / 256;
    b1 = prog2addr - b2 * 256;
    rom[PROG2S] = b1; // 0x1F6
    rom[PROG2S+1] = b2;
    /* Length of 1st program block */
    b2 = prog1len / 256;
    b1 = prog1len - b2*256;
    rom[PROG1L] = b1; // 0x1FA
    rom[PROG1L+1] = b2;
    /* Address of 1st program block */
    b2 = prog1addr / 256;
    b1 = prog1addr - b2*256;
    rom[PROG1S] = b1; // 0x1FE
    rom[PROG1S+1] = b2;
    fprintf(stderr, "%5d: Program in ROM, %d bytes\n", prog1addr, prog1len);
    if (prog2len)
        fprintf(stderr, "%5d: Program segment 2 in ROM, %d bytes\n", prog2addr, prog2len);
    if (includeVars)
        {
        fprintf(stderr, "%5d: Variables in ROM, %d bytes\n", vars1addr, vars1len);
        if (vars2len)
            fprintf(stderr, "%5d: Variables segment 2 in ROM, %d bytes\n", vars2addr, vars2len);
        }
    /* Read program into buffer */
    for (f = 0; f < prog_size; f++)
        buff[f] = fgetc(in);

    // Figure out where autoaddr is in rom[] and find the line number (b1,b2)
    if (autorun < 0) /* No autorun */
        {
        // This seems to be a special value
        b1 = 254;
        b2 = 255;
        // Not sure yet if this address matters in this case, but it works
        autoaddr = vars;
        }
    else if (0 <= autorun && autorun < 99999) /* Set autorun line other than 0 */
        {
        f = findLine(autorun);
        if (f < 0)
            {
            fprintf(stderr,"Autorun line %d not found.\n", autorun);
            exit(EXIT_FAILURE);
            }
        /* f points to the autorun line or the next line */
        autoaddr = 16509 + f;
        /* Overwrite address from P file */
        /* Get actual line number bytes */
        b1 = buff[f];
        b2 = buff[f+1];
        }
    else if (autoaddr < 16508 || autoaddr >= dfile)
        {
        // The autorun address is not valid
        b1 = 254;
        b2 = 255;
        // Do we need to set autoaddr?
        }
    else
        {
        c = autoaddr - 16509;
        b1 = buff[c];    // Line #
        b2 = buff[c+1];
        }
    autoline = b1 * 256 + b2;
    rom[AUTOLN] = b1; // 0xF8
    rom[AUTOLN+1] = b2;
    if (b1 == 254 && b2 == 255)
        fprintf(stderr, "No Autorun\n");
    else
        fprintf(stderr, "Autorun line = %d\n", autoline);
    b2 = autoaddr / 256;
    b1 = autoaddr - 256 * b2;
    rom[AUTOAD]   = b1;
    rom[AUTOAD+1] = b2;
    fprintf(stderr, "Autorun addr = %d\n", autoaddr);

    if (!out)
        {
        strcpy(outname, outroot);
        strcat(outname, outext);
        if (infoOnly)
            {
            fprintf(stderr, "ROM: %s\n", outname);
            }
        else
            {
            out = fopen(outname, "w");
            if ( out == NULL )
                {
                fprintf(stderr, "Error: couldn't write output file '%s'\n", outname);
                cleanup();
                exit(EXIT_FAILURE);
                }
            fprintf(stderr, "Writing: %s\n", outname);
            }
        }

    /* Copy program block 1 to rom */
    memcpy(rom + dataOffset, buff, prog1len);
    thisRomSize = dataOffset + prog1len;

    if (prog2len > 0)
        {
        /* Prog is split, done with first 8K ROM */
        writeROM(out, !oneRom);
        if (oneRom)
            {
            prevRomSize += romSize;
            }
        else
            {
            /* ROM B */
            prevRomSize = 0;
            c = strlen(outroot);
            outroot[c-1]++; // A->B
            strcpy(outname, outroot);
            strcat(outname, outext);
            if (infoOnly)
                {
                fprintf(stderr, "ROM: %s\n", outname);
                }
            else
                {
                fclose(out);
                out = fopen(outname,"w");
                if ( out == NULL )
                    {
                    fprintf(stderr, "Error: couldn't write output file '%s'\n", outname);
                    cleanup();
                    exit(EXIT_FAILURE);
                    }
                fprintf(stderr, "Writing: %s\n", outname);
                }
            }
        /* Write prog 2 block to rom */
        memset(rom, 0xFF, romSize);
        memcpy(rom, buff + prog1len, prog2len);
        thisRomSize = prog2len;
        }
    
    if (includeVars)
        {
        /* Skip display file in P file to get to vars */
        for (f = 0; f < dfile_size; f++)
            fgetc(in);
        /* Read vars into buffer */
        for (f = 0; f < vars_size; f++)
            buff[f] = fgetc(in);
        /* Copy first block to rom */
        memcpy(rom + vars1offs, buff, vars1len);
        thisRomSize = vars1offs + vars1len;
        if (vars2len > 0)
            {
            /* Vars is split, done with first 8K ROM */
            writeROM(out, !oneRom);
            if (oneRom)
                {
                prevRomSize += romSize;
                }
            else
                {
                /* ROM B */
                prevRomSize = 0;
                c = strlen(outroot);
                outroot[c-1]++; // A->B
                strcpy(outname, outroot);
                strcat(outname, outext);
                if (infoOnly)
                    {
                    fprintf(stderr, "ROM: %s\n", outname);
                    }
                else
                    {
                    fclose(out);
                    out = fopen(outname,"w");
                    if ( out == NULL )
                        {
                        fprintf(stderr, "Error: couldn't write output file '%s'\n", outname);
                        cleanup();
                        exit(EXIT_FAILURE);
                        }
                    fprintf(stderr, "Writing: %s\n", outname);
                    }
                }
            /* Write vars 2 block to rom */
            memset(rom, 0xFF, romSize);
            memcpy(rom, buff + vars1len, vars2len);
            thisRomSize = vars2len;
            }
        }
    /* All done */
    writeROM(out, 1);
    if (!infoOnly)
        fclose(out);
    fclose(in);
    cleanup();
    exit(EXIT_SUCCESS);

}