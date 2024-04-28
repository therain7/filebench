#ifndef _FB_BUFFER_H
#define _FB_BUFFER_H

#include "filebench.h"

struct buffer {
	struct buffer *next; /* Next in list */

	char *name;
	char *path;
	size_t size;

	size_t ism_offset;
};

struct buffer *buffer_define(char *name, char *path, size_t size);
int buffer_allocate_all();
struct buffer *buffer_find_by_name(char *name);

#endif /* _FB_BUFFER_H */
