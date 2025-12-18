# LG IoT Washer Firmware

ESP-IDF firmware for driving a scale-model LG washing machine with full wash cycle control, display UI, motor control via ODrive, and optional IoT connectivity.

## Features

- **FreeRTOS-based** task architecture with event-driven state machine
- **ST7789 TFT Display** 320x240 TFT with 188x107 sprite
- **ODrive Motor Control** via UART for precise drum speed regulation
- **DAC Audio** with ADSR envelope for auditory feedback
- **ULP Deep Sleep** with power button wake-up for low standby power
- **OTA Updates** with factory + dual OTA partition scheme
- **FreeHome IoT** integration (optional cloud connectivity)
- **Simulator Mode** for desktop development without hardware

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| ESP32 | DevKit or custom board |
| ST7789 TFT | 320×240 SPI display |
| ODrive | Brushless motor controller |
| Speaker | Connected to GPIO25 (DAC) |
| MPU6050 | IMU for balance detection (optional) |
| Buttons | Power (GPIO33), Start/Stop (GPIO32) |
| Door Sensor | GPIO34 |
| Pumps | Circulation, Drain, Fill (PWM outputs) |

### Pin Mapping

See [main/app_config.h](main/app_config.h) for complete GPIO assignments.

## Building

### Prerequisites

- ESP-IDF v5.4+ installed and sourced
- Python 3.8+

### Build & Flash

```bash
cd brands/LG/sw/LG_IOT_Washer_Firmware
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Simulator Mode

Enable `CONFIG_SIMULATOR_MODE` via `idf.py menuconfig` to run with the Python simulator. Navigate to "Component config" -> "main" and enable `Simulator Mode (CONFIG_SIMULATOR_MODE)`.

```bash
# Set the option then build/flash the firmware
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor

# In another terminal: start simulator host
python tools/simulator/sim_host.py
```

## Project Structure

```
LG_IOT_Washer_Firmware/
├── main/
│   ├── drivers/
│   │   ├── display/      # ST7789 + sprite rendering
│   │   ├── gpio_hal/     # GPIO, PWM (LEDC), DAC
│   │   ├── mpu6050/      # IMU driver (I2C)
│   │   ├── odrive/       # Motor controller (UART)
│   │   ├── sound/        # DAC audio with ADSR
│   │   ├── wifi/         # WiFi manager + HTTP server
│   │   └── freehome/     # IoT cloud integration
│   ├── machine_state/    # Wash cycle state machine
│   ├── simulator/        # UART-based simulator protocol
│   ├── ulp/              # ULP coprocessor for deep sleep wake
│   ├── app_config.h      # Hardware configuration
│   ├── wash_plan.h/cpp   # Wash program builder
│   ├── wash_types.h      # Cycle parameter definitions
│   ├── tasks.h/cpp       # FreeRTOS task creation
│   ├── ui_controller.*   # Display update logic
│   └── main.cpp          # Application entry point
├── components/
│   └── esp32-wifi-manager/
├── tools/
│   └── simulator/        # Python simulator host
├── partitions.csv        # OTA-capable partition table
├── sdkconfig.defaults    # Default SDK configuration
└── CMakeLists.txt
```

## Wash Programs

The firmware supports 14 wash programs with configurable parameters:

| Program | Description |
|---------|-------------|
| 0 | Cotton |
| 1 | Cotton Eco |
| 2 | Easy Care |
| 3 | Speed 14 |
| 4 | Delicates |
| 5 | Wool |
| 6 | Sportswear |
| 7 | Download |
| 8 | Rinse + Spin |
| 9 | Spin Only |
| 10 | Drain |
| 11 | Tub Clean |
| 12 | Quick 30 |
| 13 | Allergy Care |

Each program builds a dynamic wash plan with sections: Detecting → Saturation → Prewash (opt) → Main Wash → Interim Spin → Rinse(s) → Final Spin.

## Configuration

Key settings are now exposed via Kconfig (use `idf.py menuconfig`). Important main options include:

- `CONFIG_WIFI_ENABLED` — Enable WiFi and provisioning UI (esp32-wifi-manager).
- `CONFIG_BALANCE_DETECTION` — Enable MPU6050-based imbalance detection.
- `CONFIG_SIMULATOR_MODE` — Build firmware for use with the simulator host (disables some hardware drivers).

Key settings in `sdkconfig.defaults`:

| Option | Description |
|--------|-------------|
| `CONFIG_ULP_COPROC_ENABLED` | Enable ULP for deep sleep wake |
| `CONFIG_FREERTOS_HZ` | Tick rate (1000 Hz) |
| `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` | OTA rollback support |
| `CONFIG_DAC_DMA_AUTO_16BIT_ALIGN` | DAC audio alignment |

## License

Copyright 2025 Yusuf Emre Kenaroglu  
SPDX-License-Identifier: Apache-2.0
