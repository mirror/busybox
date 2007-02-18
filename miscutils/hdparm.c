/* vi: set sw=4 ts=4: */
/*
 * hdparm implementation for busybox
 *
 * Copyright (C) [2003] by [Matteo Croce] <3297627799@wind.it>
 * Hacked by Tito <farmatito@tiscali.it> for size optimization.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * This program is based on the source code of hdparm: see below...
 * hdparm.c - Command line interface to get/set hard disk parameters
 *          - by Mark Lord (C) 1994-2002 -- freely distributable
 */

#include "busybox.h"
#include <linux/hdreg.h>

/* device types */
/* ------------ */
#define NO_DEV                  0xffff
#define ATA_DEV                 0x0000
#define ATAPI_DEV               0x0001

/* word definitions */
/* ---------------- */
#define GEN_CONFIG		0   /* general configuration */
#define LCYLS			1   /* number of logical cylinders */
#define CONFIG			2   /* specific configuration */
#define LHEADS			3   /* number of logical heads */
#define TRACK_BYTES		4   /* number of bytes/track (ATA-1) */
#define SECT_BYTES		5   /* number of bytes/sector (ATA-1) */
#define LSECTS			6   /* number of logical sectors/track */
#define START_SERIAL            10  /* ASCII serial number */
#define LENGTH_SERIAL           10  /* 10 words (20 bytes or characters) */
#define BUF_TYPE		20  /* buffer type (ATA-1) */
#define BUFFER__SIZE		21  /* buffer size (ATA-1) */
#define RW_LONG			22  /* extra bytes in R/W LONG cmd ( < ATA-4)*/
#define START_FW_REV            23  /* ASCII firmware revision */
#define LENGTH_FW_REV		 4  /*  4 words (8 bytes or characters) */
#define START_MODEL		27  /* ASCII model number */
#define LENGTH_MODEL		20  /* 20 words (40 bytes or characters) */
#define SECTOR_XFER_MAX	        47  /* r/w multiple: max sectors xfered */
#define DWORD_IO		48  /* can do double-word IO (ATA-1 only) */
#define CAPAB_0			49  /* capabilities */
#define CAPAB_1			50
#define PIO_MODE		51  /* max PIO mode supported (obsolete)*/
#define DMA_MODE		52  /* max Singleword DMA mode supported (obs)*/
#define WHATS_VALID		53  /* what fields are valid */
#define LCYLS_CUR		54  /* current logical cylinders */
#define LHEADS_CUR		55  /* current logical heads */
#define LSECTS_CUR	        56  /* current logical sectors/track */
#define CAPACITY_LSB		57  /* current capacity in sectors */
#define CAPACITY_MSB		58
#define SECTOR_XFER_CUR		59  /* r/w multiple: current sectors xfered */
#define LBA_SECTS_LSB		60  /* LBA: total number of user */
#define LBA_SECTS_MSB		61  /*      addressable sectors */
#define SINGLE_DMA		62  /* singleword DMA modes */
#define MULTI_DMA		63  /* multiword DMA modes */
#define ADV_PIO_MODES		64  /* advanced PIO modes supported */
				    /* multiword DMA xfer cycle time: */
#define DMA_TIME_MIN		65  /*   - minimum */
#define DMA_TIME_NORM		66  /*   - manufacturer's recommended	*/
				    /* minimum PIO xfer cycle time: */
#define PIO_NO_FLOW		67  /*   - without flow control */
#define PIO_FLOW		68  /*   - with IORDY flow control */
#define PKT_REL			71  /* typical #ns from PKT cmd to bus rel */
#define SVC_NBSY		72  /* typical #ns from SERVICE cmd to !BSY */
#define CDR_MAJOR		73  /* CD ROM: major version number */
#define CDR_MINOR		74  /* CD ROM: minor version number */
#define QUEUE_DEPTH		75  /* queue depth */
#define MAJOR			80  /* major version number */
#define MINOR			81  /* minor version number */
#define CMDS_SUPP_0		82  /* command/feature set(s) supported */
#define CMDS_SUPP_1		83
#define CMDS_SUPP_2		84
#define CMDS_EN_0		85  /* command/feature set(s) enabled */
#define CMDS_EN_1		86
#define CMDS_EN_2		87
#define ULTRA_DMA		88  /* ultra DMA modes */
				    /* time to complete security erase */
#define ERASE_TIME		89  /*   - ordinary */
#define ENH_ERASE_TIME		90  /*   - enhanced */
#define ADV_PWR			91  /* current advanced power management level
				       in low byte, 0x40 in high byte. */
#define PSWD_CODE		92  /* master password revision code	*/
#define HWRST_RSLT		93  /* hardware reset result */
#define ACOUSTIC		94  /* acoustic mgmt values ( >= ATA-6) */
#define LBA_LSB			100 /* LBA: maximum.  Currently only 48 */
#define LBA_MID			101 /*      bits are used, but addr 103 */
#define LBA_48_MSB		102 /*      has been reserved for LBA in */
#define LBA_64_MSB		103 /*      the future. */
#define RM_STAT			127 /* removable media status notification feature set support */
#define SECU_STATUS		128 /* security status */
#define CFA_PWR_MODE		160 /* CFA power mode 1 */
#define START_MEDIA             176 /* media serial number */
#define LENGTH_MEDIA            20  /* 20 words (40 bytes or characters)*/
#define START_MANUF             196 /* media manufacturer I.D. */
#define LENGTH_MANUF            10  /* 10 words (20 bytes or characters) */
#define INTEGRITY		255 /* integrity word */

/* bit definitions within the words */
/* -------------------------------- */

/* many words are considered valid if bit 15 is 0 and bit 14 is 1 */
#define VALID			0xc000
#define VALID_VAL		0x4000
/* many words are considered invalid if they are either all-0 or all-1 */
#define NOVAL_0			0x0000
#define NOVAL_1			0xffff

/* word 0: gen_config */
#define NOT_ATA			0x8000
#define NOT_ATAPI		0x4000	/* (check only if bit 15 == 1) */
#define MEDIA_REMOVABLE		0x0080
#define DRIVE_NOT_REMOVABLE	0x0040  /* bit obsoleted in ATA 6 */
#define INCOMPLETE		0x0004
#define CFA_SUPPORT_VAL		0x848a	/* 848a=CFA feature set support */
#define DRQ_RESPONSE_TIME	0x0060
#define DRQ_3MS_VAL		0x0000
#define DRQ_INTR_VAL		0x0020
#define DRQ_50US_VAL		0x0040
#define PKT_SIZE_SUPPORTED	0x0003
#define PKT_SIZE_12_VAL		0x0000
#define PKT_SIZE_16_VAL		0x0001
#define EQPT_TYPE		0x1f00
#define SHIFT_EQPT		8

#define CDROM 0x0005

#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
static const char * const pkt_str[] = {
	"Direct-access device",			/* word 0, bits 12-8 = 00 */
	"Sequential-access device",		/* word 0, bits 12-8 = 01 */
	"Printer",				/* word 0, bits 12-8 = 02 */
	"Processor",				/* word 0, bits 12-8 = 03 */
	"Write-once device",			/* word 0, bits 12-8 = 04 */
	"CD-ROM",				/* word 0, bits 12-8 = 05 */
	"Scanner",				/* word 0, bits 12-8 = 06 */
	"Optical memory",			/* word 0, bits 12-8 = 07 */
	"Medium changer",			/* word 0, bits 12-8 = 08 */
	"Communications device",		/* word 0, bits 12-8 = 09 */
	"ACS-IT8 device",			/* word 0, bits 12-8 = 0a */
	"ACS-IT8 device",			/* word 0, bits 12-8 = 0b */
	"Array controller",			/* word 0, bits 12-8 = 0c */
	"Enclosure services",			/* word 0, bits 12-8 = 0d */
	"Reduced block command device",		/* word 0, bits 12-8 = 0e */
	"Optical card reader/writer",		/* word 0, bits 12-8 = 0f */
	"",					/* word 0, bits 12-8 = 10 */
	"",					/* word 0, bits 12-8 = 11 */
	"",					/* word 0, bits 12-8 = 12 */
	"",					/* word 0, bits 12-8 = 13 */
	"",					/* word 0, bits 12-8 = 14 */
	"",					/* word 0, bits 12-8 = 15 */
	"",					/* word 0, bits 12-8 = 16 */
	"",					/* word 0, bits 12-8 = 17 */
	"",					/* word 0, bits 12-8 = 18 */
	"",					/* word 0, bits 12-8 = 19 */
	"",					/* word 0, bits 12-8 = 1a */
	"",					/* word 0, bits 12-8 = 1b */
	"",					/* word 0, bits 12-8 = 1c */
	"",					/* word 0, bits 12-8 = 1d */
	"",					/* word 0, bits 12-8 = 1e */
	"Unknown",			/* word 0, bits 12-8 = 1f */
};

static const char * const ata1_cfg_str[] = {			/* word 0 in ATA-1 mode */
	"Reserved",				/* bit 0 */
	"hard sectored",			/* bit 1 */
	"soft sectored",			/* bit 2 */
	"not MFM encoded ",			/* bit 3 */
	"head switch time > 15us",		/* bit 4 */
	"spindle motor control option",		/* bit 5 */
	"fixed drive",				/* bit 6 */
	"removable drive",			/* bit 7 */
	"disk xfer rate <= 5Mbs",		/* bit 8 */
	"disk xfer rate > 5Mbs, <= 10Mbs",	/* bit 9 */
	"disk xfer rate > 5Mbs",		/* bit 10 */
	"rotational speed tol.",		/* bit 11 */
	"data strobe offset option",		/* bit 12 */
	"track offset option",			/* bit 13 */
	"format speed tolerance gap reqd",	/* bit 14 */
	"ATAPI"					/* bit 14 */
};
#endif

/* word 1: number of logical cylinders */
#define LCYLS_MAX		0x3fff /* maximum allowable value */

/* word 2: specific configuration
 * (a) require SET FEATURES to spin-up
 * (b) require spin-up to fully reply to IDENTIFY DEVICE
 */
#define STBY_NID_VAL		0x37c8  /*     (a) and     (b) */
#define STBY_ID_VAL		0x738c	/*     (a) and not (b) */
#define PWRD_NID_VAL		0x8c73	/* not (a) and     (b) */
#define PWRD_ID_VAL		0xc837	/* not (a) and not (b) */

/* words 47 & 59: sector_xfer_max & sector_xfer_cur */
#define SECTOR_XFER		0x00ff  /* sectors xfered on r/w multiple cmds*/
#define MULTIPLE_SETTING_VALID  0x0100  /* 1=multiple sector setting is valid */

/* word 49: capabilities 0 */
#define STD_STBY		0x2000  /* 1=standard values supported (ATA);
					   0=vendor specific values */
#define IORDY_SUP		0x0800  /* 1=support; 0=may be supported */
#define IORDY_OFF		0x0400  /* 1=may be disabled */
#define LBA_SUP			0x0200  /* 1=Logical Block Address support */
#define DMA_SUP			0x0100  /* 1=Direct Memory Access support */
#define DMA_IL_SUP		0x8000  /* 1=interleaved DMA support (ATAPI) */
#define CMD_Q_SUP		0x4000  /* 1=command queuing support (ATAPI) */
#define OVLP_SUP		0x2000  /* 1=overlap operation support (ATAPI) */
#define SWRST_REQ		0x1000  /* 1=ATA SW reset required (ATAPI, obsolete */

/* word 50: capabilities 1 */
#define MIN_STANDBY_TIMER	0x0001  /* 1=device specific standby timer value minimum */

/* words 51 & 52: PIO & DMA cycle times */
#define MODE			0xff00  /* the mode is in the MSBs */

/* word 53: whats_valid */
#define OK_W88			0x0004	/* the ultra_dma info is valid */
#define OK_W64_70		0x0002  /* see above for word descriptions */
#define OK_W54_58		0x0001  /* current cyl, head, sector, cap. info valid */

/*word 63,88: dma_mode, ultra_dma_mode*/
#define MODE_MAX		7	/* bit definitions force udma <=7 (when
					 * udma >=8 comes out it'll have to be
					 * defined in a new dma_mode word!) */

/* word 64: PIO transfer modes */
#define PIO_SUP			0x00ff  /* only bits 0 & 1 are used so far,  */
#define PIO_MODE_MAX		8       /* but all 8 bits are defined        */

/* word 75: queue_depth */
#define DEPTH_BITS		0x001f  /* bits used for queue depth */

/* words 80-81: version numbers */
/* NOVAL_0 or  NOVAL_1 means device does not report version */

/* word 81: minor version number */
#define MINOR_MAX		0x22
#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
static const char *minor_str[MINOR_MAX+2] = {			/* word 81 value: */
	"Unspecified",					/* 0x0000	*/
	"ATA-1 X3T9.2 781D prior to rev.4",	/* 0x0001	*/
	"ATA-1 published, ANSI X3.221-1994",		/* 0x0002	*/
	"ATA-1 X3T9.2 781D rev.4",			/* 0x0003	*/
	"ATA-2 published, ANSI X3.279-1996",		/* 0x0004	*/
	"ATA-2 X3T10 948D prior to rev.2k",	/* 0x0005	*/
	"ATA-3 X3T10 2008D rev.1",			/* 0x0006	*/
	"ATA-2 X3T10 948D rev.2k",			/* 0x0007	*/
	"ATA-3 X3T10 2008D rev.0",			/* 0x0008	*/
	"ATA-2 X3T10 948D rev.3",			/* 0x0009	*/
	"ATA-3 published, ANSI X3.298-199x",		/* 0x000a	*/
	"ATA-3 X3T10 2008D rev.6",			/* 0x000b	*/
	"ATA-3 X3T13 2008D rev.7 and 7a",		/* 0x000c	*/
	"ATA/ATAPI-4 X3T13 1153D rev.6",		/* 0x000d	*/
	"ATA/ATAPI-4 T13 1153D rev.13",		/* 0x000e	*/
	"ATA/ATAPI-4 X3T13 1153D rev.7",		/* 0x000f	*/
	"ATA/ATAPI-4 T13 1153D rev.18",		/* 0x0010	*/
	"ATA/ATAPI-4 T13 1153D rev.15",		/* 0x0011	*/
	"ATA/ATAPI-4 published, ANSI INCITS 317-1998",	/* 0x0012	*/
	"ATA/ATAPI-5 T13 1321D rev.3",
	"ATA/ATAPI-4 T13 1153D rev.14",		/* 0x0014	*/
	"ATA/ATAPI-5 T13 1321D rev.1",		/* 0x0015	*/
	"ATA/ATAPI-5 published, ANSI INCITS 340-2000",	/* 0x0016	*/
	"ATA/ATAPI-4 T13 1153D rev.17",		/* 0x0017	*/
	"ATA/ATAPI-6 T13 1410D rev.0",		/* 0x0018	*/
	"ATA/ATAPI-6 T13 1410D rev.3a",		/* 0x0019	*/
	"ATA/ATAPI-7 T13 1532D rev.1",		/* 0x001a	*/
	"ATA/ATAPI-6 T13 1410D rev.2",		/* 0x001b	*/
	"ATA/ATAPI-6 T13 1410D rev.1",		/* 0x001c	*/
	"ATA/ATAPI-7 published, ANSI INCITS 397-2005",	/* 0x001d	*/
	"ATA/ATAPI-7 T13 1532D rev.0",		/* 0x001e	*/
	"Reserved"					/* 0x001f	*/
	"Reserved"					/* 0x0020	*/
	"ATA/ATAPI-7 T13 1532D rev.4a",		/* 0x0021	*/
	"ATA/ATAPI-6 published, ANSI INCITS 361-2002",	/* 0x0022	*/
	"Reserved"					/* 0x0023-0xfffe*/
};
#endif
static const char actual_ver[MINOR_MAX+2] = {
			/* word 81 value: */
	0,		/* 0x0000	WARNING:	*/
	1,		/* 0x0001	WARNING:	*/
	1,		/* 0x0002	WARNING:	*/
	1,		/* 0x0003	WARNING:	*/
	2,		/* 0x0004	WARNING:   This array		*/
	2,		/* 0x0005	WARNING:   corresponds		*/
	3,		/* 0x0006	WARNING:   *exactly*		*/
	2,		/* 0x0007	WARNING:   to the ATA/		*/
	3,		/* 0x0008	WARNING:   ATAPI version	*/
	2,		/* 0x0009	WARNING:   listed in		*/
	3,		/* 0x000a	WARNING:   the			*/
	3,		/* 0x000b	WARNING:   minor_str		*/
	3,		/* 0x000c	WARNING:   array		*/
	4,		/* 0x000d	WARNING:   above.		*/
	4,		/* 0x000e	WARNING:			*/
	4,		/* 0x000f	WARNING:   if you change	*/
	4,		/* 0x0010	WARNING:   that one,		*/
	4,		/* 0x0011	WARNING:   change this one	*/
	4,		/* 0x0012	WARNING:   too!!!		*/
	5,		/* 0x0013	WARNING:	*/
	4,		/* 0x0014	WARNING:	*/
	5,		/* 0x0015	WARNING:	*/
	5,		/* 0x0016	WARNING:	*/
	4,		/* 0x0017	WARNING:	*/
	6,		/* 0x0018	WARNING:	*/
	6,		/* 0x0019	WARNING:	*/
	7,		/* 0x001a	WARNING:	*/
	6,		/* 0x001b	WARNING:	*/
	6,		/* 0x001c	WARNING:	*/
	7,		/* 0x001d	WARNING:	*/
	7,		/* 0x001e	WARNING:	*/
	0,		/* 0x001f	WARNING:	*/
	0,		/* 0x0020	WARNING:	*/
	7,		/* 0x0021	WARNING:	*/
	6,		/* 0x0022	WARNING:	*/
	0		/* 0x0023-0xfffe    	*/
};

/* words 82-84: cmds/feats supported */
#define CMDS_W82		0x77ff  /* word 82: defined command locations*/
#define CMDS_W83		0x3fff  /* word 83: defined command locations*/
#define CMDS_W84		0x002f  /* word 83: defined command locations*/
#define SUPPORT_48_BIT		0x0400
#define NUM_CMD_FEAT_STR	48

#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
static const char * const cmd_feat_str[] = {
	"",					/* word 82 bit 15: obsolete  */
	"NOP cmd",				/* word 82 bit 14 */
	"READ BUFFER cmd",			/* word 82 bit 13 */
	"WRITE BUFFER cmd",			/* word 82 bit 12 */
	"",					/* word 82 bit 11: obsolete  */
	"Host Protected Area feature set",	/* word 82 bit 10 */
	"DEVICE RESET cmd",			/* word 82 bit  9 */
	"SERVICE interrupt",			/* word 82 bit  8 */
	"Release interrupt",			/* word 82 bit  7 */
	"Look-ahead",				/* word 82 bit  6 */
	"Write cache",				/* word 82 bit  5 */
	"PACKET command feature set",		/* word 82 bit  4 */
	"Power Management feature set",		/* word 82 bit  3 */
	"Removable Media feature set",		/* word 82 bit  2 */
	"Security Mode feature set",		/* word 82 bit  1 */
	"SMART feature set",			/* word 82 bit  0 */
						/* --------------*/
	"",					/* word 83 bit 15: !valid bit */
	"",					/* word 83 bit 14:  valid bit */
	"FLUSH CACHE EXT cmd",		/* word 83 bit 13 */
	"Mandatory FLUSH CACHE cmd ",	/* word 83 bit 12 */
	"Device Configuration Overlay feature set ",
	"48-bit Address feature set ",		/* word 83 bit 10 */
	"",
	"SET MAX security extension",		/* word 83 bit  8 */
	"Address Offset Reserved Area Boot",	/* word 83 bit  7 */
	"SET FEATURES subcommand required to spinup after power up",
	"Power-Up In Standby feature set",	/* word 83 bit  5 */
	"Removable Media Status Notification feature set",
	"Adv. Power Management feature set",/* word 83 bit  3 */
	"CFA feature set",			/* word 83 bit  2 */
	"READ/WRITE DMA QUEUED",		/* word 83 bit  1 */
	"DOWNLOAD MICROCODE cmd",		/* word 83 bit  0 */
						/* --------------*/
	"",					/* word 84 bit 15: !valid bit */
	"",					/* word 84 bit 14:  valid bit */
	"",					/* word 84 bit 13:  reserved */
	"",					/* word 84 bit 12:  reserved */
	"",					/* word 84 bit 11:  reserved */
	"",					/* word 84 bit 10:  reserved */
	"",					/* word 84 bit  9:  reserved */
	"",					/* word 84 bit  8:  reserved */
	"",					/* word 84 bit  7:  reserved */
	"",					/* word 84 bit  6:  reserved */
	"General Purpose Logging feature set",	/* word 84 bit  5 */
	"",					/* word 84 bit  4:  reserved */
	"Media Card Pass Through Command feature set ",
	"Media serial number ",			/* word 84 bit  2 */
	"SMART self-test ",			/* word 84 bit  1 */
	"SMART error logging "			/* word 84 bit  0 */
};

static void identify(uint16_t *id_supplied) ATTRIBUTE_NORETURN;
static void identify_from_stdin(void) ATTRIBUTE_NORETURN;
#else
void identify_from_stdin(void);
#endif


/* words 85-87: cmds/feats enabled */
/* use cmd_feat_str[] to display what commands and features have
 * been enabled with words 85-87
 */

/* words 89, 90, SECU ERASE TIME */
#define ERASE_BITS		0x00ff

/* word 92: master password revision */
/* NOVAL_0 or  NOVAL_1 means no support for master password revision */

/* word 93: hw reset result */
#define CBLID			0x2000  /* CBLID status */
#define RST0			0x0001  /* 1=reset to device #0 */
#define DEV_DET			0x0006  /* how device num determined */
#define JUMPER_VAL		0x0002  /* device num determined by jumper */
#define CSEL_VAL		0x0004  /* device num determined by CSEL_VAL */

/* word 127: removable media status notification feature set support */
#define RM_STAT_BITS		0x0003
#define RM_STAT_SUP		0x0001

/* word 128: security */
#define SECU_ENABLED	0x0002
#define SECU_LEVEL		0x0010
#define NUM_SECU_STR	6
#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
static const char * const secu_str[] = {
	"supported",			/* word 128, bit 0 */
	"enabled",			/* word 128, bit 1 */
	"locked",			/* word 128, bit 2 */
	"frozen",			/* word 128, bit 3 */
	"expired: security count",	/* word 128, bit 4 */
	"supported: enhanced erase"	/* word 128, bit 5 */
};
#endif

/* word 160: CFA power mode */
#define VALID_W160		0x8000  /* 1=word valid */
#define PWR_MODE_REQ		0x2000  /* 1=CFA power mode req'd by some cmds*/
#define PWR_MODE_OFF		0x1000  /* 1=CFA power moded disabled */
#define MAX_AMPS		0x0fff  /* value = max current in ma */

/* word 255: integrity */
#define SIG			0x00ff  /* signature location */
#define SIG_VAL			0x00A5  /* signature value */

#define TIMING_MB		64
#define TIMING_BUF_MB		1
#define TIMING_BUF_BYTES	(TIMING_BUF_MB * 1024 * 1024)
#define TIMING_BUF_COUNT	(timing_MB / TIMING_BUF_MB)
#define BUFCACHE_FACTOR		2

#undef DO_FLUSHCACHE		/* under construction: force cache flush on -W0 */

/* Busybox messages and functions */
static int bb_ioctl(int fd, int request, void *argp, const char *string)
{
	int e = ioctl(fd, request, argp);
	if (e && string)
		bb_perror_msg(" %s", string);
	return e;
}

static int bb_ioctl_alt(int fd, int cmd, unsigned char *args, int alt, const char *string)
{
	if (!ioctl(fd, cmd, args))
		return 0;
	args[0] = alt;
	return bb_ioctl(fd, cmd, args, string);
}

static void on_off(unsigned int value);

static void print_flag_on_off(unsigned long get_arg, const char *s, unsigned long arg)
{
	if (get_arg) {
		printf(" setting %s to %ld", s, arg);
		on_off(arg);
	}
}

static void bb_ioctl_on_off(int fd, int request, void *argp, const char *string,
							 const char * str)
{
	if (ioctl(fd, request, &argp) != 0)
		bb_perror_msg(" %s", string);
	else {
		printf(" %s\t= %2ld", str, (unsigned long) argp);
		on_off((unsigned long) argp);
	}
}

#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
static void print_ascii(uint16_t *p, uint8_t length);

static void xprint_ascii(uint16_t *val ,int i, const char *string, int n)
{
	if (val[i]) {
		printf("\t%-20s", string);
		print_ascii(&val[i], n);
	}
}
#endif
/* end of busybox specific stuff */

#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
static uint8_t mode_loop(uint16_t mode_sup, uint16_t mode_sel, int cc, uint8_t *have_mode)
{
	uint16_t ii;
	uint8_t err_dma = 0;

	for (ii = 0; ii <= MODE_MAX; ii++) {
		if (mode_sel & 0x0001) {
			printf("*%cdma%u ",cc,ii);
			if (*have_mode)
				err_dma = 1;
			*have_mode = 1;
		} else if (mode_sup & 0x0001)
			printf("%cdma%u ",cc,ii);

		mode_sup >>= 1;
		mode_sel >>= 1;
	}
	return err_dma;
}

static void print_ascii(uint16_t *p, uint8_t length) {
	uint8_t ii;
	char cl;

	/* find first non-space & print it */
	for (ii = 0; ii < length; ii++) {
		if ((char)((*p)>>8) != ' ')
			break;
		cl = (char)(*p);
		if (cl != ' ') {
			if (cl != '\0')
				printf("%c", cl);
			p++;
			ii++;
			break;
		}
		p++;
	}
	/* print the rest */
	for (; ii< length; ii++) {
		if (!(*p))
			break; /* some older devices have NULLs */
		printf("%c%c", (char)((*p)>>8), (char)(*p));
		p++;
	}
	puts("");
}

// Parse 512 byte disk identification block and print much crap.

static void identify(uint16_t *id_supplied)
{
	uint16_t buf[256];
	uint16_t *val, ii, jj, kk;
	uint16_t like_std = 1, std = 0, min_std = 0xffff;
	uint16_t dev = NO_DEV, eqpt = NO_DEV;
	uint8_t  have_mode = 0, err_dma = 0;
	uint8_t  chksum = 0;
	uint32_t ll, mm, nn, oo;
	uint64_t bbbig; /* (:) */
	const char *strng;

	// Adjust for endianness if necessary.

	if (BB_BIG_ENDIAN) {
		swab(id_supplied, buf, sizeof(buf));
		val = buf;
	} else
		val = id_supplied;

	chksum &= 0xff;

	/* check if we recognise the device type */
	puts("");
	if (!(val[GEN_CONFIG] & NOT_ATA)) {
		dev = ATA_DEV;
		printf("ATA device, with ");
	} else if (val[GEN_CONFIG]==CFA_SUPPORT_VAL) {
		dev = ATA_DEV;
		like_std = 4;
		printf("CompactFlash ATA device, with ");
	} else if (!(val[GEN_CONFIG] & NOT_ATAPI)) {
		dev = ATAPI_DEV;
		eqpt = (val[GEN_CONFIG] & EQPT_TYPE) >> SHIFT_EQPT;
		printf("ATAPI %s, with ", pkt_str[eqpt]);
		like_std = 3;
	} else
		/*"Unknown device type:\n\tbits 15&14 of general configuration word 0 both set to 1.\n"*/
		bb_error_msg_and_die("unknown device type");

	printf("%sremovable media\n", !(val[GEN_CONFIG] & MEDIA_REMOVABLE) ? "non-" : "");
	/* Info from the specific configuration word says whether or not the
	 * ID command completed correctly.  It is only defined, however in
	 * ATA/ATAPI-5 & 6; it is reserved (value theoretically 0) in prior
	 * standards.  Since the values allowed for this word are extremely
	 * specific, it should be safe to check it now, even though we don't
	 * know yet what standard this device is using.
	 */
	if ((val[CONFIG]==STBY_NID_VAL) || (val[CONFIG]==STBY_ID_VAL)
	 || (val[CONFIG]==PWRD_NID_VAL) || (val[CONFIG]==PWRD_ID_VAL)
	) {
		like_std = 5;
		if ((val[CONFIG]==STBY_NID_VAL) || (val[CONFIG]==STBY_ID_VAL))
			printf("powers-up in standby; SET FEATURES subcmd spins-up.\n");
		if (((val[CONFIG]==STBY_NID_VAL) || (val[CONFIG]==PWRD_NID_VAL)) && (val[GEN_CONFIG] & INCOMPLETE))
			printf("\n\tWARNING: ID response incomplete.\n\tFollowing data may be incorrect.\n\n");
	}

	/* output the model and serial numbers and the fw revision */
	xprint_ascii(val, START_MODEL,  "Model Number:",        LENGTH_MODEL);
	xprint_ascii(val, START_SERIAL, "Serial Number:",       LENGTH_SERIAL);
	xprint_ascii(val, START_FW_REV, "Firmware Revision:",   LENGTH_FW_REV);
	xprint_ascii(val, START_MEDIA,  "Media Serial Num:",    LENGTH_MEDIA);
	xprint_ascii(val, START_MANUF,  "Media Manufacturer:",  LENGTH_MANUF);

	/* major & minor standards version number (Note: these words were not
	 * defined until ATA-3 & the CDROM std uses different words.) */
	printf("Standards:");
	if (eqpt != CDROM) {
		if (val[MINOR] && (val[MINOR] <= MINOR_MAX)) {
			if (like_std < 3) like_std = 3;
			std = actual_ver[val[MINOR]];
			if (std) printf("\n\tUsed: %s ",minor_str[val[MINOR]]);

		}
		/* looks like when they up-issue the std, they obsolete one;
		 * thus, only the newest 4 issues need be supported. (That's
		 * what "kk" and "min_std" are all about.) */
		if (val[MAJOR] && (val[MAJOR] != NOVAL_1)) {
			printf("\n\tSupported: ");
			jj = val[MAJOR] << 1;
			kk = like_std >4 ? like_std-4: 0;
			for (ii = 14; (ii >0)&&(ii>kk); ii--) {
				if (jj & 0x8000) {
					printf("%u ", ii);
					if (like_std < ii) {
						like_std = ii;
						kk = like_std >4 ? like_std-4: 0;
					}
					if (min_std > ii) min_std = ii;
				}
				jj <<= 1;
			}
			if (like_std < 3) like_std = 3;
		}
		/* Figure out what standard the device is using if it hasn't told
		 * us.  If we know the std, check if the device is using any of
		 * the words from the next level up.  It happens.
		 */
		if (like_std < std) like_std = std;

		if (((std == 5) || (!std && (like_std < 6))) &&
			((((val[CMDS_SUPP_1] & VALID) == VALID_VAL) &&
			((	val[CMDS_SUPP_1] & CMDS_W83) > 0x00ff)) ||
			(((	val[CMDS_SUPP_2] & VALID) == VALID_VAL) &&
			(	val[CMDS_SUPP_2] & CMDS_W84) ) )
		) {
			like_std = 6;
		} else if (((std == 4) || (!std && (like_std < 5))) &&
			((((val[INTEGRITY]	& SIG) == SIG_VAL) && !chksum) ||
			((	val[HWRST_RSLT] & VALID) == VALID_VAL) ||
			(((	val[CMDS_SUPP_1] & VALID) == VALID_VAL) &&
			((	val[CMDS_SUPP_1] & CMDS_W83) > 0x001f)) ) )
		{
			like_std = 5;
		} else if (((std == 3) || (!std && (like_std < 4))) &&
				((((val[CMDS_SUPP_1] & VALID) == VALID_VAL) &&
				(((	val[CMDS_SUPP_1] & CMDS_W83) > 0x0000) ||
				((	val[CMDS_SUPP_0] & CMDS_W82) > 0x000f))) ||
				((	val[CAPAB_1] & VALID) == VALID_VAL) ||
				((	val[WHATS_VALID] & OK_W88) && val[ULTRA_DMA]) ||
				((	val[RM_STAT] & RM_STAT_BITS) == RM_STAT_SUP) )
		) {
			like_std = 4;
		} else if (((std == 2) || (!std && (like_std < 3)))
		 && ((val[CMDS_SUPP_1] & VALID) == VALID_VAL)
		) {
			like_std = 3;
		} else if (((std == 1) || (!std && (like_std < 2))) &&
				((val[CAPAB_0] & (IORDY_SUP | IORDY_OFF)) ||
				(val[WHATS_VALID] & OK_W64_70)) )
		{
			like_std = 2;
		}

		if (!std)
			printf("\n\tLikely used: %u\n", like_std);
		else if (like_std > std)
			printf("& some of %u\n", like_std);
		else
			puts("");
	} else {
		/* TBD: do CDROM stuff more thoroughly.  For now... */
		kk = 0;
		if (val[CDR_MINOR] == 9) {
			kk = 1;
			printf("\n\tUsed: ATAPI for CD-ROMs, SFF-8020i, r2.5");
		}
		if (val[CDR_MAJOR] && (val[CDR_MAJOR] !=NOVAL_1)) {
			kk = 1;
			printf("\n\tSupported: CD-ROM ATAPI");
			jj = val[CDR_MAJOR] >> 1;
			for (ii = 1; ii < 15; ii++) {
				if (jj & 0x0001) printf("-%u ", ii);
				jj >>= 1;
			}
		}
		printf("%s\n", (!kk) ? "\n\tLikely used CD-ROM ATAPI-1" : "" );
		/* the cdrom stuff is more like ATA-2 than anything else, so: */
		like_std = 2;
	}

	if (min_std == 0xffff)
		min_std = like_std > 4 ? like_std - 3 : 1;

	printf("Configuration:\n");
	/* more info from the general configuration word */
	if ((eqpt != CDROM) && (like_std == 1)) {
		jj = val[GEN_CONFIG] >> 1;
		for (ii = 1; ii < 15; ii++) {
			if (jj & 0x0001) printf("\t%s\n",ata1_cfg_str[ii]);
			jj >>=1;
		}
	}
	if (dev == ATAPI_DEV) {
		if ((val[GEN_CONFIG] & DRQ_RESPONSE_TIME) ==  DRQ_3MS_VAL)
			strng = "3ms";
		else if ((val[GEN_CONFIG] & DRQ_RESPONSE_TIME) ==  DRQ_INTR_VAL)
			strng = "<=10ms with INTRQ";
		else if ((val[GEN_CONFIG] & DRQ_RESPONSE_TIME) ==  DRQ_50US_VAL)
			strng ="50us";
		else
			strng = "Unknown";
		printf("\tDRQ response: %s\n\tPacket size: ", strng); /* Data Request (DRQ) */

		if ((val[GEN_CONFIG] & PKT_SIZE_SUPPORTED) == PKT_SIZE_12_VAL)
			strng = "12 bytes";
		else if ((val[GEN_CONFIG] & PKT_SIZE_SUPPORTED) == PKT_SIZE_16_VAL)
			strng = "16 bytes";
		else
			strng = "Unknown";
		puts(strng);
	} else {
		/* addressing...CHS? See section 6.2 of ATA specs 4 or 5 */
		ll = (uint32_t)val[LBA_SECTS_MSB] << 16 | val[LBA_SECTS_LSB];
		mm = 0; bbbig = 0;
		if ( (ll > 0x00FBFC10) && (!val[LCYLS]))
			printf("\tCHS addressing not supported\n");
		else {
			jj = val[WHATS_VALID] & OK_W54_58;
			printf("\tLogical\t\tmax\tcurrent\n\tcylinders\t%u\t%u\n\theads\t\t%u\t%u\n\tsectors/track\t%u\t%u\n\t--\n",
					val[LCYLS],jj?val[LCYLS_CUR]:0, val[LHEADS],jj?val[LHEADS_CUR]:0, val[LSECTS],jj?val[LSECTS_CUR]:0);

			if ((min_std == 1) && (val[TRACK_BYTES] || val[SECT_BYTES]))
				printf("\tbytes/track: %u\tbytes/sector: %u\n",val[TRACK_BYTES], val[SECT_BYTES]);

			if (jj) {
				mm = (uint32_t)val[CAPACITY_MSB] << 16 | val[CAPACITY_LSB];
				if (like_std < 3) {
					/* check Endian of capacity bytes */
					nn = val[LCYLS_CUR] * val[LHEADS_CUR] * val[LSECTS_CUR];
					oo = (uint32_t)val[CAPACITY_LSB] << 16 | val[CAPACITY_MSB];
					if (abs(mm - nn) > abs(oo - nn))
						mm = oo;
				}
				printf("\tCHS current addressable sectors:%11u\n",mm);
			}
		}
		/* LBA addressing */
		printf("\tLBA    user addressable sectors:%11u\n",ll);
		if (((val[CMDS_SUPP_1] & VALID) == VALID_VAL)
		 && (val[CMDS_SUPP_1] & SUPPORT_48_BIT)
		) {
			bbbig = (uint64_t)val[LBA_64_MSB]	<< 48 |
			        (uint64_t)val[LBA_48_MSB]	<< 32 |
			        (uint64_t)val[LBA_MID]	<< 16 |
					val[LBA_LSB] ;
			printf("\tLBA48  user addressable sectors:%11"PRIu64"\n",bbbig);
		}

		if (!bbbig)
			bbbig = (uint64_t)(ll>mm ? ll : mm); /* # 512 byte blocks */
		printf("\tdevice size with M = 1024*1024: %11"PRIu64" MBytes\n",bbbig>>11);
		bbbig = (bbbig<<9)/1000000;
		printf("\tdevice size with M = 1000*1000: %11"PRIu64" MBytes ",bbbig);

		if (bbbig > 1000)
			printf("(%"PRIu64" GB)\n", bbbig/1000);
		else
			puts("");
	}

	/* hw support of commands (capabilities) */
	printf("Capabilities:\n\t");

	if (dev == ATAPI_DEV) {
		if (eqpt != CDROM && (val[CAPAB_0] & CMD_Q_SUP)) printf("Cmd queuing, ");
		if (val[CAPAB_0] & OVLP_SUP) printf("Cmd overlap, ");
	}
	if (val[CAPAB_0] & LBA_SUP) printf("LBA, ");

	if (like_std != 1) {
		printf("IORDY%s(can%s be disabled)\n",
				!(val[CAPAB_0] & IORDY_SUP) ? "(may be)" : "",
				(val[CAPAB_0] & IORDY_OFF) ? "" :"not");
	} else
		printf("no IORDY\n");

	if ((like_std == 1) && val[BUF_TYPE]) {
		printf("\tBuffer type: %04x: %s%s\n", val[BUF_TYPE],
				(val[BUF_TYPE] < 2) ? "single port, single-sector" : "dual port, multi-sector",
				(val[BUF_TYPE] > 2) ? " with read caching ability" : "");
	}

	if ((min_std == 1) && (val[BUFFER__SIZE] && (val[BUFFER__SIZE] != NOVAL_1))) {
		printf("\tBuffer size: %.1fkB\n",(float)val[BUFFER__SIZE]/2);
	}
	if ((min_std < 4) && (val[RW_LONG])) {
		printf("\tbytes avail on r/w long: %u\n",val[RW_LONG]);
	}
	if ((eqpt != CDROM) && (like_std > 3)) {
		printf("\tQueue depth: %u\n",(val[QUEUE_DEPTH] & DEPTH_BITS)+1);
	}

	if (dev == ATA_DEV) {
		if (like_std == 1)
			printf("\tCan%s perform double-word IO\n",(!val[DWORD_IO]) ?"not":"");
		else {
			printf("\tStandby timer values: spec'd by %s", (val[CAPAB_0] & STD_STBY) ? "Standard" : "Vendor");
			if ((like_std > 3) && ((val[CAPAB_1] & VALID) == VALID_VAL))
				printf(", %s device specific minimum\n",(val[CAPAB_1] & MIN_STANDBY_TIMER)?"with":"no");
			else
				puts("");
		}
		printf("\tR/W multiple sector transfer: ");
		if ((like_std < 3) && !(val[SECTOR_XFER_MAX] & SECTOR_XFER))
			printf("not supported\n");
		else {
			printf("Max = %u\tCurrent = ",val[SECTOR_XFER_MAX] & SECTOR_XFER);
			if (val[SECTOR_XFER_CUR] & MULTIPLE_SETTING_VALID)
				printf("%u\n", val[SECTOR_XFER_CUR] & SECTOR_XFER);
			else
				printf("?\n");
		}
		if ((like_std > 3) && (val[CMDS_SUPP_1] & 0x0008)) {
			/* We print out elsewhere whether the APM feature is enabled or
			   not.  If it's not enabled, let's not repeat the info; just print
			   nothing here. */
			printf("\tAdvancedPM level: ");
			if ((val[ADV_PWR] & 0xFF00) == 0x4000) {
				uint8_t apm_level = val[ADV_PWR] & 0x00FF;
				printf("%u (0x%x)\n", apm_level, apm_level);
			}
			else
				printf("unknown setting (0x%04x)\n", val[ADV_PWR]);
		}
		if (like_std > 5 && val[ACOUSTIC]) {
			printf("\tRecommended acoustic management value: %u, current value: %u\n",
									(val[ACOUSTIC] >> 8) & 0x00ff, val[ACOUSTIC] & 0x00ff);
		}
	} else {
		 /* ATAPI */
		if (eqpt != CDROM && (val[CAPAB_0] & SWRST_REQ))
			printf("\tATA sw reset required\n");

		if (val[PKT_REL] || val[SVC_NBSY]) {
			printf("\tOverlap support:");
			if (val[PKT_REL]) printf(" %uus to release bus.",val[PKT_REL]);
			if (val[SVC_NBSY]) printf(" %uus to clear BSY after SERVICE cmd.",val[SVC_NBSY]);
			puts("");
		}
	}

	/* DMA stuff. Check that only one DMA mode is selected. */
	printf("\tDMA: ");
	if (!(val[CAPAB_0] & DMA_SUP))
		printf("not supported\n");
	else {
		if (val[DMA_MODE] && !val[SINGLE_DMA] && !val[MULTI_DMA])
			printf(" sdma%u\n",(val[DMA_MODE] & MODE) >> 8);
		if (val[SINGLE_DMA]) {
			jj = val[SINGLE_DMA];
			kk = val[SINGLE_DMA] >> 8;
			err_dma += mode_loop(jj,kk,'s',&have_mode);
		}
		if (val[MULTI_DMA]) {
			jj = val[MULTI_DMA];
			kk = val[MULTI_DMA] >> 8;
			err_dma += mode_loop(jj,kk,'m',&have_mode);
		}
		if ((val[WHATS_VALID] & OK_W88) && val[ULTRA_DMA]) {
			jj = val[ULTRA_DMA];
			kk = val[ULTRA_DMA] >> 8;
			err_dma += mode_loop(jj,kk,'u',&have_mode);
		}
		if (err_dma || !have_mode) printf("(?)");
		puts("");

		if ((dev == ATAPI_DEV) && (eqpt != CDROM) && (val[CAPAB_0] & DMA_IL_SUP))
			printf("\t\tInterleaved DMA support\n");

		if ((val[WHATS_VALID] & OK_W64_70)
		 && (val[DMA_TIME_MIN] || val[DMA_TIME_NORM])
		) {
			printf("\t\tCycle time:");
			if (val[DMA_TIME_MIN]) printf(" min=%uns",val[DMA_TIME_MIN]);
			if (val[DMA_TIME_NORM]) printf(" recommended=%uns",val[DMA_TIME_NORM]);
			puts("");
		}
	}

	/* Programmed IO stuff */
	printf("\tPIO: ");
	/* If a drive supports mode n (e.g. 3), it also supports all modes less
	 * than n (e.g. 3, 2, 1 and 0).  Print all the modes. */
	if ((val[WHATS_VALID] & OK_W64_70) && (val[ADV_PIO_MODES] & PIO_SUP)) {
		jj = ((val[ADV_PIO_MODES] & PIO_SUP) << 3) | 0x0007;
		for (ii = 0; ii <= PIO_MODE_MAX ; ii++) {
			if (jj & 0x0001) printf("pio%d ",ii);
			jj >>=1;
		}
		puts("");
	} else if (((min_std < 5) || (eqpt == CDROM)) && (val[PIO_MODE] & MODE)) {
		for (ii = 0; ii <= val[PIO_MODE]>>8; ii++)
			printf("pio%d ",ii);
		puts("");
	} else
		printf("unknown\n");

	if (val[WHATS_VALID] & OK_W64_70) {
		if (val[PIO_NO_FLOW] || val[PIO_FLOW]) {
			printf("\t\tCycle time:");
			if (val[PIO_NO_FLOW]) printf(" no flow control=%uns", val[PIO_NO_FLOW]);
			if (val[PIO_FLOW]) printf("  IORDY flow control=%uns", val[PIO_FLOW]);
			puts("");
		}
	}

	if ((val[CMDS_SUPP_1] & VALID) == VALID_VAL) {
		printf("Commands/features:\n\tEnabled\tSupported:\n");
		jj = val[CMDS_SUPP_0];
		kk = val[CMDS_EN_0];
		for (ii = 0; ii < NUM_CMD_FEAT_STR; ii++) {
			if ((jj & 0x8000) && (*cmd_feat_str[ii] != '\0')) {
				printf("\t%s\t%s\n", (kk & 0x8000) ? "   *" : "", cmd_feat_str[ii]);
			}
			jj <<= 1;
			kk <<= 1;
			if (ii % 16 == 15) {
				jj = val[CMDS_SUPP_0+1+(ii/16)];
				kk = val[CMDS_EN_0+1+(ii/16)];
			}
			if (ii == 31) {
				if ((val[CMDS_SUPP_2] & VALID) != VALID_VAL)
					ii +=16;
			}
		}
	}
	/* Removable Media Status Notification feature set */
	if ((val[RM_STAT] & RM_STAT_BITS) == RM_STAT_SUP)
		printf("\t%s supported\n", cmd_feat_str[27]);

	/* security */
	if ((eqpt != CDROM) && (like_std > 3)
	 && (val[SECU_STATUS] || val[ERASE_TIME] || val[ENH_ERASE_TIME])
	) {
		printf("Security:\n");
		if (val[PSWD_CODE] && (val[PSWD_CODE] != NOVAL_1))
			printf("\tMaster password revision code = %u\n",val[PSWD_CODE]);
		jj = val[SECU_STATUS];
		if (jj) {
			for (ii = 0; ii < NUM_SECU_STR; ii++) {
				printf("\t%s\t%s\n", (!(jj & 0x0001)) ? "not" : "",  secu_str[ii]);
				jj >>=1;
			}
			if (val[SECU_STATUS] & SECU_ENABLED) {
				printf("\tSecurity level %s\n", (val[SECU_STATUS] & SECU_LEVEL) ? "maximum" : "high");
			}
		}
		jj =  val[ERASE_TIME]     & ERASE_BITS;
		kk =  val[ENH_ERASE_TIME] & ERASE_BITS;
		if (jj || kk) {
			printf("\t");
			if (jj) printf("%umin for %sSECURITY ERASE UNIT. ", jj==ERASE_BITS ? 508 : jj<<1, "");
			if (kk) printf("%umin for %sSECURITY ERASE UNIT. ", kk==ERASE_BITS ? 508 : kk<<1, "ENHANCED ");
			puts("");
		}
	}

	/* reset result */
	jj = val[HWRST_RSLT];
	if ((jj & VALID) == VALID_VAL) {
		if (!(oo = (jj & RST0)))
			jj >>= 8;
		if ((jj & DEV_DET) == JUMPER_VAL)
			strng = " determined by the jumper";
		else if ((jj & DEV_DET) == CSEL_VAL)
			strng = " determined by CSEL";
		else
			strng = "";
		printf("HW reset results:\n\tCBLID- %s Vih\n\tDevice num = %i%s\n",
				(val[HWRST_RSLT] & CBLID) ? "above" : "below", !(oo), strng);
	}

	/* more stuff from std 5 */
	if ((like_std > 4) && (eqpt != CDROM)) {
		if (val[CFA_PWR_MODE] & VALID_W160) {
			printf("CFA power mode 1:\n\t%s%s\n", (val[CFA_PWR_MODE] & PWR_MODE_OFF) ? "disabled" : "enabled",
					(val[CFA_PWR_MODE] & PWR_MODE_REQ) ? " and required by some commands" : "");

			if (val[CFA_PWR_MODE] & MAX_AMPS)
				printf("\tMaximum current = %uma\n",val[CFA_PWR_MODE] & MAX_AMPS);
		}
		if ((val[INTEGRITY] & SIG) == SIG_VAL) {
			printf("Checksum: %scorrect\n", chksum ? "in" : "");
		}
	}

	exit(EXIT_SUCCESS);
}
#endif

static int get_identity, get_geom;
static int do_flush;
static int do_ctimings, do_timings;
static unsigned long set_readahead, get_readahead, Xreadahead;
static unsigned long set_readonly, get_readonly, readonly;
static unsigned long set_unmask, get_unmask, unmask;
static unsigned long set_mult, get_mult, mult;
#if ENABLE_FEATURE_HDPARM_HDIO_GETSET_DMA
static unsigned long set_dma, get_dma, dma;
#endif
static unsigned long set_dma_q, get_dma_q, dma_q;
static unsigned long set_nowerr, get_nowerr, nowerr;
static unsigned long set_keep, get_keep, keep;
static unsigned long set_io32bit, get_io32bit, io32bit;
static unsigned long set_piomode, noisy_piomode;
static int piomode;
#ifdef HDIO_DRIVE_CMD
static unsigned long set_dkeep, get_dkeep, dkeep;
static unsigned long set_standby, get_standby, standby_requested;
static unsigned long set_xfermode, get_xfermode;
static int xfermode_requested;
static unsigned long set_lookahead, get_lookahead, lookahead;
static unsigned long set_prefetch, get_prefetch, prefetch;
static unsigned long set_defects, get_defects, defects;
static unsigned long set_wcache, get_wcache, wcache;
static unsigned long set_doorlock, get_doorlock, doorlock;
static unsigned long set_seagate, get_seagate;
static unsigned long set_standbynow, get_standbynow;
static unsigned long set_sleepnow, get_sleepnow;
static unsigned long get_powermode;
static unsigned long set_apmmode, get_apmmode, apmmode;
#endif
#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
static int get_IDentity;
#endif
#if ENABLE_FEATURE_HDPARM_HDIO_UNREGISTER_HWIF
static unsigned long unregister_hwif;
static unsigned long hwif;
#endif
#if ENABLE_FEATURE_HDPARM_HDIO_SCAN_HWIF
static unsigned long scan_hwif;
static unsigned long hwif_data;
static unsigned long hwif_ctrl;
static unsigned long hwif_irq;
#endif
#if ENABLE_FEATURE_HDPARM_HDIO_TRISTATE_HWIF
static unsigned long set_busstate, get_busstate, busstate;
#endif
static int reread_partn;

#if ENABLE_FEATURE_HDPARM_HDIO_DRIVE_RESET
static int perform_reset;
#endif /* FEATURE_HDPARM_HDIO_DRIVE_RESET */
#if ENABLE_FEATURE_HDPARM_HDIO_TRISTATE_HWIF
static unsigned long perform_tristate, tristate;
#endif /* FEATURE_HDPARM_HDIO_TRISTATE_HWIF */

// Historically, if there was no HDIO_OBSOLETE_IDENTITY, then
// then the HDIO_GET_IDENTITY only returned 142 bytes.
// Otherwise, HDIO_OBSOLETE_IDENTITY returns 142 bytes,
// and HDIO_GET_IDENTITY returns 512 bytes.  But the latest
// 2.5.xx kernels no longer define HDIO_OBSOLETE_IDENTITY
// (which they should, but they should just return -EINVAL).
//
// So.. we must now assume that HDIO_GET_IDENTITY returns 512 bytes.
// On a really old system, it will not, and we will be confused.
// Too bad, really.

#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
static const char * const cfg_str[] =
{	"",	     "HardSect",   "SoftSect",   "NotMFM",
	"HdSw>15uSec", "SpinMotCtl", "Fixed",     "Removeable",
	"DTR<=5Mbs",   "DTR>5Mbs",   "DTR>10Mbs", "RotSpdTol>.5%",
	"dStbOff",     "TrkOff",     "FmtGapReq", "nonMagnetic"
};

static const char * const BuffType[] = {"Unknown", "1Sect", "DualPort", "DualPortCache"};

static void dump_identity(const struct hd_driveid *id)
{
	int i;
	const unsigned short int *id_regs = (const void*) id;

	printf("\n Model=%.40s, FwRev=%.8s, SerialNo=%.20s\n Config={",
				id->model, id->fw_rev, id->serial_no);
	for (i=0; i<=15; i++) {
		if (id->config & (1<<i))
			printf(" %s", cfg_str[i]);
	}
	printf(	" }\n RawCHS=%u/%u/%u, TrkSize=%u, SectSize=%u, ECCbytes=%u\n"
			" BuffType=(%u) %s, BuffSize=%ukB, MaxMultSect=%u",
				id->cyls, id->heads, id->sectors, id->track_bytes,
				id->sector_bytes, id->ecc_bytes,
				id->buf_type, BuffType[(id->buf_type > 3) ? 0 :  id->buf_type],
				id->buf_size/2, id->max_multsect);
	if (id->max_multsect) {
		printf(", MultSect=");
		if (!(id->multsect_valid & 1))
			printf("?%u?", id->multsect);
		else if (id->multsect)
			printf("%u", id->multsect);
		else
			printf("off");
	}
	puts("");

	if (!(id->field_valid & 1))
		printf(" (maybe):");

	printf(" CurCHS=%u/%u/%u, CurSects=%lu, LBA=%s",id->cur_cyls, id->cur_heads,
		id->cur_sectors,
		(BB_BIG_ENDIAN) ?
			(long unsigned int)(id->cur_capacity0 << 16) | id->cur_capacity1 :
			(long unsigned int)(id->cur_capacity1 << 16) | id->cur_capacity0,
			((id->capability&2) == 0) ? "no" : "yes");

	if (id->capability & 2)
		printf(", LBAsects=%u", id->lba_capacity);

	printf("\n IORDY=%s", (id->capability & 8) ? (id->capability & 4) ?  "on/off" : "yes" : "no");

	if (((id->capability & 8) || (id->field_valid & 2)) && (id->field_valid & 2))
		printf(", tPIO={min:%u,w/IORDY:%u}", id->eide_pio, id->eide_pio_iordy);

	if ((id->capability & 1) && (id->field_valid & 2))
		printf(", tDMA={min:%u,rec:%u}", id->eide_dma_min, id->eide_dma_time);

	printf("\n PIO modes:  ");
	if (id->tPIO <= 5) {
		printf("pio0 ");
		if (id->tPIO >= 1) printf("pio1 ");
		if (id->tPIO >= 2) printf("pio2 ");
	}
	if (id->field_valid & 2) {
		if (id->eide_pio_modes & 1) printf("pio3 ");
		if (id->eide_pio_modes & 2) printf("pio4 ");
		if (id->eide_pio_modes &~3) printf("pio? ");
	}
	if (id->capability & 1) {
		if (id->dma_1word | id->dma_mword) {
			printf("\n DMA modes:  ");
			if (id->dma_1word & 0x100) printf("*");
			if (id->dma_1word & 1) printf("sdma0 ");
			if (id->dma_1word & 0x200) printf("*");
			if (id->dma_1word & 2) printf("sdma1 ");
			if (id->dma_1word & 0x400) printf("*");
			if (id->dma_1word & 4) printf("sdma2 ");
			if (id->dma_1word & 0xf800) printf("*");
			if (id->dma_1word & 0xf8) printf("sdma? ");
			if (id->dma_mword & 0x100) printf("*");
			if (id->dma_mword & 1) printf("mdma0 ");
			if (id->dma_mword & 0x200) printf("*");
			if (id->dma_mword & 2) printf("mdma1 ");
			if (id->dma_mword & 0x400) printf("*");
			if (id->dma_mword & 4) printf("mdma2 ");
			if (id->dma_mword & 0xf800) printf("*");
			if (id->dma_mword & 0xf8) printf("mdma? ");
		}
	}
	if (((id->capability & 8) || (id->field_valid & 2)) && id->field_valid & 4) {
		printf("\n UDMA modes: ");
		if (id->dma_ultra & 0x100) printf("*");
		if (id->dma_ultra & 0x001) printf("udma0 ");
		if (id->dma_ultra & 0x200) printf("*");
		if (id->dma_ultra & 0x002) printf("udma1 ");
		if (id->dma_ultra & 0x400) printf("*");
		if (id->dma_ultra & 0x004) printf("udma2 ");
#ifdef __NEW_HD_DRIVE_ID
		if (id->hw_config & 0x2000) {
#else /* !__NEW_HD_DRIVE_ID */
		if (id->word93 & 0x2000) {
#endif /* __NEW_HD_DRIVE_ID */
			if (id->dma_ultra & 0x0800) printf("*");
			if (id->dma_ultra & 0x0008) printf("udma3 ");
			if (id->dma_ultra & 0x1000) printf("*");
			if (id->dma_ultra & 0x0010) printf("udma4 ");
			if (id->dma_ultra & 0x2000) printf("*");
			if (id->dma_ultra & 0x0020) printf("udma5 ");
			if (id->dma_ultra & 0x4000) printf("*");
			if (id->dma_ultra & 0x0040) printf("udma6 ");
			if (id->dma_ultra & 0x8000) printf("*");
			if (id->dma_ultra & 0x0080) printf("udma7 ");
		}
	}
	printf("\n AdvancedPM=%s",((id_regs[83]&8)==0)?"no":"yes");
	if (id_regs[83] & 8) {
		if (!(id_regs[86] & 8))
			printf(": disabled (255)");
		else if ((id_regs[91] & 0xFF00) != 0x4000)
			printf(": unknown setting");
		else
			printf(": mode=0x%02X (%u)",id_regs[91]&0xFF,id_regs[91]&0xFF);
	}
	if (id_regs[82] & 0x20)
		printf(" WriteCache=%s",(id_regs[85]&0x20) ? "enabled" : "disabled");
#ifdef __NEW_HD_DRIVE_ID
	if ((id->minor_rev_num && id->minor_rev_num <= 31)
	 || (id->major_rev_num && id->minor_rev_num <= 31)
	) {
		printf("\n Drive conforms to: %s: ", (id->minor_rev_num <= 31) ? minor_str[id->minor_rev_num] : "Unknown");
		if (id->major_rev_num != 0x0000 &&  /* NOVAL_0 */
		    id->major_rev_num != 0xFFFF) {  /* NOVAL_1 */
			for (i=0; i <= 15; i++) {
				if (id->major_rev_num & (1<<i))
						printf(" ATA/ATAPI-%u", i);
			}
		}
	}
#endif /* __NEW_HD_DRIVE_ID */
	printf("\n\n * current active mode\n\n");
}
#endif

static void flush_buffer_cache(int fd)
{
	fsync(fd);				/* flush buffers */
	bb_ioctl(fd, BLKFLSBUF, NULL,"BLKFLSBUF" ) ;/* do it again, big time */
#ifdef HDIO_DRIVE_CMD
	sleep(1);
	if (ioctl(fd, HDIO_DRIVE_CMD, NULL) && errno != EINVAL)	/* await completion */
		bb_perror_msg("HDIO_DRIVE_CMD");
#endif
}

static int seek_to_zero(int fd)
{
	if (lseek(fd, (off_t) 0, SEEK_SET))
		return 1;
	return 0;
}

static int read_big_block(int fd, char *buf)
{
	int i;

	i = read(fd, buf, TIMING_BUF_BYTES);
	if (i != TIMING_BUF_BYTES) {
		bb_error_msg("read(%d bytes) failed (rc=%d)", TIMING_BUF_BYTES, i);
		return 1;
	}
	/* access all sectors of buf to ensure the read fully completed */
	for (i = 0; i < TIMING_BUF_BYTES; i += 512)
		buf[i] &= 1;
	return 0;
}

static void print_timing(int t, double e)
{
	if (t >= e)  /* more than 1MB/s */
		printf("%2d MB in %5.2f seconds =%6.2f %cB/sec\n", t, e, t / e, 'M');
	else
		printf("%2d MB in %5.2f seconds =%6.2f %cB/sec\n", t, e, t / e * 1024, 'k');
}

static int do_blkgetsize (int fd, unsigned long long *blksize64)
{
	int rc;
	unsigned blksize32 = 0;

	if (0 == ioctl(fd, BLKGETSIZE64, blksize64)) {	// returns bytes
		*blksize64 /= 512;
		return 0;
	}
	rc = ioctl(fd, BLKGETSIZE, &blksize32);	// returns sectors
	if (rc)
		bb_perror_msg("BLKGETSIZE");
	*blksize64 = blksize32;
	return rc;
}

static void do_time(int flag, int fd)
/*
	flag = 0 time_cache
	flag = 1 time_device
*/
{
	struct itimerval e1, e2;
	double elapsed, elapsed2;
	unsigned int max_iterations = 1024, total_MB, iterations;
	unsigned long long blksize;
	RESERVE_CONFIG_BUFFER(buf, TIMING_BUF_BYTES);

	if (mlock(buf, TIMING_BUF_BYTES)) {
		bb_perror_msg("mlock");
		goto quit2;
	}

	if (0 == do_blkgetsize(fd, &blksize)) {
		max_iterations = blksize / (2 * 1024) / TIMING_BUF_MB;
	}

	/* Clear out the device request queues & give them time to complete */
	sync();
	sleep(3);

	setitimer(ITIMER_REAL, &(struct itimerval){{1000,0},{1000,0}}, NULL);

	if (flag  == 0) {
		/* Time cache */

		if (seek_to_zero (fd)) return;
		if (read_big_block (fd, buf)) return;
		printf(" Timing cached reads:   ");
		fflush(stdout);

		/* Now do the timing */
		iterations = 0;
		getitimer(ITIMER_REAL, &e1);
		do {
			++iterations;
			if (seek_to_zero (fd) || read_big_block (fd, buf))
				goto quit;
			getitimer(ITIMER_REAL, &e2);
			elapsed = (e1.it_value.tv_sec - e2.it_value.tv_sec)
			+ ((e1.it_value.tv_usec - e2.it_value.tv_usec) / 1000000.0);
		} while (elapsed < 2.0);
		total_MB = iterations * TIMING_BUF_MB;

		/* Now remove the lseek() and getitimer() overheads from the elapsed time */
		getitimer(ITIMER_REAL, &e1);
		do {
			if (seek_to_zero (fd))
				goto quit;
			getitimer(ITIMER_REAL, &e2);
			elapsed2 = (e1.it_value.tv_sec - e2.it_value.tv_sec)
			+ ((e1.it_value.tv_usec - e2.it_value.tv_usec) / 1000000.0);
		} while (--iterations);

		elapsed -= elapsed2;
		print_timing(BUFCACHE_FACTOR * total_MB, elapsed);
		flush_buffer_cache(fd);
		sleep(1);
	} else {
		/* Time device */

		printf(" Timing buffered disk reads:  ");
		fflush(stdout);
		/*
		* getitimer() is used rather than gettimeofday() because
		* it is much more consistent (on my machine, at least).
		*/
		/* Now do the timings for real */
		iterations = 0;
		getitimer(ITIMER_REAL, &e1);
		do {
			++iterations;
			if (read_big_block (fd, buf))
				goto quit;
			getitimer(ITIMER_REAL, &e2);
			elapsed = (e1.it_value.tv_sec - e2.it_value.tv_sec)
			+ ((e1.it_value.tv_usec - e2.it_value.tv_usec) / 1000000.0);
		} while (elapsed < 3.0 && iterations < max_iterations);

		total_MB = iterations * TIMING_BUF_MB;
		print_timing(total_MB, elapsed);
	}
quit:
	munlock(buf, TIMING_BUF_BYTES);
quit2:
	RELEASE_CONFIG_BUFFER(buf);
}

static void on_off (unsigned int value)
{
	printf(value ? " (on)\n" : " (off)\n");
}

#if ENABLE_FEATURE_HDPARM_HDIO_TRISTATE_HWIF
static void bus_state_value(unsigned int value)
{
	if (value == BUSSTATE_ON)
		on_off(1);
	else if (value == BUSSTATE_OFF)
		on_off(0);
	else if (value == BUSSTATE_TRISTATE)
		printf(" (tristate)\n");
	else
		printf(" (unknown: %d)\n", value);
}
#endif

#ifdef HDIO_DRIVE_CMD
static void interpret_standby(unsigned int standby)
{
	unsigned int t;

	printf(" (");
	if (standby == 0)
		printf("off");
	else if (standby == 252)
		printf("21 minutes");
	else if (standby == 253)
		printf("vendor-specific");
	else if (standby == 254)
		printf("Reserved");
	else if (standby == 255)
		printf("21 minutes + 15 seconds");
	else {
		if (standby <= 240) {
			t = standby * 5;
			printf("%u minutes + %u seconds", t / 60, t % 60);
		} else if (standby <= 251) {
			t = (standby - 240) * 30;
			printf("%u hours + %u minutes", t / 60, t % 60);
		} else
			printf("illegal value");
	}
	printf(")\n");
}

struct xfermode_entry {
	int val;
	const char *name;
};

static const struct xfermode_entry xfermode_table[] = {
	{ 8,    "pio0" },
	{ 9,    "pio1" },
	{ 10,   "pio2" },
	{ 11,   "pio3" },
	{ 12,   "pio4" },
	{ 13,   "pio5" },
	{ 14,   "pio6" },
	{ 15,   "pio7" },
	{ 16,   "sdma0" },
	{ 17,   "sdma1" },
	{ 18,   "sdma2" },
	{ 19,   "sdma3" },
	{ 20,   "sdma4" },
	{ 21,   "sdma5" },
	{ 22,   "sdma6" },
	{ 23,   "sdma7" },
	{ 32,   "mdma0" },
	{ 33,   "mdma1" },
	{ 34,   "mdma2" },
	{ 35,   "mdma3" },
	{ 36,   "mdma4" },
	{ 37,   "mdma5" },
	{ 38,   "mdma6" },
	{ 39,   "mdma7" },
	{ 64,   "udma0" },
	{ 65,   "udma1" },
	{ 66,   "udma2" },
	{ 67,   "udma3" },
	{ 68,   "udma4" },
	{ 69,   "udma5" },
	{ 70,   "udma6" },
	{ 71,   "udma7" },
	{ 0, NULL }
};

static int translate_xfermode(char * name)
{
	const struct xfermode_entry *tmp;
	char *endptr;
	int val = -1;

	for (tmp = xfermode_table; tmp->name != NULL; ++tmp) {
		if (!strcmp(name, tmp->name))
			return tmp->val;
	}

	val = strtol(name, &endptr, 10);
	if (*endptr == '\0')
		return val;

	return -1;
}

static void interpret_xfermode(unsigned int xfermode)
{
	printf(" (");
	if (xfermode == 0)
		printf("default PIO mode");
	else if (xfermode == 1)
		printf("default PIO mode, disable IORDY");
	else if (xfermode >= 8 && xfermode <= 15)
		printf("PIO flow control mode%u", xfermode-8);
	else if (xfermode >= 16 && xfermode <= 23)
		printf("singleword DMA mode%u", xfermode-16);
	else if (xfermode >= 32 && xfermode <= 39)
		printf("multiword DMA mode%u", xfermode-32);
	else if (xfermode >= 64 && xfermode <= 71)
		printf("UltraDMA mode%u", xfermode-64);
	else
		printf("Unknown");
	printf(")\n");
}
#endif /* HDIO_DRIVE_CMD */

static void print_flag(unsigned long flag, const char *s, unsigned long value)
{
	if (flag)
		printf(" setting %s to %ld\n", s, value);
}

static void process_dev(char *devname)
{
	int fd;
	static long parm, multcount;
#ifndef HDIO_DRIVE_CMD
	int force_operation = 0;
#endif
	/* Please restore args[n] to these values after each ioctl
	   except for args[2] */
	unsigned char args[4] = {WIN_SETFEATURES,0,0,0};
	const char *fmt = " %s\t= %2ld";

	fd = xopen(devname, O_RDONLY|O_NONBLOCK);
	printf("\n%s:\n", devname);

	if (set_readahead) {
		print_flag(get_readahead,"fs readahead", Xreadahead);
		bb_ioctl(fd, BLKRASET,(int *)Xreadahead,"BLKRASET");
	}
#if ENABLE_FEATURE_HDPARM_HDIO_UNREGISTER_HWIF
	if (unregister_hwif) {
		printf(" attempting to unregister hwif#%lu\n", hwif);
		bb_ioctl(fd, HDIO_UNREGISTER_HWIF,(int *)(unsigned long)hwif,"HDIO_UNREGISTER_HWIF");
	}
#endif
#if ENABLE_FEATURE_HDPARM_HDIO_SCAN_HWIF
	if (scan_hwif) {
		printf(" attempting to scan hwif (0x%lx, 0x%lx, %lu)\n", hwif_data, hwif_ctrl, hwif_irq);
		args[0] = hwif_data;
		args[1] = hwif_ctrl;
		args[2] = hwif_irq;
		bb_ioctl(fd, HDIO_SCAN_HWIF, args, "HDIO_SCAN_HWIF");
		args[0] = WIN_SETFEATURES;
		args[1] = 0;
	}
#endif
	if (set_piomode) {
		if (noisy_piomode) {
			printf(" attempting to ");
			if (piomode == 255)
				printf("auto-tune PIO mode\n");
			else if (piomode < 100)
				printf("set PIO mode to %d\n", piomode);
			else if (piomode < 200)
				printf("set MDMA mode to %d\n", (piomode-100));
			else
				printf("set UDMA mode to %d\n", (piomode-200));
		}
		bb_ioctl(fd, HDIO_SET_PIO_MODE, (int *)(unsigned long)piomode, "HDIO_SET_PIO_MODE");
	}
	if (set_io32bit) {
		print_flag(get_io32bit,"32-bit IO_support flag", io32bit);
		bb_ioctl(fd, HDIO_SET_32BIT, (int *)io32bit, "HDIO_SET_32BIT");
	}
	if (set_mult) {
		print_flag(get_mult, "multcount", mult);
#ifdef HDIO_DRIVE_CMD
		bb_ioctl(fd, HDIO_SET_MULTCOUNT, &mult, "HDIO_SET_MULTCOUNT");
#else
		force_operation |= (!bb_ioctl(fd, HDIO_SET_MULTCOUNT, &mult, "HDIO_SET_MULTCOUNT"));
#endif
	}
	if (set_readonly) {
		print_flag_on_off(get_readonly,"readonly", readonly);
		bb_ioctl(fd, BLKROSET, &readonly, "BLKROSET");
	}
	if (set_unmask) {
		print_flag_on_off(get_unmask,"unmaskirq", unmask);
		bb_ioctl(fd, HDIO_SET_UNMASKINTR, (int *)unmask, "HDIO_SET_UNMASKINTR");
	}
#if ENABLE_FEATURE_HDPARM_HDIO_GETSET_DMA
	if (set_dma) {
		print_flag_on_off(get_dma, "using_dma", dma);
		bb_ioctl(fd, HDIO_SET_DMA, (int *)dma, "HDIO_SET_DMA");
	}
#endif /* FEATURE_HDPARM_HDIO_GETSET_DMA */
	if (set_dma_q) {
		print_flag_on_off(get_dma_q,"DMA queue_depth", dma_q);
		bb_ioctl(fd, HDIO_SET_QDMA, (int *)dma_q, "HDIO_SET_QDMA");
	}
	if (set_nowerr) {
		print_flag_on_off(get_nowerr,"nowerr", nowerr);
		bb_ioctl(fd, HDIO_SET_NOWERR, (int *)nowerr,"HDIO_SET_NOWERR");
	}
	if (set_keep) {
		print_flag_on_off(get_keep,"keep_settings", keep);
		bb_ioctl(fd, HDIO_SET_KEEPSETTINGS, (int *)keep,"HDIO_SET_KEEPSETTINGS");
	}
#ifdef HDIO_DRIVE_CMD
	if (set_doorlock) {
		args[0] = doorlock ? WIN_DOORLOCK : WIN_DOORUNLOCK;
		args[2] = 0;
		print_flag_on_off(get_doorlock,"drive doorlock", doorlock);
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args,"HDIO_DRIVE_CMD(doorlock)");
		args[0] = WIN_SETFEATURES;
	}
	if (set_dkeep) {
		/* lock/unlock the drive's "feature" settings */
		print_flag_on_off(get_dkeep,"drive keep features", dkeep);
		args[2] = dkeep ? 0x66 : 0xcc;
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args,"HDIO_DRIVE_CMD(keepsettings)");
	}
	if (set_defects) {
		args[2] = defects ? 0x04 : 0x84;
		print_flag(get_defects,"drive defect-mgmt", defects);
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args,"HDIO_DRIVE_CMD(defectmgmt)");
	}
	if (set_prefetch) {
		args[1] = prefetch;
		args[2] = 0xab;
		print_flag(get_prefetch,"drive prefetch", prefetch);
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args, "HDIO_DRIVE_CMD(setprefetch)");
		args[1] = 0;
	}
	if (set_xfermode) {
		args[1] = xfermode_requested;
		args[2] = 3;
		if (get_xfermode)
		{
			print_flag(1,"xfermode", xfermode_requested);
			interpret_xfermode(xfermode_requested);
		}
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args,"HDIO_DRIVE_CMD(setxfermode)");
		args[1] = 0;
	}
	if (set_lookahead) {
		args[2] = lookahead ? 0xaa : 0x55;
		print_flag_on_off(get_lookahead,"drive read-lookahead", lookahead);
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args, "HDIO_DRIVE_CMD(setreadahead)");
	}
	if (set_apmmode) {
		args[2] = (apmmode == 255) ? 0x85 /* disable */ : 0x05 /* set */; /* feature register */
		args[1] = apmmode; /* sector count register 1-255 */
		if (get_apmmode)
			printf(" setting APM level to %s 0x%02lX (%ld)\n", (apmmode == 255) ? "disabled" : "", apmmode, apmmode);
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args,"HDIO_DRIVE_CMD");
		args[1] = 0;
	}
	if (set_wcache)	{
#ifdef DO_FLUSHCACHE
#ifndef WIN_FLUSHCACHE
#define WIN_FLUSHCACHE 0xe7
#endif
		static unsigned char flushcache[4] = {WIN_FLUSHCACHE,0,0,0};
#endif /* DO_FLUSHCACHE */
		args[2] = wcache ? 0x02 : 0x82;
		print_flag_on_off(get_wcache,"drive write-caching", wcache);
#ifdef DO_FLUSHCACHE
		if (!wcache)
			bb_ioctl(fd, HDIO_DRIVE_CMD, &flushcache, "HDIO_DRIVE_CMD(flushcache)");
#endif /* DO_FLUSHCACHE */
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args, "HDIO_DRIVE_CMD(setcache)");
#ifdef DO_FLUSHCACHE
		if (!wcache)
			bb_ioctl(fd, HDIO_DRIVE_CMD, &flushcache, "HDIO_DRIVE_CMD(flushcache)");
#endif /* DO_FLUSHCACHE */
	}

	/* In code below, we do not preserve args[0], but the rest
	   is preserved, including args[2] */
	args[2] = 0;

	if (set_standbynow) {
#ifndef WIN_STANDBYNOW1
#define WIN_STANDBYNOW1 0xE0
#endif
#ifndef WIN_STANDBYNOW2
#define WIN_STANDBYNOW2 0x94
#endif
		if (get_standbynow) printf(" issuing standby command\n");
		args[0] = WIN_STANDBYNOW1;
		bb_ioctl_alt(fd, HDIO_DRIVE_CMD, args, WIN_STANDBYNOW2, "HDIO_DRIVE_CMD(standby)");
	}
	if (set_sleepnow) {
#ifndef WIN_SLEEPNOW1
#define WIN_SLEEPNOW1 0xE6
#endif
#ifndef WIN_SLEEPNOW2
#define WIN_SLEEPNOW2 0x99
#endif
		if (get_sleepnow) printf(" issuing sleep command\n");
		args[0] = WIN_SLEEPNOW1;
		bb_ioctl_alt(fd, HDIO_DRIVE_CMD, args, WIN_SLEEPNOW2, "HDIO_DRIVE_CMD(sleep)");
	}
	if (set_seagate) {
		args[0] = 0xfb;
		if (get_seagate) printf(" disabling Seagate auto powersaving mode\n");
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args, "HDIO_DRIVE_CMD(seagatepwrsave)");
	}
	if (set_standby) {
		args[0] = WIN_SETIDLE1;
		args[1] = standby_requested;
		if (get_standby) {
			print_flag(1,"standby", standby_requested);
			interpret_standby(standby_requested);
		}
		bb_ioctl(fd, HDIO_DRIVE_CMD, &args, "HDIO_DRIVE_CMD(setidle1)");
		args[1] = 0;
	}
#else	/* HDIO_DRIVE_CMD */
	if (force_operation) {
		char buf[512];
		flush_buffer_cache(fd);
		if (-1 == read(fd, buf, sizeof(buf)))
			bb_perror_msg("read(%d bytes) failed (rc=%d)", sizeof(buf), -1);
	}
#endif	/* HDIO_DRIVE_CMD */

	if (get_mult || get_identity) {
		multcount = -1;
		if (ioctl(fd, HDIO_GET_MULTCOUNT, &multcount)) {
			if (get_mult)
				bb_perror_msg("HDIO_GET_MULTCOUNT");
		} else if (get_mult) {
			printf(fmt, "multcount", multcount);
			on_off(multcount);
		}
	}
	if (get_io32bit) {
		if (!bb_ioctl(fd, HDIO_GET_32BIT, &parm, "HDIO_GET_32BIT")) {
			printf(" IO_support\t=%3ld (", parm);
			if (parm == 0)
				printf("default 16-bit)\n");
			else if (parm == 2)
				printf("16-bit)\n");
			else if (parm == 1)
				printf("32-bit)\n");
			else if (parm == 3)
				printf("32-bit w/sync)\n");
			else if (parm == 8)
				printf("Request-Queue-Bypass)\n");
			else
				printf("\?\?\?)\n");
		}
	}
	if (get_unmask) {
		bb_ioctl_on_off(fd, HDIO_GET_UNMASKINTR,(unsigned long *)parm,
						"HDIO_GET_UNMASKINTR","unmaskirq");
	}


#if ENABLE_FEATURE_HDPARM_HDIO_GETSET_DMA
	if (get_dma) {
		if (!bb_ioctl(fd, HDIO_GET_DMA, &parm, "HDIO_GET_DMA"))
		{
			printf(fmt, "using_dma", parm);
			if (parm == 8)
				printf(" (DMA-Assisted-PIO)\n");
			else
				on_off(parm);
		}
	}
#endif
	if (get_dma_q) {
		bb_ioctl_on_off (fd, HDIO_GET_QDMA,(unsigned long *)parm,
						  "HDIO_GET_QDMA","queue_depth");
	}
	if (get_keep) {
		bb_ioctl_on_off (fd, HDIO_GET_KEEPSETTINGS,(unsigned long *)parm,
							"HDIO_GET_KEEPSETTINGS","keepsettings");
	}

	if (get_nowerr) {
		bb_ioctl_on_off  (fd, HDIO_GET_NOWERR,(unsigned long *)&parm,
							"HDIO_GET_NOWERR","nowerr");
	}
	if (get_readonly) {
		bb_ioctl_on_off(fd, BLKROGET,(unsigned long *)parm,
						  "BLKROGET","readonly");
	}
	if (get_readahead) {
		bb_ioctl_on_off (fd, BLKRAGET, (unsigned long *) parm,
							"BLKRAGET","readahead");
	}
	if (get_geom) {
		if (!bb_ioctl(fd, BLKGETSIZE, &parm, "BLKGETSIZE")) {
			struct hd_geometry g;

			if (!bb_ioctl(fd, HDIO_GETGEO, &g, "HDIO_GETGEO"))
				printf(" geometry\t= %u/%u/%u, sectors = %ld, start = %ld\n",
						g.cylinders, g.heads, g.sectors, parm, g.start);
		}
	}
#ifdef HDIO_DRIVE_CMD
	if (get_powermode) {
#ifndef WIN_CHECKPOWERMODE1
#define WIN_CHECKPOWERMODE1 0xE5
#endif
#ifndef WIN_CHECKPOWERMODE2
#define WIN_CHECKPOWERMODE2 0x98
#endif
		const char *state;

		args[0] = WIN_CHECKPOWERMODE1;
		if (bb_ioctl_alt(fd, HDIO_DRIVE_CMD, args, WIN_CHECKPOWERMODE2, 0)) {
			if (errno != EIO || args[0] != 0 || args[1] != 0)
				state = "Unknown";
			else
				state = "sleeping";
		} else
			state = (args[2] == 255) ? "active/idle" : "standby";
		args[1] = args[2] = 0;

		printf(" drive state is:  %s\n", state);
	}
#endif
#if ENABLE_FEATURE_HDPARM_HDIO_DRIVE_RESET
	if (perform_reset) {
		bb_ioctl(fd, HDIO_DRIVE_RESET, NULL, "HDIO_DRIVE_RESET");
	}
#endif /* FEATURE_HDPARM_HDIO_DRIVE_RESET */
#if ENABLE_FEATURE_HDPARM_HDIO_TRISTATE_HWIF
	if (perform_tristate) {
		args[0] = 0;
		args[1] = tristate;
		bb_ioctl(fd, HDIO_TRISTATE_HWIF, &args, "HDIO_TRISTATE_HWIF");
	}
#endif /* FEATURE_HDPARM_HDIO_TRISTATE_HWIF */
#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
	if (get_identity) {
		static struct hd_driveid id;

		if (!ioctl(fd, HDIO_GET_IDENTITY, &id))	{
			if (multcount != -1) {
				id.multsect = multcount;
				id.multsect_valid |= 1;
			} else
				id.multsect_valid &= ~1;
			dump_identity(&id);
		} else if (errno == -ENOMSG)
			printf(" no identification info available\n");
		else
			bb_perror_msg("HDIO_GET_IDENTITY");
	}

	if (get_IDentity) {
		unsigned char args1[4+512]; /* = { ... } will eat 0.5k of rodata! */

		memset(args1, 0, sizeof(args1));
		args1[0] = WIN_IDENTIFY;
		args1[3] = 1;
		if (!bb_ioctl_alt(fd, HDIO_DRIVE_CMD, args1, WIN_PIDENTIFY, "HDIO_DRIVE_CMD(identify)"))
			identify((void *)(args1 + 4));
	}
#endif
#if ENABLE_FEATURE_HDPARM_HDIO_TRISTATE_HWIF
	if (set_busstate) {
		if (get_busstate) {
			print_flag(1, "bus state", busstate);
			bus_state_value(busstate);
		}
		bb_ioctl(fd, HDIO_SET_BUSSTATE, (int *)(unsigned long)busstate, "HDIO_SET_BUSSTATE");
	}
	if (get_busstate) {
		if (!bb_ioctl(fd, HDIO_GET_BUSSTATE, &parm, "HDIO_GET_BUSSTATE")) {
			printf(fmt, "bus state", parm);
			bus_state_value(parm);
		}
	}
#endif
	if (reread_partn)
		bb_ioctl(fd, BLKRRPART, NULL, "BLKRRPART");

	if (do_ctimings)
		do_time(0, fd); /* time cache */
	if (do_timings)
		do_time(1, fd); /* time device */
	if (do_flush)
		flush_buffer_cache(fd);
	close(fd);
}

#if ENABLE_FEATURE_HDPARM_GET_IDENTITY
static int fromhex(unsigned char c)
{
	if (isdigit(c))
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - ('a' - 10));
	bb_error_msg_and_die("bad char: '%c' 0x%02x", c, c);
}

static void identify_from_stdin(void)
{
	uint16_t sbuf[256];
	unsigned char buf[1280];
	unsigned char *b = (unsigned char *)buf;
	int i;

	xread(0, buf, 1280);

	// Convert the newline-separated hex data into an identify block.

	for (i = 0; i < 256; i++) {
		int j;
		for (j = 0; j < 4; j++)
			sbuf[i] = (sbuf[i] << 4) + fromhex(*(b++));
	}

	// Parse the data.

	identify(sbuf);
}
#endif

/* busybox specific stuff */
static void parse_opts(unsigned long *get, unsigned long *set, unsigned long *value, int min, int max)
{
	if (get) {
		*get = 1;
	}
	if (optarg) {
		*set = 1;
		*value = xatol_range(optarg, min, max);
	}
}

static void parse_xfermode(int flag, unsigned long *get, unsigned long *set, int *value)
{
	if (flag) {
		*get = 1;
		if (optarg) {
			*set = ((*value = translate_xfermode(optarg)) > -1);
		}
	}
}

/*------- getopt short options --------*/
static const char hdparm_options[] = "gfu::n::p:r::m::c::k::a::B:tTh"
	USE_FEATURE_HDPARM_GET_IDENTITY("iI")
	USE_FEATURE_HDPARM_HDIO_GETSET_DMA("d::")
#ifdef HDIO_DRIVE_CMD
	"S:D:P:X:K:A:L:W:CyYzZ"
#endif
	USE_FEATURE_HDPARM_HDIO_UNREGISTER_HWIF("U:")
#ifdef HDIO_GET_QDMA
#ifdef HDIO_SET_QDMA
	"Q:"
#else
	"Q"
#endif
#endif
	USE_FEATURE_HDPARM_HDIO_DRIVE_RESET("w")
	USE_FEATURE_HDPARM_HDIO_TRISTATE_HWIF("x::b:")
	USE_FEATURE_HDPARM_HDIO_SCAN_HWIF("R:");
/*-------------------------------------*/

/* our main() routine: */
int hdparm_main(int argc, char **argv) ATTRIBUTE_NORETURN;;
int hdparm_main(int argc, char **argv) ATTRIBUTE_NORETURN;
int hdparm_main(int argc, char **argv);
int hdparm_main(int argc, char **argv)
{
	int c;
	int flagcount = 0;

	while ((c = getopt(argc, argv, hdparm_options)) >= 0) {
		flagcount++;
		if (c == 'h') bb_show_usage(); /* EXIT */
		USE_FEATURE_HDPARM_GET_IDENTITY(get_IDentity |= (c == 'I'));
		USE_FEATURE_HDPARM_GET_IDENTITY(get_identity |= (c == 'i'));
		get_geom |= (c == 'g');
		do_flush |= (c == 'f');
		if (c == 'u') parse_opts(&get_unmask, &set_unmask, &unmask, 0, 1);
		USE_FEATURE_HDPARM_HDIO_GETSET_DMA(if (c == 'd') parse_opts(&get_dma, &set_dma, &dma, 0, 9));
		if (c == 'n') parse_opts(&get_nowerr, &set_nowerr, &nowerr, 0, 1);
		parse_xfermode((c == 'p'),&noisy_piomode, &set_piomode, &piomode);
		if (c == 'r') parse_opts(&get_readonly, &set_readonly, &readonly, 0, 1);
		if (c == 'm') parse_opts(&get_mult, &set_mult, &mult, 0, INT_MAX /*32*/);
		if (c == 'c') parse_opts(&get_io32bit, &set_io32bit, &io32bit, 0, INT_MAX /*8*/);
		if (c == 'k') parse_opts(&get_keep, &set_keep, &keep, 0, 1);
		if (c == 'a') parse_opts(&get_readahead, &set_readahead, &Xreadahead, 0, INT_MAX);
		if (c == 'B') parse_opts(&get_apmmode, &set_apmmode, &apmmode, 1, 255);
		do_flush |= do_timings |= (c == 't');
		do_flush |= do_ctimings |= (c == 'T');
#ifdef HDIO_DRIVE_CMD
		if (c == 'S') parse_opts(&get_standby, &set_standby, &standby_requested, 0, INT_MAX);
		if (c == 'D') parse_opts(&get_defects, &set_defects, &defects, 0, INT_MAX);
		if (c == 'P') parse_opts(&get_prefetch, &set_prefetch, &prefetch, 0, INT_MAX);
		parse_xfermode((c == 'X'), &get_xfermode, &set_xfermode, &xfermode_requested);
		if (c == 'K') parse_opts(&get_dkeep, &set_dkeep, &prefetch, 0, 1);
		if (c == 'A') parse_opts(&get_lookahead, &set_lookahead, &lookahead, 0, 1);
		if (c == 'L') parse_opts(&get_doorlock, &set_doorlock, &doorlock, 0, 1);
		if (c == 'W') parse_opts(&get_wcache, &set_wcache, &wcache, 0, 1);
		get_powermode |= (c == 'C');
		get_standbynow = set_standbynow |= (c == 'y');
		get_sleepnow = set_sleepnow |= (c == 'Y');
		reread_partn |= (c == 'z');
		get_seagate = set_seagate |= (c == 'Z');
#endif
		USE_FEATURE_HDPARM_HDIO_UNREGISTER_HWIF(if (c == 'U') parse_opts(NULL, &unregister_hwif, &hwif, 0, INT_MAX));
#ifdef HDIO_GET_QDMA
		if (c == 'Q') {
#ifdef HDIO_SET_QDMA
			parse_opts(&get_dma_q, &set_dma_q, &dma_q, 0, INT_MAX);
#else
			parse_opts(&get_dma_q, NULL, NULL, 0, 0);
#endif
		}
#endif
		USE_FEATURE_HDPARM_HDIO_DRIVE_RESET(perform_reset = (c == 'r'));
		USE_FEATURE_HDPARM_HDIO_TRISTATE_HWIF(if (c == 'x') parse_opts(NULL, &perform_tristate, &tristate, 0, 1));
		USE_FEATURE_HDPARM_HDIO_TRISTATE_HWIF(if (c == 'b') parse_opts(&get_busstate, &set_busstate, &busstate, 0, 2));
#if ENABLE_FEATURE_HDPARM_HDIO_SCAN_HWIF
		if (c == 'R') {
			parse_opts(NULL, &scan_hwif, &hwif_data, 0, INT_MAX);
			hwif_ctrl = xatoi_u((argv[optind]) ? argv[optind] : "");
			hwif_irq  = xatoi_u((argv[optind+1]) ? argv[optind+1] : "");
			/* Move past the 2 additional arguments */
			argv += 2;
			argc -= 2;
		}
#endif
	}
	/* When no flags are given (flagcount = 0), -acdgkmnru is assumed. */
	if (!flagcount) {
		get_mult = get_io32bit = get_unmask = get_keep = get_readonly = get_readahead = get_geom = 1;
		USE_FEATURE_HDPARM_HDIO_GETSET_DMA(get_dma = 1);
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		if (ENABLE_FEATURE_HDPARM_GET_IDENTITY && !isatty(STDIN_FILENO))
			identify_from_stdin(); /* EXIT */
		else bb_show_usage();
	}

	while (argc--) {
		process_dev(*argv);
		argv++;
	}
	exit(EXIT_SUCCESS);
}
