#include "ff.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <unistd.h>

#include <errno.h>

fr_t f_open(FIL *fil, const char *path, uint32_t flags)
{
    int open_flags = 0;
    if (flags & FA_CREATE_ALWAYS) {
	open_flags |= O_CREAT;
    }
    if (flags & FA_WRITE) {
	open_flags |= O_WRONLY;
    }

    fprintf(stderr, "opening: %s\n", path);
    int ret = open(path, open_flags, 0777);
    if (ret == 0) {
	// seriously?
	fprintf(stderr, "got zero\n");
	close(ret);
	ret = open(path, open_flags, 0777);;
	if (ret == 0) {
	    fprintf(stderr, "still got zero\n");
	}
    }
    if (ret == -1) {
	fil->fs = 0;
	return FR_BAD;
    }
    fil->fs = 1; // fake!
    fil->fd = ret;
    return FR_OK;
}

fr_t f_read(FIL *fil, char *buf, uint32_t buflen, uint32_t *bytes_read)
{
    int _bytes_read = read(fil->fd, buf, buflen);
    if (_bytes_read == -1) {
	fprintf(stderr, "error reading: %s\n", strerror(errno));
	return FR_BAD;
    }
    *bytes_read = _bytes_read;
    return FR_OK;
}

fr_t f_write(FIL *fil, char *buf, const uint32_t buflen, uint32_t *bytes_written)
{
    int _bytes_written = write(fil->fd, buf, buflen);
    if (_bytes_written == -1) {
	return FR_BAD;
    }
    *bytes_written = _bytes_written;
    return FR_OK;
}

fr_t f_lseek(FIL *fil, const uint32_t offset)
{
    if (lseek(fil->fd, offset, SEEK_SET) == -1) {
	return FR_BAD;
    }
    return FR_OK;
}

fr_t f_close(FIL *fil)
{
    close(fil->fd);
    fil->fd = -1;
    fil->fs = 0;
    return FR_OK;
}
