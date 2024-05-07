#ifndef _FB_BUFFER_H
#define _FB_BUFFER_H

#include "filebench.h"

struct buf_segment {
	uint64_t start; // starting byte
	size_t size;
};

struct buffer {
	struct buffer *next; /* Next in list */

	char *name;
	char *data_path; // path to file that's read to buffer

	struct buf_segment *segments; // segments buffer is split into
	uint64_t segments_amount;
	uint64_t segment_head; // index of segment that's used now

	size_t ism_offset; // offset into ISM where buffer is allocated
	size_t size;	   // buffer size
};

struct buffer *buffer_define(char *name, char *data_path, char *segments_path);
int buffer_allocate_all();
struct buffer *buffer_find_by_name(char *name);

#endif /* _FB_BUFFER_H */
