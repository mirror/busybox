/* vi: set sw=4 ts=4: */

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

enum dump_vflag_t { ALL, DUP, FIRST, WAIT };	/* -v values */

typedef struct FS FS;

typedef struct dumper_t {
	off_t dump_skip;                /* bytes to skip */
	int dump_length;                /* max bytes to read */
	smallint dump_vflag;            /*enum dump_vflag_t*/
	const char *eofstring;
	FS *fshead;
} dumper_t;

dumper_t* alloc_dumper(void) FAST_FUNC;
extern void bb_dump_add(dumper_t *dumper, const char *fmt) FAST_FUNC;
extern int bb_dump_dump(dumper_t *dumper, char **argv) FAST_FUNC;

POP_SAVED_FUNCTION_VISIBILITY
