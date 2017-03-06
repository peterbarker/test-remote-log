SONIX_FIRMWARE=/home/pbarker/rc/skyrocket/sonix_firmware
BASE=${SONIX_FIRMWARE}/app/dashcam/src/main_flow/daemon
INCLUDE=${SONIX_FIRMWARE}/buildscript/include

test:
	cp ${BASE}/mavlink_remote_log.h .
	cp ${BASE}/mavlink_remote_log.c .
	cp ${BASE}/mavlink.h .
	gcc -I ${INCLUDE} -O0 -g -I. test.c mavlink_remote_log.c libmid_fatfs/ff.c -o test
