#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <syslog.h>

#define TIME_FORMAT "%F %T"

extern int use_syslog;

#define USE_SYSLOG(_ident)                                      \
    do {                                                        \
        use_syslog = 1;                                         \
        openlog((_ident), LOG_CONS | LOG_PID, LOG_DAEMON);      \
    } while (0)

#define LOGI(format, ...)                                                        \
    do {                                                                         \
        if (use_syslog) {                                                        \
            syslog(LOG_INFO, format, ## __VA_ARGS__);                            \
        } else {                                                                 \
            time_t now = time(NULL);                                             \
            char timestr[20];                                                    \
            strftime(timestr, 20, TIME_FORMAT, localtime(&now));                 \
            fprintf(stdout, " %s INFO: " format "\n", timestr,                   \
                ## __VA_ARGS__);                                                 \
			fflush(stdout);                                                      \
        }                                                                        \
    }                                                                            \
    while (0)

#define LOGE(format, ...)                                                         \
    do {                                                                          \
        if (use_syslog) {                                                         \
            syslog(LOG_ERR, format, ## __VA_ARGS__);                              \
        } else {                                                                  \
            time_t now = time(NULL);                                              \
            char timestr[20];                                                     \
            strftime(timestr, 20, TIME_FORMAT, localtime(&now));                  \
            fprintf(stderr, " %s INFO: " format "\n", timestr,                    \
                ## __VA_ARGS__);                                                  \
			fflush(stderr);                                                       \
        }                                                                         \
	}                                                                             \
    while (0)

void FATAL(const char *msg);
void daemonize(const char *path);

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
//void daemonize(const char *path);

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);

#endif
