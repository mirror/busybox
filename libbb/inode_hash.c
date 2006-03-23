/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.
 * If you wrote this, please acknowledge your work.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libbb.h"

#define HASH_SIZE	311		/* Should be prime */
#define hash_inode(i)	((i) % HASH_SIZE)

typedef struct ino_dev_hash_bucket_struct {
  struct ino_dev_hash_bucket_struct *next;
  ino_t ino;
  dev_t dev;
  char name[1];
} ino_dev_hashtable_bucket_t;

static ino_dev_hashtable_bucket_t *ino_dev_hashtable[HASH_SIZE];

/*
 * Return 1 if statbuf->st_ino && statbuf->st_dev are recorded in
 * `ino_dev_hashtable', else return 0
 *
 * If NAME is a non-NULL pointer to a character pointer, and there is
 * a match, then set *NAME to the value of the name slot in that
 * bucket.
 */
int is_in_ino_dev_hashtable(const struct stat *statbuf, char **name)
{
	ino_dev_hashtable_bucket_t *bucket;

	bucket = ino_dev_hashtable[hash_inode(statbuf->st_ino)];
	while (bucket != NULL) {
	  if ((bucket->ino == statbuf->st_ino) &&
		  (bucket->dev == statbuf->st_dev))
	  {
		if (name) *name = bucket->name;
		return 1;
	  }
	  bucket = bucket->next;
	}
	return 0;
}

/* Add statbuf to statbuf hash table */
void add_to_ino_dev_hashtable(const struct stat *statbuf, const char *name)
{
	int i;
	size_t s;
	ino_dev_hashtable_bucket_t *bucket;

	i = hash_inode(statbuf->st_ino);
	s = name ? strlen(name) : 0;
	bucket = xmalloc(sizeof(ino_dev_hashtable_bucket_t) + s);
	bucket->ino = statbuf->st_ino;
	bucket->dev = statbuf->st_dev;
	if (name)
		strcpy(bucket->name, name);
	else
		bucket->name[0] = '\0';
	bucket->next = ino_dev_hashtable[i];
	ino_dev_hashtable[i] = bucket;
}

#ifdef CONFIG_FEATURE_CLEAN_UP
/* Clear statbuf hash table */
void reset_ino_dev_hashtable(void)
{
	int i;
	ino_dev_hashtable_bucket_t *bucket;

	for (i = 0; i < HASH_SIZE; i++) {
		while (ino_dev_hashtable[i] != NULL) {
			bucket = ino_dev_hashtable[i]->next;
			free(ino_dev_hashtable[i]);
			ino_dev_hashtable[i] = bucket;
		}
	}
}
#endif


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
