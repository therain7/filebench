#ifndef _FB_BUFFER_H
#define _FB_BUFFER_H

#include "filebench.h"

typedef struct buffer {
	struct buffer *next; /* Next in list */

	char *name;
	char *path;
	size_t size;

	caddr_t data;
} buffer_t;

buffer_t *buffer_define(char *name, char *path, size_t size);
int buffer_allocate_all();
buffer_t *buffer_find_by_name(char *name);

#endif /* _FB_BUFFER_H */
