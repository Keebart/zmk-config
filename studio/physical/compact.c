#include "physical.h"
#include <string.h>
struct ksh_byte { uint16_t at; uint8_t value; };
#include "compact_data.h"
static bool compact(uint16_t model,uint8_t flags) {
    return (flags & KSH_COMPACT) && (model==4 || model==0x0204 || model==0x0304 || model==0x0306);
}
uint16_t ksh_compact_key(uint16_t model,uint8_t flags,unsigned layer,unsigned cell,uint16_t fallback) {
    if(!compact(model,flags)) return fallback;
    if(layer>=7) return 0;
    if(model==4 || model==0x0204 || model==0x0304) {
        if(cell>=56) return 0;
        unsigned row=cell/7,col=cell%7;
        if(col==6 && row%4<2 && (flags & (row<4 ? KSH_LEFT_DISPLAY : KSH_RIGHT_DISPLAY))) return 0;
        return model==0x0304 ? compact_keys_772[layer][cell] : compact_keys_4[layer][cell];
    }
    return cell<48 ? compact_keys_774[layer][cell] : 0;
}
uint16_t ksh_compact_position(uint16_t model,uint8_t flags,unsigned index,uint16_t fallback) {
    if(!compact(model,flags) || (model!=0x0304 && model!=0x0306)) return fallback;
    const uint8_t *positions=model==0x0304 ? compact_positions_772 : compact_positions_774;
    unsigned count=model==0x0304 ? 46 : 42;
    return ksh_compact_key(model,flags,index/count,positions[index%count],0);
}
bool ksh_compact_locale(uint16_t model,uint8_t flags,uint8_t *record) {
    if(!compact(model,flags))return false;
    const struct ksh_byte *bytes;
    unsigned count;
#define CHOOSE(id) bytes=compact_locale_##id;count=sizeof(compact_locale_##id)/sizeof(*bytes)
    switch(model) {
    case 4: case 0x0204: CHOOSE(4);break;
    case 0x0304: CHOOSE(772);break;
    default: CHOOSE(774);break;
    }
#undef CHOOSE
    memset(record,0,3584);
    for(unsigned i=0;i<count;i++)record[bytes[i].at]=bytes[i].value;
    return true;
}
