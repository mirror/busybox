/*
** Licensed under the GPL v2, see the file LICENSE in this tarball
**
** Based on nanotop.c from floppyfw project
**
** Contact me: vda.linux@googlemail.com */

//TODO:
// simplify code
// /proc/locks
// /proc/stat:
// disk_io: (3,0):(22272,17897,410702,4375,54750)
// btime 1059401962

#include "busybox.h"
#include <time.h>

typedef unsigned long long ullong;

enum { proc_file_size = 4096 };

typedef struct proc_file {
	char *name;
	int gen;
	char *file;
} proc_file;

static proc_file proc_stat = { "/proc/stat", -1 };
static proc_file proc_loadavg = { "/proc/loadavg", -1 };
static proc_file proc_net_dev = { "/proc/net/dev", -1 };
static proc_file proc_meminfo = { "/proc/meminfo", -1 };
static proc_file proc_diskstats = { "/proc/diskstats", -1 };
// Sample #
static int gen = -1;
// Linux 2.6? (otherwise assumes 2.4)
static int is26 = 0;
static struct timeval tv;
static int delta = 1000000;
static int deltanz = 1000000;
static int need_seconds = 0;
static char *final_str = "\n";

// We depend on this being a char[], not char* - we take sizeof() of it
#define outbuf bb_common_bufsiz1
static char *cur_outbuf = outbuf;


static inline void reset_outbuf(void)
{
	cur_outbuf = outbuf;
}

static inline int outbuf_count(void)
{
	return cur_outbuf - outbuf;
}

static void print_outbuf(void)
{
	int sz = cur_outbuf - outbuf;
	if (sz > 0) {
		write(1, outbuf, sz);
		cur_outbuf = outbuf;
	}
}

static void put(const char *s)
{
	int sz = strlen(s);
	if (sz > outbuf + sizeof(outbuf) - cur_outbuf)
		sz = outbuf + sizeof(outbuf) - cur_outbuf;
	memcpy(cur_outbuf, s, sz);
	cur_outbuf += sz;
}

static void put_c(char c)
{
	if (cur_outbuf < outbuf + sizeof(outbuf))
		*cur_outbuf++ = c;
}

static void put_question_marks(int count)
{
	while (count--)
		put_c('?');
}

static int readfile_z(char *buf, int sz, const char* fname)
{
	sz = open_read_close(fname, buf, sz-1);
	if (sz < 0) {
		buf[0] = '\0';
		return 1;
	}
	buf[sz] = '\0';
	return 0;
}

static const char* get_file(proc_file *pf)
{
	if (pf->gen != gen) {
		pf->gen = gen;
		// We allocate proc_file_size bytes. This wastes memory,
		// but allows us to allocate only once (at first sample)
		// per proc file, and reuse buffer for each sample
		if (!pf->file)
			pf->file = xmalloc(proc_file_size);
		readfile_z(pf->file, proc_file_size, pf->name);
	}
	return pf->file;
}

static inline ullong read_after_slash(const char *p)
{
	p = strchr(p, '/');
	if (!p) return 0;
	return strtoull(p+1, NULL, 10);
}

enum conv_type { conv_decimal, conv_slash };

// Reads decimal values from line. Values start after key, for example:
// "cpu  649369 0 341297 4336769..." - key is "cpu" here.
// Values are stored in vec[]. arg_ptr has list of positions
// we are interested in: for example: 1,2,5 - we want 1st, 2nd and 5th value.
static int vrdval(const char* p, const char* key,
	enum conv_type conv, ullong *vec, va_list arg_ptr)
{
	int indexline;
	int indexnext;

	p = strstr(p, key);
	if (!p) return 1;

	p += strlen(key);
	indexline = 1;
	indexnext = va_arg(arg_ptr, int);
	while (1) {
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '\n' || *p == '\0') break;

		if (indexline == indexnext) { // read this value
			*vec++ = conv==conv_decimal ?
				strtoull(p, NULL, 10) :
				read_after_slash(p);
			indexnext = va_arg(arg_ptr, int);
		}
		while (*p > ' ') p++; // skip over value
		indexline++;
	}
	return 0;
}

// Parses files with lines like "cpu0 21727 0 15718 1813856 9461 10485 0 0":
// rdval(file_contents, "string_to_find", result_vector, value#, value#...)
// value# start with 1
static int rdval(const char* p, const char* key, ullong *vec, ...)
{
	va_list arg_ptr;
	int result;

	va_start(arg_ptr, vec);
	result = vrdval(p, key, conv_decimal, vec, arg_ptr);
	va_end(arg_ptr);

	return result;
}

// Parses files with lines like "... ... ... 3/148 ...."
static int rdval_loadavg(const char* p, ullong *vec, ...)
{
	va_list arg_ptr;
	int result;

	va_start(arg_ptr, vec);
	result = vrdval(p, "", conv_slash, vec, arg_ptr);
	va_end(arg_ptr);

	return result;
}

// Parses /proc/diskstats
//   1  2 3   4	 5        6(rd)  7      8     9     10(wr) 11     12 13     14
//   3  0 hda 51292 14441 841783 926052 25717 79650 843256 3029804 0 148459 3956933
//   3  1 hda1 0 0 0 0 <- ignore if only 4 fields
static int rdval_diskstats(const char* p, ullong *vec)
{
	ullong rd = 0; // to avoid "warning: 'rd' might be used uninitialized"
	int indexline = 0;
	vec[0] = 0;
	vec[1] = 0;
	while (1) {
		indexline++;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '\0') break;
		if (*p == '\n') {
			indexline = 0;
			p++;
			continue;
		}
		if (indexline == 6) {
			rd = strtoull(p, NULL, 10);
		} else if (indexline == 10) {
			vec[0] += rd;  // TODO: *sectorsize (don't know how to find out sectorsize)
			vec[1] += strtoull(p, NULL, 10);
			while (*p != '\n' && *p != '\0') p++;
			continue;
		}
		while (*p > ' ') p++; // skip over value
	}
	return 0;
}

static void scale(ullong ul)
{
	char buf[5];
	smart_ulltoa5(ul, buf);
	put(buf);
}


#define S_STAT(a) \
typedef struct a { \
	struct s_stat *next; \
	void (*collect)(struct a *s); \
	const char *label;
#define S_STAT_END(a) } a;

S_STAT(s_stat)
S_STAT_END(s_stat)

static void collect_literal(s_stat *s)
{
}

static s_stat* init_literal(void)
{
	s_stat *s = xmalloc(sizeof(s_stat));
	s->collect = collect_literal;
	return (s_stat*)s;
}

static s_stat* init_delay(const char *param)
{
	delta = strtol(param, NULL, 0)*1000;
	deltanz = delta > 0 ? delta : 1;
	need_seconds = (1000000%deltanz) != 0;
	return (s_stat*)0;
}

static s_stat* init_cr(const char *param)
{
	final_str = "\r";
	return (s_stat*)0;
}


//     user nice system idle  iowait irq  softirq (last 3 only in 2.6)
//cpu  649369 0 341297 4336769 11640 7122 1183
//cpuN 649369 0 341297 4336769 11640 7122 1183
enum { CPU_FIELDCNT = 7 };
S_STAT(cpu_stat)
	ullong old[CPU_FIELDCNT];
	int bar_sz;
	char *bar;
S_STAT_END(cpu_stat)


static void collect_cpu(cpu_stat *s)
{
	ullong data[CPU_FIELDCNT] = { 0, 0, 0, 0, 0, 0, 0 };
	unsigned frac[CPU_FIELDCNT] = { 0, 0, 0, 0, 0, 0, 0 };
	ullong all = 0;
	int norm_all = 0;
	int bar_sz = s->bar_sz;
	char *bar = s->bar;
	int i;

	if (rdval(get_file(&proc_stat), "cpu ", data, 1, 2, 3, 4, 5, 6, 7)) {
		put_question_marks(bar_sz);
		return;
	}

	for (i=0; i<CPU_FIELDCNT; i++) {
		ullong old = s->old[i];
		if (data[i] < old) old = data[i];		//sanitize
		s->old[i] = data[i];
		all += (data[i] -= old);
	}

	if (all) {
		for (i=0; i<CPU_FIELDCNT; i++) {
			ullong t = bar_sz * data[i];
			norm_all += data[i] = t / all;
			frac[i] = t % all;
		}

		while (norm_all < bar_sz) {
			unsigned max = frac[0];
			int pos = 0;
			for (i=1; i<CPU_FIELDCNT; i++) {
				if (frac[i] > max) max = frac[i], pos = i;
			}
			frac[pos] = 0;	//avoid bumping up same value twice
			data[pos]++;
			norm_all++;
		}

		memset(bar, '.', bar_sz);
		memset(bar, 'S', data[2]); bar += data[2]; //sys
		memset(bar, 'U', data[0]); bar += data[0]; //usr
		memset(bar, 'N', data[1]); bar += data[1]; //nice
		memset(bar, 'D', data[4]); bar += data[4]; //iowait
		memset(bar, 'I', data[5]); bar += data[5]; //irq
		memset(bar, 'i', data[6]); bar += data[6]; //softirq
	} else {
		memset(bar, '?', bar_sz);
	}
	put(s->bar);
}


static s_stat* init_cpu(const char *param)
{
	int sz;
	cpu_stat *s = xmalloc(sizeof(cpu_stat));
	s->collect = collect_cpu;
	sz = strtol(param, NULL, 0);
	if (sz < 10) sz = 10;
	if (sz > 1000) sz = 1000;
	s->bar = xmalloc(sz+1);
	s->bar[sz] = '\0';
	s->bar_sz = sz;
	return (s_stat*)s;
}


S_STAT(int_stat)
	ullong old;
	int no;
S_STAT_END(int_stat)

static void collect_int(int_stat *s)
{
	ullong data[1];
	ullong old;

	if (rdval(get_file(&proc_stat), "intr", data, s->no)) {
		put_question_marks(4);
		return;
	}

	old = s->old;
	if (data[0] < old) old = data[0];		//sanitize
	s->old = data[0];
	scale(data[0] - old);
}

static s_stat* init_int(const char *param)
{
	int_stat *s = xmalloc(sizeof(int_stat));
	s->collect = collect_int;
	if (param[0]=='\0') {
		s->no = 1;
	} else {
		int n = strtoul(param, NULL, 0);
		s->no = n+2;
	}
	return (s_stat*)s;
}


S_STAT(ctx_stat)
	ullong old;
S_STAT_END(ctx_stat)

static void collect_ctx(ctx_stat *s)
{
	ullong data[1];
	ullong old;

	if (rdval(get_file(&proc_stat), "ctxt", data, 1)) {
		put_question_marks(4);
		return;
	}

	old = s->old;
	if (data[0] < old) old = data[0];		//sanitize
	s->old = data[0];
	scale(data[0] - old);
}

static s_stat* init_ctx(const char *param)
{
	ctx_stat *s = xmalloc(sizeof(ctx_stat));
	s->collect = collect_ctx;
	return (s_stat*)s;
}


S_STAT(blk_stat)
	const char* lookfor;
	ullong old[2];
S_STAT_END(blk_stat)

static void collect_blk(blk_stat *s)
{
	ullong data[2];
	int i;

	if (is26) {
		i = rdval_diskstats(get_file(&proc_diskstats), data);
	} else {
		i = rdval(get_file(&proc_stat), s->lookfor, data, 1, 2);
		// Linux 2.4 reports bio in Kbytes, convert to sectors:
		data[0] *= 2;
		data[1] *= 2;
	}
	if (i) {
		put_question_marks(9);
		return;
	}

	for (i=0; i<2; i++) {
		ullong old = s->old[i];
		if (data[i] < old) old = data[i];		//sanitize
		s->old[i] = data[i];
		data[i] -= old;
	}
	scale(data[0]*512); // TODO: *sectorsize
	put_c(' ');
	scale(data[1]*512);
}

static s_stat* init_blk(const char *param)
{
	blk_stat *s = xmalloc(sizeof(blk_stat));
	s->collect = collect_blk;
	s->lookfor = "page";
	return (s_stat*)s;
}


S_STAT(fork_stat)
	ullong old;
S_STAT_END(fork_stat)

static void collect_thread_nr(fork_stat *s)
{
	ullong data[1];

	if (rdval_loadavg(get_file(&proc_loadavg), data, 4)) {
		put_question_marks(4);
		return;
	}
	scale(data[0]);
}

static void collect_fork(fork_stat *s)
{
	ullong data[1];
	ullong old;

	if (rdval(get_file(&proc_stat), "processes", data, 1)) {
		put_question_marks(4);
		return;
	}

	old = s->old;
	if (data[0] < old) old = data[0];	//sanitize
	s->old = data[0];
	scale(data[0] - old);
}

static s_stat* init_fork(const char *param)
{
	fork_stat *s = xmalloc(sizeof(fork_stat));
	if (*param == 'n') {
		s->collect = collect_thread_nr;
	} else {
		s->collect = collect_fork;
	}
	return (s_stat*)s;
}


S_STAT(if_stat)
	ullong old[4];
	const char *device;
	char *device_colon;
S_STAT_END(if_stat)

static void collect_if(if_stat *s)
{
	ullong data[4];
	int i;

	if (rdval(get_file(&proc_net_dev), s->device_colon, data, 1, 3, 9, 11)) {
		put_question_marks(10);
		return;
	}

	for (i=0; i<4; i++) {
		ullong old = s->old[i];
		if (data[i] < old) old = data[i];		//sanitize
		s->old[i] = data[i];
		data[i] -= old;
	}
	put_c(data[1] ? '*' : ' ');
	scale(data[0]);
	put_c(data[3] ? '*' : ' ');
	scale(data[2]);
}

static s_stat* init_if(const char *device)
{
	if_stat *s = xmalloc(sizeof(if_stat));

	if (!device || !device[0])
		bb_show_usage();
	s->collect = collect_if;

	s->device = device;
	s->device_colon = xmalloc(strlen(device)+2);
	strcpy(s->device_colon, device);
	strcat(s->device_colon, ":");
	return (s_stat*)s;
}


S_STAT(mem_stat)
	char opt;
S_STAT_END(mem_stat)

// "Memory" value should not include any caches.
// IOW: neither "ls -laR /" nor heavy read/write activity
//      should affect it. We'd like to also include any
//      long-term allocated kernel-side mem, but it is hard
//      to figure out. For now, bufs, cached & slab are
//      counted as "free" memory
//2.6.16:
//MemTotal:       773280 kB
//MemFree:         25912 kB - genuinely free
//Buffers:        320672 kB - cache
//Cached:         146396 kB - cache
//SwapCached:          0 kB
//Active:         183064 kB
//Inactive:       356892 kB
//HighTotal:           0 kB
//HighFree:            0 kB
//LowTotal:       773280 kB
//LowFree:         25912 kB
//SwapTotal:      131064 kB
//SwapFree:       131064 kB
//Dirty:              48 kB
//Writeback:           0 kB
//Mapped:          96620 kB
//Slab:           200668 kB - takes 7 Mb on my box fresh after boot,
//                            but includes dentries and inodes
//                            (== can take arbitrary amount of mem)
//CommitLimit:    517704 kB
//Committed_AS:   236776 kB
//PageTables:       1248 kB
//VmallocTotal:   516052 kB
//VmallocUsed:      3852 kB
//VmallocChunk:   512096 kB
//HugePages_Total:     0
//HugePages_Free:      0
//Hugepagesize:     4096 kB
static void collect_mem(mem_stat *s)
{
	ullong m_total = 0;
	ullong m_free = 0;
	ullong m_bufs = 0;
	ullong m_cached = 0;
	ullong m_slab = 0;

	if (rdval(get_file(&proc_meminfo), "MemTotal:", &m_total, 1)) {
		put_question_marks(4);
		return;
	}
	if (s->opt == 'f') {
		scale(m_total << 10);
		return;
	}

	if (rdval(proc_meminfo.file, "MemFree:", &m_free  , 1)
	 || rdval(proc_meminfo.file, "Buffers:", &m_bufs  , 1)
	 || rdval(proc_meminfo.file, "Cached:",  &m_cached, 1)
	 || rdval(proc_meminfo.file, "Slab:",    &m_slab  , 1)
	) {
		put_question_marks(4);
		return;
	}

	m_free += m_bufs + m_cached + m_slab;
	switch (s->opt) {
	case 'f':
		scale(m_free << 10); break;
	default:
		scale((m_total - m_free) << 10); break;
	}
}

static s_stat* init_mem(const char *param)
{
	mem_stat *s = xmalloc(sizeof(mem_stat));
	s->collect = collect_mem;
	s->opt = param[0];
	return (s_stat*)s;
}


S_STAT(swp_stat)
S_STAT_END(swp_stat)

static void collect_swp(swp_stat *s)
{
	ullong s_total[1];
	ullong s_free[1];
	if (rdval(get_file(&proc_meminfo), "SwapTotal:", s_total, 1)
	 || rdval(proc_meminfo.file,       "SwapFree:" , s_free,  1)
	) {
		put_question_marks(4);
		return;
	}
	scale((s_total[0]-s_free[0]) << 10);
}

static s_stat* init_swp(const char *param)
{
	swp_stat *s = xmalloc(sizeof(swp_stat));
	s->collect = collect_swp;
	return (s_stat*)s;
}


S_STAT(fd_stat)
S_STAT_END(fd_stat)

static void collect_fd(fd_stat *s)
{
	char file[4096];
	ullong data[2];

	readfile_z(file, sizeof(file), "/proc/sys/fs/file-nr");
	if (rdval(file, "", data, 1, 2)) {
		put_question_marks(4);
		return;
	}

	scale(data[0] - data[1]);
}

static s_stat* init_fd(const char *param)
{
	fd_stat *s = xmalloc(sizeof(fd_stat));
	s->collect = collect_fd;
	return (s_stat*)s;
}


S_STAT(time_stat)
	int prec;
	int scale;
S_STAT_END(time_stat)

static void collect_time(time_stat *s)
{
	char buf[sizeof("12:34:56.123456")];
	struct tm* tm;
	int us = tv.tv_usec + s->scale/2;
	time_t t = tv.tv_sec;

	if (us >= 1000000) {
		t++;
		us -= 1000000;
	}
	tm = localtime(&t);

	sprintf(buf, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
	if (s->prec)
		sprintf(buf+8, ".%0*d", s->prec, us / s->scale);
	put(buf);
}

static s_stat* init_time(const char *param)
{
	int prec;
	time_stat *s = xmalloc(sizeof(time_stat));

	s->collect = collect_time;
	prec = param[0]-'0';
	if (prec < 0) prec = 0;
	else if (prec > 6) prec = 6;
	s->prec = prec;
	s->scale = 1;
	while (prec++ < 6)
		s->scale *= 10;
	return (s_stat*)s;
}

static void collect_info(s_stat *s)
{
	gen++;
	while (s) {
		put(s->label);
		s->collect(s);
		s = s->next;
	}
}


typedef s_stat* init_func(const char *param);

static const char options[] = "ncmsfixptbdr";
static init_func* init_functions[] = {
	init_if,
	init_cpu,
	init_mem,
	init_swp,
	init_fd,
	init_int,
	init_ctx,
	init_fork,
	init_time,
	init_blk,
	init_delay,
	init_cr,
};

int nmeter_main(int argc, char* argv[])
{
	char buf[32];
	s_stat *first = NULL;
	s_stat *last = NULL;
	s_stat *s;
	char *cur, *prev;

	if (argc != 2)
		bb_show_usage();

	if (open_read_close("/proc/version", buf, sizeof(buf)) > 0)
		is26 = (strstr(buf, " 2.4.")==NULL);

	// Can use argv[1] directly, but this will mess up
	// parameters as seen by e.g. ps. Making a copy...
	cur = xstrdup(argv[1]);
	while (1) {
		char *param, *p;
		prev = cur;
again:
		cur = strchr(cur, '%');
		if (!cur)
			break;
		if (cur[1]=='%') {	// %%
			strcpy(cur, cur+1);
			cur++;
			goto again;
		}
		*cur++ = '\0';		// overwrite %
		if (cur[0] == '[') {
			// format: %[foptstring]
			cur++;
			p = strchr(options, cur[0]);
			param = cur+1;
			while (cur[0] != ']') {
				if (!cur[0])
					bb_show_usage();
				cur++;
			}
			*cur++ = '\0';	// overwrite [
		} else {
			// format: %NNNNNNf
			param = cur;
			while (cur[0] >= '0' && cur[0] <= '9')
				cur++;
			if (!cur[0])
				bb_show_usage();
			p = strchr(options, cur[0]);
			*cur++ = '\0';	// overwrite format char
		}
		if (!p)
			bb_show_usage();
		s = init_functions[p-options](param);
		if (s) {
			s->label = prev;
			s->next = 0;
			if (!first)
				first = s;
			else
				last->next = s;
			last = s;
		} else {
			// %NNNNd or %r option. remove it from string
			strcpy(prev + strlen(prev), cur);
			cur = prev;
		}
	}
	if (prev[0]) {
		s = init_literal();
		s->label = prev;
		s->next = 0;
		if (!first)
			first = s;
		else
			last->next = s;
		last = s;
	}

	// Generate first samples but do not print them, they're bogus
	collect_info(first);
	reset_outbuf();
	if (delta >= 0) {
		gettimeofday(&tv, 0);
		usleep(delta > 1000000 ? 1000000 : delta - tv.tv_usec%deltanz);
	}

	while (1) {
		gettimeofday(&tv, 0);
		collect_info(first);
		put(final_str);
		print_outbuf();

		// Negative delta -> no usleep at all
		// This will hog the CPU but you can have REALLY GOOD
		// time resolution ;)
		// TODO: detect and avoid useless updates
		// (like: nothing happens except time)
		if (delta >= 0) {
			int rem;
			// can be commented out, will sacrifice sleep time precision a bit
			gettimeofday(&tv, 0);
			if (need_seconds)
				rem = delta - ((ullong)tv.tv_sec*1000000+tv.tv_usec)%deltanz;
			else
				rem = delta - tv.tv_usec%deltanz;
			// Sometimes kernel wakes us up just a tiny bit earlier than asked
			// Do not go to very short sleep in this case
			if (rem < delta/128) {
				rem += delta;
			}
			usleep(rem);
		}
	}

	return 0;
}
