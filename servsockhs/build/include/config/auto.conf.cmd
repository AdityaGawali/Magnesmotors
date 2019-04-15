deps_config := \
	/home/aditya/esp/esp-idf/components/app_trace/Kconfig \
	/home/aditya/esp/esp-idf/components/aws_iot/Kconfig \
	/home/aditya/esp/esp-idf/components/bt/Kconfig \
	/home/aditya/esp/esp-idf/components/driver/Kconfig \
	/home/aditya/esp/esp-idf/components/esp32/Kconfig \
	/home/aditya/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/aditya/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/aditya/esp/esp-idf/components/ethernet/Kconfig \
	/home/aditya/esp/esp-idf/components/fatfs/Kconfig \
	/home/aditya/esp/esp-idf/components/freertos/Kconfig \
	/home/aditya/esp/esp-idf/components/heap/Kconfig \
	/home/aditya/esp/esp-idf/components/http_server/Kconfig \
	/home/aditya/esp/esp-idf/components/libsodium/Kconfig \
	/home/aditya/esp/esp-idf/components/log/Kconfig \
	/home/aditya/esp/esp-idf/components/lwip/Kconfig \
	/home/aditya/esp/esp-idf/components/mbedtls/Kconfig \
	/home/aditya/esp/esp-idf/components/mdns/Kconfig \
	/home/aditya/esp/esp-idf/components/mqtt/Kconfig \
	/home/aditya/esp/esp-idf/components/nvs_flash/Kconfig \
	/home/aditya/esp/esp-idf/components/openssl/Kconfig \
	/home/aditya/esp/esp-idf/components/pthread/Kconfig \
	/home/aditya/esp/esp-idf/components/spi_flash/Kconfig \
	/home/aditya/esp/esp-idf/components/spiffs/Kconfig \
	/home/aditya/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/aditya/esp/esp-idf/components/vfs/Kconfig \
	/home/aditya/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/aditya/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/aditya/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/aditya/esp/espcodes/servsockhs/main/Kconfig.projbuild \
	/home/aditya/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/aditya/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
