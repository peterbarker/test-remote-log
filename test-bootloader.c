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
	    fprintf(stderr, "write failed: %s\n", strerror(errno));
	    return;
	}
	fprintf(stderr, "write failed: %s\n", strerror(errno));
	exit(1);
    }
    if (written < count) {
        fprintf(stderr, "short write (%u < %u)\n", written, count);
    }
}

int uart2_readbyte(uint8_t *b, const ssize_t timeout)
{
    if (!xQueueReceive(NULL, b, timeout)) {
	return 0;
    }
    return 1;
}



void XQueueReset(QueueHandle_t queue)
{
    tcflush(serial_fd, TCIFLUSH);
}

int xQueueReceive(QueueHandle_t queue, char *dest, uint32_t timeout)
{
   /* fprintf(stderr, "xQueueReceive\n"); */
    while (1) {
	int bytes_read = read(serial_fd, dest, 1);
	if (bytes_read == -1) {
	    fprintf(stderr, "read failed\n");
	    abort();
	}
	if (bytes_read == 0) {
	    /* fprintf(stderr, "no bytes\n"); */
	    continue;
	}
	fprintf(stderr, "got a byte (0x%02x) (%c)\n", dest[0], (isprint(dest[0]) ? dest[0] : ' '));
	return 1; /* success */
    }
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
    bootloader_send_mavlink_reboot();

    tcflush(serial_fd, TCIFLUSH);
    char c;
    while (read(serial_fd, &c, 1) == 1) {
	fprintf(stderr, "WTF?\n");
    }
    /* XQueueReset(); */
    if (bootloader_get_sync() == -1) {
	fprintf(stderr, "Failed to get sync\n");
    } else {
	fprintf(stderr, "Got sync\n");
    }
    if (bootloader_get_sync() == -1) {
	fprintf(stderr, "Failed to get sync\n");
    } else {
	fprintf(stderr, "Got sync\n");
    }
    if (bootloader_get_sync() == -1) {
	fprintf(stderr, "Failed to get sync\n");
    } else {
	fprintf(stderr, "Got sync\n");
    }
    if (bootloader_get_sync() == -1) {
	fprintf(stderr, "Failed to get sync\n");
    } else {
	fprintf(stderr, "Got sync\n");
    }

    uint32_t rev;
    if (bootloader_get_bl_rev(&rev) == -1) {
    	fprintf(stderr, "Failed to get rev\n");
    	abort();
    }
    fprintf(stderr, "Got rev (%u)\n", rev);

    uint32_t board_id;
    if (bootloader_get_board_id(&board_id) == -1) {
    	fprintf(stderr, "Failed to get board_id\n");
    	abort();
    }
    fprintf(stderr, "Got board_id (%u)\n", board_id);

    uint32_t board_rev;
    if (bootloader_get_board_rev(&board_rev) == -1) {
    	fprintf(stderr, "Failed to get board_rev\n");
    	abort();
    }
    fprintf(stderr, "Got board_rev (%u)\n", board_rev);

    uint32_t fw_size;
    if (bootloader_get_fw_size(&fw_size) == -1) {
    	fprintf(stderr, "Failed to get fw_size\n");
    	abort();
    }
    fprintf(stderr, "Got fw_size (%u)\n", fw_size);

    uint32_t vecs[4];
    if (bootloader_get_vec_area(vecs) == -1) {
    	fprintf(stderr, "Failed to get vec_area\n");
    	abort();
    }
    fprintf(stderr, "Got vec_area (%x) (%x) (%x) (%x)\n", vecs[0],vecs[1],vecs[2],vecs[3]);


    if (bootloader_send_nop(&rev) == -1) {
    	fprintf(stderr, "Failed to do nop\n");
    	abort();
    }
    fprintf(stderr, "NOP done\n");

    uint32_t chip_id;
    if (bootloader_get_chip_id(&chip_id) == -1) {
    	fprintf(stderr, "Failed to get chip_id\n");
    	abort();
    }
    fprintf(stderr, "Got chip_id (%x)\n", chip_id);

    uint32_t crc;
    if (bootloader_get_crc(&crc) == -1) {
    	fprintf(stderr, "Failed to get crc\n");
    	abort();
    }
    fprintf(stderr, "Got crc (%x)\n", crc);

    uint32_t sn[3];
    if (bootloader_get_sn(&sn) == -1) {
    	fprintf(stderr, "Failed to get sn\n");
    	abort();
    }
    fprintf(stderr, "Got sn (%08x %08x %08x)\n", sn[0], sn[1], sn[2]);

    #define otpsize 512
    uint8_t otp[otpsize];
    if (bootloader_get_otp(&otp) == -1) {
    	fprintf(stderr, "Failed to get otp\n");
    	abort();
    }
    fprintf(stderr, "Got otp\n");
    for (uint16_t i=0; i<otpsize; i++) {
	if (i % 32 == 0) {
	    fprintf(stderr, "\n");
	}
	fprintf(stderr,"%02x", otp[i]);
    }
    fprintf(stderr, "\n");

    const char chip_des[65537];
    memset(chip_des, '\0', sizeof(chip_des));
    if (bootloader_get_chip_des(&chip_des, sizeof(chip_des)) == -1) {
    	fprintf(stderr, "Failed to do get chip des\n");
    	abort();
    }
    fprintf(stderr, "Chip description: %s\n", chip_des);

    if (bootloader_get_sync() == -1) {
	fprintf(stderr, "Failed to get sync\n");
    } else {
	fprintf(stderr, "Got sync\n");
    }

    if (bootloader_boot() == -1) {
	fprintf(stderr, "Failed to boot\n");
    } else {
	fprintf(stderr, "Got boot\n");
    }

    fprintf(stderr, "test-bootloader: All done\n");
}
