SONIX_FIRMWARE=/home/pbarker/rc/skyrocket/sonix_firmware
BASE=${SONIX_FIRMWARE}/snx_sdk/app/dashcam/src/main_flow/daemon
INCLUDE=${SONIX_FIRMWARE}/buildscript/include

test:
	cp ${BASE}/mavlink_remote_log.h .
	cp ${BASE}/mavlink_remote_log.c .
	cp ${BASE}/mavlink.h .
	gcc -I ${INCLUDE} -O0 -g -I. test.c mavlink_remote_log.c libmid_fatfs/ff.c -o test


test-bootloader:
	cp -f ${BASE}/bootloader_client.h .
	chmod -w bootloader_client.h
	cp -f ${BASE}/bootloader_client.c .
	chmod -w bootloader_client.c
	gcc -I ${INCLUDE} -O0 -g -I. slurp.c bootloader_client.c test-bootloader.c  libmid_fatfs/ff.c queue.c -o test-bootloader

test-ublox:
	cp -f ${BASE}/ublox.h .
	chmod -w ublox.h
	perl -pe 's/static void ublox_parser/ublox_parser/' -i ublox.h
	perl -pe 's%/UBLOX%/tmp/ublox%' -i ublox.h
	perl -pe 's/static void handle_ublox_data/void handle_ublox_data/' -i ublox.h
	perl -pe 's/static uint16_t ublox_make_ubx_mga_ini_time_utc/uint16_t ublox_make_ubx_mga_ini_time_utc/' -i ublox.h
	perl -pe 's/static void ublox_make_ubx_/void ublox_make_ubx/' -i ublox.h
	cp -f ${BASE}/ublox.c .
	perl -pe 's/static void ublox_parser/ublox_parser/' -i ublox.c
	perl -pe 's/static void handle_ublox_data/void handle_ublox_data/' -i ublox.c
	perl -pe 's/static uint16_t ublox_make_ubx_mga_ini_time_utc/uint16_t ublox_make_ubx_mga_ini_time_utc/' -i ublox.c
	perl -pe 's/static void ublox_make_ubx_utc/void ublox_make_ubx/' -i ublox.c
	chmod -w ublox.c
	cp -f ${BASE}/ports.h .
	chmod -w ports.h
	gcc -I ${INCLUDE} -O0 -g -I. slurp.c ublox.c test-ublox.c  libmid_fatfs/ff.c queue.c -o test-ublox

clean:
	rm -f *.o test-bootloader test
