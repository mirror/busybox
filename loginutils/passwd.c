/* vi: set sw=4 ts=4: */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include <syslog.h>
#include <time.h>
#include <sys/resource.h>
#include <errno.h>

#include "busybox.h"

static char crypt_passwd[128];

static int create_backup(const char *backup, FILE * fp);
static int new_password(const struct passwd *pw, int amroot, int algo);
static void set_filesize_limit(int blocks);


int get_algo(char *a)
{
	int x = 1;					/* standard: MD5 */

	if (strcasecmp(a, "des") == 0)
		x = 0;
	return x;
}


extern int update_passwd(const struct passwd *pw, char *crypt_pw)
{
	char filename[1024];
	char buf[1025];
	char buffer[80];
	char username[32];
	char *pw_rest;
	int has_shadow = 0;
	int mask;
	int continued;
	FILE *fp;
	FILE *out_fp;
	struct stat sb;
	struct flock lock;

	if (access(bb_path_shadow_file, F_OK) == 0) {
		has_shadow = 1;
	}
	if (has_shadow) {
		snprintf(filename, sizeof filename, "%s", bb_path_shadow_file);
	} else {
		snprintf(filename, sizeof filename, "%s", bb_path_passwd_file);
	}

	if (((fp = fopen(filename, "r+")) == 0) || (fstat(fileno(fp), &sb))) {
		/* return 0; */
		return 1;
	}

	/* Lock the password file before updating */
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	if (fcntl(fileno(fp), F_SETLK, &lock) < 0) {
		fprintf(stderr, "%s: %s\n", filename, strerror(errno));
		return 1;
	}
	lock.l_type = F_UNLCK;

	snprintf(buf, sizeof buf, "%s-", filename);
	if (create_backup(buf, fp)) {
		fcntl(fileno(fp), F_SETLK, &lock);
		fclose(fp);
		return 1;
	}
	snprintf(buf, sizeof buf, "%s+", filename);
	mask = umask(0777);
	out_fp = fopen(buf, "w");
	umask(mask);
	if ((!out_fp) || (fchmod(fileno(out_fp), sb.st_mode & 0777))
		|| (fchown(fileno(out_fp), sb.st_uid, sb.st_gid))) {
		fcntl(fileno(fp), F_SETLK, &lock);
		fclose(fp);
		fclose(out_fp);
		return 1;
	}

	continued = 0;
	snprintf(username, sizeof username, "%s:", pw->pw_name);
	rewind(fp);
	while (!feof(fp)) {
		fgets(buffer, sizeof buffer, fp);
		if (!continued) {		// Check to see if we're updating this line.
			if (strncmp(username, buffer, strlen(username)) == 0) {	// we have a match.
				pw_rest = strchr(buffer, ':');
				*pw_rest++ = '\0';
				pw_rest = strchr(pw_rest, ':');
				fprintf(out_fp, "%s:%s%s", buffer, crypt_pw, pw_rest);
			} else {
				fputs(buffer, out_fp);
			}
		} else {
			fputs(buffer, out_fp);
		}
		if (buffer[strlen(buffer) - 1] == '\n') {
			continued = 0;
		} else {
			continued = 1;
		}
		bzero(buffer, sizeof buffer);
	}

	if (fflush(out_fp) || fsync(fileno(out_fp)) || fclose(out_fp)) {
		unlink(buf);
		fcntl(fileno(fp), F_SETLK, &lock);
		fclose(fp);
		return 1;
	}
	if (rename(buf, filename) < 0) {
		fcntl(fileno(fp), F_SETLK, &lock);
		fclose(fp);
		return 1;
	} else {
		fcntl(fileno(fp), F_SETLK, &lock);
		fclose(fp);
		return 0;
	}
}


extern int passwd_main(int argc, char **argv)
{
	int amroot;
	char *cp;
	char *np;
	char *name;
	char *myname;
	int flag;
	int algo = 1;				/* -a - password algorithm */
	int lflg = 0;				/* -l - lock account */
	int uflg = 0;				/* -u - unlock account */
	int dflg = 0;				/* -d - delete password */
	const struct passwd *pw;
	unsigned short ruid;

#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	const struct spwd *sp;
#endif							/* CONFIG_FEATURE_SHADOWPASSWDS */
	amroot = (getuid() == 0);
	openlog("passwd", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_AUTH);
	while ((flag = getopt(argc, argv, "a:dlu")) != EOF) {
		switch (flag) {
		case 'a':
			algo = get_algo(optarg);
			break;
		case 'd':
			dflg++;
			break;
		case 'l':
			lflg++;
			break;
		case 'u':
			uflg++;
			break;
		default:
			bb_show_usage();
		}
	}
	ruid = getuid();
	pw = (struct passwd *) getpwuid(ruid);
	if (!pw) {
               bb_error_msg_and_die("Cannot determine your user name.");
	}
	myname = (char *) bb_xstrdup(pw->pw_name);
	if (optind < argc) {
		name = argv[optind];
	} else {
		name = myname;
	}
	if ((lflg || uflg || dflg) && (optind >= argc || !amroot)) {
		bb_show_usage();
	}
	pw = getpwnam(name);
	if (!pw) {
		bb_error_msg_and_die("Unknown user %s\n", name);
	}
	if (!amroot && pw->pw_uid != getuid()) {
		syslog(LOG_WARNING, "can't change pwd for `%s'", name);
		bb_error_msg_and_die("Permission denied.\n");
	}
#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	sp = getspnam(name);
	if (!sp) {
		sp = (struct spwd *) pwd_to_spwd(pw);
	}
	cp = sp->sp_pwdp;
	np = sp->sp_namp;
#else
	cp = pw->pw_passwd;
	np = name;
#endif							/* CONFIG_FEATURE_SHADOWPASSWDS */

	safe_strncpy(crypt_passwd, cp, sizeof(crypt_passwd));
	if (!(dflg || lflg || uflg)) {
		if (!amroot) {
			if (cp[0] == '!') {
				syslog(LOG_WARNING, "password locked for `%s'", np);
				bb_error_msg_and_die( "The password for `%s' cannot be changed.\n", np);
			}
		}
		printf("Changing password for %s\n", name);
		if (new_password(pw, amroot, algo)) {
			bb_error_msg_and_die( "The password for %s is unchanged.\n", name);
		}
	} else if (lflg) {
		if (crypt_passwd[0] != '!') {
			memmove(&crypt_passwd[1], crypt_passwd,
					sizeof crypt_passwd - 1);
			crypt_passwd[sizeof crypt_passwd - 1] = '\0';
			crypt_passwd[0] = '!';
		}
	} else if (uflg) {
		if (crypt_passwd[0] == '!') {
			memmove(crypt_passwd, &crypt_passwd[1],
					sizeof crypt_passwd - 1);
		}
	} else if (dflg) {
		crypt_passwd[0] = '\0';
	}
	set_filesize_limit(30000);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	umask(077);
	if (setuid(0)) {
		syslog(LOG_ERR, "can't setuid(0)");
		bb_error_msg_and_die( "Cannot change ID to root.\n");
	}
	if (!update_passwd(pw, crypt_passwd)) {
		syslog(LOG_INFO, "password for `%s' changed by user `%s'", name,
			   myname);
		printf("Password changed.\n");
	} else {
		syslog(LOG_WARNING, "an error occurred updating the password file");
		bb_error_msg_and_die("An error occurred updating the password file.\n");
	}
	return (0);
}



static int create_backup(const char *backup, FILE * fp)
{
	struct stat sb;
	struct utimbuf ub;
	FILE *bkfp;
	int c, mask;

	if (fstat(fileno(fp), &sb))
		/* return -1; */
		return 1;

	mask = umask(077);
	bkfp = fopen(backup, "w");
	umask(mask);
	if (!bkfp)
		/* return -1; */
		return 1;

	/* TODO: faster copy, not one-char-at-a-time.  --marekm */
	rewind(fp);
	while ((c = getc(fp)) != EOF) {
		if (putc(c, bkfp) == EOF)
			break;
	}
	if (c != EOF || fflush(bkfp)) {
		fclose(bkfp);
		/* return -1; */
		return 1;
	}
	if (fclose(bkfp))
		/* return -1; */
		return 1;

	ub.actime = sb.st_atime;
	ub.modtime = sb.st_mtime;
	utime(backup, &ub);
	return 0;
}

static int i64c(int i)
{
	if (i <= 0)
		return ('.');
	if (i == 1)
		return ('/');
	if (i >= 2 && i < 12)
		return ('0' - 2 + i);
	if (i >= 12 && i < 38)
		return ('A' - 12 + i);
	if (i >= 38 && i < 63)
		return ('a' - 38 + i);
	return ('z');
}

static char *crypt_make_salt(void)
{
	time_t now;
	static unsigned long x;
	static char result[3];

	time(&now);
	x += now + getpid() + clock();
	result[0] = i64c(((x >> 18) ^ (x >> 6)) & 077);
	result[1] = i64c(((x >> 12) ^ x) & 077);
	result[2] = '\0';
	return result;
}


static int new_password(const struct passwd *pw, int amroot, int algo)
{
	char *clear;
	char *cipher;
	char *cp;
	char orig[200];
	char pass[200];
	time_t start, now;

	if (!amroot && crypt_passwd[0]) {
		if (!(clear = getpass("Old password:"))) {
			/* return -1; */
			return 1;
		}
		cipher = pw_encrypt(clear, crypt_passwd);
		if (strcmp(cipher, crypt_passwd) != 0) {
			syslog(LOG_WARNING, "incorrect password for `%s'",
				   pw->pw_name);
			time(&start);
			now = start;
			while (difftime(now, start) < FAIL_DELAY) {
				sleep(FAIL_DELAY);
				time(&now);
			}
			fprintf(stderr, "Incorrect password.\n");
			/* return -1; */
			return 1;
		}
		safe_strncpy(orig, clear, sizeof(orig));
		bzero(clear, strlen(clear));
		bzero(cipher, strlen(cipher));
	} else {
		orig[0] = '\0';
	}
	if (! (cp=getpass("Enter the new password (minimum of 5, maximum of 8 characters)\n"
					  "Please use a combination of upper and lower case letters and numbers.\n"
					  "Enter new password: ")))
	{
		bzero(orig, sizeof orig);
		/* return -1; */
		return 1;
	}
	safe_strncpy(pass, cp, sizeof(pass));
	bzero(cp, strlen(cp));
	/* if (!obscure(orig, pass, pw)) { */
	if (obscure(orig, pass, pw)) {
		if (amroot) {
			printf("\nWarning: weak password (continuing).\n");
		} else {
			/* return -1; */
			return 1;
		}
	}
	if (!(cp = getpass("Re-enter new password: "))) {
		bzero(orig, sizeof orig);
		/* return -1; */
		return 1;
	}
	if (strcmp(cp, pass)) {
		fprintf(stderr, "Passwords do not match.\n");
		/* return -1; */
		return 1;
	}
	bzero(cp, strlen(cp));
	bzero(orig, sizeof(orig));

	if (algo == 1) {
		cp = pw_encrypt(pass, "$1$");
	} else
		cp = pw_encrypt(pass, crypt_make_salt());
	bzero(pass, sizeof pass);
	safe_strncpy(crypt_passwd, cp, sizeof(crypt_passwd));
	return 0;
}

static void set_filesize_limit(int blocks)
{
	struct rlimit rlimit_fsize;

	rlimit_fsize.rlim_cur = rlimit_fsize.rlim_max = 512L * blocks;
	setrlimit(RLIMIT_FSIZE, &rlimit_fsize);
}
