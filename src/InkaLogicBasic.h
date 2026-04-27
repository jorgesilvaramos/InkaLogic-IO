#pragma once

/**
 * @file    InkaLogicBasic.h
 * @brief   Low-level driver for InkaLogic Basic — direct GPIO inputs + PCA9536 outputs.
 *
 * @details The InkaLogic Basic board uses:
 *          - **Inputs**  : 4 direct ESP32 GPIO pins (35, 34, 39, 36), read in real time.
 *          - **Outputs** : 4 open-collector outputs driven by a PCA9536 I/O expander
 *            over I2C (address `0x41`).
 *
 *          An internal shadow register (`_output_state`) tracks the current output
 *          state so that individual bits can be set / cleared without an I2C read.
 *
 * ## Prerequisite
 * `Wire.begin()` must be called in the sketch before any function in this driver.
 *
 * ## I2C register map (PCA9536)
 *
 * | Register              | Address | Description                  |
 * |-----------------------|---------|------------------------------|
 * | `INPUT_PORT_REG`      | `0x00`  | Read current input states    |
 * | `OUTPUT_PORT_REG`     | `0x01`  | Write output latch           |
 * | `POLARITY_INVERSION_REG` | `0x02` | Invert input polarity     |
 * | `CONFIG_REG`          | `0x03`  | Direction (0 = output)       |
 */

#include <Arduino.h>
#include <Wire.h>

// ── I2C address and PCA9536 registers ────────────────────────────────────────

#define PCA9536_ADDR            0x41 ///< 7-bit I2C address of the PCA9536
#define INPUT_PORT_REG          0x00 ///< Input port register
#define OUTPUT_PORT_REG         0x01 ///< Output port register (write to drive outputs)
#define POLARITY_INVERSION_REG  0x02 ///< Polarity inversion register
#define CONFIG_REG              0x03 ///< Configuration register (0 = output, 1 = input)

// ── Digital input GPIO pins (ESP32) ──────────────────────────────────────────

#define BASIC_GPIO_IN0          35   ///< DI0 — ESP32 GPIO 35 (input only)
#define BASIC_GPIO_IN1          34   ///< DI1 — ESP32 GPIO 34 (input only)
#define BASIC_GPIO_IN2          39   ///< DI2 — ESP32 GPIO 39 (input only)
#define BASIC_GPIO_IN3          36   ///< DI3 — ESP32 GPIO 36 (input only)

// ── Digital output identifiers (PCA9536 bits 0-3) ────────────────────────────

/**
 * @brief PCA9536 output bit positions.
 *
 * Each value corresponds to a bit index in the PCA9536 output latch register.
 */
typedef enum {
    BASIC_OUT0 = 0, ///< DO0 — PCA9536 bit 0
    BASIC_OUT1,     ///< DO1 — PCA9536 bit 1
    BASIC_OUT2,     ///< DO2 — PCA9536 bit 2
    BASIC_OUT3,     ///< DO3 — PCA9536 bit 3
} basic_output_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief  Initializes InkaLogic Basic hardware.
 *
 * @details Performs the following steps:
 *          1. Configures GPIO 35, 34, 39, 36 as inputs.
 *          2. Verifies PCA9536 presence on the I2C bus.
 *          3. Sets all PCA9536 pins to output mode (`CONFIG_REG = 0x00`).
 *          4. Drives all outputs LOW (`OUTPUT_PORT_REG = 0x00`).
 *
 * @pre    `Wire.begin()` must have been called in the sketch.
 *
 * @return `true`  if the PCA9536 acknowledged and was configured successfully.
 * @return `false` if the device was not found or an I2C error occurred.
 */
bool Inkalogic_Basic_Init(void);

/**
 * @brief  Reads the logical state of a digital input GPIO.
 *
 * @details Calls `digitalRead()` directly on the specified GPIO pin.
 *          No snapshot or debounce is applied at this layer.
 *
 * @param  gpio  GPIO number to read. Use the `BASIC_GPIO_INx` constants.
 * @return `true` = HIGH, `false` = LOW.
 */
bool Inkalogic_Basic_Digital_Read(uint8_t gpio);

/**
 * @brief  Writes the state of a PCA9536 output.
 *
 * @details Updates the internal shadow register and writes the new value
 *          to the PCA9536 `OUTPUT_PORT_REG` in a single I2C transaction.
 *          Only the lower 4 bits of the shadow register are ever modified.
 *
 * @param  output  Output to drive (`basic_output_t`).
 * @param  state   `true` = HIGH, `false` = LOW.
 */
void Inkalogic_Basic_Digital_Write(basic_output_t output, bool state);

/**
 * @brief  Toggles the current state of a PCA9536 output.
 *
 * @details XORs the corresponding bit in the internal shadow register and
 *          writes the result to the PCA9536.
 *
 * @param  output  Output to toggle (`basic_output_t`).
 */
void Inkalogic_Basic_Toggle_Output(basic_output_t output);