#include "filebench.h"
#include "utils.h"

/* Parses buffer segments from file. Sets relevant fields of buffer structure */
static int
parse_segments(char *segments_path, struct buffer *buf)
{
	int ret = FILEBENCH_ERROR;

	char *fcontents = read_entire_file(segments_path);
	if (!fcontents) {
		return FILEBENCH_ERROR;
	}

	uint64_t lines = 0;
	for (uint64_t i = 0; fcontents[i] != 0; i++) {
		if (fcontents[i] == '\n')
			lines++;
	}

	struct buf_segment *segments = ipc_buf_segments_alloc(lines);
	if (!segments) {
		filebench_log(LOG_ERROR, "Allocation failed for buffer segments");
		goto free_contents;
	}

	uint64_t max_seg_end = 0;
	char *str = fcontents;
	for (uint64_t i = 0; i < lines; i++) {
		char *line_right = strsep(&str, "\n");
		char *line_left = strsep(&line_right, ":");

		if (line_right == NULL) {
			filebench_log(LOG_ERROR, "Failed to parse segments file");
			goto free_contents;
		}

		char *endptr;
		uint64_t seg_start = strtoull(line_left, &endptr, 10);
		if (*endptr != '\0') {
			filebench_log(LOG_ERROR, "Failed to parse segment's starting byte");
			goto free_contents;
		}

		uint64_t seg_end = strtoull(line_right, &endptr, 10);
		if (*endptr != '\0') {
			filebench_log(LOG_ERROR, "Failed to parse segment's ending byte");
			goto free_contents;
		}

		if (seg_start > seg_end) {
			filebench_log(LOG_ERROR, "Invalid segment %llu:%llu", seg_start,
						  seg_end);
			goto free_contents;
		}

		if (seg_end > max_seg_end)
			max_seg_end = seg_end;

		segments[i] = (struct buf_segment){.start = seg_start,
										   .size = seg_end - seg_start + 1};
	}

	buf->segments = segments;
	buf->segments_amount = lines;
	buf->segment_head = 0;
	buf->size = max_seg_end;
	ret = FILEBENCH_OK;

free_contents:
	free(fcontents);
	return ret;
}

/*
 * This routine implements the 'define buffer' calls found in a .f
 * workload, such as in the following example:
 * define buffer name=mybuffer,path=/opt/data/buf.txt,size=10000
 */
struct buffer *
buffer_define(char *name, char *data_path, char *segments_path)
{
	struct buffer *buf = ipc_malloc(FILEBENCH_BUFFER);
	if (!buf) {
		filebench_log(LOG_ERROR, "Can't allocate buffer %s", name);
		return NULL;
	}

	filebench_log(LOG_DEBUG_IMPL, "Defining buffer %s", name);

	buf->name = name;
	buf->data_path = data_path;

	if (parse_segments(segments_path, buf)) {
		return NULL;
	}

	/* Add buffer to global list */
	(void)ipc_mutex_lock(&filebench_shm->shm_buffer_lock);
	if (filebench_shm->shm_bufferlist == NULL) {
		filebench_shm->shm_bufferlist = buf;
		buf->next = NULL;
	} else {
		buf->next = filebench_shm->shm_bufferlist;
		filebench_shm->shm_bufferlist = buf;
	}
	(void)ipc_mutex_unlock(&filebench_shm->shm_buffer_lock);

	// increase amount of required ISM
	// as buffer's data will be allocted there
	filebench_shm->ism_required += buf->size;

	return buf;
}

static int
buffer_allocate(struct buffer *buf)
{
	filebench_log(LOG_INFO, "Allocating buffer %s", buf->name);

	ssize_t ism_offset = ipc_ismmalloc(buf->size);
	if (ism_offset == -1) {
		filebench_log(LOG_ERROR, "Can't allocate data for buffer %s",
					  buf->name);
		return FILEBENCH_ERROR;
	}
	buf->ism_offset = ism_offset;

	FILE *file = fopen(buf->data_path, "rb");
	if (!file) {
		filebench_log(LOG_ERROR, "Failed to open file %s to allocate buffer %s",
					  buf->data_path, buf->name);
		return FILEBENCH_ERROR;
	}
	if (fread(filebench_ism + ism_offset, 1, buf->size, file) != buf->size) {
		(void)fclose(file);
		filebench_log(LOG_ERROR,
					  "Failed to read %d bytes from file %s to buffer %s",
					  buf->size, buf->data_path, buf->name);
		return FILEBENCH_ERROR;
	}
	(void)fclose(file);

	return FILEBENCH_OK;
}

/*
 * Allocates memory for all buffers
 * and reads data files into them.
 */
int
buffer_allocate_all()
{
	int ret;

	(void)ipc_mutex_lock(&filebench_shm->shm_buffer_lock);

	struct buffer *buf = filebench_shm->shm_bufferlist;
	while (buf) {
		if ((ret = buffer_allocate(buf))) {
			return ret;
		}
		buf = buf->next;
	}

	(void)ipc_mutex_unlock(&filebench_shm->shm_buffer_lock);

	return FILEBENCH_OK;
}

/* Finds buffer with specified name in global list */
struct buffer *
buffer_find_by_name(char *name)
{
	(void)ipc_mutex_lock(&filebench_shm->shm_buffer_lock);

	struct buffer *buf = filebench_shm->shm_bufferlist;
	while (buf) {
		if (!strcmp(name, buf->name)) {
			break;
		}
		buf = buf->next;
	}

	(void)ipc_mutex_unlock(&filebench_shm->shm_buffer_lock);

	return buf;
}
