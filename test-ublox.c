#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>

#include "ublox.h"

#include "slurp.h"

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

/* uint64_t now_ms() */
/* { */
/*     struct timespec spec; */
/*     if (clock_gettime(CLOCK_REALTIME, &spec) == -1) { */
/*         fprintf(stderr, "Failed to get time: %m"); */
/*         abort(); */
/*     } */

/*     return (spec.tv_sec * 1000 + spec.tv_nsec/1000000); */
/* } */


uint8_t *fw;
ssize_t fw_len;

ssize_t pos;

int mygetchar(void *state)
{
    if (pos == fw_len) {
        return -1; // EOF
    }
    return fw[pos++];
}

/* void msghandler(void *state, const char *msg, uint32_t msglen); */


/* void msghandler(const uint8_t *msg, const uint32_t msglen) */
/* { */
/*     fprintf(stderr, "msghandler called\n"); */
/* } */

void time_msghandler(void *state, ublox_class_t class_id, ublox_msg_t msg_id, const uint8_t *msg, const uint32_t msglen)
{
    fprintf(stderr, "msghandler called class_id=(0x%02x) msg_id=(0x%02x)\n", class_id, msg_id);
}

int main(int argc, const char *argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Usage: test-ublox TEST-FILE-UBX\n");
	exit(1);
    }

    const char *filepath = argv[1];
    fw_len = slurp_file(filepath, &fw);
    if (fw_len == -1) {
	exit(1);
    }

//    ublox_parser(mygetchar, NULL, msghandler);
    handle_ublox_data(fw, fw_len);

    time_t now = time(NULL);
    uint8_t ubx[UBLOX_MAX_MESSAGE_SIZE];
    uint16_t len = ublox_make_ubx_mga_ini_time_utc(&ubx, now);
    fprintf(stderr, "got packet of size (%u)\n", len);
    fw = ubx; // global variable pointing to a stack variable.  WCPGW?
    fw_len = len;
    pos = 0;
    ublox_parser(mygetchar, NULL, time_msghandler);

    exit(0);
}

void ublox_debug(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

void console_printf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

void port_slurp_then_call(const char *description, int port, uint32_t maxsize, void (*munch)(const uint8_t*, const uint32_t))
{
}
