idf_component_register(
    SRCS
        "main.cpp"
        "../../esp32_common/wifi_ap.cpp"
        "../../esp32_common/http_server.cpp"
        "../../esp32_common/websocket_server.cpp"
        "uart_bridge.cpp"
        "../../common/protocol.cpp"
        "../../common/uart_bridge_base.cpp"
    INCLUDE_DIRS
        "."
        "../../common"
        "../../esp32_common"
    PRIV_REQUIRES
        esp_wifi
        esp_http_server
        nvs_flash
        # ESP-IDF v6 splits drivers into esp_driver_* components.
        # Keep explicit deps so headers like driver/gpio.h and driver/uart.h resolve.
        esp_driver_gpio
        esp_driver_uart
        freertos
        cjson
)

# C++26 (если поддерживается компилятором), иначе C++23.
set(RC_VEHICLE_CXX_STANDARD 23)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
    set(RC_VEHICLE_CXX_STANDARD 26)
endif()

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD ${RC_VEHICLE_CXX_STANDARD})
set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_EXTENSIONS ON)

