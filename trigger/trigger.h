// trigger.h
// Rising-edge trigger on an analog input (0 - 3.3 V) using ADC1
// on the STM32L432KC. Input pin is set below.

#ifndef TRIGGER_H
#define TRIGGER_H

#include <stdint.h>

// ---------------- Input pin ----------------
// Pick a pin from this table and set the three defines to match.
//
//   Pin   Nucleo   ADC channel   Notes
//   PA0   A0       5
//   PA1   A1       6
//   PA3   A2       8
//   PA4   A3       9
//   PA5   A4       10            shorted to D5 (PB6) by SB18 by default
//   PA6   A5       11            shorted to D4 (PB7) by SB16 by default
//   PA7   A6       12
//   PB0   D3       15
//   PB1   D6       16
//   (PA2 = A7 is the USB virtual COM port TX, so avoid it)
//
#define TRIG_GPIO        GPIOB               // GPIOA or GPIOB
#define TRIG_GPIO_EN     RCC_AHB2ENR_GPIOBEN  // RCC_AHB2ENR_GPIOAEN or _GPIOBEN
#define TRIG_PIN         0                    // pin number within the port
#define TRIG_ADC_CH      15                    // ADC1 channel from the table

// ---------------- User settings ----------------
#define VREF_MV            3300u   // VDDA on the Nucleo board
#define ADC_MAX            4095u   // 12-bit ADC
#define TRIGGER_LEVEL_MV   1000u   // trigger threshold (1.65 V)
#define HYSTERESIS_MV      500u    // must drop this far below level to re-arm

// ---------------- Functions ----------------

// Set up PA0 and ADC1, then arm the trigger. Call once at startup.
void trigger_init(void);

// Take one ADC sample and run the trigger logic.
// Returns 1 if a rising-edge trigger happened on this sample, 0 otherwise.
// Prints a message to the debugger on each trigger.
uint8_t trigger_update(void);

// Read one raw 12-bit ADC sample from PA0.
uint16_t adc_read(void);

// Convert ADC counts to millivolts.
uint32_t counts_to_mv(uint16_t counts);

#endif // TRIGGER_H