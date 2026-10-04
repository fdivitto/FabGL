# FabGL with ESP-IDF 5 / 6

FabGL can be built with ESP-IDF in two ways, both using this repository as an IDF component:

| Project | What it is |
|---------|------------|
| `NativeVGATerminal` | Plain IDF application (`app_main`), no Arduino core involved |
| `ArduinoSketch` | Builds any of the Arduino examples of this repository, using the Arduino core as IDF component |

Both have been built with ESP-IDF 6.0.1. `ArduinoSketch` uses Arduino core 4.0.0-alpha1, which is the release based on ESP-IDF 6.0.


## Required configuration

`sdkconfig.fabgl` contains the settings FabGL needs. Both projects use it as `SDKCONFIG_DEFAULTS`: do the same in your own projects, or copy its content into your `sdkconfig.defaults`.

The important ones:

- `CONFIG_ULP_COPROC_ENABLED`, `CONFIG_ULP_COPROC_TYPE_FSM` and `CONFIG_ULP_COPROC_RESERVE_MEM=4096`. PS/2 ports are handled by the ULP coprocessor, whose program and variables take 2112 bytes of RTC slow memory. Since IDF 5 the system places its own data just after the reserved area, so a smaller value makes them overlap. FabGL refuses to compile in this case.
- `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240` and `CONFIG_FREERTOS_HZ=1000`, which is what the Arduino core uses and FabGL has been tuned for.
- `CONFIG_ESP_WIFI_IRAM_OPT`, `CONFIG_ESP_WIFI_RX_IRAM_OPT` and `CONFIG_ESP_PHY_IRAM_OPT` disabled. IDF enables them by default, placing about 25 KB of WiFi code in IRAM. Unused IRAM becomes heap, and FabGL allocates from it.


## NativeVGATerminal

```
cd idf/NativeVGATerminal
idf.py build flash monitor
```

Shows a terminal on VGA, echoes the PS/2 keyboard and reports free memory, on screen and on the serial console.


## ArduinoSketch

The `SKETCH` environment variable selects the example, as path relative to the `examples` directory:

```
cd idf/ArduinoSketch
SKETCH=VGA/HardwareTest idf.py -B build/HardwareTest build flash monitor
```

- `sdkconfig` is kept inside the build directory. Use a different build directory for each sketch (`-B`), or remove the build directory when switching sketch.
- A `sdkconfig.defaults` file inside the sketch directory is added to the configuration. `VGA/PCEmulator` uses it to map PSRAM without adding it to the heap.
- `ino2cpp.py` does what the Arduino IDE does with `.ino` files: includes `Arduino.h` and adds function prototypes.
- `partitions.csv` has a 3 MB application and 960 KB of SPIFFS, like "Huge APP" in the Arduino IDE.
