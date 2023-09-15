#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <curl/curl.h>

#include "utils.h"

int use_syslog = 0;

void FATAL(const char *msg)
{
    LOGE("%s", msg);
    exit(-1);
}

void daemonize(const char *path)
{
    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    /* If we got a good PID, then
     * we can exit the parent process. */
    if (pid > 0) {
        FILE *file = fopen(path, "w");
        if (file == NULL) {
            FATAL("Invalid pid file\n");
        }

        fprintf(file, "%d", (int)pid);
        fclose(file);
        exit(EXIT_SUCCESS);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}


void *ss_malloc(size_t size)
{
    void *tmp = malloc(size);

    if (tmp == NULL) {
        exit(EXIT_FAILURE);
	}

    return tmp;
}

int balloc(buffer_t *ptr, size_t capacity)
{
    memset(ptr, 0, sizeof(buffer_t));
    ptr->data = ss_malloc(capacity);
    ptr->capacity = capacity;

    return capacity;
}

void bfree(buffer_t *ptr)
{
	if (ptr == NULL) {
		return;
	}

    ptr->idx = 0;
    ptr->len = 0;
    ptr->capacity = 0;
    if (ptr->data != NULL) {
        ss_free(ptr->data);
    }
}

int download_file(const char *url, const char *out_file, int timeout_ms)
{
	int rc = 0;
    FILE *fp = NULL;

    CURL *curl;
    CURLcode res;

	if (!url || !out_file)
		return -1;

	curl = curl_easy_init();
	if (!curl)
		return -1;

	fp = fopen(out_file, "wb");
	if (!fp) {
		rc = -1;
		goto out;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		rc = -1;
	}

out:
	if (curl)
		curl_easy_cleanup(curl);

	if (fp)
		fclose(fp);

	return rc;
}

ssize_t readn(int fd, void *vptr, size_t n)
{
    size_t  nleft;
    ssize_t nread;
    char   *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;      /* and call read() again */
            else
                return -1;
        } else if (nread == 0)
            break;              /* EOF */

        nleft -= nread;
        ptr += nread;
    }

    return (n - nleft);         /* return >= 0 */
}

ssize_t writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;   /* and call write() again */
            else
                return -1;    /* error */
         }

         nleft -= nwritten;
         ptr += nwritten;
    }

    return (n);
}






