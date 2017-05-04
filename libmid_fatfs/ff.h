#include <stdio.h>
#include <stdint.h>

typedef struct {
    int fs; // fake file descriptor

    int fd; // our actual file descriptor
} FIL;

#define FA_READ 0

typedef enum {
    FR_OK,
    FR_BAD=-1
} fr_t;


#define FA_CREATE_ALWAYS 1
#define FA_WRITE 2

fr_t f_open(FIL *fil, const char *path, uint32_t flags);

fr_t f_read(FIL *fil, char *buf, uint32_t buflen, uint32_t *bytes_read);

fr_t f_write(FIL *fil, char *buf, const uint32_t buflen, uint32_t *bytes_written);

fr_t f_lseek(FIL *fil, uint32_t offset);

fr_t f_close(FIL *fil);

uint8_t exists(const char *path);
