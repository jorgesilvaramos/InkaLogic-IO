# InkaLogic IO Library for Arduino

A unified Hardware Abstraction Layer (HAL) for InkaLogic Basic and InkaLogic Pro IO expansion boards, designed for ESP32-based Arduino projects. This library provides a device-agnostic API to handle digital inputs and outputs, supporting both polling and interrupt-driven modes.

## Features

- **Unified API**: Single set of functions for both InkaLogic Basic and Pro devices.
- **Device Detection**: Automatic routing to appropriate low-level drivers based on selected device.
- **Polling Mode**: Simple periodic scanning of inputs with optional debounce.
- **Interrupt Mode**: Efficient interrupt-driven input handling (Pro only) with debounce support.
- **Edge Detection**: Rising and falling edge detection for event-based programming.
- **Debounce Support**: Configurable per-pin debounce filtering to eliminate noise.
- **Arduino Compatible**: Works seamlessly with Arduino IDE and ESP32 boards.

## Hardware Support

### InkaLogic Basic
- **Inputs**: 4 direct ESP32 GPIO pins (35, 34, 39, 36).
- **Outputs**: 4 open-collector outputs via PCA9536 I2C expander (address 0x41).
- **Limitations**: No interrupt support, no edge detection.

### InkaLogic Pro
- **Inputs**: 10 digital inputs via TCA9535 I2C expander (address 0x20).
- **Outputs**: 6 digital outputs via TCA9535.
- **Interrupt**: Hardware interrupt on GPIO 39 for efficient input change detection.
- **Advanced Features**: Full edge detection and interrupt-driven scanning.

## Installation

1. Download or clone this repository.
2. Copy the following files to your Arduino libraries folder:
   - `InkaLogic_IO.h`
   - `InkaLogic_IO.cpp`
   - `InkaLogicBasic.h`
   - `InkaLogicBasic.cpp`
   - `InkaLogicPro.h`
   - `InkaLogicPro.cpp`
3. Restart the Arduino IDE.
4. Include the library in your sketch: `#include "InkaLogic_IO.h"`

Ensure your ESP32 board has I2C support and that `Wire.begin()` is called in `setup()`.

## Usage

### Initialization

```cpp
#include <Wire.h>
#include "InkaLogic_IO.h"

void setup() {
    Wire.begin();
    Wire.setClock(100000);  // Recommended I2C clock speed

    if (!InkaLogic_Init(DEVICE_INKALOGIC_PRO)) {  // or DEVICE_INKALOGIC_BASIC
        // Handle initialization failure
    }
}
```

### Polling Mode (Basic and Pro)

```cpp
void loop() {
    InkaLogic_Scan(20);  // Scan inputs with 20ms debounce

    // Read inputs
    bool input0 = InkaLogic_Read(INKA_IN0);

    // Write outputs
    InkaLogic_Write(INKA_OUT0, input0);

    // Toggle output
    InkaLogic_Toggle(INKA_OUT1);

    // Edge detection (Pro only)
    if (InkaLogic_Rising_Edge(INKA_IN2)) {
        // Handle rising edge
    }
}
```

### Interrupt Mode (Pro only)

```cpp
volatile bool interruptFlag = false;

void IRAM_ATTR interruptHandler() {
    interruptFlag = true;
}

void setup() {
    // ... initialization ...
    InkaLogic_Attach_Interrupt(interruptHandler);
}

void loop() {
    if (interruptFlag) {
        if (InkaLogic_Scan_ISR(20)) {  // Non-blocking scan with debounce
            interruptFlag = false;  // Clear flag after successful scan

            // Process inputs and edges
            if (InkaLogic_Rising_Edge(INKA_IN0)) {
                // Handle event
            }
        }
    }
}
```

## API Reference

### Device Selection
- `InkaLogic_Init(inka_device_t device)`: Initialize the selected device.
- `InkaLogic_Get_Device()`: Get the currently active device type.

### Input/Output Operations
- `InkaLogic_Read(inka_input_t input)`: Read digital input state.
- `InkaLogic_Write(inka_output_t output, bool state)`: Write digital output state.
- `InkaLogic_Toggle(inka_output_t output)`: Toggle digital output state.

### Scanning and Edge Detection
- `InkaLogic_Scan(uint16_t debounce_ms)`: Polling mode scan with debounce.
- `InkaLogic_Scan_ISR(uint16_t debounce_ms)`: Interrupt mode scan (Pro only).
- `InkaLogic_Rising_Edge(inka_input_t input)`: Detect rising edge.
- `InkaLogic_Falling_Edge(inka_input_t input)`: Detect falling edge.

### Interrupt Handling
- `InkaLogic_Attach_Interrupt(void (*isr)(void))`: Attach ISR for hardware interrupt (Pro only).

### Enums
- `inka_device_t`: `DEVICE_INKALOGIC_BASIC`, `DEVICE_INKALOGIC_PRO`
- `inka_input_t`: `INKA_IN0` to `INKA_IN9` (IN4-IN9 Pro only)
- `inka_output_t`: `INKA_OUT0` to `INKA_OUT5` (OUT4-OUT5 Pro only)

For detailed function documentation, see the header files.

## Examples

Two example sketches are provided in the `examples/` directory:

1. **InkaLogic_Polling_Example**: Demonstrates polling mode usage for both Basic and Pro.
2. **InkaLogic_Interrupt_Example**: Shows interrupt-driven operation (Pro only).

## File Structure

- `InkaLogic_IO.h` / `InkaLogic_IO.cpp`: Main HAL implementation with unified API.
- `InkaLogicBasic.h` / `InkaLogicBasic.cpp`: Low-level driver for InkaLogic Basic (PCA9536 + GPIO).
- `InkaLogicPro.h` / `InkaLogicPro.cpp`: Low-level driver for InkaLogic Pro (TCA9535 with interrupt).
- `examples/`: Example Arduino sketches demonstrating library usage.

## Dependencies

- Arduino framework
- ESP32 board support
- Wire library (included with Arduino)

## License

This project is released under the MIT License. See LICENSE file for details.

## Contributing

Contributions are welcome! Please submit issues and pull requests on GitHub.

## Support

For questions or issues, please open an issue on the GitHub repository.
