#if ENABLE_FEATURE_AIX_LABEL
/*
 * Copyright (C) Andreas Neuper, Sep 1998.
 *      This file may be redistributed under
 *      the terms of the GNU Public License.
 */

typedef struct {
	unsigned int   magic;        /* expect AIX_LABEL_MAGIC */
	unsigned int   fillbytes1[124];
	unsigned int   physical_volume_id;
	unsigned int   fillbytes2[124];
} aix_partition;

#define AIX_LABEL_MAGIC         0xc9c2d4c1
#define AIX_LABEL_MAGIC_SWAPPED 0xc1d4c2c9
#define AIX_INFO_MAGIC          0x00072959
#define AIX_INFO_MAGIC_SWAPPED  0x59290700

#define aixlabel ((aix_partition *)MBRbuffer)


/*
  Changes:
  * 1999-03-20 Arnaldo Carvalho de Melo <acme@conectiva.com.br>
  *     Internationalization
  *
  * 2003-03-20 Phillip Kesling <pkesling@sgi.com>
  *      Some fixes
*/

static int aix_other_endian;
static short aix_volumes = 1;

/*
 * only dealing with free blocks here
 */

static void
aix_info(void)
{
	puts(
		_("\n\tThere is a valid AIX label on this disk.\n"
		"\tUnfortunately Linux cannot handle these\n"
		"\tdisks at the moment.  Nevertheless some\n"
		"\tadvice:\n"
		"\t1. fdisk will destroy its contents on write.\n"
		"\t2. Be sure that this disk is NOT a still vital\n"
		"\t   part of a volume group. (Otherwise you may\n"
		"\t   erase the other disks as well, if unmirrored.)\n"
		"\t3. Before deleting this physical volume be sure\n"
		"\t   to remove the disk logically from your AIX\n"
		"\t   machine.  (Otherwise you become an AIXpert).")
	);
}

static int
check_aix_label(void)
{
	if (aixlabel->magic != AIX_LABEL_MAGIC &&
		aixlabel->magic != AIX_LABEL_MAGIC_SWAPPED) {
		current_label_type = 0;
		aix_other_endian = 0;
		return 0;
	}
	aix_other_endian = (aixlabel->magic == AIX_LABEL_MAGIC_SWAPPED);
	update_units();
	current_label_type = label_aix;
	partitions = 1016;
	aix_volumes = 15;
	aix_info();
	/*aix_nolabel();*/              /* %% */
	/*aix_label = 1;*/              /* %% */
	return 1;
}
#endif  /* AIX_LABEL */
