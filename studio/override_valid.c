#include "override_bridge.h"
#include "locales/locale.h"
bool ks_override_valid(const struct ks_override *e){
    uint16_t codes[2]={e->trigger,e->replacement};
    if(!e->trigger&&!e->replacement&&!e->trigger_mods)return true;
    if(e->options&0x40)return false;
    for(unsigned i=0;i<2;i++){uint16_t c=codes[i];unsigned k=c&255;if(c && !ksl_character(c) && !(c<0x2000 && ((k>=4&&k<=0xa4)||(k>=0xa8&&k<=0xc2))))return false;}
    return true;
}
