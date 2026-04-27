/**
 * @file    InkaLogic_Interrupt_Example.ino
 * @brief   Example of using the InkaLogic_IO HAL in interrupt mode (Pro only).
 *
 * The TCA9535 in InkaLogic Pro has an active FALLING INT pin:
 *   - Triggers when any input changes state.
 *   - The ISR only sets a volatile flag; all logic goes in loop().
 *
 * Behavior of this example:
 *   - INKA_OUT0 follows the state of INKA_IN0
 *   - INKA_OUT1 follows the state of INKA_IN1
 *   - Counter that increments on rising edge of INKA_IN2
 *   - INKA_OUT2 turns off on falling edge of INKA_IN3
 *
 * ── Why use Inka_Scan_ISR() instead of Inka_Scan() ───────────────────────────
 *
 *   Inka_Scan() clears _prev at the start of each call (designed for polling).
 *   If used in interrupt mode with debounce, _prev would be equalized to
 *   _cur before the pin is confirmed → the edge is lost.
 *
 *   Inka_Scan_ISR() preserves _prev until all pending pins are confirmed,
 *   and only then returns true. The flag is cleared AFTER,
 *   also avoiding that an interrupt arriving during the I2C transaction
 *   is lost silently.
 */

#include <Wire.h>
#include "InkaLogic_IO.h"

#define DEBOUNCE_MS  20

// ── ISR ───────────────────────────────────────────────────────────────────────
/**
 * @brief Interrupt Service Routine for IO interrupt.
 *
 * Sets the interrupt flag when an IO change occurs.
 */
volatile bool flag_int = false;

void IRAM_ATTR handle_io_int(void)
{
    flag_int = true;
}

// ── Application variables ─────────────────────────────────────────────────────
/**
 * @brief Counter variable for INKA_IN2 rising edge events.
 */
static int contador = 0;

/**
 * @brief Arduino setup function.
 *
 * Initializes serial communication, I2C bus, InkaLogic Pro device,
 * and attaches the interrupt handler.
 */
void setup()
{
    Serial.begin(115200);

    Wire.begin();
    Wire.setClock(100000);

    if (!InkaLogic_Init(DEVICE_INKALOGIC_PRO)) {
        Serial.println("[ERROR] InkaLogic Pro not found on I2C bus.");
        while (true) delay(1000);
    }

    InkaLogic_Attach_Interrupt(handle_io_int);

    Serial.println("[OK] InkaLogic Pro initialized in interrupt mode.");
}

/**
 * @brief Arduino main loop function.
 *
 * Checks for interrupt flag, performs debounced scan, and handles IO logic.
 */
void loop()
{
    if (!flag_int) return;

    // Scan_ISR returns false while debounce has not confirmed all
    // pending pins → we keep calling it in each loop() until it
    // returns true, at which point the snapshot is ready.
    if (!InkaLogic_Scan_ISR(DEBOUNCE_MS)) return;

    // All pins confirmed: clear flag AFTER Scan_ISR.
    // If another interrupt arrives during Scan_ISR, flag_int will already
    // be true and the next loop() cycle will process it correctly.
    flag_int = false;

    // Direct input to output
    InkaLogic_Write(INKA_OUT0, InkaLogic_Read(INKA_IN0));
    InkaLogic_Write(INKA_OUT1, InkaLogic_Read(INKA_IN1));

    // Counter on rising edge
    if (InkaLogic_Rising_Edge(INKA_IN2)) {
        contador++;
        Serial.print("[RISING+] IN2 → counter = ");
        Serial.println(contador);
    }

    // Turn off OUT2 on falling edge of IN3
    if (InkaLogic_Falling_Edge(INKA_IN3)) {
        InkaLogic_Toggle(INKA_OUT2);
    }
}
