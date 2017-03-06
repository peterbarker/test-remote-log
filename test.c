#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>


#include "mavlink_remote_log.h"

time_t rtc_get_rtctimer()
{
    return time(NULL);
}

int serial_fd;

void uart2_write(char *buf, const uint32_t count)
{
    fprintf(stderr, "uartt2_write\n");
    int written = write(serial_fd, buf, count);
    if (written == -1) {
	fprintf(stderr, "write failed: %s\n", strerror(errno));
	exit(1);
    }
    if (written < count) {
	fprintf(stderr, "sort write\n");
    }
}


/*
 * handle an (as yet undecoded) mavlink message
 */
static void mavlink_handle_msg(const mavlink_message_t *msg)
{
    switch(msg->msgid) {
    case MAVLINK_MSG_ID_REMOTE_LOG_DATA_BLOCK: {
	fprintf(stderr, "Received log data block message\n");
	mavlink_remote_log_data_block_t decoded_message;
	mavlink_msg_remote_log_data_block_decode(msg, &decoded_message);
	mavlink_handle_remote_log_data_block(&decoded_message);
	break;
    }
    default:
	break;
    }
}

void handle_serial_read(const char *buffer, const uint32_t bytes_read)
{
    for (uint32_t i=0; i<bytes_read; i++) {
        mavlink_status_t status;
        mavlink_message_t msg;
	if (mavlink_parse_char(MAVLINK_COMM_0, buffer[i], &msg, &status)) {
	    /* fprintf(stderr, "Decoded a message with id %u\n", msg.msgid); */
	    mavlink_handle_msg(&msg);
	}
    }
}

int main(int argc, const char *argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Usage: test DEVICENAME\n");
	exit(1);
    }
    const char *serial_path = argv[1];
    serial_fd = open(serial_path, O_RDWR| O_NONBLOCK | O_NDELAY);
    if (serial_fd == -1) {
	fprintf(stderr, "Failed to open (%s): %s\n", serial_path, strerror(errno));
	abort();
    }
    /* insert all sorts of serial-port configuration goodness here */

    fprintf(stderr, "opened OK\n");
    while (1) {
	fd_set readfds;
	fd_set writefds;
	fd_set exceptfds;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	int nfds = 0;

	FD_SET(serial_fd, &readfds);
	if (0) { // we unconditionally send in uart-write
	    FD_SET(serial_fd, &writefds);
	}
	FD_SET(serial_fd, &exceptfds);
	nfds=serial_fd+1;

	struct timeval timeout = {
	    0,
	    100000
	};
	if (select(nfds+1, &readfds, &writefds, &exceptfds, &timeout) == -1) {
	    fprintf(stderr, "select failed: %s\n", strerror(errno));
	    exit(1);
	}
	if (FD_ISSET(serial_fd, &readfds)) {
	    char buffer[1024];
	    int bytes_read = read(serial_fd, buffer, sizeof(buffer));
	    if (bytes_read == -1) {
		fprintf(stderr, "read failed: %s", strerror(errno));
		exit(1);
	    }
	    /* fprintf(stderr, "read (%u) bytes from serial\n", bytes_read); */
	    handle_serial_read(buffer, bytes_read);
	}
	mavlink_remote_log_periodic();
    }
}
