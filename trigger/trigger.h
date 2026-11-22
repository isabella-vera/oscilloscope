// Esteban Gonzalez and Isabella Vera
// estgonzalez@g.hmc.edu  ivera@g.hmc.edu
// 11/15/2026
// trigger.h
// Header file for trigger. Sets all the settings necessary. 


#ifndef TRIGGER_H
#define TRIGGER_H

#include <stdint.h>
// Measurement signal pin
#define TRIG_GPIO        GPIOB                
#define TRIG_GPIO_EN     RCC_AHB2ENR_GPIOBEN  
#define TRIG_PIN         0                    
#define TRIG_ADC_CH      15                   

// Trigger Voltage Pin
#define LEVEL_GPIO       GPIOA
#define LEVEL_GPIO_EN    RCC_AHB2ENR_GPIOAEN
#define LEVEL_PIN        5                    
#define LEVEL_ADC_CH     10
#define LEVEL_ADC_SMP    6u                   
#define LEVEL_AVERAGE    8u                   

//  Sampling
#define SAMPLE_RATE_HZ   100000u   // ADC samples per second 
// ADC sample time. Amount of cycles for a sample
#define ADC_SMP          5u

//  Frame
#define FRAME_SAMPLES       640u   // one sample per pixel for each coloumn
#define PRETRIGGER_SAMPLES  160u   // samples shown before the trigger. For step responses
#define AUTO_TIMEOUT_MS     100u   // no trigger this long still send a frame over

//  Trigger 
#define VREF_MV            3300u   // 4095 counts = 3.3 V 
#define ADC_MAX            4095u   // 12-bit ADC
#define HYSTERESIS_MV      500u    // must drop this far below the level to retrigger


//  Functions

// Set up the pin, ADC, DMA and TIM6, then start capturing the first frame
void trigger_init(void);

// 1 when a complete frame is waiting to be sent
uint8_t trigger_frame_ready(void);

// Return the pointer for the array of measured voltages for our frame since 
const uint16_t *trigger_get_frame(void);

// 1 if the frame was captured on a trigger, 0 if it came from the auto timeout
uint8_t trigger_frame_was_triggered(void);

// Call after you've finished with the frame to start capturing the next one
void trigger_rearm(void);

// Trigger level and re-arm level (in ADC counts) used for the current frame
uint16_t trigger_get_level(void);
uint16_t trigger_get_rearm_level(void);

// Convert ADC counts to millivolts
uint32_t counts_to_mv(uint16_t counts);

#endif

