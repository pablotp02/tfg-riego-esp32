#include "fsm.h"
#include "dht22_test.h" 

void app_main(void)
{
 /* dht22_test_start();
    return; // solo para la prueba del dht22 y no arrancar la fsm */
    system_ctx_t ctx;
    fsm_init(&ctx);

    while (1) {
        fsm_step(&ctx);
    }
}
