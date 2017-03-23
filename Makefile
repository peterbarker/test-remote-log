SONIX_FIRMWARE=/home/pbarker/rc/skyrocket/sonix_firmware
BASE=${SONIX_FIRMWARE}/app/dashcam/src/main_flow/daemon
INCLUDE=${SONIX_FIRMWARE}/buildscript/include

test:
	cp ${BASE}/mavlink_remote_log.h .
	cp ${BASE}/mavlink_remote_log.c .
	cp ${BASE}/mavlink.h .
	gcc -I ${INCLUDE} -O0 -g -I. test.c mavlink_remote_log.c libmid_fatfs/ff.c -o test


test-bootloader:
	cp ${BASE}/bootloader_client.h .
	cp ${BASE}/bootloader_client.c .
	gcc -I ${INCLUDE} -O0 -g -I. test-bootloader.c bootloader_client.c libmid_fatfs/ff.c queue.c -o test-bootloader

clean:
	rm -f *.o test-bootloader test
