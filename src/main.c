#include "fsm.h"

// comentario para probar el commit
void app_main(void)
{
    system_ctx_t ctx;
    fsm_init(&ctx);

    while (1) {
        fsm_step(&ctx);
    }
}
