/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include "libbb.h"

file_headers_t *append_archive_list(file_headers_t *head, file_headers_t *tail_entry)
{
	file_headers_t *tail_ptr;
	file_headers_t *new_node = (file_headers_t *) malloc(sizeof(file_headers_t));

	*new_node = *tail_entry;
	new_node->next = NULL;

	if (head->name == NULL) {
		head = new_node;
	} else {
		tail_ptr = head;
		while(tail_ptr->next != NULL) {
			tail_ptr = tail_ptr->next;
		}
		tail_ptr->next = new_node;
	}

	return(head);
}