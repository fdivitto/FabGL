set(FABGL_REQUIRES fatfs
                   spiffs
                   ulp
                   nvs_flash
                   soc)

if("${IDF_VERSION_MAJOR}" GREATER_EQUAL 5)
  # IDF 5 removed esp_adc_cal and no more gives implicit access to other components
  list(APPEND FABGL_REQUIRES esp_adc
                             esp_event
                             esp_http_client
                             esp_hw_support
                             esp_netif
                             esp_rom
                             esp_system
                             esp_timer
                             esp_wifi
                             sdmmc
                             spi_flash
                             vfs)
  if(NOT "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS "5.3")
    # IDF 5.3 split the monolithic "driver" component
    list(APPEND FABGL_REQUIRES esp_driver_dac
                               esp_driver_gpio
                               esp_driver_i2c
                               esp_driver_sdm
                               esp_driver_sdspi
                               esp_driver_spi
                               esp_driver_uart)
  else()
    list(APPEND FABGL_REQUIRES driver)
  endif()
else()
  list(APPEND FABGL_REQUIRES esp_adc_cal)
endif()

idf_component_register(SRC_DIRS src
                                src/comdrivers
                                src/devdrivers
                                src/dispdrivers
                                src/emudevs
                                src/network
                       INCLUDE_DIRS src
                                src/fonts
                                src/images
                                src/comdrivers
                                src/devdrivers
                                src/dispdrivers
                                src/emudevs
                                src/network
                       REQUIRES ${FABGL_REQUIRES})

# When built together with the Arduino core (Arduino as IDF component) FabGL uses its API
if(COMMAND idf_component_optional_requires)
  idf_component_optional_requires(PUBLIC espressif__arduino-esp32 arduino-esp32 arduino)
endif()
