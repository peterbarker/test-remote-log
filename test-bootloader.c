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

#include "bootloader_client.h"

time_t rtc_get_rtctimer()
{
    return time(NULL);
}

int serial_fd;

void uart2_write(char *buf, const uint32_t count)
{
    //    fprintf(stderr, "uartt2_write (%u bytes)\n", count);
    int written = write(serial_fd, buf, count);
    if (written == -1) {
        if (errno == EAGAIN) {
	    //fprintf(stderr, "write failed: %s\n", strerror(errno));
	    return;
	}
	fprintf(stderr, "write failed: %s\n", strerror(errno));
	exit(1);
    }
    if (written < count) {
        fprintf(stderr, "short write (%u < %u)\n", written, count);
    }
}

void XQueueReset(QueueHandle_t queue)
{
    tcflush(serial_fd, TCIFLUSH);
}

int xQueueReceive(QueueHandle_t queue, char *dest, uint32_t timeout)
{
   /* fprintf(stderr, "xQueueReceive\n"); */
  int bytes_read = read(serial_fd, dest, 1);
  if (bytes_read == -1) {
    fprintf(stderr, "read failed\n");
    abort();
  }
  if (bytes_read == 0) {
  /* fprintf(stderr, "no bytes\n"); */
    return 0;
  }
  fprintf(stderr, "got a byte (0x%02x)\n", (dest[0]));
  return 1; /* success */
}


/*
 * handle an (as yet undecoded) mavlink message
 */
/* static void mavlink_handle_msg(const mavlink_message_t *msg) */
/* { */
/*     switch(msg->msgid) { */
/*     case MAVLINK_MSG_ID_REMOTE_LOG_DATA_BLOCK: { */
/* 	fprintf(stderr, "Received log data block message\n"); */
/* 	mavlink_remote_log_data_block_t decoded_message; */
/* 	mavlink_msg_remote_log_data_block_decode(msg, &decoded_message); */
/* 	mavlink_handle_remote_log_data_block(&decoded_message); */
/* 	break; */
/*     } */
/*     default: */
/* 	break; */
/*     } */
/* } */

void handle_serial_read(const char *buffer, const uint32_t bytes_read)
{
    /* for (uint32_t i=0; i<bytes_read; i++) { */
    /*     mavlink_status_t status; */
    /*     mavlink_message_t msg; */
    /* 	if (mavlink_parse_char(MAVLINK_COMM_0, buffer[i], &msg, &status)) { */
    /* 	    /\* fprintf(stderr, "Decoded a message with id %u\n", msg.msgid); *\/ */
    /* 	    mavlink_handle_msg(&msg); */
    /* 	} */
    /* } */
}

/*
 * slurp_file - read a file into memory
 */
int slurp_file(const char *filepath, uint8_t **fw)
{
    struct stat stat_buf;
    off_t bytes_read = -1;

    if (stat(filepath, &stat_buf) == -1) {
	fprintf(stderr, "Failed to stat (%s): %s\n", filepath, strerror(errno));
	goto ERR;
    }

    fprintf(stderr, "Loading elf of size %lu\n", stat_buf.st_size);
    int fw_fd = open(filepath, O_RDONLY);
    if (fw_fd == -1) {
	fprintf(stderr, "Failed to open (%s): %s\n", filepath, strerror(errno));
	goto ERR;
    }

    const off_t _fw_size = stat_buf.st_size + (stat_buf.st_size%4);
    uint8_t *_fw = (uint8_t*)malloc(_fw_size);
    if (_fw == NULL) {
	fprintf(stderr, "Failed to allocate (%lu) bytes\n", stat_buf.st_size);
	goto ERR_FH;
    }

    memset(_fw, '\xff', _fw_size);

    bytes_read = 0;
    while (bytes_read < stat_buf.st_size) {
	const ssize_t n = read(fw_fd, &_fw[bytes_read], stat_buf.st_size-bytes_read);
	if (n == -1) {
	    fprintf(stderr, "Failed to read from (%s): %s\n", filepath, strerror(errno));
            bytes_read = -1;
	    goto ERR_FH;
	}
	if (n == 0) {
	    fprintf(stderr, "EOF reading from (%s): %s\n", filepath, strerror(errno));
            bytes_read = -1;
	    goto ERR_FH;
	}
	bytes_read += n;
    }

    *fw = _fw;

 ERR_FH:
    close(fw_fd);
 ERR:
    return bytes_read;
}


int main(int argc, const char *argv[])
{
    if (argc < 3) {
	fprintf(stderr, "Usage: test FIRMWARE DEVICENAME\n");
	exit(1);
    }

    const char *filepath = argv[1];
    uint8_t *fw;
    const ssize_t fw_len = slurp_file(filepath, &fw);
    if (fw_len == -1) {
	exit(1);
    }

    const char *serial_path = argv[2];
    serial_fd = open(serial_path, O_RDWR| O_NONBLOCK | O_NDELAY);
    if (serial_fd == -1) {
	fprintf(stderr, "test-bootloader: Failed to open (%s): %s\n", serial_path, strerror(errno));
	abort();
    }

    /* insert fancy serial-port handling here */
    bootloader_context_t context;
    bootloader_init_context(&context);

    bootloader_send_mavlink_reboot();

    enum my_state {
      state_get_sync,
      state_get_rev,
      state_do_nop,
      state_get_chip_des,
      state_done,
    };


    enum my_state state;
    int done = 0;
    while (state != state_done) {
      int ret;
      uint32_t rev;
      const char *chip_des;
      switch(state) {
      case state_get_sync:
	ret = bootloader_get_sync(&context, &rev);
	break;
      case state_get_rev: {
	ret = bootloader_get_rev(&context, &rev);
	break;
      }
      case state_do_nop: {
	ret = bootloader_send_nop(&context, &rev);
	break;
      }
      case state_get_chip_des: {
	ret = bootloader_get_chip_des(&context, &chip_des);
	break;
      }
      }
      if (ret == -1) {
	  fprintf(stderr, "failed (%u)\n", state);
	  abort();
      }
      if (ret == 0) {
	  switch(state) {
	  case state_get_sync:
	      fprintf(stderr, "Got sync\n");
	      state = state_get_rev;
	      break;
	  case state_get_rev:
	      fprintf(stderr, "Got rev (%u)\n", rev);
	      state = state_get_chip_des;
	      break;
	  case state_do_nop:
	      fprintf(stderr, "NOP done\n");
	      state = state_get_chip_des;
	      break;
	  case state_get_chip_des:
	      fprintf(stderr, "Chip description: %s\n", chip_des);
	      state = state_done;
	      break;
	  }
      }
    }
      //    bl_upload_firmware(fw, fw_len);
    fprintf(stderr, "test-bootloader: All done\n");

    /* fprintf(stderr, "opened OK\n"); */
    /* while (1) { */
    /* 	fd_set readfds; */
    /* 	fd_set writefds; */
    /* 	fd_set exceptfds; */

    /* 	FD_ZERO(&readfds); */
    /* 	FD_ZERO(&writefds); */
    /* 	FD_ZERO(&exceptfds); */
    /* 	int nfds = 0; */

    /* 	FD_SET(serial_fd, &readfds); */
    /* 	if (0) { // we unconditionally send in uart-write */
    /* 	    FD_SET(serial_fd, &writefds); */
    /* 	} */
    /* 	FD_SET(serial_fd, &exceptfds); */
    /* 	nfds=serial_fd+1; */

    /* 	struct timeval timeout = { */
    /* 	    0, */
    /* 	    100000 */
    /* 	}; */
    /* 	if (select(nfds+1, &readfds, &writefds, &exceptfds, &timeout) == -1) { */
    /* 	    fprintf(stderr, "select failed: %s\n", strerror(errno)); */
    /* 	    exit(1); */
    /* 	} */
    /* 	if (FD_ISSET(serial_fd, &readfds)) { */
    /* 	    char buffer[1024]; */
    /* 	    int bytes_read = read(serial_fd, buffer, sizeof(buffer)); */
    /* 	    if (bytes_read == -1) { */
    /* 		fprintf(stderr, "read failed: %s", strerror(errno)); */
    /* 		exit(1); */
    /* 	    } */
    /* 	    /\* fprintf(stderr, "read (%u) bytes from serial\n", bytes_read); *\/ */
    /* 	    handle_serial_read(buffer, bytes_read); */
    /* 	} */
    /* 	mavlink_remote_log_periodic(); */
    /* } */
}
