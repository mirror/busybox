/* vi: set sw=4 ts=4: */

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility push(hidden)
#endif

#define	F_IGNORE	0x01		/* %_A */
#define	F_SETREP	0x02		/* rep count set, not default */
#define	F_ADDRESS	0x001		/* print offset */
#define	F_BPAD		0x002		/* blank pad */
#define	F_C		0x004		/* %_c */
#define	F_CHAR		0x008		/* %c */
#define	F_DBL		0x010		/* %[EefGf] */
#define	F_INT		0x020		/* %[di] */
#define	F_P		0x040		/* %_p */
#define	F_STR		0x080		/* %s */
#define	F_U		0x100		/* %_u */
#define	F_UINT		0x200		/* %[ouXx] */
#define	F_TEXT		0x400		/* no conversions */

enum _vflag { ALL, DUP, FIRST, WAIT };	/* -v values */

typedef struct _pr {
	struct _pr *nextpr;		/* next print unit */
	unsigned int flags;			/* flag values */
	int bcnt;			/* byte count */
	char *cchar;			/* conversion character */
	char *fmt;			/* printf format */
	char *nospace;			/* no whitespace version */
} PR;

typedef struct _fu {
	struct _fu *nextfu;		/* next format unit */
	struct _pr *nextpr;		/* next print unit */
	unsigned int flags;			/* flag values */
	int reps;			/* repetition count */
	int bcnt;			/* byte count */
	char *fmt;			/* format string */
} FU;

typedef struct _fs {			/* format strings */
	struct _fs *nextfs;		/* linked list of format strings */
	struct _fu *nextfu;		/* linked list of format units */
	int bcnt;
} FS;

extern void bb_dump_add(const char *fmt);
extern int bb_dump_dump(char **argv);
extern int bb_dump_size(FS * fs);

extern FS *bb_dump_fshead;		/* head of format strings */
extern int bb_dump_blocksize;				/* data block size */
extern int bb_dump_length;			/* max bytes to read */
extern enum _vflag bb_dump_vflag;
extern off_t bb_dump_skip;                      /* bytes to skip */

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility pop
#endif
