# 1. Get the raw git description string
execute_process(
    COMMAND git describe --always --dirty
    OUTPUT_VARIABLE GIT_RAW_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${COMPONENT_DIR}
)

# 2. Clean up the "-gXXXXXXX" hash using CMake's native regex
string(REGEX REPLACE "-g[a-f0-9]+$" "" GIT_FW_VERSION "${GIT_RAW_VERSION}")

# 3. Process the .in file and output the final header using explicit COMPONENT_DIR references
set(VERSION_IN_FILE "${COMPONENT_DIR}/version.h.in")
set(VERSION_OUT_FILE "${COMPONENT_DIR}/version.h")

configure_file(
    "${VERSION_IN_FILE}"
    "${VERSION_OUT_FILE}"
    @ONLY
)

# 4. Initialize an empty list for the text files you want to embed
set(embed_files "")

# 5. Check SDKCONFIG variables using absolute COMPONENT_DIR paths
if(CONFIG_NORTH_INTERFACE_MQTT)
    list(APPEND embed_files "${COMPONENT_DIR}/cert_bundle.pem")
endif()

if(CONFIG_WIFI_PROVISIONING_PORTAL)
    list(APPEND embed_files "${COMPONENT_DIR}/portal.html")
endif()

if(CONFIG_NORTH_INTERFACE_HTTP)
    list(APPEND embed_files 
        "${COMPONENT_DIR}/http/index.css" 
        "${COMPONENT_DIR}/http/index.html" 
        "${COMPONENT_DIR}/http/index.js"
    )
endif()

# 6. Automatically grab all C files in the main directory
set(src_files
	"app_actuator.c"
	"app_at.c"
	"app_bell.c"
	"app_bell_engine.c"
	"app_bh1750.c"
	"app_bme280.c"
	"app_coap_client.c"
	"app_coap_server.c"
	"app_contact.c"
	"app_http_server.c"
	"app_i2c.c"
	"app_main.c"
	"app_motion.c"
	"app_mqtt.c"
	"app_nvs.c"
	"app_ops.c"
	"app_presence.c"
	"app_publish_data.c"
	"app_relay.c"
	"app_scheduler.c"
	"app_sensors.c"
	"app_switch.c"
	"app_system.c"
	"app_thermostat.c"
	"app_valve.c"
	"app_waterpump.c"
	"app_wifi_portal.c"
)


if(IDF_TARGET STREQUAL "esp8266")
    list(APPEND src_files "app_smart_config_esp8266.c")
    list(APPEND src_files "app_ota_esp8266.c")
    list(APPEND src_files "app_wifi_esp8266.c")
else()
    list(APPEND src_files "app_smart_config_esp32.c")
    list(APPEND src_files "app_ota_esp32.c")
    list(APPEND src_files "app_wifi_esp32.c")
endif()

message(STATUS "--------------------------------------------------------")
message(STATUS "TINTA DETECTATA DE CMAKE: ${current_target}")
message(STATUS "LISTA DE FISIERE .C ADUNATE PENTRU COMPILARE:")
foreach(file IN LISTS src_files)
    message(STATUS " -> ${file}")
endforeach()
message(STATUS "--------------------------------------------------------")



idf_component_register(
    SRCS "${src_files}"
    PRIV_REQUIRES esp-tls esp_wifi esp_event nvs_flash esp_driver_gpio app_update esp_driver_uart esp_http_client esp_http_server
    INCLUDE_DIRS "."
    EMBED_TXTFILES "${embed_files}"
)

# Ensures wait state validation orders
set_source_files_properties("app_main.c" PROPERTIES OBJECT_DEPENDS "${VERSION_OUT_FILE}")

