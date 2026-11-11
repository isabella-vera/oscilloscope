// main.c
// Rising-edge trigger test: signal on PA0 (Nucleo A0), 0 - 3.3 V.
// Trigger messages print to the debugger terminal.
 
#include "trigger.h"
 
int main(void) {
    trigger_init();
 
    while (1) {
        trigger_update();
    }
}

//#include <stdio.h>
//#include "trigger.h"

//int main(void) {
//    trigger_init();

//    uint32_t n = 0;
//    while (1) {
//        uint16_t s = adc_read();
//        if (++n % 20000 == 0) {          // print every 20000th sample
//            printf("raw=%u  mv=%lu\n", s, (unsigned long)counts_to_mv(s));
//        }
//    }
//}