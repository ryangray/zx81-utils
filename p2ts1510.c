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

#define VERSION "1.0.4"

#define ROM8K 8192      /* 8K buffer size for making the ROM images */
#define BUFFSZ 16384    /* Buffer size for P file */
#define SAVE_TOKEN 0xF8

/* Define byte and 16-bit address to avoid problems with int on DOS builds */
typedef unsigned char       BYTE;   /* For bytes, unsigned 8-bit */
typedef unsigned short int  ADDR;   /* For addresses, unsigned 16-bit */
typedef short int           ROMP;   /* For a rom[] or buff[] pointer or offset, signed 16-bit */

ADDR prog_size = 0;
ADDR pfile_size = 0;
char *infile = "";
char *outfile = "";
char *outfile_malloc = NULL;
char *outname = NULL;
char *outext;
char *outroot;
int includeVars = 1; /* Some programs need vars, so we default to include */
int autorun = 10000; /* 10000=Use the setting in the P file, <0=disable, >=0=set*/
int shortRomFile = 0;
int oneRom = 1;
int infoOnly = 0;    /* Only printing P file and block info but no ROMs */
int tapeLikeLoader = 0; /* Load every byte of the P file like loading from tape */
ADDR thisRomSize = 0;
ADDR prevRomSize = 0; /* Length of ROM written so far */

/* Memory map:
 *
 * $2000-$3FFF ROM A in memory
 * $2000-$20FF Loader program
 * $2100-$3FEB Program data to load
 * $3FEB-$3FFF Addresses & lengths of program & variables blocks, autostart line
 */

/* Memory address origins of ROMs */
#define ORGA 0x2000 /* ROM A */
#define ORGB 0x8000 /* ROM B */

/* Reserved location offsets after the end of the ROM loader code */
/* Add ORGA to these offsets to get a memory address */
/* Add loaderSize to get a rom[] offset */
/* NOTE: The original loaders had these at the end of the 1st 8K ROM and in a different order */

#define PROG1S 0x00 /* Source address of program block 1 */
#define PROG1L 0x02 /* Length of program block 1 */
#define PROG2S 0x04 /* Second program block source address (likely $8000) */
#define PROG2L 0x06 /* Second program block length (zero if no 2nd program block) */
#define VARS1S 0x08 /* Source address of vars block 1 */
#define VARS1L 0x0A /* Length of VARS block 1 */
#define VARS2S 0x0C /* Source address of vars block 2 */
#define VARS2L 0x0E /* Length of VARS block 2 */
#define AUTOAD 0x10 /* Auto start line address (need to figure) */
#define AUTOLN 0x12 /* Program line to start */
#define PCDFLAG 0x14 /* Store value of CDFLAG from P file */

/* Some of the system variable addresses */
#define RAMTOP  16388 /* 0x4004 */
#define PPC     16391 /* 0x4007 */
/* ^--Not SAVEd,  v---SAVEd in P file */
#define SYSSAVE 16393 /* 0x4009 */
#define VERSN   16393 /* 0x4009 */
#define D_FILE  16396 /* 0x400C */
#define VARS    16400 /* 0x4010 */
#define E_LINE  16404 /* 0x4014 */
#define CH_ADD  16406 /* 0x4016 */
#define STKBOT  16410 /* 0x401A */
#define STKEND  16412 /* 0x401C */
#define NXTLIN  16425 /* 0x4029 */
#define OLDPPC  16427 /* 0x402B */
#define CDFLAG  16443 /* 0x403B */

#define PROGRAM 16509 /* 0x407D */

BYTE rom[ROM8K];    /* Holds each 8K ROM image being built */
BYTE buff[BUFFSZ];  /* Holds the contents of the P file */
/* Pointers to the sections of the P file */
BYTE *prg;
BYTE *var;

BYTE ldr1[] = {
    0x01, 0x00, 0x00,       /* ld bc, $0000 (So byte 0 contains 0x01) */
    0xd3, 0xfd,             /* out (0fdh),a */
    0xf3,                   /* di */
    /* This appears to be clearing RAM from RAMTOP-1 down to $4000, which includes */
    /* the system variables, so we have to save RAMTOP in de register to put back after. */
    0x2a, 0x04, 0x40,       /* ld hl,(RAMTOP) */
    0x54,                   /* ld d,h  Copy RAMTOP to de */
    0x5d,                   /* ld e,l */
    0x2b,                   /* dec hl           hl = RAMTOP-1 */
    0x3e, 0x3f,             /* ld a,0x3f        High-byte val to check on hl */
    0x36, 0x00,             /* ld (hl),0x00     Store $00 there */
    0x2b,                   /* dec hl */
    0xbc,                   /* cp h             Loop until hl=$3fff (h=$3f) */
    0x20, 0xfa,             /* jr nz -6 */
    0xeb,                   /* ex de,hl         Get RAMTOP back from de */
    0x22, 0x04, 0x40,       /* ld (04004h),hl   Set RAMTOP back after clearing sys vars area */
    /* Set up stack */
    0x2b,                   /* dec hl           hl = RAMTOP-1 */
    0x36, 0x3e,             /* ld (hl),0x3e     Put $3e at top of BASIC RAM */
    0x2b,                   /* dec hl */
    0xf9,                   /* ld sp,hl         Point sp just below that */
    0x2b,                   /* dec hl */
    0x2b,                   /* dec hl */
    0x22, 0x02, 0x40,       /* ld (ERR_SP),hl   Set address of first item on machine stack */
    /* Other setup */
    0x3e, 0x1e,             /* ld a,0x1e */
    0xed, 0x47,             /* ld i,a */
    0xed, 0x56,             /* im 1 */
    0xfd, 0x21, 0x00, 0x40, /* ld iy,0x4000     Set index to start of RAM */
    0x3a, 0xce, 0x20,		/* ld a,(PCDFLAG)   Get CDFLAG value stored at end of ROM  */
    0xfd, 0x77, 0x3b,       /* ld (iy+03bh),a   Set CDFLAG */
    0xed, 0x4b, 0xbc, 0x20, /* ld bc,(PROG1L)   Get length of block to copy */
    0x78,                   /* ld a,b */
    0xb1,                   /* or c */
    0x28, 0x15,             /* jr z, +0x15      Skip prog block copy for bc==0 (probably vars only) */
    0x2a, 0xba, 0x20,       /* ld hl,(PROG1S)   Get block source from end of rom */
    0x11, 0x7d, 0x40,       /* ld de,0x407d     Set block destination to start of program (16509) */
    0xed, 0xb0,             /* ldir             Copy first program block */
    /* Check for second prog block to copy and copy it */
    0xed, 0x4b, 0xc0, 0x20, /* ld bc,(PROG2L)   Load bc with length of 2nd program block */
    0x78,                   /* ld a,b */
    0xb1,                   /* or c */
    0x28, 0x05,             /* jr z, +5         Skip copy for bc==0 */
    0x2a, 0xbe, 0x20,       /* ld hl,(PROG2S)   Get source address of 2nd program block */
    0xed, 0xb0,             /* ldir             Copy program block 2 */
    0xeb,                   /* ex de,hl         hl = de, which is dest byte after program (D_FILE should start there) */
    0x22, 0x0c, 0x40,       /* ld (D_FILE),hl   Set D_FILE location to be after the program */
    0x06, 0x19,             /* ld b,0x19        Set it up as 25 newlines */
    0x3e, 0x76,             /* ld a,0x76 */
    0x77,                   /* ld (hl),a        for a collapsed display file. */
    0x23,                   /* inc hl */
    0x10, 0xfc,             /* djnz -4 */
    0x22, 0x10, 0x40,       /* ld (VARS),hl     Point VARS to just after display file */
    0xcd, 0x9a, 0x14,       /* call 0x149a  CLEAR: clears the variable area (sets hl and E_LINE) */
    0xcd, 0xad, 0x14,       /* call 0x14ad  CURSOR-IN: sets up lower screen to 2 lines and clear calc stack (uses hl) */
    0xcd, 0x07, 0x02,       /* call 0x0207  SLOW/FAST: test CDFLAG bit 6 to set mode (uses hl, a, b) */
    0xcd, 0x2a, 0x0a,       /* call 0x0a2a  CLS: will expand a collapsed display file if enough RAM (uses bc, a, hl, de) */
    0xed, 0x4b, 0xc4, 0x20, /* ld bc,(VARS1L)   Get variables block 1 length */
    /* If bc==0, no vars block, so skip vars loading */
    0x78,                   /* ld a,b */
    0xb1,                   /* or c */
    0x28, 0x25,             /* jr z, +0x21      Skip vars copy for bc==0 */
    0x2a, 0xc8, 0x20,       /* ld hl,(VARS2L)   Get variables block 2 length */
    0x09,                   /* add hl, bc  hl=hl+bc = Total vars size */
    0x44,                   /* ld b,h bc = hl = total vars length */
    0x4d,                   /* ld c,l */
    0x2a, 0x14, 0x40,       /* ld hl,(E_LINE)   Get new E_LINE */
    0x2b,                   /* dec hl           why? Point to the $80 at end of empty vars? */
    0xcd, 0x9e, 0x09,       /* call 0x099e      Making room for the vars block? We will have to add VARS1L and VARS2L */
    0x23,                   /* inc hl           hl must point to VARS-1 after? */
    0xeb,                   /* ex de,hl		    de=hl to set the destination (VARS) for the vars block */
    0x2a, 0xc2, 0x20,       /* ld hl,(VARS1S)   Get variables block 1 source address */
    0xed, 0x4b, 0xc4, 0x20, /* ld bc,(VARS1L)   Get variables block 1 length */
    0xed, 0xb0,             /* ldir             Copy the vars block 1 */
    0xed, 0x4b, 0xc8, 0x20, /* ld bc,(VARS2L)   Get varaibles block 2 length */
    0x78,                   /* ld a,b */
    0xb1,                   /* or c */
    0x28, 0x05,             /* jr z, +5         Skip vars 2 copy for bc==0 */
    0x2a, 0xc6, 0x20,       /* ld hl,(VARS2S)   Get variables block 2 source address */
    0xed, 0xb0,             /* ldir             Copy the vars block 2 */
    /* All done copying, set auto start */
    0xed, 0x4b, 0xcc, 0x20, /* ld bc,(AUTOLN)   Get program line to start */
    0xed, 0x5b, 0xca, 0x20, /* ld de,(AUTOAD)   Get program address to start **** This needs to be NXTLIN because */
    0x62,                   /* ld h,d hl=de     For call to NEXT-LINE later     **** we dec de to set CH_ADD with it */
    0x6b,                   /* ld l,e */
    0x1b,                   /* dec de           CH_ADD points one less than you would think */
    0xed, 0x53, 0x16, 0x40, /* ld (CH_ADD),de   Set address of next char to be interpreted */
    0xed, 0x43, 0x07, 0x40, /* ld (PPC),bc      Set line number of statement being executed */
    /* Set STKEND and FLAGS */
    0xfd, 0x36, 0x22, 0x02, /* ld (iy+022h),0x02  Load DF_SZ with 2 lines for lower screen */
    0xfd, 0x36, 0x01, 0x80, /* ld (iy+001h),080h  Load FLAGS,$80 */
    /* Start BASIC interpreter */
    0x3e, 0xff,             /* ld a,0xff */
    0x32, 0x7c, 0x40,       /* ld (16508),a     Why are we setting the unused byte before the program to $FF? */
    0xc3, 0x6c, 0x06        /* jp 0x066c        This sets NXTLIN to hl */
};

ADDR ldr1_size = sizeof(ldr1); /* Currently $00b6 */

BYTE ldrp[] = {
    0x01, 0x00, 0x00,       /* ld bc, $0000 (So byte 0 contains 0x01) */
    0xd3, 0xfd,             /* out (0fdh),a */
    0xf3,                   /* di */
    /* Other setup */
    0x3e, 0x1e,             /* ld a,0x1e */
    0xed, 0x47,             /* ld i,a */
    0xed, 0x56,             /* im 1 */
    0x2a, 0x31, 0x20,       /* ld hl,(LDRVAR+PROG1S)   Get block source from rom */
    0x11, 0x09, 0x40,       /* ld de,0x4009     Set block destination to start of saved system variables (16393) */
    0xed, 0x4b, 0x33, 0x20, /* ld bc,(LDRVAR+PROG1L)   Get length of block to copy */
    0xed, 0xb0,             /* ldir             Copy first program block */
    /* Check for second prog block to copy and copy it */
    0xed, 0x4b, 0x37, 0x20, /* ld bc,(LDRVAR+PROG2L)  Load bc with length of 2nd program block */
    0x78,                   /* ld a,b */
    0xb1,                   /* or c */
    0x28, 0x05,             /* jr z, +5         Skip for bc==0 */
    0x2a, 0x35, 0x20,       /* ld hl,(LDRVAR+PROG2S) */
    0xed, 0xb0,             /* ldir             Copy program block 2 */
    0xcd, 0xad, 0x14,       /* call 0x14ad  CURSOR-IN: sets up lower screen to 2 lines and clear calc stack (uses hl) */
    0xcd, 0x07, 0x02,       /* call 0x0207  SLOW/FAST: test CDFLAG bit 6 to set mode (uses hl, a, b) */
    /* Start BASIC interpreter */
    0x2a, 0x29, 0x40,       /* ld hl,(NXTLIN)   Address of next line to interpret */
    0xc3, 0x6c, 0x06        /* jp 0x066c        This sets NXTLIN to hl */
};

ADDR ldrp_size = sizeof(ldrp); /* Currently 0x36 */

BYTE *ldr;       /* Point to selected loader source */
ADDR loaderSize; /* Size of the loader selected */
ADDR dataOffset; /* Where data starts in the first ROM  */
ADDR sizeLimit;  /* Remaining space in ROM A for data */

/* Character map to print program code lines */

#define NAK "#"
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

char *tokens[256]=
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
    printf("  -v          Will cause the variables saved in the P file NOT to be included.\n");
    printf("  -o outfile  Give the name of the output file rather than using the default.\n");
    printf("  -a line     Will set the autorun line number. Negative to disable autorun.\n");
    printf("  -s          Output short ROM files that are not padded to 8K boundaries.\n");
    printf("  -1          Output only a single ROM file (the default).\n");
    printf("  -2          Output separate (two or more) ROM files.\n");
    printf("  -i          Print the P file and block info but don't output the ROMs.\n");
    printf("  -t          Use tape-like loader: sys vars, program, display, and variables.\n");
    printf("  -?          Print this help.\n");
    printf("The default output file name is taken from the input file name.\n");
    printf("The input can be standard input or you can give '-' as the file name.\n");
    printf("The output can be standard input or you can give '-' as the file name.\n");
    printf("Programs requiring more than one 8K ROM will have '_A', etc. added to the name.\n");
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
            case '2':
                oneRom = 0;
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
                includeVars = 0;
                break;
            case 'o':
                outfile = argv[2];
                ++argv;
                --argc;
                break;
            case 'i':
                infoOnly = 1;
                break;
            case 't':
                tapeLikeLoader = 1;
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


void writeROM(FILE *out, int endRom)
{
    ROMP f, len;

    if (shortRomFile)
        len = thisRomSize;
    else
        len = ROM8K;
    if (endRom)
        {
        fprintf(stderr, "ROM : %s\nSize: %d bytes", outname, prevRomSize + len);
        if (infoOnly)
            fprintf(stderr, " (not written)\n");
        else
            fprintf(stderr,"\n");
        }
    if (!infoOnly)
        {
        for (f = 0; f < len; f++)
            fputc(rom[f], out);
        }
}


ROMP findLine (ROMP line)
{
    /* Search rom for the offset of a given line number l or the next line after */
    /* Returns -1 if line is greater than the last line */

    ROMP ithis = PROGRAM - SYSSAVE;                       /* First line index  */
    ROMP thisline = 256 * buff[ithis] + buff[ithis+1];    /* First line number */
    ROMP inext = buff[ithis+2] + 256 * buff[ithis+3] + 4; /* Next line index   */

    while (line > thisline && inext < prog_size)  {
        ithis = inext;
        thisline = 256 * buff[ithis] + buff[ithis+1];
        inext = ithis + 4 + buff[ithis+2] + 256 * buff[ithis+3];
        }
    if (line <= thisline)
        return ithis;
    else
        return -1;
}


BYTE peek (ADDR addr)
{
    ADDR i = addr - SYSSAVE;
    if (i < 0 || i > pfile_size)
        return 0;
    else
        return buff[i];
}

void poke (ADDR addr, BYTE value)
{
    ADDR i = addr - SYSSAVE;
    if (i >= 0 && i < pfile_size)
        buff[i] = value;
}

ADDR dpeek (ADDR addr)
{
    ADDR i = addr - SYSSAVE;
    if (i < 0 || i > pfile_size)
        return 0;
    else
        return buff[i] + 256 * buff[i+1];
}

void dpoke (ADDR addr, ADDR value)
{
    ADDR i = addr - SYSSAVE;
    BYTE h = value / 256;
    if (i < 0 || i > pfile_size)
        return;
    buff[i]     = value - h * 256;
    buff[i + 1] = h;
}

void romStoreAddr (ROMP i, ADDR addr)
{
    BYTE h = addr / 256;
    rom[i+loaderSize] = addr - h * 256;
    rom[i+loaderSize+1] = h;
}


void printLine (FILE* f, ADDR lineAddr)
{
    ROMP x;
    ROMP len;
    ROMP end;
    BYTE c;

    if (lineAddr < SYSSAVE || lineAddr > SYSSAVE + pfile_size)
        {
        /* Address is outside the P file */
        return;
        }
    x = lineAddr - SYSSAVE;
    len = buff[x+2] + 256 * buff[x+3];
    len = len < 256 ? len : 256; /* Limit length */
    end = x + 4 + len;
    fprintf(f, "Autorun line code:\n");
    fprintf(f, "  %4d", 256 * buff[x] + buff[x+1]); /* Line number */
    for (x += 4; x < end; x++)
        {
        c = buff[x];
        if (c == 0x7E) /* Number */
            {
            x += 5;
            }
        else if (c == 0x76) /* Newline */
            fprintf(f,"\n");
        else
            {
            fprintf(f,"%s", tokens[c]);
            }
        }
    if (c != 0x76)
        fprintf(f, "\n");
}

int main (int argc, char *argv[])
{
    FILE *in = NULL, *out = NULL;
    int f, b1, b2, c;
    BYTE cdflag;
    ADDR dfile, dfile_size;
    ADDR vars, vars_size;
    ADDR eline, ch_add, nxtlin;
    ADDR autoaddr, autoline;
    ADDR prog1len = 0;
    ADDR vars1len = 0;
    ADDR prog1offs = 0;
    ADDR prog1addr = 0;
    ADDR vars1addr = 0;
    ADDR prog2addr = 0;
    ADDR vars2addr = 0;
    ADDR vars1offs = 0;
    ADDR prog2len = 0;
    ADDR vars2len = 0;
    ADDR prog2offs = 0;
    char R[] = "_A";
    int autorun_warn = 0;
    int autorun_check = 0;

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
            exit(EXIT_FAILURE);
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

    if (tapeLikeLoader)
        {
        ldr = ldrp;
        loaderSize = ldrp_size;
        dataOffset = ldrp_size + 0x08;
        }
    else
        {
        ldr = ldr1;
        loaderSize = ldr1_size;
        dataOffset = ldr1_size + 0x15;
        }

    sizeLimit = ROM8K - dataOffset; /* Space in ROM A for data */


    /* Copy the loader to the ROM image */
    memset(rom, 0xFF, ROM8K);
    memcpy(rom, ldr, loaderSize);

    /* Load the .P file */

    for (f = 0; !feof(in) && f < BUFFSZ; f++)
        {
        c = fgetc(in);
        if (c == EOF) break;
        buff[f] = c;
        }
    pfile_size = f; /* Actual size, but can contain extra bytes beyond vars */

    if (buff[0] != 0)
        {
        fprintf(stderr,"Doesn't appear to be a valid ZX81 P file.\n");
        cleanup();
        exit(EXIT_FAILURE);
        }

    /* Get the program size */

    dfile = dpeek(D_FILE); /* Get value of D_FILE from system variables */
    prog_size = dfile - PROGRAM; /* The prog is from 16509 to dfile-1 in RAM */
    fprintf(stderr, "16509: Program, %d bytes\n", prog_size);

    vars = dpeek(VARS);

    /* Display file starts with a newline 0x76 and then has 24 newlines after 
    that for a collapsed file or 24 full lines of 32 spaces plus a newline for
    1+24*33 = 793 bytes. */
    dfile_size = vars - dfile;
    fprintf(stderr, "%5d: D_FILE, %d bytes", dfile, dfile_size);
    if (dfile_size < 33*24+1)
        fprintf(stderr," (collapsed)\n");
    else
        fprintf(stderr," (full)\n");

    eline = dpeek(E_LINE);
    vars_size = eline - vars - 1; /* Don't count the 0x80 byte at E_LINE-1 */
    fprintf(stderr, "%5d: VARS, %d bytes\n", vars, vars_size);
    fprintf(stderr, "%5d: E_LINE\n", eline);

    /* Set pointers into buffer for program and vars blocks */
    var = buff + vars - SYSSAVE;
    if (tapeLikeLoader)
        prg = buff; /* whole thing is the program block */
    else /* segment loader */
        prg = buff + PROGRAM - SYSSAVE;

    /* pfile_size = 116 + prog_size + dfile_size + vars_size + 1;
       There is supposed to be a 0x80 byte after variables, then E_LINE
       The P file may include bytes after the 0x80
    */
    pfile_size = eline - SYSSAVE; /* Used size can be smaller than file size */
    if (pfile_size > BUFFSZ)
        {
        /* We didn't get all of the used portion into the buffer */
        fprintf(stderr, "Error: P file size (%d) is too large for buffer (%d)\n", pfile_size, BUFFSZ);
        cleanup();
        exit(EXIT_FAILURE);
        }

    /* An auto run program in a P file should have CH_ADD set to NXTLIN-1.
     * A non-autorun program may not have this set as it was not running when
     * saved.
     */
    ch_add = dpeek(CH_ADD);
    fprintf(stderr, "CH_ADD = %d\n", ch_add);

    /* NXTLIN has the autostart address, which is the first byte of the line */
    nxtlin = dpeek(NXTLIN);
    fprintf(stderr, "NXTLIN = %d\n", nxtlin);
    /* Set as default autorun address unless -a option overrides */
    autoaddr = nxtlin;

    /* CDFLAG contains the SLOW/FAST flag bit */
    cdflag = peek(CDFLAG);
    fprintf(stderr, "CDFLAG = 0x%02x, ", cdflag);
    if (cdflag & 0x40)
        fprintf(stderr,"SLOW mode\n");
    else
        fprintf(stderr,"FAST mode\n");

    /* Work out program and variable blocks for storing in ROM */

    prog1offs = dataOffset;
    prog1addr = ORGA + dataOffset;

    if (tapeLikeLoader)
        {
        /* Put whole P file in ROM and the loader loads it all. 
         * The whole thing will be in prog1 and (possibly) prog2 blocks.
         */
        includeVars = 0; /* Everything is included in the program block(s) */
        if (pfile_size > sizeLimit)
            {
            /* P file is split across ROMs */
            prog1len = sizeLimit;
            prog2len  = pfile_size - prog1len;
            prog2addr = ORGB; /* 2nd part starts on ROM B */
            prog2offs = 0;
            if (prog2len > ROM8K)
                {
                fprintf(stderr, "Error: P file size is larger than two 8K ROMs.\n");
                cleanup();
                exit(EXIT_FAILURE);
                }
            if (!oneRom)
                strcat(outroot, R);
            }
        else /* Fits in the one ROM */
            {
            prog1len  = pfile_size;
            prog2len  = 0;
            }
        }
    else if (prog_size > sizeLimit)
        {
        /* Program is split */
        prog1len  = sizeLimit;
        prog2len  = prog_size - prog1len;
        prog2addr = ORGB; /* 2nd part starts on ROM B */
        prog2offs = 0;
        /* Vars are not split but are on ROM B */
        if (includeVars)
            {
            if (prog2len + vars_size > ROM8K)
                {
                fprintf(stderr, "Error: Program + variables size is larger than two 8K ROMs.\n");
                cleanup();
                exit(EXIT_FAILURE);
                }
            vars1len  = vars_size;
            vars2len  = 0;
            vars1offs = prog2offs + prog2len;
            vars1addr = prog2addr + prog2len;
            }
        else if (prog2len > ROM8K)
            {
            fprintf(stderr, "Error: Program size is larger than two 8K ROMs.\n");
            cleanup();
            exit(EXIT_FAILURE);
            }
        if (!oneRom)
            strcat(outroot, R);
        }
    else
        {
        /* Program is not split */
        prog1len  = prog_size;
        prog2len  = 0;
        prog2offs = 0;
        prog2addr = 0;
        if (includeVars)
            {
            if (vars_size > sizeLimit - prog_size)
                {
                /* Vars are split */
                vars1len  = sizeLimit - prog_size;
                vars2len  = vars_size - vars1len;
                vars1offs = prog1offs + prog_size;
                vars1addr = prog1addr + prog_size;
                vars2addr = ORGB;
                if (vars2len > ROM8K)
                    {
                    fprintf(stderr, "Error: Program + variables size is larger than two 8K ROMs.\n");
                    cleanup();
                    exit(EXIT_FAILURE);
                    }
                if (!oneRom)
                    strcat(outroot, R);
                }
            else
                {
                /* Vars are not split */
                vars1len  = vars_size;
                vars2len  = 0;
                vars1addr = prog1addr + prog1len;
                vars1offs = prog1offs + prog1len;
                }
            }
        }

    /* Set block info in ROM */

    if (tapeLikeLoader)
        {
        fprintf(stderr," 8192: Tape-like loader in ROM, %d bytes\n", dataOffset);
        }
    else
        {
        rom[loaderSize + PCDFLAG] = cdflag;
        romStoreAddr(VARS2L, vars2len);     /* Length  of 2nd variables block */
        romStoreAddr(VARS2S, vars2addr);    /* Address of 2nd variables block */
        romStoreAddr(VARS1L, vars1len);     /* Length  of 1st variables block */
        romStoreAddr(VARS1S, vars1addr);    /* Address of 1st variables block */
        fprintf(stderr," 8192: Segment loader in ROM, %d bytes\n", dataOffset);
        }
    romStoreAddr(PROG2L, prog2len);     /* Length  of 2nd program block */
    romStoreAddr(PROG2S, prog2addr);    /* Address of 2nd program block */
    romStoreAddr(PROG1L, prog1len);     /* Length  of 1st program block */
    romStoreAddr(PROG1S, prog1addr);    /* Address of 1st program block */

    if (prog2len)
        {
        fprintf(stderr, "%5d: Program segment 1 in ROM, %d bytes\n", prog1addr, prog1len);
        fprintf(stderr, "%5d: Program segment 2 in ROM, %d bytes\n", prog2addr, prog2len);
        }
    else
        fprintf(stderr, "%5d: Program in ROM, %d bytes\n", prog1addr, prog1len);

    if (includeVars)
        {
        if (vars2len)
            {
            fprintf(stderr, "%5d: Variables segment 1 in ROM, %d bytes\n", vars1addr, vars1len);
            fprintf(stderr, "%5d: Variables segment 2 in ROM, %d bytes\n", vars2addr, vars2len);
            }
        else
            fprintf(stderr, "%5d: Variables in ROM, %d bytes\n", vars1addr, vars1len);
        }

    /* Sort out the autorun address and line number */

    if (autorun < 0) /* No autorun forced by '-a -1' option */
        {
        /* This seems to be the special value for no autorun */
        b1 = 254;
        b2 = 255;
        autoaddr = dfile;
        autoline = -1;
        if (tapeLikeLoader) /* Change the system variables */
            {
            dpoke(NXTLIN, autoaddr);
            dpoke(CH_ADD, autoaddr-1);
            }
        }
    else if (0 <= autorun && autorun < 10000) /* Autorun line set by '-a' option */
        {
        /* f points to the autorun line or the next line */
        f = findLine(autorun);
        /* Override address from P file */
        autoaddr = SYSSAVE + f;
        /* Get actual line number bytes */
        b1 = buff[f];
        b2 = buff[f+1];
        autoline = b1 * 256 + b2;
        if (autoline != autorun)
            {
            fprintf(stderr,"Autorun line %d not found, using the next line: %d\n", autorun, autoline);
            }
        else
            {
            fprintf(stderr,"Autorun line %d found at: %d\n", autorun, autoaddr);
            }
        autorun_check = 1;
        if (tapeLikeLoader) /* Override system variables */
            {
            dpoke(NXTLIN, autoaddr);
            dpoke(CH_ADD, autoaddr-1);
            }
        }
    else if (autoaddr == dfile)
        {
        /* No autorun set in P file */
        b1 = 254; /* This seems to be a special value for no autorun */
        b2 = 255;
        autoline = -1;
        }
    else if (autoaddr < SYSSAVE)
        {
        fprintf(stderr,"Warning: Autorun address (%d) is in the ROM area, autorun may fail.\n", autoaddr);
        /* We don't have the bytes to load for the line number to later load
        into PPC, so we set them to 0 */
        b1 = 0;
        b2 = 0;
        autorun_warn = 1;
        autoline = -1;
        }
    else if (autoaddr < PROGRAM)
        {
        fprintf(stderr,"Warning: Autorun address (%d) is in the system area, autorun may fail.\n", autoaddr);
        if (!tapeLikeLoader)
            {
            fprintf(stderr,"Warning: You will need to use the -t option to include the system variables.\n");
            }
        b1 = peek(autoaddr);
        b2 = peek(autoaddr + 1);
        autoline = b1 * 256 + b2;
        f = autoaddr - SYSSAVE;
        autorun_warn = 1;
        autorun_check = 1;
        }
    else if (autoaddr > dfile) /* The autorun address is not normally valid */
        {
        /* But some things put one in the variables area */
        fprintf(stderr,"Warning: Autorun address (%d) is not within the BASIC program,\n         autorun may fail.\n", autoaddr);
        autorun_warn = 1;
        autorun_check = 1;
        f = autoaddr - SYSSAVE;
        /* Get actual line number bytes */
        b1 = buff[f];
        b2 = buff[f+1];
        autoline = b1 * 256 + b2;
        if (f < pfile_size && !tapeLikeLoader && !includeVars)
            fprintf(stderr,"Warning: You will need to use the -t option or not use -v to include variables.\n");
        }
    else /* Use valid autorun from P file */
        {
        f = autoaddr - SYSSAVE;
        /* Get actual line number bytes */
        b1 = peek(autoaddr);
        b2 = buff[f+1];
        autoline = b1 * 256 + b2;
        autorun_check = 1;
        }

    /* At this point, autorun==line user requested or one from the P file, and
    autoline==the actual line number or -1 for no autorun or no line number */

    if (autorun_check)
        {
        /* Do some sanity checks as some P files have issues */
        if (autoline > 9999)
            {
            fprintf(stderr,"Warning: autorun line number %d is out of range.\n", autoline);
            autorun_warn = 1;
            }
        if (f + SYSSAVE < PROGRAM || f + SYSSAVE > PROGRAM + prog_size)
            {
            /* Outside the BASIC program */
            }
        else if (f == PROGRAM)
            {
            fprintf(stderr,"Warning: autorun line is the first line. This is usually not possible.\n");
            autorun_warn = 1;
            }
        else if (buff[f-1] != 0x76)
            {
            fprintf(stderr,"Warning: Autorun address may be bad or P file corrupt.\n");
            fprintf(stderr,"         Byte before this address should be a newline.\n");
            autorun_warn = 1;
            }
        else
            {
            /* Check if the previous line is a SAVE command */
            c = f - 2;
            while (c >= 0 && buff[c] != 0x76)
                c--;
            c += 5;
            if (buff[c] != SAVE_TOKEN)
                {
                fprintf(stderr,"Warning: Line before autorun line is not a SAVE command.\n");
                autorun_warn = 1;
                }
            else
                {
                /* Look at end of the SAVE command for an inverted character */
                if (buff[f-2] == 11 && buff[f-3] < 128)
                    {
                    fprintf(stderr,"Warning: Filename of SAVE before autorun line doesn't end with an inverse char.\n");
                    autorun_warn = 1;
                    }
                }
            }
        }

    if (!tapeLikeLoader)
        {
        rom[loaderSize+AUTOLN]   = b1;
        rom[loaderSize+AUTOLN+1] = b2;
        romStoreAddr(AUTOAD, autoaddr);
        }
    fprintf(stderr, "Autorun addr = %d\n", autoaddr);
    if (b1 == 254 && b2 == 255)
        {
        fprintf(stderr, "No Autorun\n");
        if (autorun_warn == 1)
            {
            fprintf(stderr,"Warning: There were possible issues with autorun. You can check the code for\n");
            fprintf(stderr,"what line it should be and use the '-a' option to set the correct start line.\n");
            fprintf(stderr,"The line is normally the line after a SAVE command.\n");
            }
        }
    else
        {
        fprintf(stderr, "Autorun line = %d\n", autoline);
        /* Print the line that will be autorun */
        printLine(stderr, autoaddr);
        if (autorun_warn == 1)
            {
            fprintf(stderr,"Warning: There were possible issues with autorun. You can use the '-a' option\n");
            fprintf(stderr,"with -1 as the line number to disable and check the code for the correct line.\n");
            fprintf(stderr,"The line is normally the line after a SAVE command.\n");
            }
        }

    if (!out)
        {
        strcpy(outname, outroot);
        strcat(outname, outext);
        if (!infoOnly)
            {
            out = fopen(outname, "w");
            if ( out == NULL )
                {
                fprintf(stderr, "Error: couldn't write output file '%s'\n", outname);
                cleanup();
                exit(EXIT_FAILURE);
                }
            }
        }

    /* Copy program block 1 to rom */
    memcpy(rom + dataOffset, prg, prog1len);
    thisRomSize = dataOffset + prog1len;

    if (prog2len > 0)
        {
        /* Prog is split, done with first 8K ROM */
        writeROM(out, !oneRom);
        if (oneRom)
            {
            prevRomSize += ROM8K;
            }
        else
            {
            /* ROM B */
            prevRomSize = 0;
            c = strlen(outroot);
            outroot[c-1]++; /* A->B */
            strcpy(outname, outroot);
            strcat(outname, outext);
            if (!infoOnly)
                {
                fclose(out);
                out = fopen(outname,"w");
                if (out == NULL)
                    {
                    fprintf(stderr, "Error: couldn't write output file '%s'\n", outname);
                    cleanup();
                    exit(EXIT_FAILURE);
                    }
                fprintf(stderr, "Writing: %s\n", outname);
                }
            }
        /* Write prog 2 block to rom */
        memset(rom, 0xFF, ROM8K);
        memcpy(rom, prg + prog1len, prog2len);
        thisRomSize = prog2len;
        }
    
    if (includeVars)
        {
        /* Copy first block to rom */
        memcpy(rom + vars1offs, var, vars1len);
        thisRomSize = vars1offs + vars1len;
        if (vars2len > 0)
            {
            /* Vars is split, done with first 8K ROM */
            writeROM(out, !oneRom);
            if (oneRom)
                {
                prevRomSize += ROM8K;
                }
            else
                {
                /* Prepare ROM B file */
                prevRomSize = 0;
                c = strlen(outroot);
                outroot[c-1]++; /* Change 'A' to 'B' */
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
            memset(rom, 0xFF, ROM8K);
            memcpy(rom, var + vars1len, vars2len);
            thisRomSize = vars2len;
            }
        }

    /* All done */
    writeROM(out, 1);
    if (!infoOnly)
        fclose(out);
    fclose(in);
    cleanup();
    return EXIT_SUCCESS;

}
