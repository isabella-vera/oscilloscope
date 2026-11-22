////// main.c
////// Each captured frame is summarized on the
////// debugger terminal. Later, replace with a function
////// that sends the frame over SPI to the FPGA.

#include <stdio.h>
#include "clock.h"
#include "trigger.h"




// Functions to help debug in the oscope 
static uint32_t next_edge(const uint16_t *f) {
    uint16_t level = trigger_get_level();
    uint16_t rearm = trigger_get_rearm_level();
    uint8_t  armed = 0;
    for (uint32_t i = PRETRIGGER_SAMPLES + 1; i < FRAME_SAMPLES; i++) {
        if (!armed && f[i] < rearm)       armed = 1;
        else if (armed && f[i] >= level)  return i;
    }
    return 0;
}

static void print_frame_summary(uint32_t frameNum) {
    const uint16_t *f = trigger_get_frame();

    uint16_t lo = 0xFFFF, hi = 0;
    for (uint32_t i = 0; i < FRAME_SAMPLES; i++) {
        if (f[i] < lo) lo = f[i];
        if (f[i] > hi) hi = f[i];
    }

    unsigned long levelMv = counts_to_mv(trigger_get_level());

    if (!trigger_frame_was_triggered()) {
        printf("Frame %lu: AUTO (no trigger) TRIG level %lu mV min %lu mV  max %lu mV\n",
               (unsigned long)frameNum, levelMv,
               (unsigned long)counts_to_mv(lo), (unsigned long)counts_to_mv(hi));
        return;
    }

    printf("Frame %lu: TRIG  level %lu mV    min %lu mV  max %lu mV   at index %u %lu mV -> %lu mV ",
           (unsigned long)frameNum, levelMv,
           //(unsigned long)counts_to_mv(f[PRETRIGGER_SAMPLES - 1]),
           //(unsigned long)counts_to_mv(f[PRETRIGGER_SAMPLES]),
           
           (unsigned long)counts_to_mv(lo), (unsigned long)counts_to_mv(hi), PRETRIGGER_SAMPLES, (unsigned long)counts_to_mv(f[PRETRIGGER_SAMPLES - 1]),
           (unsigned long)counts_to_mv(f[PRETRIGGER_SAMPLES]));

    uint32_t e = next_edge(f);
    if (e) {
        uint32_t periodSamples = e - PRETRIGGER_SAMPLES;
        printf("  period %lu samples = %lu Hz\n",
               (unsigned long)periodSamples,
               (unsigned long)(SAMPLE_RATE_HZ / periodSamples));
    } else {
        printf("  (no 2nd edge in frame)\n");
    }
}

int main(void) {
    clock_init();      
    trigger_init();    

    printf("Fixed-rate capture: %lu Sa/s, %u samples/frame (%lu us window)\n",
           (unsigned long)SAMPLE_RATE_HZ, FRAME_SAMPLES,
           (unsigned long)((FRAME_SAMPLES * 1000000ull) / SAMPLE_RATE_HZ));

    uint32_t frameNum = 0;
    while (1) {
        if (trigger_frame_ready()) {
            frameNum++;
            print_frame_summary(frameNum);   
            trigger_rearm();                 // start capturing the next frame
        }
    }
}
