#include "filebench.h"

/*
 * This routine implements the 'define buffer' calls found in a .f
 * workload, such as in the following example:
 * define buffer name=mybuffer,path=/opt/data/buf.txt,size=10000
 */
struct buffer *
buffer_define(char *name, char *path, size_t size)
{
	struct buffer *buf = ipc_malloc(FILEBENCH_BUFFER);
	if (!buf) {
		filebench_log(LOG_ERROR, "Can't allocate buffer %s", name);
		return NULL;
	}

	filebench_log(LOG_DEBUG_IMPL, "Defining buffer %s", name);

	buf->name = name;
	buf->path = path;
	buf->size = size;

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
	filebench_shm->ism_required += size;

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

	/* Path is not specified */
	if (!buf->path)
		return FILEBENCH_OK;

	FILE *file;
	if (!(file = fopen(buf->path, "r"))) {
		filebench_log(LOG_ERROR, "Failed to open file %s to allocate buffer",
					  buf->path);
		return FILEBENCH_ERROR;
	}
	if (fread(filebench_ism + ism_offset, 1, buf->size, file) != buf->size) {
		filebench_log(LOG_ERROR,
					  "Failed to read %d bytes from file %s to buffer",
					  buf->size, buf->path);
		return FILEBENCH_ERROR;
	}
	fclose(file);

	return FILEBENCH_OK;
}

/* Allocates all buffers in global list */
int
buffer_allocate_all()
{
	int ret = 0;

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
