// Esteban Gonzalez and Isabella Vera
// estgonzalez@g.hmc.edu  ivera@g.hmc.edu
// 11/15/2026
// trigger.c
// This file takes care of the trigger. It samples 
// at a fixed rate (for example, 600 samples per frame). Uses DMA in circular buffer
// mode meaning we use a set amount of RAM. The trigger
// is rising edge and aligns the frame to send over to FPGA. 


#include "stm32l432xx.h"
#include "clock.h"
#include "trigger.h"

// Math to convert from milivolts to counts for ADC and hysteresis
#define MV_TO_COUNTS(mv)   (((mv) * ADC_MAX) / VREF_MV)
#define HYST_COUNTS        MV_TO_COUNTS(HYSTERESIS_MV)

// ADC settings for the two modes the ADC switches between
#define ADC_SETTING  ((1u  << ADC_CFGR_EXTEN_Pos) | (13u << ADC_CFGR_EXTSEL_Pos) | (ADC_CFGR_DMAEN) | (ADC_CFGR_DMACFG))

#define INPUT_SIGNAL        ((uint32_t)TRIG_ADC_CH  << ADC_SQR1_SQ1_Pos)
#define TRIGGER_LEVEL         ((uint32_t)LEVEL_ADC_CH << ADC_SQR1_SQ1_Pos) // same positions, just swapping which one gets done

#define TIMER_END          ((SYSCLK_HZ / SAMPLE_RATE_HZ) - 1u)
#define AUTO_SAMPLE       ((SAMPLE_RATE_HZ / 1000u) * AUTO_TIMEOUT_MS)

// Ring buffer the DMA fills. The DMA interrupts at the half-way point and at
// the end, so the CPU processes half while the other half fills up, so a ping pong buffer. Helpful if we want to speed 
// up our sampling rate down the line. 
#define RING_LEN           2048u
#define HALF_LEN           (RING_LEN / 2u)

// helpful debug 
////#if FRAME_SAMPLES + HALF_LEN >= RING_LEN
////#error "FRAME_SAMPLES is too large for RING_LEN"
////#endif
////#if AUTO_SAMPLE < FRAME_SAMPLES
////#error "AUTO_TIMEOUT_MS is too short to fill one frame"
////#endif
////#if PRETRIGGER_SAMPLES >= FRAME_SAMPLES
////#error "PRETRIGGER_SAMPLES must be smaller than FRAME_SAMPLES"
////#endif

//  Arrays 
static volatile uint16_t ring[RING_LEN];     // written by DMA length
static uint16_t          frame[FRAME_SAMPLES]; //our frame samples

// Capture state / fsm 
// Doing absolute counts AKA a monotonically increasing counter
typedef enum { WAITING, CAPTURING, DONE } capture_state;

static volatile capture_state state = DONE;
static volatile uint8_t  frameReady     = 0;
static volatile uint8_t  frameTriggered = 0;

static uint32_t written;     // samples processed so far in this capture 
static uint32_t trigAbs;     // absolute sample number of the trigger 
static uint16_t prev;        // previous sample to compare to current one 
static uint8_t  armed;       // 1 = signal has been below rearmLevel, so it can trigger again
static uint16_t triggerLevel; // trigger voltage counts, read from pin each frame
static uint16_t rearmLevel;   // this ensures there are no false triggers due to noise. Is found by triggerLevel - hysteresis 

// Need to let ADC warm up? stop all functions of cpu for one clock cycle 
static void delay_loops(volatile uint32_t n) {
    while (n--) { __NOP(); }
}
//convert from counts back to millivolts
uint32_t counts_to_mv(uint16_t counts) {
    return ((uint32_t)counts * VREF_MV) / ADC_MAX; // need to typecast since if we multiply the counts by VREF_MV, we could get a value greater than a 16 bit integer can hold. This would cause our math to be messed up.
}

//  Initialization  
static void gpio_init(void) {
    RCC->AHB2ENR |= TRIG_GPIO_EN | LEVEL_GPIO_EN;

    TRIG_GPIO->MODER  |=  (3u << (2u * TRIG_PIN));    // analog mode
    TRIG_GPIO->PUPDR  &= ~(3u << (2u * TRIG_PIN));    // no pull-up/down
    LEVEL_GPIO->MODER |=  (3u << (2u * LEVEL_PIN));   // analog mode
    LEVEL_GPIO->PUPDR &= ~(3u << (2u * LEVEL_PIN));   // no pull-up/down
}


static void adc_init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;

    // ADC clock = HCLK / 1 = 80 MHz (synchronous mode, CKMODE = 01)
    ADC1_COMMON->CCR &= ~ADC_CCR_CKMODE;
    ADC1_COMMON->CCR |=  (1u << ADC_CCR_CKMODE_Pos);

    // turn on the regulator, wait > 20 us
    ADC1->CR &= ~ADC_CR_DEEPPWD;
    ADC1->CR |=  ADC_CR_ADVREGEN;
    delay_loops(4000);

    // Calibrate (single-ended)
    ADC1->CR &= ~(ADC_CR_ADEN | ADC_CR_ADCALDIF);
    ADC1->CR |=  ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL);
    delay_loops(100);

    // Conversions started by TIM6_TRGO, results sent to DMA in circular mode
    uint32_t cfgr = ADC1->CFGR;
             cfgr &= ~(ADC_CFGR_RES | ADC_CFGR_ALIGN | ADC_CFGR_CONT | ADC_CFGR_EXTEN | ADC_CFGR_EXTSEL);
             cfgr |= ADC_SETTING | ADC_CFGR_OVRMOD;             
             ADC1->CFGR = cfgr;

    // One conversion per trigger, on the signal channel
    ADC1->SQR1 = INPUT_SIGNAL;              // L = 0 -> one conversion

    ADC1->SMPR2 &= ~(ADC_SMPR2_SMP15 | ADC_SMPR2_SMP10);
    ADC1->SMPR2 |= (ADC_SMP << ADC_SMPR2_SMP15_Pos)| (LEVEL_ADC_SMP << ADC_SMPR2_SMP10_Pos);  

    // Enable the ADC
    ADC1->ISR = ADC_ISR_ADRDY;
    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY));
}

static void dma_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    // DMA1 Channel 1, request 0 = ADC1
    DMA1_CSELR->CSELR &= ~DMA_CSELR_C1S;

    DMA1_Channel1->CCR  = 0;                         // channel off while configuring
    DMA1_Channel1->CPAR = (uint32_t)&ADC1->DR;       // from ADC data register
    DMA1_Channel1->CMAR = (uint32_t)ring;            // to ring buffer
    DMA1_Channel1->CCR  = DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 | DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_HTIE | DMA_CCR_TCIE | DMA_CCR_PL_1;              

    NVIC_SetPriority(DMA1_Channel1_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

static void timer_init(void) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;

    TIM6->PSC = 0;                       // count at 80 MHz
    TIM6->ARR = TIMER_END;               // overflow 
    TIM6->CR2 = (2u << TIM_CR2_MMS_Pos); // TRGO on every update 
    TIM6->EGR = TIM_EGR_UG;              // load PSC/ARR now
    TIM6->CR1 |= TIM_CR1_CEN;            // start counting 
}

// Swap between looking at measurement signal vs trigger voltage. Need to stop adc first
static void adc_stop(void) {
    if (ADC1->CR & ADC_CR_ADSTART) {
        ADC1->CR |= ADC_CR_ADSTP;
        while (ADC1->CR & ADC_CR_ADSTP);
    }
}

// Read the trigger-level pin while the ADC is stopped between frames.
// Temporarily switches the ADC to software-started single conversions with
// no DMA, takes LEVEL_AVERAGE readings, then restores the capture settings.
// Takes about 30 us.
static void read_trigger_level(void) {
    ADC1->CFGR &= ~ADC_SETTING;
    ADC1->SQR1  = TRIGGER_LEVEL;
    ADC1->ISR   = ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;   // clear leftover flags

    uint32_t sum = 0;
    for (uint32_t i = 0; i < LEVEL_AVERAGE + 1u; i++) {
        ADC1->CR |= ADC_CR_ADSTART;
        while (!(ADC1->ISR & ADC_ISR_EOC));
        uint16_t v = (uint16_t)ADC1->DR;
        if (i > 0) sum += v;               // discard first reading (leftover charge from signal channel)
    }
    triggerLevel = (uint16_t)(sum / LEVEL_AVERAGE); // another typecast
    rearmLevel   = (triggerLevel > HYST_COUNTS) ? (uint16_t)(triggerLevel - HYST_COUNTS) : 0u; //usinge a ternary operator to calculate the voltage threshold

    ADC1->SQR1  = INPUT_SIGNAL;             // back to normal capture settings
    ADC1->CFGR |= ADC_SETTING;
}

static void capture_start(void) {
    adc_stop();
    read_trigger_level();

    // Reset the DMA so the next sample lands at first index
    DMA1_Channel1->CCR  &= ~DMA_CCR_EN;
    DMA1_Channel1->CNDTR = RING_LEN;
    DMA1->IFCR = DMA_IFCR_CGIF1;
    DMA1_Channel1->CCR  |= DMA_CCR_EN;

    written        = 0;
    armed          = 0;
    frameReady     = 0;
    frameTriggered = 0;
    state          = WAITING;

    ADC1->ISR = ADC_ISR_OVR | ADC_ISR_EOC | ADC_ISR_EOS;
    ADC1->CR |= ADC_CR_ADSTART;   // when TIM6 overflow starts a conversion
}

// Copy fram samples starting at absolute sample number start from the ring.
// Called from the DMA interrupt right after the samples are complete. The
// oldest sample needed is at most samples + half length behind the newest.
static void copy_frame(uint32_t start) {
    for (uint32_t i = 0; i < FRAME_SAMPLES; i++) {
        frame[i] = ring[(start + i) % RING_LEN];
    }
}

static void finish_frame(uint8_t triggered) {
    adc_stop();                 // stop sampling until the frame has been sent
    frameTriggered = triggered; //was it triggered or auto
    state          = DONE; //flag
    frameReady     = 1; //flag
}

// Run the trigger logic on one half of the ring 
static void process_half(uint32_t ringOffset) {
    if (state == WAITING) {
        for (uint32_t k = 0; k < HALF_LEN; k++) {
            uint32_t n = written + k;          // absolute sample number
            uint16_t s = ring[ringOffset + k];

            if (n == 0) {                      // very first sample, no previous ones so just save current one
                prev = s;
                continue;
            }

            if (!armed) {
                if (s < rearmLevel) armed = 1;
            } else if (s >= triggerLevel) {
                if (n >= PRETRIGGER_SAMPLES) {
                    trigAbs = n;               // rising edge with enough frames to show before it, doesn't matter if we're showing zero frames before trigger but for step responses could play a role.
                    state   = CAPTURING;
                    break;
                }
                armed = 0;                     // too early for a full frame so it skips this edge. Again, only really matters for step responses
            }
            prev = s;
        }
    }

    written += HALF_LEN;

    if (state == CAPTURING) {
        uint32_t start = trigAbs - PRETRIGGER_SAMPLES;
        if (written >= start + FRAME_SAMPLES) {
            copy_frame(start);
            finish_frame(1);
        }
    } else if (state == WAITING && written >= AUTO_SAMPLE) {
        copy_frame(written - FRAME_SAMPLES);   // sends over samples if no edge is found
        finish_frame(0);
    }
}

void DMA1_Channel1_IRQHandler(void) {
    uint32_t isr = DMA1->ISR;
    //clears the flag and tells cpu to analyze samples
    if (isr & DMA_ISR_HTIF1) {                 // first half full
        DMA1->IFCR = DMA_IFCR_CHTIF1;
        if (state != DONE) process_half(0);
    }
    //clears the flag and tells cpu to analyze second half samples 
    if (isr & DMA_ISR_TCIF1) {                 // second half full
        DMA1->IFCR = DMA_IFCR_CTCIF1;
        if (state != DONE) process_half(HALF_LEN);
    }
}

//  Public functions 
void trigger_init(void) {
    gpio_init();
    adc_init();
    dma_init();
    timer_init();
    capture_start();
}

uint8_t trigger_frame_ready(void)          { return frameReady; }
const uint16_t *trigger_get_frame(void)    { return frame; }
uint8_t trigger_frame_was_triggered(void)  { return frameTriggered; }
void trigger_rearm(void)                   { capture_start(); }
uint16_t trigger_get_level(void)           { return triggerLevel; }
uint16_t trigger_get_rearm_level(void)     { return rearmLevel; }



