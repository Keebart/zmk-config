#include "locale.h"
#include "locale_data.h"
#include <string.h>
static uint16_t rd16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t rd32(const uint8_t *p){return rd16(p)|(uint32_t)rd16(p+2)<<16;}
uint32_t ksl_crc(const uint8_t *p,size_t n){uint32_t c=~0U;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((c&1)?0xedb88320U:0);}return ~c;}
static bool zero(const uint8_t *p,size_t n){while(n--)if(*p++)return false;return true;}
static bool utf8(const uint8_t *p,size_t n){
    for(size_t i=0;i<n;){
        uint32_t c=p[i++];unsigned extra=0;uint32_t min=0;
        if(c<0x80){if(c<32||c==127)return false;continue;}
        if(c>=0xc2&&c<=0xdf){extra=1;c&=31;min=0x80;}
        else if(c>=0xe0&&c<=0xef){extra=2;c&=15;min=0x800;}
        else if(c>=0xf0&&c<=0xf4){extra=3;c&=7;min=0x10000;}else return false;
        if(i+extra>n)return false;
        while(extra--){uint8_t v=p[i++];if((v&0xc0)!=0x80)return false;c=(c<<6)|(v&63);}
        if(c<min||c>0x10ffff||(c>=0xd800&&c<=0xdfff)||(c>=0x80&&c<=0x9f))return false;
    }return true;
}
bool ksl_supported(uint16_t code){return ksl_character(code)||(code>=KSL_SELECT&&code<=KSL_PREVIOUS)||(code>=KSL_COMPOUND&&code<KSL_COMPOUND+KSL_ACTIONS);}
bool ksl_compound_key(uint16_t c){
    return ksl_character(c)||c<=0x1fff||(c>=0x5000&&c<=0x52ff)||(c>=0x7700&&c<=0x770f)||
        (c>=0x7800&&c<=0x784c)||(c>=0x7c00&&c<=0x7c01)||(c>=0x7c5d&&c<=0x7c5f)||
        c==0x7c73||(c>=0x7c77&&c<=0x7c7a)||c==0x7c7b||(c>=KSL_SELECT&&c<=KSL_PREVIOUS)||
        (c>=0x7e00&&c<=0x7e04)||(c>=0x7e10&&c<=0x7e13)||(c>=0x7e20&&c<=0x7e22);
}
void ksl_default(uint8_t *p){memset(p,0,KSL_BYTES);memcpy(p,"KSL2",4);uint32_t crc=ksl_crc(p,KSL_CRC_AT);for(unsigned i=0;i<4;i++)p[KSL_CRC_AT+i]=crc>>(i*8);}
// Stock is distinct from an empty record: migrations must preserve raw keymaps.
void ksl_stock(uint8_t *p){
    ksl_default(p);p[4]=1;p[8]=KSL_STOCK_LAYOUT;p[9]=KSL_STOCK_LAYOUT>>8;
    p[10]=255;p[11]=11;memcpy(p+12,"My computer",11);
    uint32_t crc=ksl_crc(p,KSL_CRC_AT);for(unsigned i=0;i<4;i++)p[KSL_CRC_AT+i]=crc>>(i*8);
}
uint16_t ksl_stock_code(uint16_t code){
    for(unsigned i=0;i<sizeof(ksl_stock_characters)/sizeof(*ksl_stock_characters);i++)
        if(ksl_stock_characters[i][0]==code)return ksl_stock_characters[i][1];
    return code;
}
static bool common_valid(const uint8_t *p){
    if(p[4]>KSL_COMPUTERS||p[6]>KSL_ACTIONS||p[7]>1)return false;
    if((p[4]&&p[5]>=p[4])||(!p[4]&&(p[5]||p[6])))return false;
    unsigned slots=0;
    for(unsigned i=0;i<KSL_COMPUTERS;i++){
        const uint8_t *c=p+8+i*36;if(i>=p[4]){if(!zero(c,36))return false;continue;}
        if(rd16(c)>=KSL_LAYOUT_COUNT||!c[3]||c[3]>32||!utf8(c+4,c[3])||!zero(c+4+c[3],32-c[3]))return false;
        if(c[2]!=255){if(c[2]>=5||(slots&(1U<<c[2])))return false;slots|=1U<<c[2];}
    }
    for(unsigned i=0;i<KSL_ACTIONS;i++){
        const uint8_t *a=p+224+i*8;if(i>=p[6]){if(!zero(a,8))return false;continue;}
        uint16_t hold=rd16(a+2),term=rd16(a+4);
        if(!ksl_compound_key(rd16(a))||!term||term>1000||(hold<=1||!ksl_compound_key(hold))||(a[6]&~15)||((a[6]&3)==3)||a[7])return false;
    }
    return true;
}
bool ksl_legacy_valid(const uint8_t *p){return !memcmp(p,"KSL1",4)&&rd32(p+508)==ksl_crc(p,508)&&common_valid(p)&&zero(p+480,28);}
bool ksl_valid(const uint8_t *p){
    if(memcmp(p,"KSL2",4)||rd32(p+KSL_CRC_AT)!=ksl_crc(p,KSL_CRC_AT)||!common_valid(p))return false;
    unsigned count=rd16(p+480);
    if(count>KSL_BINDINGS||(!p[4]&&count)||p[482]>1||p[483])return false;
    for(unsigned i=0;i<count;i++)for(unsigned computer=0;computer<KSL_COMPUTERS;computer++){
        uint16_t code=rd16(p+KSL_BINDING_AT+i*12+computer*2);
        if(computer>=p[4]){if(code)return false;continue;}
        if(code>=0x8000&&!ksl_character(code)&&!(code>=KSL_COMPOUND&&code<KSL_COMPOUND+p[6]))return false;
        if(code>=KSL_SELECT&&code<KSL_NEXT&&code>=KSL_SELECT+p[4])return false;
    }
    unsigned timing=p[482]?p[6]*4:0;
    if(KSL_BINDING_AT+count*12+timing>KSL_LAYER_RULE_AT)return false;
    if(!zero(p+KSL_BINDING_AT+count*12,KSL_LAYER_RULE_AT-KSL_BINDING_AT-count*12-timing))return false;
    if(timing){
        bool any=false;
        for(unsigned i=KSL_LAYER_RULE_AT-timing;i<KSL_LAYER_RULE_AT;i+=2){unsigned term=rd16(p+i);if(term>1000)return false;any|=term!=0;}
        if(!any)return false;
    }else if(p[482])return false;
    uint16_t all_inputs=0,all_targets=0;bool ended=false;
    for(unsigned i=0;i<KSL_LAYER_RULES;i++){
        const uint8_t *rule=p+KSL_LAYER_RULE_AT+i*4;unsigned mask=rd16(rule),target=rule[2];
        if(!mask){if(!zero(rule,4))return false;ended=true;continue;}
        if(ended||rule[3]||(mask&1)||target<1||target>15||__builtin_popcount(mask)<2||__builtin_popcount(mask)>4||(mask&(1u<<target)))return false;
        for(unsigned j=0;j<i;j++)if(rd16(p+KSL_LAYER_RULE_AT+j*4)==mask)return false;
        all_inputs|=mask;all_targets|=1u<<target;
    }
    return !(all_inputs&all_targets);
}
bool ksl_upgrade(uint8_t *out,const uint8_t *saved){
    if(!saved)return false;
    if(ksl_valid(saved)){memmove(out,saved,KSL_BYTES);return true;}
    if(!ksl_legacy_valid(saved))return false;
    uint8_t prefix[480];memcpy(prefix,saved,sizeof(prefix));ksl_default(out);memcpy(out,prefix,sizeof(prefix));out[3]='2';
    uint32_t crc=ksl_crc(out,KSL_CRC_AT);for(unsigned i=0;i<4;i++)out[KSL_CRC_AT+i]=crc>>(8*i);return true;
}
uint16_t ksl_binding(const uint8_t *p,unsigned computer,uint16_t code){
    if(code<KSL_BINDING||code>=KSL_BINDING+KSL_BINDINGS)return code;
    unsigned slot=code-KSL_BINDING;if(slot>=rd16(p+480)||computer>=p[4])return 0;
    return rd16(p+KSL_BINDING_AT+slot*12+computer*2);
}
static const struct ksl_recipe *recipe(unsigned layout,uint16_t code){
    if(layout>=KSL_LAYOUT_COUNT)return NULL;
    unsigned lo=ksl_layouts[layout][0],hi=lo+ksl_layouts[layout][1];
    while(lo<hi){unsigned at=lo+(hi-lo)/2;uint16_t c=ksl_recipes[at].code;if(c<code)lo=at+1;else hi=at;}
    unsigned end=ksl_layouts[layout][0]+ksl_layouts[layout][1];
    return lo<end&&ksl_recipes[lo].code==code?&ksl_recipes[lo]:NULL;
}
bool ksl_resolve(const uint8_t *p,unsigned computer,uint16_t code,uint8_t mods,bool caps,struct ksl_output *out){
    memset(out,0,sizeof(*out));
    if(computer>=p[4])return false;
    unsigned layout=rd16(p+8+computer*36);
    const struct ksl_recipe *r=recipe(layout,code);if(!r)return false;
    bool shifted=(mods&0x22)!=0;
    if((r->flags&2)&&(shifted!=caps)){r=recipe(layout,r->upper);if(!r)return false;}
    out->count=(r->flags&1)?2:1;
    out->strokes[0]=(struct ksl_stroke){r->key,r->mods};
    out->strokes[1]=(struct ksl_stroke){r->next_key,r->next_mods};
    // The recipe describes the desired case with Caps Lock off. Compensate on
    // alphabetic strokes without changing the computer's lock state.
    if(caps&&(r->flags&4))out->strokes[0].mods^=2;
    if(caps&&(r->flags&8))out->strokes[1].mods^=2;
    return true;
}
unsigned ksl_bluetooth(const uint8_t *p,unsigned slot,unsigned previous){
    for(unsigned i=0;i<p[4];i++)if(p[8+i*36+2]==slot)return i;
    return previous;
}
uint16_t ksl_compound(const uint8_t *p,uint16_t code,unsigned field){
    if(code<KSL_COMPOUND||code>=KSL_COMPOUND+p[6]||field>5)return 0;
    if(field>=4)return p[482]?rd16(p+KSL_LAYER_RULE_AT-p[6]*4+(code-KSL_COMPOUND)*4+(field-4)*2):0;
    return rd16(p+224+(code-KSL_COMPOUND)*8+field*2);
}

unsigned ksl_os_computer(const uint8_t *p,unsigned os){
    if(!os||os>3||p[7])return 255;
    unsigned found=255;
    for(unsigned i=0;i<p[4];i++){
        unsigned layout=rd16(p+8+i*36);
        if(layout<KSL_LAYOUT_COUNT&&ksl_layout_os[layout]==os){if(found!=255)return 255;found=i;}
    }
    return found;
}
