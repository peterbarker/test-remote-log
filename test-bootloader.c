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


#include "bootloader_client.h"

#include "slurp.h"

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))

time_t rtc_get_rtctimer()
{
    return time(NULL);
}

int serial_fd;

uint64_t now_ms()
{
    struct timespec spec;
    if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
        fprintf(stderr, "Failed to get time: %m");
        abort();
    }

    return (spec.tv_sec * 1000 + spec.tv_nsec/1000000);
}

ssize_t bootloader_write(const uint8_t *bytes, const size_t bytecount, const bl_timeout_t tickstowait)
{
    fprintf(stderr, "uartt2_write (%lu bytes)\n", bytecount);
    const uint64_t ms_to_wait = tickstowait/10;
    const uint64_t start_time_ms = now_ms();

    ssize_t total_written = 0;
    while (total_written < bytecount) {
        const uint32_t delta = now_ms() - start_time_ms;
        if (delta > ms_to_wait) {
            fprintf(stderr, "write timeout after %lums (now=%lu start=%lu delta=%u)\n", ms_to_wait, now_ms(), start_time_ms, delta);
            return -1;
        }
        int thistime = write(serial_fd, &bytes[total_written], bytecount);
        if (thistime == -1) {
            if (errno == EAGAIN) {
                continue;
            }
            /* fprintf(stderr, "write failed: %s\n", strerror(errno)); */
            continue;
        }
        if (thistime < bytecount) {
            fprintf(stderr, "short write (%d < %lu)\n", thistime, bytecount);
        }
        total_written += thistime;
    }
    fsync(serial_fd);
    fprintf(stderr, "total written: %lu\n", total_written);
    return total_written;
}

ssize_t bootloader_readbyte(uint8_t *byte, const bl_timeout_t timeout)
{
    const uint64_t ms_to_wait = timeout/10;

    const uint64_t start_time_ms = now_ms();
   /* fprintf(stderr, "xQueueReceive\n"); */
    while (1) {
	int bytes_read = read(serial_fd, byte, 1);
	if (bytes_read == -1) {
            if (errno == EAGAIN) {
                continue;
            }
	    fprintf(stderr, "read failed: %m\n");
	    exit(1);
	}
	if (bytes_read == 1) {
            fprintf(stderr, "got a byte (0x%02x) (%c)\n", byte[0], (isprint(byte[0]) ? byte[0] : ' '));
            return 1; /* success */
        }
        const uint32_t delta = now_ms() - start_time_ms;
        if (delta > ms_to_wait) {
            fprintf(stderr, "read timeout after %lums (now=%lu start=%lu delta=%u)\n", ms_to_wait, now_ms(), start_time_ms, delta);
            return 0;
        }
    }
}

void bootloader_debug(const char  *format, ...)
{
    va_list argptr;
    va_start(argptr,format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
}

/*
 * configures serial port, ensures bootloader is listening
 */
int configure_serial(int serial_fd)
{
    /* insert fancy serial-port handling here */
    typedef struct {
        uint32_t baud;
        const char *name;
    } serial_config_t;

    const serial_config_t serial_configs[] = {
        { B38400, "38400" },
        { B57600, "57600" },
        { B115200, "115200" }
    };

    struct termios options;
    if (tcgetattr(serial_fd, &options) == -1) {
        fprintf(stderr, "Failed to get serial options: %m\n");
        exit(1);
    }
    while (1) {
        for (uint8_t i=0; i<ARRAY_LEN(serial_configs); i++) {
            serial_config_t config = serial_configs[i];
            fprintf(stderr, "trying sync at %s\n", config.name);
            cfsetispeed(&options, config.baud);
            cfsetospeed(&options, config.baud);
            if (tcsetattr(serial_fd, TCSANOW, &options) == -1) {
                fprintf(stderr, "Failed to set serial options: %m\n");
                /* exit(1); */
            }

            tcflush(serial_fd, TCIFLUSH);
            tcflush(serial_fd, TCOFLUSH);

            // try syncing before we do a mavlink reset (which can confuse bl)
            if (bootloader_get_sync() == -1) {
                fprintf(stderr, "Failed to get sync\n");
            } else {
                fprintf(stderr, "Got sync\n");
                return 0;
            }

            bootloader_send_mavlink_reboot();
            sleep(1);

            tcflush(serial_fd, TCIFLUSH);
            tcflush(serial_fd, TCOFLUSH);

            if (bootloader_get_sync() == -1) {
                fprintf(stderr, "Failed to get sync\n");
            } else {
                fprintf(stderr, "Got sync\n");
                return 0;
            }

            char c;
            while (read(serial_fd, &c, 1) == 1) {
                fprintf(stderr, "WTF?\n");
            }

            if (bootloader_get_sync() == -1) {
                fprintf(stderr, "Failed to get sync\n");
            } else {
                fprintf(stderr, "Got sync\n");
                return 0;
            }
        }
    }
}

void bootloader_progress(uint8_t percent)
{
    fprintf(stderr, "%u percent\n", percent);
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

    if (configure_serial(serial_fd) == -1) {
        fprintf(stderr, "Failed to configure serial\n");
        abort();
    }

    uint32_t crc;

    if (bootloader_get_sync() == -1) {
	fprintf(stderr, "Failed to get sync\n");
    } else {
	fprintf(stderr, "Got sync\n");
    }

    /* if (bootloader_set_boot_delay(5) == -1) { */
    /*     fprintf(stderr, "Failed to set delay\n"); */
    /* } else { */
    /*     fprintf(stderr, "Set delay\n"); */
    /* } */

    /* exit(0); */

    if (bootloader_get_crc(&crc) == -1) {
    	fprintf(stderr, "Failed to get crc\n");
    } else {
        fprintf(stderr, "Got crc (%x)\n", crc);
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
    fprintf(stderr, "Bootloader revision (%u)\n", rev);

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

    const uint32_t calculated_crc = bootloader_crc32_firmware(fw, fw_len, fw_size);
    fprintf(stderr, "calculated crc: %x\n", calculated_crc);

    if (crc == calculated_crc) {
        fprintf(stderr, "Checksums match\n");
    } else {
        fprintf(stderr, "CRC mismatch (calculated=%x) (bl says %x)\n", calculated_crc, crc);
    }

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

    uint32_t sn[3];
    if (bootloader_get_sn(sn) == -1) {
    	fprintf(stderr, "Failed to get sn\n");
    	abort();
    }
    fprintf(stderr, "Got sn (%08x %08x %08x)\n", sn[0], sn[1], sn[2]);

    #define otpsize 512
    uint8_t otp[otpsize];
    if (bootloader_get_otp(otp) == -1) {
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

    char chip_des[65537];
    memset(chip_des, '\0', sizeof(chip_des));
    if (bootloader_get_chip_des(chip_des, sizeof(chip_des)) == -1) {
    	fprintf(stderr, "Failed to do get chip des\n");
    	abort();
    }
    fprintf(stderr, "Chip description: %s\n", chip_des);

    if (bootloader_get_sync() == -1) {
	fprintf(stderr, "Failed to get sync\n");
    } else {
	fprintf(stderr, "Got sync\n");
    }


    if (bootloader_get_crc(&crc) == -1) {
    	fprintf(stderr, "Failed to get crc\n");
    	abort();
    } else {
        fprintf(stderr, "Got crc (%x)\n", crc);
    }

    /* erase the chip */
    fprintf(stderr, "Erasing (~10 seconds)\n");
    int erase_done = 0;
    bootloader_erase_send();
    for (uint8_t i=0; i<15; i++) {
        bl_reply_t status;
        if (bootloader_recv_sync(&status, 100) == -1) {
	    fprintf(stderr, "\nFailed to get sync\n");
	} else {
	    fprintf(stderr, "\nGot sync\n");
            erase_done = 1;
	    break;
	}
	fprintf(stderr, ".");
	sleep(1);
    }
    fprintf(stderr, "\n");
    if (!erase_done) {
        fprintf(stderr, "Erase failed\n");
        exit(1);
    }

    fprintf(stderr, "Programming (~10 seconds)\n");
    if (bootloader_program(fw, fw_len, bootloader_progress) == -1) {
        fprintf(stderr, "program failed\n");
    }

    if (bootloader_get_crc(&crc) == -1) {
    	fprintf(stderr, "Failed to get crc\n");
    } else {
        fprintf(stderr, "Got crc (%x)\n", crc);
    }
    if (crc != calculated_crc) {
        fprintf(stderr, "CRC mismatch (calculated=%x) (bl says %x)\n", calculated_crc, crc);
    } else {
        fprintf(stderr, "checksums match\n");
    }

    if (bootloader_boot() == -1) {
    	fprintf(stderr, "Failed to boot\n");
    } else {
    	fprintf(stderr, "Got boot\n");
    }

    fprintf(stderr, "test-bootloader: All done\n");
    exit(0);
}
