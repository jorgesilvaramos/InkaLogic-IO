#pragma once

/**
 * @file    InkaLogic_IO.h
 * @brief   Unified HAL for InkaLogic Basic (PCA9536 + GPIO) and InkaLogic Pro (TCA9535 + INT).
 *
 * @details This header provides a single, device-agnostic API to interact with both
 *          InkaLogic hardware variants. Select the active device via `InkaLogic_Init()`
 *          and use the unified `INKA_IN` / `INKA_OUT` pin enums regardless of the
 *          underlying hardware. Out-of-range pins (e.g. `INKA_IN4` on Basic) return
 *          `false` or are silently ignored.
 *
 * ---
 *
 * ## Typical usage — Polling mode
 *
 * ```cpp
 * // setup()
 * Wire.begin();
 * Wire.setClock(400000);
 * InkaLogic_Init(DEVICE_INKALOGIC_PRO);
 *
 * // loop()
 * InkaLogic_Scan(20);                              // Call ONCE at the top of loop()
 * InkaLogic_Write(INKA_OUT0, InkaLogic_Read(INKA_IN0));
 * if (InkaLogic_Rising_Edge(INKA_IN2))  { ... }
 * if (InkaLogic_Falling_Edge(INKA_IN3)) { ... }
 * ```
 *
 * ## Typical usage — Interrupt mode (Pro only)
 *
 * ```cpp
 * volatile bool flag_int = false;
 * void IRAM_ATTR isr() { flag_int = true; }
 *
 * // setup()
 * Wire.begin();
 * Wire.setClock(400000);
 * InkaLogic_Init(DEVICE_INKALOGIC_PRO);
 * InkaLogic_Attach_Interrupt(isr);
 *
 * // loop()
 * if (flag_int) {
 *     if (InkaLogic_Scan_ISR(20)) {       // Returns true once all pins are confirmed
 *         flag_int = false;               // Clear AFTER Scan_ISR
 *         if (InkaLogic_Rising_Edge(INKA_IN2))  { ... }
 *         if (InkaLogic_Falling_Edge(INKA_IN3)) { ... }
 *     }
 * }
 * ```
 *
 * ---
 *
 * ## Pin mapping
 *
 * | Variant | Inputs            | Physical mapping          |
 * |---------|-------------------|---------------------------|
 * | Basic   | INKA_IN0..IN3     | GPIO 35, 34, 39, 36       |
 * | Pro     | INKA_IN0..IN9     | TCA9535 bits 0–9          |
 *
 * | Variant | Outputs           | Physical mapping          |
 * |---------|-------------------|---------------------------|
 * | Basic   | INKA_OUT0..OUT3   | PCA9536 bits 0–3          |
 * | Pro     | INKA_OUT0..OUT5   | TCA9535 PORT1 bits 2–7    |
 *
 * > **Note:** Edge detection (`Rising_Edge` / `Falling_Edge`) and `Scan_ISR`
 * > always return `false` on Basic.
 */

#include "InkaLogicBasic.h"
#include "InkaLogicPro.h"

// ── Device type ───────────────────────────────────────────────────────────────

/**
 * @brief Selects the active InkaLogic hardware variant.
 */
typedef enum {
    DEVICE_INKALOGIC_BASIC = 0, ///< InkaLogic Basic — PCA9536 outputs + direct GPIO inputs
    DEVICE_INKALOGIC_PRO   = 1, ///< InkaLogic Pro   — TCA9535 I/O expander with interrupt
} inka_device_t;

// ── Unified pin enums ─────────────────────────────────────────────────────────

/**
 * @brief Unified digital input identifiers.
 *
 * `INKA_IN4` through `INKA_IN9` are only valid on InkaLogic Pro.
 * Passing them to Basic functions returns `false`.
 */
typedef enum {
    INKA_IN0 = 0, ///< Digital input 0 (Basic + Pro)
    INKA_IN1,     ///< Digital input 1 (Basic + Pro)
    INKA_IN2,     ///< Digital input 2 (Basic + Pro)
    INKA_IN3,     ///< Digital input 3 (Basic + Pro)
    INKA_IN4,     ///< Digital input 4 (Pro only)
    INKA_IN5,     ///< Digital input 5 (Pro only)
    INKA_IN6,     ///< Digital input 6 (Pro only)
    INKA_IN7,     ///< Digital input 7 (Pro only)
    INKA_IN8,     ///< Digital input 8 (Pro only)
    INKA_IN9,     ///< Digital input 9 (Pro only)
} inka_input_t;

/**
 * @brief Unified digital output identifiers.
 *
 * `INKA_OUT4` and `INKA_OUT5` are only valid on InkaLogic Pro.
 * Passing them to Basic functions is a silent no-op.
 */
typedef enum {
    INKA_OUT0 = 0, ///< Digital output 0 (Basic + Pro)
    INKA_OUT1,     ///< Digital output 1 (Basic + Pro)
    INKA_OUT2,     ///< Digital output 2 (Basic + Pro)
    INKA_OUT3,     ///< Digital output 3 (Basic + Pro)
    INKA_OUT4,     ///< Digital output 4 (Pro only)
    INKA_OUT5,     ///< Digital output 5 (Pro only)
} inka_output_t;

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief  Selects the active device and initializes its hardware.
 *
 * @details `Wire.begin()` must be called before this function.
 *          Internally calls either `Inkalogic_Basic_Init()` or
 *          `Inkalogic_Pro_Init()` depending on the selected device.
 *
 * @param  device  Target hardware variant (`DEVICE_INKALOGIC_BASIC` or
 *                 `DEVICE_INKALOGIC_PRO`).
 * @return `true`  if the hardware acknowledged the I2C transaction.
 * @return `false` on I2C error or unrecognized device.
 */
bool InkaLogic_Init(inka_device_t device);

/**
 * @brief  Attaches a user ISR to the hardware interrupt pin (Pro only).
 *
 * @details The interrupt fires on the falling edge of the TCA9535 INT pin.
 *          On InkaLogic Basic this call is silently ignored.
 *
 * @param  isr  Pointer to the user ISR. Must be declared `IRAM_ATTR` on
 *              ESP32 and should only set a `volatile bool` flag — no I2C
 *              or heavy logic inside the ISR.
 */
void InkaLogic_Attach_Interrupt(void (*isr)(void));

/**
 * @brief  **Polling mode** — refreshes the input snapshot with debounce.
 *
 * @details Call this function **exactly once** at the top of `loop()`.
 *          It clears the previous-state snapshot (`_prev`) at entry, so
 *          `Rising_Edge` / `Falling_Edge` are valid for exactly one loop
 *          iteration after a transition.
 *
 *          - **Pro:** reads TCA9535 via I2C and applies per-pin debounce.
 *          - **Basic:** no-op; GPIO pins are read in real time by
 *            `InkaLogic_Read()`.
 *
 * @param  debounce_ms  Stabilisation time in milliseconds. `0` = no filter.
 */
void InkaLogic_Scan(uint16_t debounce_ms);

/**
 * @brief  **Interrupt mode** — non-blocking snapshot update (Pro only).
 *
 * @details Call this inside `if (flag_int)` on every `loop()` iteration.
 *          Unlike `InkaLogic_Scan()`, it does **not** update `_prev` until
 *          all pending pins have been debounce-confirmed, preserving edge
 *          detection across multiple loop cycles when debounce is active.
 *
 *          Clear `flag_int` and read edges **only** when this function
 *          returns `true`.
 *
 *          On InkaLogic Basic this always returns `false`.
 *
 * @param  debounce_ms  Stabilisation time in milliseconds. `0` = no filter.
 * @return `true`  when all pending pins are confirmed and the snapshot is
 *                 ready for edge detection.
 * @return `false` while pins are still within the debounce window (call
 *                 again on the next `loop()` iteration).
 */
bool InkaLogic_Scan_ISR(uint16_t debounce_ms);

/**
 * @brief  Reads the logical state of a digital input.
 *
 * @details On Basic, reads the GPIO pin directly (no snapshot).
 *          On Pro, returns the value from the last confirmed snapshot.
 *
 * @note   `INKA_IN4`..`INKA_IN9` always return `false` on Basic.
 *
 * @param  input  Input identifier (`inka_input_t`).
 * @return `true` = HIGH, `false` = LOW.
 */
bool InkaLogic_Read(inka_input_t input);

/**
 * @brief  Writes the logical state of a digital output.
 *
 * @note   `INKA_OUT4` and `INKA_OUT5` are silently ignored on Basic.
 *
 * @param  output  Output identifier (`inka_output_t`).
 * @param  state   `true` = HIGH, `false` = LOW.
 */
void InkaLogic_Write(inka_output_t output, bool state);

/**
 * @brief  Toggles the current state of a digital output.
 *
 * @note   `INKA_OUT4` and `INKA_OUT5` are silently ignored on Basic.
 *
 * @param  output  Output identifier (`inka_output_t`).
 */
void InkaLogic_Toggle(inka_output_t output);

/**
 * @brief  Detects a rising edge (LOW → HIGH) since the last `Scan` / `Scan_ISR`.
 *
 * @details Returns `true` for exactly **one** loop cycle after the transition.
 *          Always returns `false` on InkaLogic Basic.
 *
 * @param  input  Input to evaluate (`inka_input_t`).
 * @return `true` if a rising edge occurred on this cycle.
 */
bool InkaLogic_Rising_Edge(inka_input_t input);

/**
 * @brief  Detects a falling edge (HIGH → LOW) since the last `Scan` / `Scan_ISR`.
 *
 * @details Returns `true` for exactly **one** loop cycle after the transition.
 *          Always returns `false` on InkaLogic Basic.
 *
 * @param  input  Input to evaluate (`inka_input_t`).
 * @return `true` if a falling edge occurred on this cycle.
 */
bool InkaLogic_Falling_Edge(inka_input_t input);

/**
 * @brief  Returns the currently active device type.
 *
 * @return `DEVICE_INKALOGIC_BASIC` or `DEVICE_INKALOGIC_PRO`.
 */
inka_device_t InkaLogic_Get_Device(void);