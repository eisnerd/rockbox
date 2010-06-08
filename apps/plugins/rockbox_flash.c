/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id$
*
* Plugin for reprogramming only the second image in Flash ROM.
* !!! DON'T MESS WITH THIS CODE UNLESS YOU'RE ABSOLUTELY SURE WHAT YOU DO !!!
*
* Copyright (C) 2003 Jörg Hohensohn aka [IDC]Dragon
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
*
****************************************************************************/
#include "plugin.h"

#if (CONFIG_CPU == SH7034) /* Only for SH targets */

PLUGIN_HEADER

/* define DUMMY if you only want to "play" with the UI, does no harm */
/* #define DUMMY */

#define LATEST_BOOTLOADER_VERSION 3 /* update this with the bootloader */

#ifndef UINT8
#define UINT8 unsigned char
#endif

#ifndef UINT16
#define UINT16 unsigned short
#endif

#ifndef UINT32
#define UINT32 unsigned long
#endif

/* hard-coded values */
static volatile UINT8* FB = (UINT8*)0x02000000; /* Flash base address */
#define SECTORSIZE 4096 /* size of one flash sector */

#define ROCKBOX_DEST 0x09000000
#define ROCKBOX_EXEC 0x09000200
#define BOOT_VERS_ADR 0xFA /* position of bootloader version value in Flash */
#define FW_VERS_ADR 0xFE /* position of firmware version value in Flash */
#define UCL_HEADER 26 /* size of the header generated by uclpack */

#if CONFIG_KEYPAD == ONDIO_PAD /* limited keypad */
#define KEY1 BUTTON_LEFT
#define KEY2 BUTTON_UP
#define KEYNAME1 "Left"
#define KEYNAME2 "Up"
#else /* recorder keypad */
#define KEY1 BUTTON_F1
#define KEY2 BUTTON_F2
#define KEYNAME1 "F1"
#define KEYNAME2 "F2"
#endif

typedef struct 
{
    UINT32 destination; /* address to copy it to */
    UINT32 size;        /* how many bytes of payload (to the next header) */
    UINT32 execute;     /* entry point */
    UINT32 flags;       /* uncompressed or compressed */
    /* end of header, now comes the payload */
} tImageHeader;

/* result of the CheckFirmwareFile() function */
typedef enum
{
    eOK = 0,
    eFileNotFound, /* errors from here on */
    eTooBig,
    eTooSmall,
    eReadErr,
    eNotUCL,
    eWrongAlgorithm,
    eMultiBlocks,
    eBadRomLink
} tCheckResult;

typedef struct 
{
    UINT8 manufacturer;
    UINT8 id;
    int size;
    char name[32];
} tFlashInfo;

static UINT8* sector; /* better not place this on the stack... */

/***************** Flash Functions *****************/


/* read the manufacturer and device ID */
static bool ReadID(volatile UINT8* pBase, UINT8* pManufacturerID,
                   UINT8* pDeviceID)
{
    UINT8 not_manu, not_id; /* read values before switching to ID mode */
    UINT8 manu, id; /* read values when in ID mode */

    pBase = (UINT8*)((UINT32)pBase & 0xFFF80000); /* round down to 512k align,
                                                     to make sure */

    not_manu = pBase[0]; /* read the normal content */
    not_id   = pBase[1]; /* should be 'A' (0x41) and 'R' (0x52) from the
                            "ARCH" marker */

    pBase[0x5555] = 0xAA; /* enter command mode */
    pBase[0x2AAA] = 0x55;
    pBase[0x5555] = 0x90; /* ID command */
    rb->sleep(HZ/50);     /* Atmel wants 20ms pause here */

    manu = pBase[0];
    id   = pBase[1];
    
    pBase[0] = 0xF0;  /* reset flash (back to normal read mode) */
    rb->sleep(HZ/50); /* Atmel wants 20ms pause here */

    /* I assume success if the obtained values are different from
       the normal flash content. This is not perfectly bulletproof, they 
       could theoretically be the same by chance, causing us to fail. */
    if (not_manu != manu || not_id != id) /* a value has changed */
    {
        *pManufacturerID = manu; /* return the results */
        *pDeviceID = id;
        return true; /* success */
    }
    return false; /* fail */
}

/* erase the sector which contains the given address */
static bool EraseSector(volatile UINT8* pAddr)
{
#ifdef DUMMY
    (void)pAddr; /* prevents warning */
    return true;
#else
    volatile UINT8* pBase =
        (UINT8*)((UINT32)pAddr & 0xFFF80000); /* round down to 512k align */
    unsigned timeout = 43000; /* the timeout loop should be no less than
                                 25ms */

    pBase[0x5555] = 0xAA; /* enter command mode */
    pBase[0x2AAA] = 0x55;
    pBase[0x5555] = 0x80; /* erase command */
    pBase[0x5555] = 0xAA; /* enter command mode */
    pBase[0x2AAA] = 0x55;
    *pAddr = 0x30;        /* erase the sector */

    /* I counted 7 instructions for this loop -> min. 0.58 us per round
       Plus memory waitstates it will be much more, gives margin */
    while (*pAddr != 0xFF && --timeout); /* poll for erased */

    return (timeout != 0);
#endif
}

/* address must be in an erased location */
static inline bool ProgramByte(volatile UINT8* pAddr, UINT8 data)
{
#ifdef DUMMY
    (void)pAddr; /* prevents warnings */
    (void)data;
    return true;
#else
    unsigned timeout = 35; /* the timeout loop should be no less than 20us */

    if (~*pAddr & data) /* just a safety feature, not really necessary */
        return false; /* can't set any bit from 0 to 1 */

    FB[0x5555] = 0xAA; /* enter command mode */
    FB[0x2AAA] = 0x55;
    FB[0x5555] = 0xA0; /* byte program command */

    *pAddr = data;

    /* I counted 7 instructions for this loop -> min. 0.58 us per round
       Plus memory waitstates it will be much more, gives margin */
    while (*pAddr != data && --timeout); /* poll for programmed */

    return (timeout != 0);
#endif
}

/* this returns true if supported and fills the info struct */
static bool GetFlashInfo(tFlashInfo* pInfo)
{
    rb->memset(pInfo, 0, sizeof(tFlashInfo));

    if (!ReadID(FB, &pInfo->manufacturer, &pInfo->id))
        return false;

    if (pInfo->manufacturer == 0xBF) /* SST */
    {
        if (pInfo->id == 0xD6)
        {
            pInfo->size = 256* 1024; /* 256k */
            rb->strcpy(pInfo->name, "SST39VF020");
            return true;
        }
        else if (pInfo->id == 0xD7)
        {
            pInfo->size = 512* 1024; /* 512k */
            rb->strcpy(pInfo->name, "SST39VF040");
            return true;
        }
        else
            return false;
    }
    return false;
}


/*********** Tool Functions ************/

/* read a 32 bit value from memory, big endian */
static UINT32 Read32(UINT8* pByte)
{
    UINT32 value;

    value = (UINT32)pByte[0] << 24;
    value |= (UINT32)pByte[1] << 16;
    value |= (UINT32)pByte[2] << 8;
    value |= (UINT32)pByte[3];

    return value;
}

/* get the start address of the second image */
static tImageHeader* GetSecondImage(void)
{
    tImageHeader* pImage1;
    UINT32 pos = 0;    /* default: not found */
    UINT32* pFlash = (UINT32*)FB;

    /* determine the first image position */
    pos = pFlash[2] + pFlash[3]; /* position + size of the bootloader
                                    = after it */
    pos = (pos + 3) & ~3; /* be sure it's 32 bit aligned */
    pImage1 = (tImageHeader*)pos;

    if (pImage1->destination != ROCKBOX_DEST ||
        pImage1->execute != ROCKBOX_EXEC)
        return 0; /* seems to be no Archos/Rockbox image in here */

    if (pImage1->size != 0)
    {
        /* success, we have a second image */
        pos = (UINT32)pImage1 + sizeof(tImageHeader) + pImage1->size;
        if (((pos + SECTORSIZE-1) & ~(SECTORSIZE-1)) != pos)
        {    /* not sector-aligned */
            pos = 0; /* sanity check failed */
        }
    }

    return (tImageHeader*)pos;
}

/* return bootloader version */
static inline unsigned BootloaderVersion(void)
{
    return FB[BOOT_VERS_ADR];
}

/*********** Image File Functions ************/

/* so far, only compressed images in UCL NRV algorithm 2e supported */
tCheckResult CheckImageFile(char* filename, int space,
                            tImageHeader* pHeader, UINT8* pos)
{
    int i;
    int fd;
    int filesize; /* size info */

    int fileread = 0; /* total size as read from the file */
    int read; /* how many for this sector */

    /* magic file header for compressed files */
    static const UINT8 magic[8] = { 0x00,0xe9,0x55,0x43,0x4c,0xff,0x01,0x1a };
    UINT8 ucl_header[UCL_HEADER];

    fd = rb->open(filename, O_RDONLY);
    if (fd < 0)
        return eFileNotFound;

    filesize = rb->filesize(fd);
    if (filesize - (int)sizeof(ucl_header) - 8 > space)
    {
        rb->close(fd);
        return eTooBig;
    }
    else if (filesize < 10000) /* give it some reasonable lower limit */
    {
        rb->close(fd);
        return eTooSmall;
    }

    /* do some sanity checks */

    read = rb->read(fd, ucl_header, sizeof(ucl_header));
    fileread += read;
    if (read != sizeof(ucl_header))
    {
        rb->close(fd);
        return eReadErr;
    }

    /* compare the magic header */
    for (i=0; i<8; i++)
    {
        if (ucl_header[i] != magic[i])
        {
            rb->close(fd);
            return eNotUCL;
        }
    }

    pHeader->size = Read32(ucl_header + 22); /* compressed size */
    if (pHeader->size != filesize - sizeof(ucl_header) - 8)
    {
        rb->close(fd);
        return eMultiBlocks;
    }

    /* fill in the hardcoded defaults of the header */
    pHeader->destination = ROCKBOX_DEST;
    pHeader->execute = ROCKBOX_EXEC;

    if (Read32(ucl_header + 18) > pHeader->size) /* compare with uncompressed
                                                    size */
    {   /* compressed, normal case */
        pHeader->flags = 0x00000001; /* flags for UCL compressed */

        /* check for supported algorithm */
        if (ucl_header[12] != 0x2E)
        {
            rb->close(fd);
            return eWrongAlgorithm;
        }
    }
    else
    {   /* uncompressed, either to be copied or run directly in flash */
        UINT32 reset_vector; /* image has to start with reset vector */

        pHeader->flags = 0x00000000; /* uncompressed */

        read = rb->read(fd, &reset_vector, sizeof(reset_vector));
        fileread += read;
        if (read != sizeof(reset_vector))
        {
            rb->close(fd);
            return eReadErr;
        }
        if (reset_vector >= (UINT32)FB 
         && reset_vector <  (UINT32)FB+512*1024) /* ROM address? */
        {
            /* assume in-place, executing directly in flash */
            pHeader->destination = (UINT32)(pos + sizeof(tImageHeader));

            /* for new RomBox, this isn't the reset vector,
               but the link address, for us to check the position */
            if(pHeader->destination != reset_vector) /* compare link addr. */
            {
                rb->close(fd);
                return eBadRomLink; /* not matching the start address */
            }

            /* read the now following reset vector */
            read = rb->read(fd, &reset_vector, sizeof(reset_vector));
            fileread += read;
            if (read != sizeof(reset_vector))
            {
                rb->close(fd);
                return eReadErr;
            }
        }

        pHeader->execute = reset_vector;
    }
    
    /* check if we can read the whole file */
    do
    {
        read = rb->read(fd, sector, SECTORSIZE);
        fileread += read;
    } while (read == SECTORSIZE);
    
    rb->close(fd);

    if (fileread != filesize)
        return eReadErr;
    
    return eOK;
}


/* returns the # of failures, 0 on success */
static unsigned ProgramImageFile(char* filename, UINT8* pos,
                                 tImageHeader* pImageHeader,
                                 int start, int size)
{
    int i;
    int fd;
    int read; /* how many for this sector */
    unsigned failures = 0;

    fd = rb->open(filename, O_RDONLY);
    if (fd < 0)
        return false;

    /* no error checking necessary here, we checked for minimum size
       already */
    rb->lseek(fd, start, SEEK_SET); /* go to start position */

    *(tImageHeader*)sector = *pImageHeader; /* copy header into sector
                                               buffer */
    read = rb->read(fd, sector + sizeof(tImageHeader),
                    SECTORSIZE - sizeof(tImageHeader)); /* payload behind */
    size -= read;
    read += sizeof(tImageHeader); /* to be programmed, but not part of the
                                     file */

    do {
        if (!EraseSector(pos))
        {
            /* nothing we can do, let the programming count the errors */
        }
        
        for (i=0; i<read; i++)
        {
            if (!ProgramByte(pos + i, sector[i]))
            {
                failures++;
            }
        }

        pos += SECTORSIZE;
        read = rb->read(fd, sector, (size > SECTORSIZE) ? SECTORSIZE : size);
        /* payload for next sector */
        size -= read;
    } while (read > 0);
    
    rb->close(fd);

    return failures;
}

/* returns the # of failures, 0 on success */
static unsigned VerifyImageFile(char* filename, UINT8* pos,
                                tImageHeader* pImageHeader,
                                int start, int size)
{
    int i;
    int fd;
    int read; /* how many for this sector */
    unsigned failures = 0;

    fd = rb->open(filename, O_RDONLY);
    if (fd < 0)
        return false;

    /* no error checking necessary here, we checked for minimum size
       already */
    rb->lseek(fd, start, SEEK_SET); /* go to start position */

    *(tImageHeader*)sector = *pImageHeader; /* copy header into sector
                                               buffer */
    read = rb->read(fd, sector + sizeof(tImageHeader),
                    SECTORSIZE - sizeof(tImageHeader)); /* payload behind */

    size -= read;
    read += sizeof(tImageHeader); /* to be programmed, but not part of the
                                     file */

    do
    {
        for (i=0; i<read; i++)
        {
            if (pos[i] != sector[i])
            {
                failures++;
            }
        }
        
        pos += SECTORSIZE;
        read = rb->read(fd, sector, (size > SECTORSIZE) ? SECTORSIZE : size);
        /* payload for next sector */
        size -= read;
    } while (read);
    
    rb->close(fd);
    
    return failures;
}


/***************** User Interface Functions *****************/

static int WaitForButton(void)
{
    int button;
    
    do
    {
        button = rb->button_get(true);
    } while (IS_SYSEVENT(button) || (button & BUTTON_REL));
    
    return button;
}

#ifdef HAVE_LCD_BITMAP
/* helper for DoUserDialog() */
static void ShowFlashInfo(tFlashInfo* pInfo, tImageHeader* pImageHeader)
{
    char buf[32];

    if (!pInfo->manufacturer)
    {
        rb->lcd_puts_scroll(0, 0, "Flash: M=?? D=??");
    }
    else
    {
        if (pInfo->size)
        {
            rb->snprintf(buf, sizeof(buf), "Flash size: %d KB",
                         pInfo->size / 1024);
            rb->lcd_puts_scroll(0, 0, buf);
        }
        else
        {
            rb->lcd_puts_scroll(0, 0, "Unsupported chip");
        }
        
    }

    if (pImageHeader)
    {
        rb->snprintf(buf, sizeof(buf), "Image at %d KB",
                     ((UINT8*)pImageHeader - FB) / 1024);
        rb->lcd_puts_scroll(0, 1, buf);
    }
    else
    {
        rb->lcd_puts_scroll(0, 1, "No image found!");
    }
}


/* Kind of our main function, defines the application flow. */
/* recorder version */
static void DoUserDialog(char* filename)
{
    tImageHeader ImageHeader;
    tFlashInfo FlashInfo;
    static char buf[MAX_PATH];
    int button;
    int rc; /* generic return code */
    UINT32 space, aligned_size, true_size;
    UINT8* pos;
    size_t memleft;
    unsigned bl_version;
    bool show_greet = false;
    
    /* this can only work if Rockbox runs in DRAM, not flash ROM */
    if ((UINT8*)rb >= FB && (UINT8*)rb < FB + 4096*1024) /* 4 MB max */
    {   /* we're running from flash */
        rb->splash(HZ*3, "Not from ROM");
        return; /* exit */
    }

    /* refuse to work if the power may fail meanwhile */
    if (!rb->battery_level_safe())
    {
        rb->splash(HZ*3, "Battery too low!");
        return; /* exit */
    }
    
    /* "allocate" memory */
    sector = rb->plugin_get_buffer(&memleft);
    if (memleft < SECTORSIZE) /* need buffer for a flash sector */
    {
        rb->splash(HZ*3, "Out of memory");
        return; /* exit */
    }

    rb->lcd_setfont(FONT_SYSFIXED);

    pos = (void*)GetSecondImage();
    rc = GetFlashInfo(&FlashInfo);

    ShowFlashInfo(&FlashInfo, (void*)pos);
    rb->lcd_update();

    if (FlashInfo.size == 0) /* no valid chip */
    {
        rb->splash(HZ*3, "Not flashable");
        return; /* exit */
    }
    else if (pos == 0)
    {
        rb->splash(HZ*3, "No image");
        return; /* exit */
    }

    bl_version = BootloaderVersion();
    if (bl_version < LATEST_BOOTLOADER_VERSION)
    {
        rb->snprintf(buf, sizeof(buf), "Bootloader V%d", bl_version);
        rb->lcd_puts(0, 0, buf);
        rb->lcd_puts(0, 1, "Hint: You're not  ");
        rb->lcd_puts(0, 2, "using the latest  ");
        rb->lcd_puts(0, 3, "bootloader.       ");
        rb->lcd_puts(0, 4, "A full reflash is ");
        rb->lcd_puts(0, 5, "recommended, but  ");
        rb->lcd_puts(0, 6, "not required.     ");
        rb->lcd_puts(0, 7, "Press " KEYNAME1 " to ignore");
        rb->lcd_update();

        if (WaitForButton() != KEY1)
        {
            return;
        }
        rb->lcd_clear_display();
    }
    
    rb->lcd_puts(0, show_greet ? 0 : 3, "Checking...");
    rb->lcd_update();

    space = FlashInfo.size - (pos-FB + sizeof(ImageHeader));
    /* size minus start */
    
    rc = CheckImageFile(filename, space, &ImageHeader, pos);
    if (rc != eOK)
    {
        rb->lcd_clear_display(); /* make room for error message */
        show_greet = true; /* verbose */
    }

    rb->lcd_puts(0, show_greet ? 0 : 3, "Checked:");
    switch (rc) {
        case eOK:
            rb->lcd_puts(0, show_greet ? 0 : 4, "File OK.");
            break;
    case eNotUCL:
            rb->lcd_puts(0, 1, "File not UCL ");
            rb->lcd_puts(0, 2, "compressed.");
            rb->lcd_puts(0, 3, "Use uclpack --2e");
            rb->lcd_puts(0, 4, " --10 rockbox.bin");
            break;
    case eWrongAlgorithm:
            rb->lcd_puts(0, 1, "Wrong algorithm");
            rb->lcd_puts(0, 2, "for compression.");
            rb->lcd_puts(0, 3, "Use uclpack --2e");
            rb->lcd_puts(0, 4, " --10 rockbox.bin");
            break;
    case eFileNotFound:
            rb->lcd_puts(0, 1, "File not found:");
            rb->lcd_puts_scroll(0, 2, filename);
            break;
    case eTooBig:
            rb->lcd_puts(0, 1, "File too big,");
            rb->lcd_puts(0, 2, "won't fit in chip.");
            break;
    case eTooSmall:
            rb->lcd_puts(0, 1, "File too small.");
            rb->lcd_puts(0, 2, "Incomplete?");
            break;
    case eReadErr:
            rb->lcd_puts(0, 1, "File read error.");
            break;
    case eMultiBlocks:
            rb->lcd_puts(0, 1, "File invalid.");
            rb->lcd_puts(0, 2, "Blocksize");
            rb->lcd_puts(0, 3, " too small?");
            break;
    case eBadRomLink:
            rb->lcd_puts(0, 1, "RomBox mismatch.");
            rb->lcd_puts(0, 2, "Wrong ROM position");
            break;
    default:
            rb->lcd_puts(0, 1, "Check failed.");
            break;
    }

    if (rc == eOK)
    {    /* was OK */
        rb->lcd_puts(0, 6, "[" KEYNAME2 "] to program");
        rb->lcd_puts(0, 7, "other key to exit");
    }
    else
    { /* error occured */
        rb->lcd_puts(0, 6, "Any key to exit");
    }
    rb->lcd_update();

    button = WaitForButton();
    if (rc != eOK || button != KEY2)
    {
        return;
    }
    
    true_size = ImageHeader.size;
    aligned_size = ((sizeof(tImageHeader) + true_size + SECTORSIZE-1) &
                    ~(SECTORSIZE-1)) - sizeof(tImageHeader); /* round up to
                                                                next flash
                                                                sector */
    ImageHeader.size = aligned_size; /* increase image size such that we reach
                                        the next sector */
    
    rb->lcd_clear_display();
    rb->lcd_puts_scroll(0, 0, "Programming...");
    rb->lcd_update();

    rc = ProgramImageFile(filename, pos, &ImageHeader, UCL_HEADER, true_size);
    if (rc)
    {   /* errors */
        rb->lcd_clear_display();
        rb->snprintf(buf, sizeof(buf), "%d errors", rc);
        rb->lcd_puts(0, 0, "Error:");
        rb->lcd_puts(0, 1, "Programming fail!");
        rb->lcd_puts(0, 2, buf);
        rb->lcd_update();
        button = WaitForButton();
    }
    
    rb->lcd_clear_display();
    rb->lcd_puts_scroll(0, 0, "Verifying...");
    rb->lcd_update();

    rc = VerifyImageFile(filename, pos, &ImageHeader, UCL_HEADER, true_size);

    rb->lcd_clear_display();
    if (rc == 0)
    {
        rb->lcd_puts(0, 0, "Verify OK.");
    }
    else
    {
        rb->snprintf(buf, sizeof(buf), "%d errors", rc);
        rb->lcd_puts(0, 0, "Error:");
        rb->lcd_puts(0, 1, "Verify fail!");
        rb->lcd_puts(0, 2, buf);
        rb->lcd_puts(0, 3, "Use safe image");
        rb->lcd_puts(0, 4, "if booting hangs:");
        rb->lcd_puts(0, 5, "F1 during power-on");
    }
    rb->lcd_puts(0, 7, "Any key to exit");
    rb->lcd_update();
    WaitForButton();
}

#else /* #ifdef HAVE_LCD_BITMAP */

/* Player version */
static void DoUserDialog(char* filename)
{
    tImageHeader ImageHeader;
    tFlashInfo FlashInfo;
    static char buf[MAX_PATH];
    int button;
    int rc; /* generic return code */
    UINT32 space, aligned_size, true_size;
    UINT8* pos;
    size_t memleft;
    unsigned bl_version;

    /* this can only work if Rockbox runs in DRAM, not flash ROM */
    if ((UINT8*)rb >= FB && (UINT8*)rb < FB + 4096*1024) /* 4 MB max */
    {   /* we're running from flash */
        rb->splash(HZ*3, "Not from ROM");
        return; /* exit */
    }

    /* refuse to work if the power may fail meanwhile */
    if (!rb->battery_level_safe())
    {
        rb->splash(HZ*3, "Batt. too low!");
        return; /* exit */
    }
    
    /* "allocate" memory */
    sector = rb->plugin_get_buffer(&memleft);
    if (memleft < SECTORSIZE) /* need buffer for a flash sector */
    {
        rb->splash(HZ*3, "Out of memory");
        return; /* exit */
    }

    pos = (void*)GetSecondImage();
    rc = GetFlashInfo(&FlashInfo);

    if (FlashInfo.size == 0) /* no valid chip */
    {
        rb->splash(HZ*3, "Not flashable");
        return; /* exit */
    }
    else if (pos == 0)
    {
        rb->splash(HZ*3, "No image");
        return; /* exit */
    }
    
    bl_version = BootloaderVersion();
    if (bl_version < LATEST_BOOTLOADER_VERSION)
    {
        rb->lcd_puts_scroll(0, 0, "Hint: You're not using the latest bootloader. A full reflash is recommended, but not required.");
        rb->lcd_puts_scroll(0, 1, "Press [Menu] to ignore");
        rb->lcd_update();

        if (WaitForButton() != BUTTON_MENU)
        {
            return;
        }
        rb->lcd_clear_display();
    }

    rb->lcd_puts(0, 0, "Checking...");
    rb->lcd_update();

    space = FlashInfo.size - (pos-FB + sizeof(ImageHeader));
    /* size minus start */
    
    rc = CheckImageFile(filename, space, &ImageHeader, pos);
    rb->lcd_puts(0, 0, "Checked:");
    switch (rc) {
        case eOK:
            rb->lcd_puts(0, 1, "File OK.");
            rb->sleep(HZ*1);
            break;
    case eNotUCL:
            rb->lcd_puts_scroll(0, 1, "File not UCL compressed.");
            break;
    case eWrongAlgorithm:
            rb->lcd_puts_scroll(0, 1, "Wrong compression algorithm.");
            break;
    case eFileNotFound:
            rb->lcd_puts_scroll(0, 1, "File not found.");
            break;
    case eTooBig:
            rb->lcd_puts_scroll(0, 1, "File too big.");
            break;
    case eTooSmall:
            rb->lcd_puts_scroll(0, 1, "File too small. Incomplete?");
            break;
    case eReadErr:
            rb->lcd_puts_scroll(0, 1, "File read error.");
            break;
    case eMultiBlocks:
            rb->lcd_puts_scroll(0, 1, "File invalid. Blocksize too small?");
            break;
    case eBadRomLink:
            rb->lcd_puts_scroll(0, 1, "RomBox mismatch.");
            break;
    default:
            rb->lcd_puts_scroll(0, 1, "Check failed.");
            break;
    }
    rb->lcd_update();

    if (rc == eOK)
    {    /* was OK */
        rb->lcd_clear_display();
        rb->lcd_puts_scroll(0, 0, "[ON] to program,");
        rb->lcd_puts_scroll(0, 1, "other key to exit.");
    }
    else
    { /* error occured */
        WaitForButton();
        rb->lcd_clear_display();
        rb->lcd_puts_scroll(0, 0, "Flash failed.");
        rb->lcd_puts_scroll(0, 1, "Any key to exit.");
    }
    rb->lcd_update();

    button = WaitForButton();
    if (rc != eOK || button != BUTTON_ON)
    {
        return;
    }
    
    true_size = ImageHeader.size;
    aligned_size = ((sizeof(tImageHeader) + true_size + SECTORSIZE-1) &
                    ~(SECTORSIZE-1)) - sizeof(tImageHeader); /* round up to
                                                                next flash
                                                                sector */
    ImageHeader.size = aligned_size; /* increase image size such that we reach
                                        the next sector */
    
    rb->lcd_clear_display();
    rb->lcd_puts_scroll(0, 0, "Programming...");
    rb->lcd_update();

    rc = ProgramImageFile(filename, pos, &ImageHeader, UCL_HEADER, true_size);
    if (rc)
    {   /* errors */
        rb->lcd_clear_display();
        rb->snprintf(buf, sizeof(buf), "%d errors", rc);
        rb->lcd_puts_scroll(0, 0, "Programming failed!");
        rb->lcd_puts_scroll(0, 1, buf);
        rb->lcd_update();
        button = WaitForButton();
    }
    
    rb->lcd_clear_display();
    rb->lcd_puts_scroll(0, 0, "Verifying...");
    rb->lcd_update();

    rc = VerifyImageFile(filename, pos, &ImageHeader, UCL_HEADER, true_size);

    rb->lcd_clear_display();
    if (rc == 0)
    {
        rb->lcd_puts(0, 0, "Verify OK.");
        rb->lcd_update();
    }
    else
    {
        rb->snprintf(buf, sizeof(buf), "Verify fail! %d errors", rc);
        rb->lcd_puts_scroll(0, 0, buf);
        rb->lcd_puts_scroll(0, 1, "Use safe image if booting hangs: [-] during power-on");
        rb->lcd_update();
        button = WaitForButton();
    }
}

#endif /* not HAVE_LCD_BITMAP */



/***************** Plugin Entry Point *****************/

enum plugin_status plugin_start(const void* parameter)
{
    int oldmode;

    if (parameter == NULL)
    {
        rb->splash(HZ*3, "Play .ucl file!");
        return PLUGIN_OK;
    }

    /* now go ahead and have fun! */
    oldmode = rb->system_memory_guard(MEMGUARD_NONE); /*disable memory guard */
    DoUserDialog((char*) parameter);
    rb->system_memory_guard(oldmode);              /* re-enable memory guard */

    return PLUGIN_OK;
}


#endif /* SH-target */
