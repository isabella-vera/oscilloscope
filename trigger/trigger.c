// trigger.c
// Rising-edge trigger on an analog input using ADC1 on the STM32L432KC.
//
// Trigger logic (with hysteresis so noise doesn't cause false triggers):
//   - The trigger "arms" once the signal falls below (LEVEL - HYST).
//   - While armed, the first sample at or above LEVEL fires the trigger
//     and disarms it until the signal drops below (LEVEL - HYST) again.

#include <stdio.h>
#include "stm32l432xx.h"
#include "trigger.h"

// Convert millivolts to ADC counts at compile time
#define MV_TO_COUNTS(mv)   (((mv) * ADC_MAX) / VREF_MV)
#define TRIGGER_LEVEL      MV_TO_COUNTS(TRIGGER_LEVEL_MV)
#define REARM_LEVEL        MV_TO_COUNTS(TRIGGER_LEVEL_MV - HYSTERESIS_MV)

// ---------------- Trigger state (private to this file) ----------------
static uint16_t prevSample = 0;
static uint8_t  armed      = 0;
static uint32_t trigCount  = 0;
static uint32_t sampleNum  = 0;

// ---------------- Helpers ----------------
static void delay_loops(volatile uint32_t n) {
    while (n--) { __NOP(); }
}

uint32_t counts_to_mv(uint16_t counts) {
    return ((uint32_t)counts * VREF_MV) / ADC_MAX;
}

// ---------------- ADC ----------------
static void adc_init(void) {
        // 1. Clocks: GPIOB and the ADC
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_ADCEN;

    // 2. PB0 -> analog mode (MODER = 0b11), no pull-up/down
    GPIOB->MODER |=  (3u << GPIO_MODER_MODE0_Pos);
    GPIOB->PUPDR &= ~(3u << GPIO_PUPDR_PUPD0_Pos);

    // 3. ADC clock = HCLK / 1 (synchronous mode, CKMODE = 01)
    ADC1_COMMON->CCR &= ~ADC_CCR_CKMODE;
    ADC1_COMMON->CCR |=  (1u << ADC_CCR_CKMODE_Pos);

    // 4. Wake the ADC: leave deep power-down, turn on the voltage regulator
    ADC1->CR &= ~ADC_CR_DEEPPWD;
    ADC1->CR |=  ADC_CR_ADVREGEN;
    delay_loops(2000);          // need >= 20 us

    // 5. Calibrate (ADC must be disabled, single-ended mode)
    ADC1->CR &= ~ADC_CR_ADEN;
    ADC1->CR &= ~ADC_CR_ADCALDIF;
    ADC1->CR |=  ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL);   // hardware clears when done
    delay_loops(100);

    // 6. Conversion settings
    ADC1->CFGR &= ~ADC_CFGR_RES;       // 12-bit resolution
    ADC1->CFGR &= ~ADC_CFGR_ALIGN;     // right aligned
    ADC1->CFGR &= ~ADC_CFGR_CONT;      // single conversion per ADSTART
    ADC1->CFGR &= ~ADC_CFGR_EXTEN;     // software triggered (for now)
    ADC1->CFGR |=  ADC_CFGR_OVRMOD;    // overwrite DR if a value is missed

     // Sequence: 1 conversion (L = 0), channel IN15 (PB0)
    ADC1->SQR1 = (0u << ADC_SQR1_L_Pos) | (15u << ADC_SQR1_SQ1_Pos);

    // Sample time for channel 15: 47.5 ADC cycles (SMP = 0b100)
    ADC1->SMPR2 &= ~ADC_SMPR2_SMP15;
    ADC1->SMPR2 |=  (4u << ADC_SMPR2_SMP15_Pos);

    // 7. Enable the ADC and wait until it's ready
    ADC1->ISR |= ADC_ISR_ADRDY;        // clear ready flag (write 1)
    ADC1->CR  |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY));
}

uint16_t adc_read(void) {
    ADC1->CR |= ADC_CR_ADSTART;               // start one conversion
    while (!(ADC1->ISR & ADC_ISR_EOC));       // wait for end of conversion
    return (uint16_t)ADC1->DR;                // reading DR clears EOC
}

// ---------------- Trigger ----------------
void trigger_init(void) {
    adc_init();

    printf("ADC trigger test\n");
    printf("Trigger level: %u mV (%u counts), re-arm below %u mV (%u counts)\n",
           TRIGGER_LEVEL_MV, TRIGGER_LEVEL,
           TRIGGER_LEVEL_MV - HYSTERESIS_MV, REARM_LEVEL);

    prevSample = adc_read();
    armed      = (prevSample < REARM_LEVEL);  // only arm if starting low
    trigCount  = 0;
    sampleNum  = 0;
}

uint8_t trigger_update(void) {
    uint16_t sample = adc_read();
    uint8_t  fired  = 0;
    sampleNum++;

    if (!armed && sample < REARM_LEVEL) {
        armed = 1;                              // signal went low: arm
    } else if (armed && sample >= TRIGGER_LEVEL) {
        armed = 0;                              // rising edge: fire
        fired = 1;
        trigCount++;
        printf("TRIGGER #%lu at sample %lu: %lu mV -> %lu mV\n",
               (unsigned long)trigCount,
               (unsigned long)sampleNum,
               (unsigned long)counts_to_mv(prevSample),
               (unsigned long)counts_to_mv(sample));
    }

    prevSample = sample;
    return fired;
}