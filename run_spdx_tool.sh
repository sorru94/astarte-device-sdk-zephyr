rm -r build
west spdx --init -d build
west build -d build -p always -b native_sim ./astarte-device-sdk-zephyr/samples/scheleton_app/
# west build -d build -p always -b frdm_rw612 ./astarte-device-sdk-zephyr/samples/scheleton_app/
# west build -d build -p always -b frdm_rw612 ./astarte-device-sdk-zephyr/samples/astarte_app/ -DEXTRA_CONF_FILE="prj-wifi.conf"
west spdx -d build
