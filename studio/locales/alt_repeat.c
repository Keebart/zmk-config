#include "alt_repeat.h"
#include <string.h>
static uint8_t keymods(uint16_t code){uint8_t m=(code>>8)&15;return code&0x1000?m<<4:m;}
static uint16_t normalize(uint16_t code,uint8_t *mods){
    if(code>=0x100&&code<0x2000){*mods|=keymods(code);return code&255;}
    if(code>=0x2000&&code<0x5000)return code&255;
    return code;
}
bool kar_allowed(uint16_t c){
    unsigned key=c&255;
    return c<=1||ksl_character(c)||(c>=0x7700&&c<0x7710)||
        (c<0x5000&&((key>=4&&key<=0xa4)||(key>=0xa8&&key<=0xdf)));
}
bool kar_valid(const struct kar_rule *e){return !(e->options&~15U)&&kar_allowed(e->keycode)&&kar_allowed(e->alt_keycode);}
void kar_note(struct kar_state *s,uint16_t code,uint8_t mods){
    if(s->replaying||code<=1||!kar_allowed(code))return;
    s->last=normalize(code,&mods);s->mods=mods;
}
static unsigned bits(uint8_t m){unsigned n=0;while(m){n+=m&1;m>>=1;}return n;}
static bool matches(uint8_t mods,uint8_t required,uint8_t allowed,uint8_t options){
    allowed|=required;
    if(options&4){mods=(mods&15)|(mods>>4);required=(required&15)|(required>>4);allowed=(allowed&15)|(allowed>>4);}
    return (mods&required)==required&&!(mods&~allowed);
}
static uint16_t pair(uint16_t key,const uint16_t (*pairs)[2],unsigned count){for(unsigned i=0;i<count;i++){if(key==pairs[i][0])return pairs[i][1];if(key==pairs[i][1])return pairs[i][0];}return 0;}
uint16_t kar_alternate(const struct kar_rule *rules,uint16_t code,uint8_t mods,uint8_t *output_mods){
    code=normalize(code,&mods);int best=-1;uint16_t result=1;uint8_t result_mods=0;
    for(unsigned i=0;i<KAR_COUNT;i++){
        const struct kar_rule *e=&rules[i];if(!(e->options&8))continue;
        uint8_t a=0,b=0;uint16_t from=normalize(e->keycode,&a),to=normalize(e->alt_keycode,&b);
        if(code==from&&matches(mods,a,e->allowed_mods,e->options)&&(int)bits(a)>best){best=bits(a);result=to;result_mods=b;}
        if((e->options&2)&&code==to&&matches(mods,b,e->allowed_mods,e->options)&&(int)bits(b)>best){best=bits(b);result=from;result_mods=a;}
        if((e->options&1)&&best<0&&result==1&&matches(mods,0,e->allowed_mods,e->options)){result=to;result_mods=b;}
    }
    if(result!=1){*output_mods=result_mods;return result;}
    static const uint16_t hotkeys[][2]={{9,5},{7,24},{17,19},{4,8},{18,12}};
    static const uint16_t letters[][2]={{13,14},{11,15},{26,5},{8,5}};
    static const uint16_t common[][2]={{80,79},{82,81},{74,77},{75,78},{42,76},{47,48},{0xb9,0xba},{0xab,0xac},{0xbc,0xbb},{0xa9,0xaa},{0xbd,0xbe},{0xcd,0xce},{0xcf,0xd0},{0xda,0xdb},{0xd9,0xdc}};
    result=pair(code,(mods&0xdd)?hotkeys:letters,(mods&0xdd)?sizeof(hotkeys)/sizeof(*hotkeys):sizeof(letters)/sizeof(*letters));
    if(!result)result=pair(code,common,sizeof(common)/sizeof(*common));
    *output_mods=mods;return result;
}
uint8_t kar_mods(const struct kar_state *s){uint8_t mods=0;for(unsigned i=0;i<KAR_HELD;i++)if(s->held[i].used)mods|=s->held[i].mods;return mods;}
bool kar_event(struct kar_state *s,const struct kar_rule *rules,unsigned position,uint16_t code,bool down,kar_emit emit,void *ctx){
    if(code!=KAR_REPEAT&&code!=KAR_ALT)return false;
    if(s->replaying)return true;
    struct kar_press *found=NULL,*free=NULL;
    for(unsigned i=0;i<KAR_HELD;i++){if(s->held[i].used&&s->held[i].position==position)found=&s->held[i];if(!s->held[i].used&&!free)free=&s->held[i];}
    if(down){
        if(found||!free)return true;
        uint8_t mods=s->mods;uint16_t output=code==KAR_ALT?kar_alternate(rules,s->last,mods,&mods):s->last;
        if(output<=1)return true;
        *free=(struct kar_press){.position=position,.code=output,.mods=mods,.used=true};
        s->replaying=true;emit(ctx,position,output,mods,true);s->replaying=false;
    }else if(found){
        struct kar_press p=*found;memset(found,0,sizeof(*found));
        s->replaying=true;emit(ctx,position,p.code,p.mods,false);s->replaying=false;
    }
    return true;
}
void kar_cancel(struct kar_state *s,kar_emit emit,void *ctx){
    for(unsigned i=0;i<KAR_HELD;i++)if(s->held[i].used){struct kar_press p=s->held[i];memset(&s->held[i],0,sizeof(s->held[i]));s->replaying=true;emit(ctx,p.position,p.code,p.mods,false);s->replaying=false;}
    s->last=0;s->mods=0;
}
