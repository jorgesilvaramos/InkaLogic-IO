/**
 * @file    InkaLogic_IO.cpp
 * @brief   Implementation of the unified HAL for InkaLogic Basic and InkaLogic Pro.
 *
 * @details Dispatches every public API call to the appropriate low-level driver
 *          (InkaLogicBasic or InkaLogicPro) based on the device selected via
 *          `InkaLogic_Init()`. Internal lookup tables map the unified
 *          `inka_input_t` / `inka_output_t` enums to driver-specific pin
 *          identifiers.
 */

#include "InkaLogic_IO.h"

// ── Internal state ────────────────────────────────────────────────────────────
static inka_device_t _device      = DEVICE_INKALOGIC_BASIC;
static bool          _initialized = false;

// ── HAL → driver mapping tables ──────────────────────────────────────────────

/** Maps unified INKA_INx to Basic GPIO numbers (BASIC_GPIO_INx). */
static const uint8_t _basic_gpio[4] = {
    BASIC_GPIO_IN0,   // INKA_IN0
    BASIC_GPIO_IN1,   // INKA_IN1
    BASIC_GPIO_IN2,   // INKA_IN2
    BASIC_GPIO_IN3,   // INKA_IN3
};

/** Maps unified INKA_INx to Pro input identifiers (pro_input_t). */
static const pro_input_t _pro_input[10] = {
    DIGITAL_IN0,  // INKA_IN0
    DIGITAL_IN1,  // INKA_IN1
    DIGITAL_IN2,  // INKA_IN2
    DIGITAL_IN3,  // INKA_IN3
    DIGITAL_IN4,  // INKA_IN4
    DIGITAL_IN5,  // INKA_IN5
    DIGITAL_IN6,  // INKA_IN6
    DIGITAL_IN7,  // INKA_IN7
    DIGITAL_IN8,  // INKA_IN8
    DIGITAL_IN9,  // INKA_IN9
};

/** Maps unified INKA_OUTx to Pro output identifiers (pro_output_t). */
static const pro_output_t _pro_output[6] = {
    DIGITAL_OUT0,  // INKA_OUT0
    DIGITAL_OUT1,  // INKA_OUT1
    DIGITAL_OUT2,  // INKA_OUT2
    DIGITAL_OUT3,  // INKA_OUT3
    DIGITAL_OUT4,  // INKA_OUT4
    DIGITAL_OUT5,  // INKA_OUT5
};

/** Maps unified INKA_OUTx to Basic output identifiers (basic_output_t). */
static const basic_output_t _basic_output[4] = {
    BASIC_OUT0,  // INKA_OUT0
    BASIC_OUT1,  // INKA_OUT1
    BASIC_OUT2,  // INKA_OUT2
    BASIC_OUT3,  // INKA_OUT3
};

// ── Public API ────────────────────────────────────────────────────────────────

bool InkaLogic_Init(inka_device_t device)
{
    _device      = device;
    _initialized = false;

    switch (device) {
        case DEVICE_INKALOGIC_BASIC:
            _initialized = Inkalogic_Basic_Init();
            break;
        case DEVICE_INKALOGIC_PRO:
            _initialized = Inkalogic_Pro_Init();
            break;
        default:
            return false;
    }
    return _initialized;
}

void InkaLogic_Attach_Interrupt(void (*isr)(void))
{
    if (!_initialized) return;
    if (_device == DEVICE_INKALOGIC_PRO)
        Inkalogic_Pro_Attach_Interrupt(isr);
}

void InkaLogic_Scan(uint16_t debounce_ms)
{
    if (!_initialized) return;
    if (_device == DEVICE_INKALOGIC_PRO)
        Inkalogic_Pro_Scan(debounce_ms);
}

bool InkaLogic_Scan_ISR(uint16_t debounce_ms)
{
    if (!_initialized)                    return false;
    if (_device != DEVICE_INKALOGIC_PRO)  return false;
    return Inkalogic_Pro_Scan_ISR(debounce_ms);
}

bool InkaLogic_Read(inka_input_t input)
{
    if (!_initialized) return false;

    switch (_device) {
        case DEVICE_INKALOGIC_BASIC:
            if (input > INKA_IN3) return false;
            return Inkalogic_Basic_Digital_Read(_basic_gpio[input]);

        case DEVICE_INKALOGIC_PRO:
            if (input > INKA_IN9) return false;
            return Inkalogic_Pro_Read_Input(_pro_input[input]);

        default:
            return false;
    }
}

void InkaLogic_Write(inka_output_t output, bool state)
{
    if (!_initialized) return;

    switch (_device) {
        case DEVICE_INKALOGIC_BASIC:
            if (output > INKA_OUT3) return;
            Inkalogic_Basic_Digital_Write(_basic_output[output], state);
            break;

        case DEVICE_INKALOGIC_PRO:
            if (output > INKA_OUT5) return;
            Inkalogic_Pro_Write_Output(_pro_output[output], state);
            break;

        default:
            break;
    }
}

void InkaLogic_Toggle(inka_output_t output)
{
    if (!_initialized) return;

    switch (_device) {
        case DEVICE_INKALOGIC_BASIC:
            if (output > INKA_OUT3) return;
            Inkalogic_Basic_Toggle_Output(_basic_output[output]);
            break;

        case DEVICE_INKALOGIC_PRO:
            if (output > INKA_OUT5) return;
            Inkalogic_Pro_Toggle_Output(_pro_output[output]);
            break;

        default:
            break;
    }
}

bool InkaLogic_Rising_Edge(inka_input_t input)
{
    if (!_initialized)                    return false;
    if (_device != DEVICE_INKALOGIC_PRO)  return false;
    if (input > INKA_IN9)                 return false;
    return Inkalogic_Pro_Rising_Edge(_pro_input[input]);
}

bool InkaLogic_Falling_Edge(inka_input_t input)
{
    if (!_initialized)                    return false;
    if (_device != DEVICE_INKALOGIC_PRO)  return false;
    if (input > INKA_IN9)                 return false;
    return Inkalogic_Pro_Falling_Edge(_pro_input[input]);
}

inka_device_t InkaLogic_Get_Device(void)
{
    return _device;
}