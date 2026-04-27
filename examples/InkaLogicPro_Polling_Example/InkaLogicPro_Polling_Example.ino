/**
 * @file    InkaLogic_Polling_Example.ino
 * @brief   Example usage of the InkaLogic_IO HAL in polling mode.
 *
 * Works with both InkaLogic Basic and InkaLogic Pro:
 *   - Change DEVICE_INKALOGIC_BASIC / DEVICE_INKALOGIC_PRO according to your board.
 *   - On Basic, INKA_IN4..INKA_IN9 return false and INKA_OUT4/5 are no-ops.
 *   - InkaLogic_Rising_Edge / InkaLogic_Falling_Edge return false on Basic.
 *
 * Example behavior:
 *   - INKA_OUT0 follows INKA_IN0 (direct input-to-output copy)
 *   - INKA_OUT1 follows INKA_IN1
 *   - INKA_OUT2 toggles on each rising edge of INKA_IN2
 *   - INKA_OUT3 toggles on each falling edge of INKA_IN3
 */

#include <Wire.h>
#include "InkaLogic_IO.h"

// ── Select your hardware ─────────────────────────────────────────────────────
#define MY_DEVICE   DEVICE_INKALOGIC_PRO   // or DEVICE_INKALOGIC_BASIC

#define DEBOUNCE_MS  20   // ms debounce per pin

static bool out2_state = false;

void setup()
{
    Serial.begin(115200);

    Wire.begin();
    Wire.setClock(100000);

    if (!InkaLogic_Init(MY_DEVICE)) {
        Serial.println("[ERROR] InkaLogic not found on the I2C bus.");
        while (true) delay(1000);
    }

    Serial.print("[OK] InkaLogic initialized. Device: ");
    Serial.println(InkaLogic_Get_Device() == DEVICE_INKALOGIC_PRO ? "PRO" : "BASIC");
}

void loop()
{
    // 1. Refresh the input snapshot (call ONCE at the top of loop)
    InkaLogic_Scan(DEBOUNCE_MS);

    // 2. Direct input-to-output copy
    InkaLogic_Write(INKA_OUT0, InkaLogic_Read(INKA_IN0));
    InkaLogic_Write(INKA_OUT1, InkaLogic_Read(INKA_IN1));

    // 3. Toggle on rising edge (Pro only; Basic returns false -> no effect)
    if (InkaLogic_Rising_Edge(INKA_IN2)) {
        out2_state = !out2_state;
        InkaLogic_Write(INKA_OUT2, out2_state);
        Serial.print("[RISING] IN2 -> OUT2 = ");
        Serial.println(out2_state);
    }

    // 4. Toggle on falling edge
    if (InkaLogic_Falling_Edge(INKA_IN3)) {
        InkaLogic_Toggle(INKA_OUT3);
        Serial.println("[FALLING] IN3 -> OUT3 toggle");
    }
}
