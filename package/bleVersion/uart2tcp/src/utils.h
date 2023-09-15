#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>

typedef struct buffer {
	size_t idx;
	size_t len;
	size_t capacity;
	char *data;
} buffer_t;

#ifndef container_of
#define container_of(ptr, type, member)                             \
	({                                                              \
		const __typeof__(((type *) NULL)->member) *__mptr = (ptr);  \
		(type *) ((char *) __mptr - offsetof(type, member));        \
	})
#endif

#define ss_free(ptr)   \
	do {               \
		free(ptr);     \
		ptr = NULL;    \
	} while (0)

void *ss_malloc(size_t size);

int balloc(buffer_t *ptr, size_t capacity);
void bfree(buffer_t *ptr);

int download_file(const char *url, const char *out_file, int timeout_ms);
void daemonize(const char *path);

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);

#endif
