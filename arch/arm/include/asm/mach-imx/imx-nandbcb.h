/*
 * Copyright (C) 2017 Amarula Solutions B.V.
 * Author: Jagan Teki <jagan at amarulasolutions.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _IMX_NAND_BCB_H_
#define _IMX_NAND_BCB_H_

#define FCB_FINGERPRINT		0x20424346      /* 'FCB' */
#define FCB_VERSION_1		0x01000000

#define DBBT_FINGERPRINT2	0x54424244	/* 'DBBT' */
#define DBBT_VERSION_1		0x01000000

struct dbbt_block {
	u32 checksum;	/* reserved on i.MX6 */
	u32 fingerprint;
	u32 version;
	u32 numberbb;	/* reserved on i.MX6 */
	u32 dbbtnumofpages;
};

struct fcb_block {
	u32 checksum;		/* First fingerprint in first byte */
	u32 fingerprint;	/* 2nd fingerprint at byte 4 */
	u32 version;		/* 3rd fingerprint at byte 8 */
	u8 datasetup;
	u8 datahold;
	u8 addresssetup;
	u8 dsample_time;

	/* These are for application use only and not for ROM. */
	u8 nandtimingstate;
	u8 rea;
	u8 rloh;
	u8 rhoh;
	u32 pagedatasize;	/* 2048 for 2K pages, 4096 for 4K pages */
	u32 totalpagesize;	/* 2112 for 2K pages, 4314 for 4K pages */
	u32 sectorsperblock;	/* Number of 2K sections per block */
	u32 numberofnands;	/* Total Number of NANDs - not used by ROM */
	u32 totalinternaldie;	/* Number of separate chips in this NAND */
	u32 celltype;		/* MLC or SLC */
	u32 eccblocknecctype;	/* Type of ECC, can be one of BCH-0-20 */
	u32 eccblock0size; /* Number of bytes for Block0 - BCH */
	u32 eccblocknsize; /* Block size in bytes for all blocks other than Block0 - BCH */
	u32 eccblock0ecctype; /* Ecc level for Block 0 - BCH */
	u32 metadatabytes; /* Metadata size - BCH */
	u32 numeccblocksperpage; /* Number of blocks per page for ROM use - BCH */
	u32 eccblocknecclevelsdk; /* Type of ECC, can be one of BCH-0-20 */
	u32 eccblock0sizesdk; /* Number of bytes for Block0 - BCH */
	u32 eccblocknsizesdk; /* Block size in bytes for all blocks other than Block0 - BCH */
	u32 eccblock0ecclevelsdk; /* Ecc level for Block 0 - BCH */
	u32 numeccblocksperpagesdk; /* Number of blocks per page for SDK use - BCH */
	u32 metadatabytessdk; /* Metadata size - BCH */
	u32 erasethreshold; /* To set into BCH_MODE register */
	u32 bootpatch; /* 0 for normal boot and 1 to load patch starting next to FCB */
	u32 patchsectors; /* Size of patch in sectors */
	u32 firmware1_startingpage; /* Firmware image starts on this sector */
	u32 firmware2_startingpage; /* Secondary FW Image starting Sector */
	u32 pagesinfirmware1; /* Number of sectors in firmware image */
	u32 pagesinfirmware2; /* Number of sector in secondary FW image */
	u32 dbbtsearchareastartaddress; /* Page address where dbbt search area begins */

	/* Byte in page data that have manufacturer marked bad block marker,
	 * this will be swapped with metadata[0] to complete page data.
	 */
	u32 badblockmarkerbyte;	/* Byte in page data that have manufacturer marked bad block marker, */

	/* For BCH ECC sizes other than 8 and 16 the bad block marker does not
	 * start at 0th bit of badblockmarkerbyte. This field is used to get to
	 * the start bit of bad block marker byte with in badblockmarkerbyte
	 */
	u32 badblockmarkerstartbit;

	/* FCB value that gives byte offset for
	 * bad block marker on physical NAND page
	 */
	u32 bbmarkerphysicaloffset;
	u32 bchtype;

	u32 tmtiming2_readlatency;
	u32 tmtiming2_preambledelay;
	u32 tmtiming2_cedelay;
	u32 tmtiming2_postambledelay;
	u32 tmtiming2_cmdaddpause;
	u32 tmtiming2_datapause;
	u32 tmspeed;
	u32 tmtiming1_busytimeout;

	/* the flag to enable (1)/disable(0) bi swap */
	u32 disbbm;

	/* The swap position of main area in spare area */
	u32 bbmarkerphysicaloffsetinsparedata;
};

#endif	/* _IMX_NAND_BCB_H_ */