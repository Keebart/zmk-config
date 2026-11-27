#pragma once
#include <stdbool.h>
#include <stdint.h>
static inline uint32_t kspw_policy_idle(const uint8_t p[6]){return p[0]==255?UINT32_MAX:(p[0]?p[0]:5)*60000U;}
static inline uint32_t kspw_policy_sleep(const uint8_t p[6]){unsigned n=p[1]|(unsigned)p[2]<<8;return n==65535?UINT32_MAX:(n?n:60)*60000U;}
static inline uint8_t kspw_policy_factor(const uint8_t p[6]){return p[3]&2?0:(100-p[5])*255/100;}
static inline bool kspw_policy_display_dark(const uint8_t p[6]){return (p[3]&4)!=0;}
static inline bool kspw_policy_valid(const uint8_t p[6]){
 unsigned idle=p[0]==255?0:p[0]?p[0]:5,sleep=p[1]|(unsigned)p[2]<<8;sleep=sleep==65535?0:sleep?sleep:60;
 return idle<=120&&sleep<=1440&&(!sleep||sleep>=idle)&&p[3]<=7&&p[4]<=25&&p[5]<=100;
}
static inline uint16_t kspw_policy_crc(const uint8_t *p,unsigned n){uint16_t c=0xffff;while(n--){c^=*p++<<8;for(unsigned i=0;i<8;i++)c=c&0x8000?(c<<1)^0x1021:c<<1;}return c;}
