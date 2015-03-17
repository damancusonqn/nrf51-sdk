RELAYR_ROOT?= ${SDKDIR}/relayr

CFLAGS+= \
	-I${RELAYR_ROOT}/src \
	-I${SDKDIR}/segger/RTT

SDKINCDIRS+= \
	device \
	ble/common \
	libraries/util \
	libraries/bootloader_dfu \
	ble/ble_services/ble_dfu \
	ble/device_manager \
	libraries/trace \
	softdevice/common/softdevice_handler

SRCS+= \
	${RELAYR_ROOT}/src/indicator.c \
	${RELAYR_ROOT}/src/onboard-led.c \
	${RELAYR_ROOT}/src/simble.c \
	${RELAYR_ROOT}/src/util.c \
	${RELAYR_ROOT}/src/batt_serv.c \
	${RELAYR_ROOT}/src/segger_rtt_init.c \
	${RELAYR_ROOT}/src/app_error.c \
	${SDKDIR}/segger/RTT/SEGGER_RTT.c \
	${SDKDIR}/segger/RTT/SEGGER_RTT_printf.c \
	${SDKDIR}/segger/Syscalls/RTT_Syscalls_GCC.c 


SDKSRCS+= \
	libraries/bootloader_dfu/dfu_app_handler.c \
	libraries/bootloader_dfu/bootloader_util_gcc.c \
	ble/ble_services/ble_dfu/ble_dfu.c \
	drivers_nrf/hal/nrf_delay.c \
	ble/device_manager/device_manager_peripheral.c \
	drivers_nrf/pstorage/pstorage.c \
	softdevice/common/softdevice_handler/softdevice_handler.c

ifeq (${USE_SOFTDEVICE},s120)
DEFINES+= SD120

SRCS+= \
	${RELAYR_ROOT}/src/simble_central.c

SDKSRCS+= \
	ble/device_manager/device_manager_central.c \
	drivers_nrf/pstorage/pstorage.c


SDKINCDIRS+= \
	libraries/trace
endif

DEFINES+= BLE_STACK_SUPPORT_REQD SOFTDEVICE_PRESENT __HEAP_SIZE=0