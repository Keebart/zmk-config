#include "physical.h"
#include <string.h>
bool ksh_valid(uint16_t model, uint8_t flags) {
    if (flags & ~7u) return false;
    switch (model) {
    case 0x0004: case 0x0304: case 0x0306: return true;
    case 0x0005: case 0x0305: return !(flags & KSH_COMPACT);
    case 0x0204: return !(flags & ~KSH_COMPACT);
    case 0x0006: return flags == 0;
    default: return false;
    }
}
uint32_t ksh_crc(const uint8_t *bytes, unsigned length) {
    uint32_t crc = ~0u;
    for (unsigned i=0;i<length;i++) {
        crc ^= bytes[i];
        for (unsigned bit=0;bit<8;bit++) crc = (crc >> 1) ^ (0xedb88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
uint8_t ksh_decode(const uint8_t record[KSH_BYTES], uint16_t model) {
    uint32_t crc = (uint32_t)record[12] | ((uint32_t)record[13]<<8) |
                   ((uint32_t)record[14]<<16) | ((uint32_t)record[15]<<24);
    if (memcmp(record,"KSHW0001",8) || record[8]!=(uint8_t)model ||
        record[9]!=(uint8_t)(model>>8) || record[10]!=1 ||
        !ksh_valid(model,record[11]) || crc != ksh_crc(record,12)) return 0;
    return record[11];
}
uint8_t ksh_led(uint16_t model, uint8_t flags, uint8_t index) {
    unsigned count = (model==0x0005 || model==0x0305) ? 30 :
                     (model==0x0006 ? 18 : (model==0x0306 ? 21 : 23));
    if ((flags & KSH_COMPACT) && (model==0x0004 || model==0x0204 || model==0x0304 || model==0x0306)) {
        count -= 3;
        if (index >= count) return 255;
        /* The three outer-column LEDs follow the first eighteen LEDs. */
        return index < 18 ? index : index+3;
    }
    return index < count ? index : 255;
}
