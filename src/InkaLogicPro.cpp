/**
 * @file    InkaLogicPro.cpp
 * @brief   Low-level driver implementation for InkaLogic Pro (TCA9535).
 *
 * @details Maintains three internal state variables:
 *          - `_cur`     : confirmed input snapshot (updated after debounce).
 *          - `_prev`    : previous confirmed snapshot (used for edge detection).
 *          - `_out`     : shadow of the PORT1 output latch.
 *
 *          Debounce is implemented per-pin: each pin that changes state
 *          is timestamped and placed in `_pending`. It is only committed to
 *          `_cur` once it has been stable for at least `debounce_ms`
 *          milliseconds.
 */

#include "InkaLogicPro.h"

// ── Internal state ────────────────────────────────────────────────────────────

static uint16_t _cur     = 0x0000; ///< Current confirmed input snapshot
static uint16_t _prev    = 0x0000; ///< Previous confirmed snapshot (edge detection)
static uint8_t  _out     = 0x00;   ///< PORT1 output latch shadow

/** Per-pin timestamp of the last detected change (for debounce). */
static unsigned long _change_ts[TCA9535_INPUT_COUNT] = {0};

/** Bitmask of input pins currently awaiting debounce confirmation. */
static uint16_t _pending = 0x0000;

// ── Private helper: read both ports (repeated-start) ─────────────────────────

/**
 * @brief  Reads PORT0 and PORT1 from the TCA9535 in a single repeated-start transaction.
 * @return 16-bit raw value: `(PORT1 << 8) | PORT0`.
 */
static uint16_t _read_ports(void)
{
    Wire.beginTransmission(TCA9535_ADDRESS);
    Wire.write(TCA9535_REG_INPUT0);
    Wire.endTransmission(false);   // Repeated-start
    Wire.requestFrom(TCA9535_ADDRESS, 2);
    uint8_t p0 = Wire.read();
    uint8_t p1 = Wire.read();
    return ((uint16_t)p1 << 8) | p0;
}

// ── Private helper: write output latch ───────────────────────────────────────

/**
 * @brief  Writes the output shadow `_out` to `TCA9535_REG_OUTPUT1`.
 */
static void _write_output(void)
{
    Wire.beginTransmission(TCA9535_ADDRESS);
    Wire.write(TCA9535_REG_OUTPUT1);
    Wire.write(_out);
    Wire.endTransmission();
}

// ── Private helper: debounce state machine ────────────────────────────────────

/**
 * @brief  Runs one iteration of the per-pin debounce state machine.
 *
 * @details For each bit that has changed relative to `_cur`:
 *          - If not yet in `_pending`, records the current timestamp and marks
 *            it as pending.
 *          - If already pending and the stabilisation time has elapsed, commits
 *            the new value to `_cur` and clears the pending flag.
 *
 * @param  raw          Raw 16-bit port reading from `_read_ports()`.
 * @param  debounce_ms  Stabilisation window in milliseconds. `0` = instant commit.
 */
static void _process_debounce(uint16_t raw, uint16_t debounce_ms)
{
    uint16_t changed = raw ^ _cur;

    if (changed == 0 && _pending == 0)
        return;

    unsigned long now = millis();

    for (uint8_t i = 0; i < TCA9535_INPUT_COUNT; i++) {
        uint16_t mask = (1u << i);

        // New change detected — start the debounce timer for this pin
        if ((changed & mask) && !(_pending & mask)) {
            _change_ts[i] = now;
            _pending |= mask;
        }

        if (_pending & mask) {
            bool stable = (debounce_ms == 0) ||
                          ((now - _change_ts[i]) >= debounce_ms);

            if (stable) {
                bool raw_bit = (raw  >> i) & 0x01u;
                bool cur_bit = (_cur >> i) & 0x01u;

                // Commit only if the pin level actually changed
                if (raw_bit != cur_bit)
                    _cur = (_cur & ~mask) | (raw & mask);

                _pending      &= ~mask;
                _change_ts[i]  = 0;
            }
        }
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

bool Inkalogic_Pro_Init(void)
{
    // Check device presence
    Wire.beginTransmission(TCA9535_ADDRESS);
    if (Wire.endTransmission() != 0)
        return false;

    // PORT0 — all inputs
    Wire.beginTransmission(TCA9535_ADDRESS);
    Wire.write(TCA9535_REG_CONFIG0);
    Wire.write(0xFF);
    if (Wire.endTransmission() != 0)
        return false;

    // PORT1 — P10/P11 inputs, P12-P17 outputs (0x03 = 0b00000011)
    Wire.beginTransmission(TCA9535_ADDRESS);
    Wire.write(TCA9535_REG_CONFIG1);
    Wire.write(0x03);
    if (Wire.endTransmission() != 0)
        return false;

    // Drive all outputs LOW
    _out = 0x00;
    _write_output();

    // Take initial snapshot to prevent false edges on first Scan
    _prev = _cur = _read_ports();

    return true;
}

uint16_t Inkalogic_Pro_Read_All(void)
{
    uint16_t new_val = _read_ports();
    if (new_val != _cur) {
        _prev = _cur;
        _cur  = new_val;
    }
    return _cur;
}

void Inkalogic_Pro_Scan(uint16_t debounce_ms)
{
    uint16_t raw = _read_ports();
    _prev = _cur;   // Always update _prev at the start (polling mode)
    _process_debounce(raw, debounce_ms);
}

bool Inkalogic_Pro_Scan_ISR(uint16_t debounce_ms)
{
    // In interrupt mode _prev must only be updated once all pending pins are
    // confirmed, so that edges are never lost when debounce spans multiple
    // loop() cycles.
    //
    // Sequence mirrors what worked in the original single-file sketch:
    //   last_input_state = input_state;   ← save _prev first
    //   Read_All_Input();                 ← then read hardware
    //   Rising/Falling compares both      ← sketch reads after

    if (_pending == 0) {
        // No debounce in progress — safe to advance the snapshot baseline.
        _prev = _cur;
    }

    uint16_t raw = _read_ports();
    _process_debounce(raw, debounce_ms);

    // Return true only when every pending pin has been confirmed.
    // At that point _prev holds the pre-transition state and _cur holds the
    // new state, so Rising / Falling edge detection will be correct.
    return (_pending == 0);
}

bool Inkalogic_Pro_Read_Input(pro_input_t input)
{
    return (_cur >> input) & 0x01u;
}

bool Inkalogic_Pro_Rising_Edge(pro_input_t input)
{
    return ((_cur  >> input) & 0x01u) &&
          !((_prev >> input) & 0x01u);
}

bool Inkalogic_Pro_Falling_Edge(pro_input_t input)
{
    return !((_cur  >> input) & 0x01u) &&
            ((_prev >> input) & 0x01u);
}

void Inkalogic_Pro_Write_Output(pro_output_t output, bool state)
{
    if (state) _out |=  (1u << output);
    else       _out &= ~(1u << output);
    _write_output();
}

void Inkalogic_Pro_Toggle_Output(pro_output_t output)
{
    _out ^= (1u << output);
    _write_output();
}

void Inkalogic_Pro_Attach_Interrupt(void (*isr)(void))
{
    pinMode(PRO_IO_INT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(PRO_IO_INT_PIN), isr, FALLING);
}