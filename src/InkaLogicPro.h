#pragma once

/**
 * @file    InkaLogicPro.h
 * @brief   Low-level driver for InkaLogic Pro — TCA9535 16-bit I/O expander with interrupt.
 *
 * @details The InkaLogic Pro board uses a TCA9535 connected over I2C (`0x20`) to provide:
 *          - **10 digital inputs** : PORT0 (bits 0–7) + PORT1 (bits 0–1)
 *          - **6 digital outputs** : PORT1 bits 2–7
 *          - **Hardware interrupt** : active-low INT pin (`PRO_IO_INT_PIN` = GPIO 39)
 *            that fires whenever any input changes state.
 *
 *          An internal 16-bit snapshot (`_cur`) is updated by `Inkalogic_Pro_Scan()`
 *          (polling) or `Inkalogic_Pro_Scan_ISR()` (interrupt-driven). Edge detection
 *          compares `_cur` against the previous snapshot (`_prev`).
 *
 * ## Prerequisite
 * `Wire.begin()` must be called in the sketch before any function in this driver.
 *
 * ## TCA9535 register map
 *
 * | Register                | Address | Description                    |
 * |-------------------------|---------|--------------------------------|
 * | `TCA9535_REG_INPUT0`    | `0x00`  | PORT0 input state (read)       |
 * | `TCA9535_REG_INPUT1`    | `0x01`  | PORT1 input state (read)       |
 * | `TCA9535_REG_OUTPUT0`   | `0x02`  | PORT0 output latch (write)     |
 * | `TCA9535_REG_OUTPUT1`   | `0x03`  | PORT1 output latch (write)     |
 * | `TCA9535_REG_POL0`      | `0x04`  | PORT0 polarity inversion       |
 * | `TCA9535_REG_POL1`      | `0x05`  | PORT1 polarity inversion       |
 * | `TCA9535_REG_CONFIG0`   | `0x06`  | PORT0 direction (1 = input)    |
 * | `TCA9535_REG_CONFIG1`   | `0x07`  | PORT1 direction (1 = input)    |
 *
 * ## Port configuration after `Inkalogic_Pro_Init()`
 *
 * | Port  | Bits  | Direction | Usage              |
 * |-------|-------|-----------|--------------------|
 * | PORT0 | 0–7   | Input     | DI5..DI0, SW2, SW1 |
 * | PORT1 | 0–1   | Input     | S3, S2             |
 * | PORT1 | 2–7   | Output    | DO0..DO5           |
 */

#include <Arduino.h>
#include <Wire.h>

// ── I2C address and register map ─────────────────────────────────────────────

#define TCA9535_ADDRESS           0x20 ///< 7-bit I2C address of the TCA9535
#define TCA9535_INPUT_COUNT       10   ///< Number of valid digital inputs (bits 0–9)

#define TCA9535_REG_INPUT0        0x00 ///< PORT0 input register
#define TCA9535_REG_INPUT1        0x01 ///< PORT1 input register
#define TCA9535_REG_OUTPUT0       0x02 ///< PORT0 output latch
#define TCA9535_REG_OUTPUT1       0x03 ///< PORT1 output latch
#define TCA9535_REG_POL0          0x04 ///< PORT0 polarity inversion
#define TCA9535_REG_POL1          0x05 ///< PORT1 polarity inversion
#define TCA9535_REG_CONFIG0       0x06 ///< PORT0 direction register
#define TCA9535_REG_CONFIG1       0x07 ///< PORT1 direction register

// ── Interrupt pin ─────────────────────────────────────────────────────────────

#define PRO_IO_INT_PIN            39 ///< ESP32 GPIO connected to TCA9535 INT (active LOW / FALLING)

// ── Digital input identifiers (TCA9535 bit positions) ────────────────────────

/**
 * @brief TCA9535 input bit positions.
 *
 * Values correspond to bit indices in the 16-bit word formed by
 * `(PORT1 << 8) | PORT0`. Physical silk-screen labels are shown in comments.
 */
typedef enum {
    DIGITAL_IN5 = 0,  ///< DI5  → PORT0 bit 0
    DIGITAL_IN4 = 1,  ///< DI4  → PORT0 bit 1
    DIGITAL_IN3 = 2,  ///< DI3  → PORT0 bit 2
    DIGITAL_IN2 = 3,  ///< DI2  → PORT0 bit 3
    DIGITAL_IN1 = 4,  ///< DI1  → PORT0 bit 4
    DIGITAL_IN0 = 5,  ///< DI0  → PORT0 bit 5
    DIGITAL_IN6 = 6,  ///< SW2  → PORT0 bit 6
    DIGITAL_IN7 = 7,  ///< SW1  → PORT0 bit 7
    DIGITAL_IN8 = 8,  ///< S3   → PORT1 bit 0
    DIGITAL_IN9 = 9,  ///< S2   → PORT1 bit 1
} pro_input_t;

// ── Digital output identifiers (TCA9535 PORT1 bits 2-7) ──────────────────────

/**
 * @brief TCA9535 output bit positions within PORT1.
 *
 * Values correspond to bit indices within the PORT1 output latch byte.
 */
typedef enum {
    DIGITAL_OUT0 = 2,  ///< DO0 → PORT1 bit 2
    DIGITAL_OUT1 = 3,  ///< DO1 → PORT1 bit 3
    DIGITAL_OUT2 = 4,  ///< DO2 → PORT1 bit 4
    DIGITAL_OUT3 = 5,  ///< DO3 → PORT1 bit 5
    DIGITAL_OUT4 = 6,  ///< DO4 → PORT1 bit 6
    DIGITAL_OUT5 = 7,  ///< DO5 → PORT1 bit 7
} pro_output_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief  Initializes the TCA9535 and takes an initial input snapshot.
 *
 * @details Performs the following steps:
 *          1. Verifies TCA9535 presence on the I2C bus.
 *          2. Sets PORT0 entirely to inputs (`CONFIG0 = 0xFF`).
 *          3. Sets PORT1 bits 0–1 as inputs and bits 2–7 as outputs (`CONFIG1 = 0x03`).
 *          4. Drives all outputs LOW.
 *          5. Reads the current input state into `_prev` and `_cur` to prevent
 *             false edge detections on the first `Scan` call.
 *
 * @pre    `Wire.begin()` must have been called in the sketch.
 *
 * @return `true`  if all I2C transactions completed successfully.
 * @return `false` if the device was not found or an I2C error occurred.
 */
bool Inkalogic_Pro_Init(void);

/**
 * @brief  Reads both TCA9535 ports in a single repeated-start transaction.
 *
 * @details Issues a repeated-start I2C read starting at `TCA9535_REG_INPUT0`
 *          to fetch PORT0 and PORT1 in one bus transaction. Directly updates
 *          `_prev` and `_cur` if the value changed.
 *
 *          Use this only in interrupt mode without debounce, or to force an
 *          immediate hardware read.
 *
 * @return Raw 16-bit value: `(PORT1 << 8) | PORT0`. Bits 0–9 are valid inputs.
 */
uint16_t Inkalogic_Pro_Read_All(void);

/**
 * @brief  **Polling mode** — updates the input snapshot with per-pin debounce.
 *
 * @details Reads the TCA9535, saves the current snapshot to `_prev`, then
 *          runs the debounce state machine. `_prev` is always updated at the
 *          start of this call, so `Rising_Edge` / `Falling_Edge` are valid
 *          for exactly **one** loop cycle after each confirmed transition.
 *
 *          Call this **once** at the top of `loop()`.
 *
 * @param  debounce_ms  Per-pin stabilisation time in milliseconds. `0` = no filter.
 */
void Inkalogic_Pro_Scan(uint16_t debounce_ms);

/**
 * @brief  **Interrupt mode** — non-blocking snapshot update with debounce.
 *
 * @details Designed to be called on every `loop()` iteration while an interrupt
 *          flag is set. Key difference from `Inkalogic_Pro_Scan()`: `_prev` is
 *          **not** updated until all pending pins complete their debounce window,
 *          which ensures that edge information is never lost across loop cycles.
 *
 *          Recommended usage pattern:
 *          ```cpp
 *          if (flag_int) {
 *              if (Inkalogic_Pro_Scan_ISR(debounce_ms)) {
 *                  flag_int = false;   // Clear flag AFTER Scan_ISR returns true
 *                  // Read Rising / Falling edges here
 *              }
 *          }
 *          ```
 *
 * @param  debounce_ms  Per-pin stabilisation time in milliseconds. `0` = no filter.
 * @return `true`  when all pending pins are confirmed — snapshot is ready.
 * @return `false` while pins are still within the debounce window.
 */
bool Inkalogic_Pro_Scan_ISR(uint16_t debounce_ms);

/**
 * @brief  Returns the state of a single input from the current snapshot.
 *
 * @param  input  Input identifier (`pro_input_t`).
 * @return `true` = HIGH, `false` = LOW.
 */
bool Inkalogic_Pro_Read_Input(pro_input_t input);

/**
 * @brief  Detects a rising edge (LOW → HIGH) since the last `Scan` / `Scan_ISR`.
 *
 * @details Compares `_cur` and `_prev`. Returns `true` for exactly one loop
 *          cycle after the transition is confirmed.
 *
 * @param  input  Input to evaluate (`pro_input_t`).
 * @return `true` if a rising edge was detected on this cycle.
 */
bool Inkalogic_Pro_Rising_Edge(pro_input_t input);

/**
 * @brief  Detects a falling edge (HIGH → LOW) since the last `Scan` / `Scan_ISR`.
 *
 * @details Compares `_cur` and `_prev`. Returns `true` for exactly one loop
 *          cycle after the transition is confirmed.
 *
 * @param  input  Input to evaluate (`pro_input_t`).
 * @return `true` if a falling edge was detected on this cycle.
 */
bool Inkalogic_Pro_Falling_Edge(pro_input_t input);

/**
 * @brief  Writes the state of a TCA9535 output.
 *
 * @details Updates the internal output shadow byte and writes it to
 *          `TCA9535_REG_OUTPUT1` in a single I2C transaction.
 *
 * @param  output  Output to drive (`pro_output_t`).
 * @param  state   `true` = HIGH, `false` = LOW.
 */
void Inkalogic_Pro_Write_Output(pro_output_t output, bool state);

/**
 * @brief  Toggles the current state of a TCA9535 output.
 *
 * @details XORs the corresponding bit in the output shadow byte and writes
 *          the result to `TCA9535_REG_OUTPUT1`.
 *
 * @param  output  Output to toggle (`pro_output_t`).
 */
void Inkalogic_Pro_Toggle_Output(pro_output_t output);

/**
 * @brief  Attaches a user ISR to the TCA9535 hardware interrupt pin.
 *
 * @details Configures `PRO_IO_INT_PIN` as an input and calls
 *          `attachInterrupt()` for the `FALLING` edge. The TCA9535 INT
 *          line is active-low and is asserted whenever any input changes.
 *
 *          The ISR must be declared `IRAM_ATTR` on ESP32 and should only
 *          set a `volatile bool` flag. All debounce and I2C work must be
 *          done in `loop()` via `Inkalogic_Pro_Scan_ISR()`.
 *
 * @param  isr  Pointer to the user interrupt service routine.
 */
void Inkalogic_Pro_Attach_Interrupt(void (*isr)(void));