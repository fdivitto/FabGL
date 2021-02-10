
idf_component_register(SRC_DIRS src
                                src/devdrivers
                                src/comdrivers
                                src/dispdrivers
                       INCLUDE_DIRS src
                                    src/devdrivers
                                    src/comdrivers
                                    src/dispdrivers
                                    src/fonts
                                    src/images
                       REQUIRES fatfs
                                spiffs
                                esp_adc_cal
                                ulp
                                soc)

