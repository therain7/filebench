#include "filebench.h"

/*
 * This routine implements the 'define buffer' calls found in a .f
 * workload, such as in the following example:
 * define buffer name=mybuffer,path=/opt/data/buf.txt,size=10000
 */
buffer_t *
buffer_define(char *name, char *path, size_t size)
{
	buffer_t *buffer = ipc_malloc(FILEBENCH_BUFFER);
	if (!buffer) {
		filebench_log(LOG_ERROR, "Can't allocate buffer %s", name);
		return NULL;
	}

	filebench_log(LOG_DEBUG_IMPL, "Defining buffer %s", name);

	buffer->name = name;
	buffer->path = path;
	buffer->size = size;

	/* Add buffer to global list */
	(void)ipc_mutex_lock(&filebench_shm->shm_buffer_lock);
	if (filebench_shm->shm_bufferlist == NULL) {
		filebench_shm->shm_bufferlist = buffer;
		buffer->next = NULL;
	} else {
		buffer->next = filebench_shm->shm_bufferlist;
		filebench_shm->shm_bufferlist = buffer;
	}
	(void)ipc_mutex_unlock(&filebench_shm->shm_buffer_lock);

	/* Increase amount of required shared memory
	 * as buffer's data will be allocted there */
	filebench_shm->shm_required += size;

	return buffer;
}

static int
buffer_allocate(buffer_t *buf)
{
	filebench_log(LOG_INFO, "Allocating buffer %s", buf->name);

	caddr_t data = ipc_ismmalloc(buf->size);
	if (!data) {
		filebench_log(LOG_ERROR, "Can't allocate data for buffer %s",
					  buf->name);
		return FILEBENCH_ERROR;
	}

	FILE *file;
	if (!(file = fopen(buf->path, "r"))) {
		filebench_log(LOG_ERROR, "Failed to open file %s to allocate buffer",
					  buf->path);
		return FILEBENCH_ERROR;
	}
	if (fread(data, 1, buf->size, file) != buf->size) {
		filebench_log(LOG_ERROR,
					  "Failed to read %d bytes from file %s to buffer",
					  buf->size, buf->path);
		return FILEBENCH_ERROR;
	}
	fclose(file);

	buf->data = data;

	return FILEBENCH_OK;
}

/* Allocates all buffers in global list */
int
buffer_allocate_all()
{
	int ret = 0;

	(void)ipc_mutex_lock(&filebench_shm->shm_buffer_lock);

	buffer_t *buf = filebench_shm->shm_bufferlist;
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
buffer_t *
buffer_find_by_name(char *name)
{
	(void)ipc_mutex_lock(&filebench_shm->shm_buffer_lock);

	buffer_t *buf = filebench_shm->shm_bufferlist;
	while (buf) {
		if (!strcmp(name, buf->name)) {
			break;
		}
		buf = buf->next;
	}

	(void)ipc_mutex_unlock(&filebench_shm->shm_buffer_lock);

	return buf;
}
