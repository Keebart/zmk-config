#include "hardware.h"
#include "physical/physical.h"
#define KSH_MODEL KS_MODEL_ID
#include "physical/record.inc"
#include <zephyr/sys/util.h>
bool ks_display_present(void) {
    return ksh_flags() & (IS_ENABLED(CONFIG_KEEBART_STUDIO) ? KSH_LEFT_DISPLAY : KSH_RIGHT_DISPLAY);
}
