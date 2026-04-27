/**
 * @file    InkaLogicBasic.cpp
 * @brief   Low-level driver implementation for InkaLogic Basic.
 *
 * @details Manages a shadow register (`_output_state`) that mirrors the current
 *          PCA9536 output latch, avoiding unnecessary I2C reads before writes.
 *          All I2C communication targets address `PCA9536_ADDR` (0x41).
 */

#include "InkaLogicBasic.h"

// ── Output shadow register ────────────────────────────────────────────────────

/**
 * Mirrors the current PCA9536 output latch.
 * Updated on every write / toggle so the driver never needs to read the
 * output register over I2C before modifying individual bits.
 */
static uint8_t _output_state = 0x00;

// ── Private helper ────────────────────────────────────────────────────────────

/**
 * @brief  Writes a single byte to a PCA9536 register.
 *
 * @param  reg    Target register address.
 * @param  value  Byte to write.
 * @return `true` if the I2C transaction completed without error.
 */
static bool _pca_write(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(PCA9536_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
}

// ── Public API ────────────────────────────────────────────────────────────────

bool Inkalogic_Basic_Init(void)
{
    // Configure digital input GPIOs
    pinMode(BASIC_GPIO_IN0, INPUT);
    pinMode(BASIC_GPIO_IN1, INPUT);
    pinMode(BASIC_GPIO_IN2, INPUT);
    pinMode(BASIC_GPIO_IN3, INPUT);

    // Verify PCA9536 presence on the bus
    Wire.beginTransmission(PCA9536_ADDR);
    if (Wire.endTransmission() != 0)
        return false;

    // Configure all 4 PCA9536 pins as outputs (CONFIG_REG = 0x00)
    if (!_pca_write(CONFIG_REG, 0x00))
        return false;

    // Drive all outputs LOW on startup
    _output_state = 0x00;
    if (!_pca_write(OUTPUT_PORT_REG, _output_state))
        return false;

    return true;
}

bool Inkalogic_Basic_Digital_Read(uint8_t gpio)
{
    return digitalRead(gpio);
}

void Inkalogic_Basic_Digital_Write(basic_output_t output, bool state)
{
    if (state)
        _output_state |=  (1u << output);
    else
        _output_state &= ~(1u << output);

    _output_state &= 0x0Fu;   // Guard upper bits — only 4 outputs are valid

    _pca_write(OUTPUT_PORT_REG, _output_state);
}

void Inkalogic_Basic_Toggle_Output(basic_output_t output)
{
    _output_state ^= (1u << output);
    _output_state &= 0x0Fu;   // Guard upper bits

    _pca_write(OUTPUT_PORT_REG, _output_state);
}