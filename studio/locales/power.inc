/* Included by locale.c. Power uses the reserved bytes in the fixed rule tail. */
#include "power.h"
#include "power_policy.h"
void kspw_options(const uint8_t *p,uint8_t out[6]){for(unsigned i=0;i<6;i++)out[i]=p[KSL_LAYER_RULE_AT+i*4+3];}
uint8_t kspw_flags(const uint8_t *p){return p[KSL_LAYER_RULE_AT+15];}
uint8_t kspw_dim(const uint8_t *p){return p[KSL_LAYER_RULE_AT+23];}
uint32_t kspw_idle_ms(const uint8_t *p){uint8_t v[6];kspw_options(p,v);return kspw_policy_idle(v);}
uint32_t kspw_sleep_ms(const uint8_t *p){uint8_t v[6];kspw_options(p,v);return kspw_policy_sleep(v);}
bool kspw_display_off(const uint8_t *p){return kspw_flags(p)&1;}
uint8_t kspw_factor(const uint8_t *p){uint8_t v[6];kspw_options(p,v);return kspw_policy_factor(v);}
bool kspw_valid(const uint8_t p[6]){return kspw_policy_valid(p);}
static bool power_set(uint8_t *p,uint8_t flags,uint8_t dim){
 p[KSL_LAYER_RULE_AT+15]=(kspw_flags(p)&1)|(flags&2);p[KSL_LAYER_RULE_AT+23]=dim;
 uint32_t crc=ksl_crc(p,KSL_CRC_AT);for(unsigned i=0;i<4;i++)p[KSL_CRC_AT+i]=crc>>(i*8);
 return true;
}
bool kspw_set(struct kspw_state *s,uint8_t *p,uint8_t flags,uint8_t dim,uint32_t now){
 if(flags>3||dim>100)return false;
 uint8_t v[6];kspw_options(p,v);
 if((flags&2)==(v[3]&2)&&dim==v[5])return false;
 if(!power_set(p,flags,dim))return false;
 s->changed=now;s->dirty=true;return true;
}
bool kspw_key(uint16_t c){return c==0x7820||c==0x7e20||c==0x7827||c==0x7828||(c>=0x7840&&c<=0x7842)||c==0x7849||c==0x784a;}
bool kspw_action(struct kspw_state *s,uint8_t *p,uint16_t c,bool down,uint32_t now){
 if(!kspw_key(c))return false;
 if(!down)return true;
 uint8_t v[6];kspw_options(p,v);uint8_t flags=v[3],dim=v[5];int step=v[4]?v[4]:5;
 if(c==0x7840)flags&=~2;else if(c==0x7841)flags|=2;
 else if(c==0x7820||c==0x7e20||c==0x7842)flags^=2;
 else {int d=dim+((c==0x7827||c==0x7849)?-step:step);dim=d<0?0:d>100?100:d;}
 kspw_set(s,p,flags,dim,now);
 return true;
}
void kspw_committed(struct kspw_state *s,uint32_t generation){s->base=generation;s->dirty=false;}
void kspw_init(struct kspw_state *s,uint8_t *p,const uint8_t record[8],uint32_t generation){
 memset(s,0,sizeof(*s));kspw_committed(s,generation);
 if(record&&rd32(record)==s->base&&record[4]<=3&&record[5]<=100&&
    rd16(record+6)==(uint16_t)ksl_crc(record,6))power_set(p,record[4],record[5]);
}
bool kspw_due(const struct kspw_state *s,uint32_t now,bool busy){return s->dirty&&!busy&&(uint32_t)(now-s->changed)>=KSPW_SAVE_MS;}
void kspw_record(const struct kspw_state *s,const uint8_t *p,uint8_t out[8]){
 for(unsigned i=0;i<4;i++)out[i]=s->base>>(8*i);
 uint8_t v[6];kspw_options(p,v);out[4]=v[3]&2;out[5]=v[5];
 uint16_t crc=ksl_crc(out,6);out[6]=crc;out[7]=crc>>8;
}
