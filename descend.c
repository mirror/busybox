#include "internal.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>


static int 
noDots(const struct dirent * e)
{
	if ( e->d_name[0] == '.'
	 && (e->d_name[1] == '\0'
	  || (e->d_name[1] == '.' && e->d_name[2] == '\0')) )
		return 0;
	else
		return 1;
}

extern int
descend(
 struct FileInfo *oldInfo
,int 		(*function)(const struct FileInfo * i))
{
	char			pathname[1024];
	struct dirent * *	names;
	struct dirent * *	n;
	int			length;
	char *			filename;
	int			status = 0;
	int			count;

	if ( *oldInfo->source == '\0' ) {
		errno = EINVAL;
		return -1;
	}

	if ( oldInfo->stat.st_dev == 0
	 && oldInfo->stat.st_ino == 0
	 && oldInfo->stat.st_mode == 0 ) {
		if ( lstat(oldInfo->source, &oldInfo->stat) != 0 )
			return -1;
		oldInfo->isSymbolicLink = ((oldInfo->stat.st_mode & S_IFMT) == S_IFLNK);

		if ( oldInfo->isSymbolicLink )
			if ( stat(oldInfo->source, &oldInfo->stat) != 0 )
				memset((void *)&oldInfo->stat, 0, sizeof(oldInfo->stat));
	}

	if ( !oldInfo->processDirectoriesAfterTheirContents ) {
		if ( function )
			status = (*function)(oldInfo);
		if ( status == 0 )
			status = post_process(oldInfo);
	}

	if ( (count = scandir(oldInfo->source, &names, noDots, alphasort)) < 0 )
		return -1;

	length = strlen(oldInfo->source);
	if ( oldInfo->source[length-1] == '/' )
		length--;

	memcpy(pathname, oldInfo->source, length+1);
	pathname[length] = '/';
	filename = &pathname[length+1];
	
	n = names;
	while ( count-- > 0 ) {
		struct FileInfo		i = *oldInfo;

		strcpy(filename, (*n)->d_name);
		free(*n++);

		if ( lstat(pathname, &i.stat) != 0 && errno != ENOENT ) {
			fprintf(stderr, "Can't stat %s: %s\n", pathname, strerror(errno));
			return -1;
		}
		i.isSymbolicLink = ((i.stat.st_mode & S_IFMT) == S_IFLNK);

		if ( i.isSymbolicLink )
			if ( stat(pathname, &i.stat) != 0 )
				memset((void *)&i.stat, 0, sizeof(i.stat));

		i.source = pathname;

		if ( i.dyadic ) {
			char	d[1024];

			i.destination = join_paths(d, i.destination, &i.source[i.directoryLength]);
		}
		else
			i.destination = i.source;

		 if ( !i.isSymbolicLink && (i.stat.st_mode & S_IFMT) == S_IFDIR )
			status = descend(&i, function);
		else {
			if ( function )
				status = (*function)(&i);
			if ( status == 0 )
				status = post_process(&i);
		}

		if ( !i.processDirectoriesAfterTheirContents
		 && status == 0
		 && (i.stat.st_mode & S_IFMT) == S_IFDIR )
			descend(&i, function);

		if ( status != 0 && !i.force ) {
			while ( count-- > 0 )
				free(*n++);
			break;
		}
	}
	free(names);

	if ( oldInfo->processDirectoriesAfterTheirContents ) {
		if ( function )
			status = (*function)(oldInfo);
		if ( status == 0 )
			status = post_process(oldInfo);
	}

	return status;
}
