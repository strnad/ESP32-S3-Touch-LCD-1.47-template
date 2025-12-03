# ESP32-S3-Touch-LCD-1.47 Template Project

This is a clean template project for the **Waveshare ESP32-S3-Touch-LCD-1.47** development board. It is configured with necessary drivers for the display, touch controller, and other peripherals, ready for your custom application.

## Features

- **Display**: JD9853 driver (172x320 resolution)
- **Touch**: CST816S/AXS5106 driver
- **LVGL**: Pre-configured LVGL port
- **Peripherals**: Drivers for Battery, WiFi, SD Card (optional), and RGB LED.

## Project Structure

```
├── components
│   ├── esp_bsp             # Board Support Package (BSP)
│   ├── esp_lcd_jd9853      # LCD Driver
│   ├── esp_lcd_touch_axs5106 # Touch Driver
│   └── ...
├── main
│   ├── main.c              # Main application entry point
│   ├── lv_fs_port.c        # LVGL File System port (FatFS)
│   └── CMakeLists.txt
├── CMakeLists.txt
└── README.md
```

## How to Use

1.  **Clone/Copy** this project to a new directory.
2.  **Build** using ESP-IDF:
    ```bash
    idf.py build
    ```
3.  **Flash** to the board:
    ```bash
    idf.py -p PORT flash monitor
    ```

## Configuration

- **Display Rotation**: Modify `EXAMPLE_DISPLAY_ROTATION` in `main/main.c`.
- **WiFi**: Uncomment and set credentials in `app_main` if needed.
