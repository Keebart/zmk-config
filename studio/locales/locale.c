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
bool ksl_supported(uint16_t code){return kse_code(code)||ksl_character(code)||(code>=KSL_SELECT&&code<=KSL_PREVIOUS)||(code>=KSL_COMPOUND&&code<KSL_COMPOUND+KSL_ACTIONS);}
bool ksl_compound_key(uint16_t c){
    return kse_code(c)||ksl_character(c)||c<=0x1fff||(c>=0x5000&&c<=0x52ff)||(c>=0x7700&&c<=0x771f)||
        (c>=0x7f40&&c<=0x7f5f)||(c>=0x7800&&c<=0x784c)||(c>=0x7c00&&c<=0x7c01)||(c>=0x7c5d&&c<=0x7c5f)||
        c==0x7c73||(c>=0x7c77&&c<=0x7c7a)||c==0x7c7b||(c>=KSL_SELECT&&c<=KSL_PREVIOUS)||
        (c>=0x7e00&&c<=0x7e04)||(c>=0x7e10&&c<=0x7e13)||(c>=0x7e20&&c<=0x7e22);
}
unsigned ksl_compound_at(unsigned i){return i<32?224+i*8:484+(i-32)*8;}
static bool compact(const uint8_t *p){return !memcmp(p,"KSL3",4);}
static unsigned extra_combo_at(const uint8_t *p){return 484+(p[6]>32?(p[6]-32)*8:0);}
unsigned ksl_binding_at(const uint8_t *p){
    if(!compact(p))return KSL_BINDING_AT;
    unsigned at=extra_combo_at(p);
    return at+((p[7]&128)?1+p[at]*10:0);
}
unsigned ksl_binding_stride(const uint8_t *p){return compact(p)?p[4]*2:12;}
unsigned ksl_prefix_end(const uint8_t *p){return ksl_binding_at(p)+rd16(p+480)*ksl_binding_stride(p);}
bool ksl_extra_combo(const uint8_t *p,unsigned i,uint16_t out[5]){
    memset(out,0,10);
    if(!compact(p)||!(p[7]&128)||i>=32)return false;
    unsigned at=extra_combo_at(p),count=p[at];
    if(count>32||i>=count)return false;
    for(unsigned j=0;j<5;j++)out[j]=rd16(p+at+1+i*10+j*2);
    return true;
}
static bool extra_combos_valid(const uint8_t *p){
    if(!(p[7]&128))return true;
    if(!compact(p))return false;
    unsigned count=p[extra_combo_at(p)];
    if(!count||count>32)return false;
    for(unsigned i=0;i<count;i++){
        uint16_t entry[5];ksl_extra_combo(p,i,entry);unsigned keys=0;bool ended=false,any=false;
        for(unsigned j=0;j<5;j++)any|=entry[j]!=0;
        if(!any){if(i==count-1)return false;continue;}
        for(unsigned j=0;j<4;j++){
            if(!entry[j]){ended=true;continue;}
            if(ended||entry[j]<=1||entry[j]==0x7c03)return false;
            for(unsigned k=0;k<j;k++)if(entry[k]==entry[j])return false;
            keys++;
        }
        if(keys<2||entry[4]<=1||entry[4]==0x7c03||(entry[4]>=0x5700&&entry[4]<0x5720))return false;
    }
    return true;
}
void ksl_default(uint8_t *p){memset(p,0,KSL_BYTES);memcpy(p,"KSL2",4);uint32_t crc=ksl_crc(p,KSL_CRC_AT);for(unsigned i=0;i<4;i++)p[KSL_CRC_AT+i]=crc>>(i*8);}
// Stock is distinct from an empty record: migrations must preserve raw keymaps.
void ksl_stock(uint8_t *p){
    ksl_default(p);p[4]=1;p[8]=KSL_STOCK_LAYOUT;p[9]=KSL_STOCK_LAYOUT>>8;
    p[10]=255;p[11]=11;memcpy(p+12,"My computer",11);
    static const uint8_t names[]={4,4,66,97,115,101,7,83,121,109,98,111,108,115,10,78,97,118,105,103,97,116,105,111,110,6,83,121,115,116,101,109,34,0};
    p[7]|=32;memcpy(p+KSL_LAYER_RULE_AT-sizeof(names),names,sizeof(names));
    uint32_t crc=ksl_crc(p,KSL_CRC_AT);for(unsigned i=0;i<4;i++)p[KSL_CRC_AT+i]=crc>>(i*8);
}
uint16_t ksl_stock_code(uint16_t code){
    for(unsigned i=0;i<sizeof(ksl_stock_characters)/sizeof(*ksl_stock_characters);i++)
        if(ksl_stock_characters[i][0]==code)return ksl_stock_characters[i][1];
    return code;
}
static bool common_valid(const uint8_t *p){
    if(p[4]>KSL_COMPUTERS||p[6]>(compact(p)?KSL_ACTIONS:32)||(!compact(p)&&p[7]>127))return false;
    if((p[4]&&p[5]>=p[4])||(!p[4]&&(p[5]||p[6])))return false;
    unsigned slots=0;
    for(unsigned i=0;i<KSL_COMPUTERS;i++){
        const uint8_t *c=p+8+i*36;if(i>=p[4]){if(!zero(c,36))return false;continue;}
        if(rd16(c)>=KSL_LAYOUT_COUNT||!c[3]||c[3]>32||!utf8(c+4,c[3])||!zero(c+4+c[3],32-c[3]))return false;
        if(c[2]!=255){if(c[2]>=5||(slots&(1U<<c[2])))return false;slots|=1U<<c[2];}
    }
    for(unsigned i=0;i<(p[6]>32?p[6]:32);i++){
        const uint8_t *a=p+ksl_compound_at(i);if(i>=p[6]){if(!zero(a,8))return false;continue;}
        uint16_t hold=rd16(a+2),term=rd16(a+4);
        if(!ksl_compound_key(rd16(a))||!term||term>1000||(hold<=1||!ksl_compound_key(hold))||(a[6]&~15)||((a[6]&3)==3)||a[7])return false;
    }
    return true;
}
bool ksl_legacy_valid(const uint8_t *p){return p[7]<=7&&!memcmp(p,"KSL1",4)&&rd32(p+508)==ksl_crc(p,508)&&common_valid(p)&&zero(p+480,28);}
#include "extended.inc"
// Names share the existing bounded extension pool; no persistent partition moves.
static unsigned names_end(const uint8_t *p){return kse_end(p)-kse_size(p);}
static unsigned names_size(const uint8_t *p){return (p[7]&32)?rd16(p+names_end(p)-2):0;}
static bool names_valid(const uint8_t *p){
    if(!(p[7]&32))return true;
    unsigned end=names_end(p),n=names_size(p);
    if(n<4||n>end-KSL_BINDING_AT)return false;
    unsigned at=end-n,count=p[at++];
    if(!count||count>32)return false;
    unsigned last=0;
    for(unsigned i=0;i<count;i++){
        if(at>=end-2)return false;
        unsigned len=p[at++];last=len;
        if(len>128||len>end-2-at||!utf8(p+at,len))return false;
        unsigned chars=0;for(unsigned j=0;j<len;j++)if((p[at+j]&0xc0)!=0x80)chars++;
        if(chars>32)return false;
        at+=len;
    }
    return last&&at==end-2;
}
void ksl_layer_name(const uint8_t *p,unsigned layer,char *out,size_t size){
    if(!size)return;
    out[0]=0;
    // Runtime records have already passed full CRC and bounds validation.
    if(!(p[7]&32))return;
    unsigned at=names_end(p)-names_size(p),count=p[at++];
    if(layer>=count)return;
    for(unsigned i=0;i<count;i++){
        unsigned len=p[at++];
        if(i==layer){
            size_t n=len<size-1?len:size-1;
            while(n<len&&n&&(p[at+n]&0xc0)==0x80)n--;
            memcpy(out,p+at,n);out[n]=0;return;
        }
        at+=len;
    }
}
static unsigned macro_names_end(const uint8_t *p){return names_end(p)-names_size(p);}
static unsigned macro_names_size(const uint8_t *p){return (p[7]&64)?rd16(p+macro_names_end(p)-2):0;}
static bool macro_names_valid(const uint8_t *p){
    if(!(p[7]&64))return true;
    unsigned end=macro_names_end(p),n=macro_names_size(p);
    if(n<4||n>end-KSL_BINDING_AT)return false;
    unsigned at=end-n,count=p[at++];
    if(!count||count>32)return false;
    unsigned last=0;
    for(unsigned i=0;i<count;i++){
        if(at>=end-2)return false;
        unsigned len=p[at++];last=len;
        if(len>160||len>end-2-at||!utf8(p+at,len))return false;
        unsigned chars=0;for(unsigned j=0;j<len;j++)if((p[at+j]&0xc0)!=0x80)chars++;
        if(chars>40)return false;
        at+=len;
    }
    return last&&at==end-2;
}
void ksl_macro_name(const uint8_t *p,unsigned layer,char *out,size_t size){
    if(!size)return;
    out[0]=0;
    // Runtime records have already passed full CRC and bounds validation.
    if(!(p[7]&64))return;
    unsigned at=macro_names_end(p)-macro_names_size(p),count=p[at++];
    if(layer>=count)return;
    for(unsigned i=0;i<count;i++){
        unsigned len=p[at++];
        if(i==layer){
            size_t n=len<size-1?len:size-1;
            while(n<len&&n&&(p[at+n]&0xc0)==0x80)n--;
            memcpy(out,p+at,n);out[n]=0;return;
        }
        at+=len;
    }
}
bool ksl_valid(const uint8_t *p){
    if((memcmp(p,"KSL2",4)&&!compact(p))||rd32(p+KSL_CRC_AT)!=ksl_crc(p,KSL_CRC_AT)||!common_valid(p))return false;
    unsigned count=rd16(p+480);
    if(count>KSL_BINDINGS||(!p[4]&&count)||p[482]>1||p[483]>32)return false;
    if(ksl_prefix_end(p)>KSL_LAYER_RULE_AT)return false;
    if(!extra_combos_valid(p)||!kse_valid(p)||!names_valid(p)||!macro_names_valid(p))return false;
    for(unsigned i=0;i<count;i++)for(unsigned computer=0;computer<(compact(p)?p[4]:KSL_COMPUTERS);computer++){
        uint16_t code=rd16(p+ksl_binding_at(p)+i*ksl_binding_stride(p)+computer*2);
        if(computer>=p[4]){if(code)return false;continue;}
        if(code>=0x8000&&!ksl_character(code)&&!(code>=KSE_UNICODE&&code<KSE_UNICODE+kse_options(p)[2])&&!(code>=KSL_COMPOUND&&code<KSL_COMPOUND+p[6]))return false;
        if(code>=KSL_SELECT&&code<KSL_NEXT&&code>=KSL_SELECT+p[4])return false;
    }
    unsigned timing=p[482]?p[6]*4:0;
    unsigned colors=p[483]*6,exempt=(p[7]&2)?16:0,sleep=(p[7]&4)?2:0,typing=ksl_typing_size(p);
    if(ksl_prefix_end(p)+timing+colors+exempt+sleep+typing+kse_size(p)+names_size(p)+macro_names_size(p)>KSL_LAYER_RULE_AT)return false;
    if(!zero(p+ksl_prefix_end(p),KSL_LAYER_RULE_AT-ksl_prefix_end(p)-timing-colors-exempt-sleep-typing-kse_size(p)-names_size(p)-macro_names_size(p)))return false;
    if(typing){
        const uint8_t *t=ksl_typing(p);unsigned term=rd16(t+2);
        if((t[0]&0x30)||t[1]>1||zero(t,typing))return false;
        if(typing==16){if(t[14]||t[15])return false;}
        else {
            if(!t[14]||t[14]>7||t[15]>15||!zero(t+28,3)||t[31]!=0xa1)return false;
            if(t[14]&1){if(!rd16(t+16)||rd16(t+16)>1000||rd16(t+18)>1000||rd16(t+20)>1000||(t[15]&3)==3)return false;}
            else if(!zero(t+15,7))return false;
            if(t[14]&2){if(rd16(t+22)>1000||rd16(t+24)>1000||!t[26])return false;}
            else if(!zero(t+22,5))return false;
            if(t[27]>15||(!(t[14]&4)&&t[27]))return false;
        }
        if(t[0]&0x40){if(!term||term>1000||((t[0]&0x80)&&!(t[0]&15)))return false;}
        else if(t[0]||term)return false;
        if(t[1]){if(!t[5]||!t[6]||t[6]>127||!t[7]||!t[8]||!t[10]||!t[11]||t[11]>127||!t[12]||!t[13])return false;}
        else if(!zero(t+4,10))return false;
    }
    if(sleep){unsigned seconds=ksl_lighting_idle_seconds(p);if(!seconds||seconds>3600)return false;}
    if(exempt&&zero(p+KSL_LAYER_RULE_AT-timing-colors-exempt,exempt))return false;
    if(timing){
        bool any=false;
        for(unsigned i=KSL_LAYER_RULE_AT-timing;i<KSL_LAYER_RULE_AT;i+=2){unsigned term=rd16(p+i);if(term>1000)return false;any|=term!=0;}
        if(!any)return false;
    }else if(p[482])return false;
    for(unsigned i=KSL_LAYER_RULE_AT-timing-colors;i<KSL_LAYER_RULE_AT-timing;i+=6)if(rd16(p+i)>360||rd16(p+i+2)>100||rd16(p+i+4)>100)return false;
    uint16_t all_inputs=0,all_targets=0;bool ended=false;
    for(unsigned i=0;i<KSL_LAYER_RULES;i++){
        const uint8_t *rule=p+KSL_LAYER_RULE_AT+i*4;unsigned mask=rd16(rule),target=rule[2];
        if(!mask){if(!zero(rule,3))return false;ended=true;continue;}
        if(ended||(mask&1)||target<1||target>15||__builtin_popcount(mask)<2||__builtin_popcount(mask)>4||(mask&(1u<<target)))return false;
        for(unsigned j=0;j<i;j++)if(rd16(p+KSL_LAYER_RULE_AT+j*4)==mask)return false;
        all_inputs|=mask;all_targets|=1u<<target;
    }
    uint8_t power[6];kspw_options(p,power);return !(all_inputs&all_targets)&&kspw_valid(power);
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
    return rd16(p+ksl_binding_at(p)+slot*ksl_binding_stride(p)+computer*2);
}
static uint16_t entry_code(uint16_t idx){
    if(idx<KSL_RECIPES1_COUNT)return ksl_recipes1[idx].lo|((uint16_t)ksl_recipes1[idx].hi<<8);
    if(idx<KSL_RECIPES_COUNT){const struct ksl_recipe2 *r=&ksl_recipes2[idx-KSL_RECIPES1_COUNT];return r->lo|((uint16_t)r->hi<<8);}
    return ksl_deleted_codes[idx-KSL_RECIPES_COUNT];
}
static bool recipe(unsigned layout,uint16_t code,struct ksl_recipe *out){
    if(layout>=KSL_LAYOUT_COUNT)return false;
    for(unsigned cur=layout;cur!=255;cur=ksl_layout_parent[cur]){
        unsigned lo=ksl_layouts[cur][0],end=lo+ksl_layouts[cur][1],hi=end;
        while(lo<hi){unsigned at=lo+(hi-lo)/2;uint16_t c=entry_code(ksl_recipe_indices[at]);if(c<code)lo=at+1;else hi=at;}
        if(lo<end){
            uint16_t idx=ksl_recipe_indices[lo];
            if(entry_code(idx)==code){
                if(idx>=KSL_RECIPES_COUNT)return false;
                if(out){
                    if(idx<KSL_RECIPES1_COUNT){
                        const struct ksl_recipe1 *r=&ksl_recipes1[idx];
                        *out=(struct ksl_recipe){code,r->key,r->mods,0,0,r->flags};
                    }else{
                        const struct ksl_recipe2 *r=&ksl_recipes2[idx-KSL_RECIPES1_COUNT];
                        *out=(struct ksl_recipe){code,r->key,r->mods,r->next_key,r->next_mods,r->flags};
                    }
                }
                return true;
            }
        }
    }
    return false;
}
static uint16_t unicode_upper(uint16_t code){
    uint16_t c=code&0x7fff;
    unsigned lo=0,hi=sizeof(ksl_unicode_case)/sizeof(ksl_unicode_case[0]);
    while(lo<hi){unsigned at=lo+(hi-lo)/2;if(ksl_unicode_case[at].start<=c)lo=at+1;else hi=at;}
    if(lo){
        const struct ksl_case_run *r=&ksl_unicode_case[lo-1];
        unsigned diff=c-r->start;
        if(diff<(unsigned)r->count*r->step&&(diff%r->step)==0)return 0x8000u|(uint16_t)(c+r->delta);
    }
    return code;
}
static uint16_t recipe_upper(unsigned layout,uint16_t code){
    if(code>=0xb000)return code;
    uint16_t c=code&0x7fff;
    if(layout<KSL_LAYOUT_COUNT&&(ksl_layout_os[layout]&0x80)){
        if(c=='i')return 0x8000u|0x0130u;
        if(c==0x0131u)return 0x8000u|'I';
    }
    if(c>='a'&&c<='z')return (uint16_t)(code-32);
    if(c>=128)return unicode_upper(code);
    return code;
}
static bool unicode_character(unsigned layout,uint16_t code){return layout==KSL_UNICODE_HEX_LAYOUT&&code>=0x80a0&&code<0xb000;}
// Character shifting is logical, not a Shift modifier on the host's raw key.
// The US symbol pairs preserve Miryoku across computer layouts (2 -> @ even
// when the host uses German). Explicit shifted symbols have no second action.
uint16_t ksl_shifted_character(const uint8_t *p,unsigned computer,uint16_t code){
    if(computer>=p[4]||!ksl_character(code))return code;
    unsigned layout=rd16(p+8+computer*36);
    for(unsigned i=0;i<sizeof(ksl_shift_pairs)/sizeof(*ksl_shift_pairs);i++)if(code==ksl_shift_pairs[i][0]){
        uint16_t shifted=ksl_shift_pairs[i][1];
        return recipe(layout,shifted,NULL)?shifted:code;
    }
    if(unicode_character(layout,code))return unicode_upper(code);
    struct ksl_recipe r;if(!recipe(layout,code,&r))return code;
    uint16_t upper=recipe_upper(layout,code);
    if((r.flags&2)&&upper!=code&&recipe(layout,upper,NULL))return upper;
    return code;
}
bool ksl_letter(const uint8_t *p,unsigned computer,uint16_t code){
    if(computer>=p[4])return false;
    unsigned layout=rd16(p+8+computer*36);
    if(unicode_character(layout,code)){
        unsigned c=code&0x7fff;
        for(unsigned i=0;i<sizeof(ksl_unicode_letters)/sizeof(ksl_unicode_letters[0]);i++){
            if(c<ksl_unicode_letters[i][0])break;
            if(c<=ksl_unicode_letters[i][1])return true;
        }
        return false;
    }
    struct ksl_recipe r;
    return recipe(layout,code,&r)&&(r.flags&16);
}
bool ksl_auto_shift_character(const uint8_t *p,unsigned computer,uint16_t code){
    const uint8_t *t=ksl_typing(p);
    if(ksl_shifted_character(p,computer,code)==code)return false;
    if(ksl_letter(p,computer,code))return t[0]&1;
    if(code>=0x8030&&code<=0x8039)return t[0]&2;
    return t[0]&4;
}
bool ksl_resolve(const uint8_t *p,unsigned computer,uint16_t code,uint8_t mods,bool caps,struct ksl_output *out){
    memset(out,0,sizeof(*out));
    if(computer>=p[4])return false;
    unsigned layout=rd16(p+8+computer*36);
    if(unicode_character(layout,code)){
        uint16_t target=code;
        if(mods&0x22){
            target=ksl_shifted_character(p,computer,code);
            if(caps&&target==unicode_upper(code))target=code;
        }else if(caps)target=unicode_upper(code);
        out->unicode=target&0x7fff;
        return true;
    }
    struct ksl_recipe r;if(!recipe(layout,code,&r))return false;
    bool shifted=(mods&0x22)!=0;
    uint16_t upper=recipe_upper(layout,code);
    uint16_t target=code;
    if(shifted){
        target=ksl_shifted_character(p,computer,code);
        // Caps Lock reverses letter case, not logical punctuation pairs.
        if(caps&&(r.flags&2)&&target==upper)target=code;
    }else if(caps&&(r.flags&2))target=upper;
    struct ksl_recipe translated;
    if(target!=code&&recipe(layout,target,&translated))r=translated;
    out->count=(r.flags&1)?2:1;
    out->strokes[0]=(struct ksl_stroke){r.key,r.mods};
    out->strokes[1]=(struct ksl_stroke){r.next_key,r.next_mods};
    // The recipe describes the desired case with Caps Lock off. Compensate on
    // alphabetic strokes without changing the computer's lock state.
    if(caps&&(r.flags&4))out->strokes[0].mods^=2;
    if(caps&&(r.flags&8))out->strokes[1].mods^=2;
    return true;
}
unsigned ksl_bluetooth(const uint8_t *p,unsigned slot,unsigned previous){
    for(unsigned i=0;i<p[4];i++)if(p[8+i*36+2]==slot)return i;
    return previous;
}
uint16_t ksl_compound(const uint8_t *p,uint16_t code,unsigned field){
    if(code<KSL_COMPOUND||code>=KSL_COMPOUND+p[6]||field>5)return 0;
    if(field>=4)return p[482]?rd16(p+KSL_LAYER_RULE_AT-p[6]*4+(code-KSL_COMPOUND)*4+(field-4)*2):0;
    return rd16(p+ksl_compound_at(code-KSL_COMPOUND)+field*2);
}

unsigned ksl_os_computer(const uint8_t *p,unsigned os){
    if(!os||os>3||(p[7]&1))return 255;
    unsigned found=255;
    for(unsigned i=0;i<p[4];i++){
        unsigned layout=rd16(p+8+i*36);
        if(layout<KSL_LAYOUT_COUNT&&(ksl_layout_os[layout]&3)==os){if(found!=255)return 255;found=i;}
    }
    return found;
}

bool ksl_rgb(const uint8_t *p,uint16_t code,uint16_t out[3]){
 if(code<0x7f40||code>=0x7f40+p[483])return false;
 unsigned at=KSL_LAYER_RULE_AT-(p[482]?p[6]*4:0)-p[483]*6+(code-0x7f40)*6;
 for(unsigned i=0;i<3;i++)out[i]=rd16(p+at+i*2);
 return true;
}

bool ksl_chordal_exempt(const uint8_t *p,unsigned pos){
 if(!(p[7]&2)||pos>=128)return false;
 unsigned at=KSL_LAYER_RULE_AT-(p[482]?p[6]*4:0)-p[483]*6-16;
 return (p[at+pos/8]&(1u<<(pos%8)))!=0;
}

unsigned ksl_lighting_idle_seconds(const uint8_t *p){
 if(!(p[7]&4))return 0;
 unsigned at=KSL_LAYER_RULE_AT-(p[482]?p[6]*4:0)-p[483]*6-((p[7]&2)?16:0)-2;
 return rd16(p+at);
}
bool ksl_lighting_awake(const uint8_t *p,uint32_t elapsed,bool default_awake){
 unsigned seconds=ksl_lighting_idle_seconds(p);
 return seconds?elapsed<(uint32_t)seconds*1000:default_awake;
}

static unsigned typing_end(const uint8_t *p){return KSL_LAYER_RULE_AT-(p[482]?p[6]*4:0)-p[483]*6-((p[7]&2)?16:0)-((p[7]&4)?2:0);}
unsigned ksl_typing_size(const uint8_t *p){return !(p[7]&8)?0:p[typing_end(p)-1]==0xa1?32:16;}
const uint8_t *ksl_typing(const uint8_t *p){
 static const uint8_t defaults[32]={0};
 if(!(p[7]&8))return defaults;
 unsigned at=typing_end(p)-ksl_typing_size(p);
 return p+at;
}

#include "display_labels.inc"

uint16_t ksl_tap_delay(const uint8_t *p,uint16_t code){
 const uint8_t *t=ksl_typing(p);
 return ksl_typing_size(p)==32&&(t[14]&2)?rd16(t+(code==57?24:22)):(code==57?80:5);
}
uint16_t ksl_hold_term(const uint8_t *p){const uint8_t *t=ksl_typing(p);return ksl_typing_size(p)==32&&(t[14]&1)?rd16(t+16):200;}
uint8_t ksl_tapping_toggle(const uint8_t *p){const uint8_t *t=ksl_typing(p);return ksl_typing_size(p)==32&&(t[14]&2)?t[26]:5;}

#include "power.inc"
