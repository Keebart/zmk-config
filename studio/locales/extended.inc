/* Included in locale.c: public, shared record validation. */
#include "power.h"
static unsigned kse_end(const uint8_t *p){return KSL_LAYER_RULE_AT-(p[482]?p[6]*4:0)-p[483]*6-((p[7]&2)?16:0)-((p[7]&4)?2:0)-ksl_typing_size(p);}
unsigned kse_size(const uint8_t *p){return (p[7]&16)?rd16(p+kse_end(p)-2):0;}
const uint8_t *kse_options(const uint8_t *p){
    static const uint8_t defaults[46]={1,4,0,0,0,14,0,0,0,0,0,0,40,0,0x88,0x13,0x20,3,0xf4,1,0,0,5,0,0,0,0,0,0,0,0,0,' ','v','s','.','|',' ','e','t','c','.','|','.','.','.'};
    return (p[7]&16)?p+kse_end(p)-kse_size(p):defaults;
}
static bool scalar(uint32_t c){return c>=32&&c<=0x10ffff&&!(c>=0xd800&&c<=0xdfff)&&!(c>=0x7f&&c<=0x9f);}
static bool sequence_key(uint16_t c){return (c>=4&&c<=0xdf)||(c>=0x104&&c<=0x1fff)||ksl_character(c);}
uint16_t kse_leader_tap(const uint8_t *p,uint16_t c){
    if(c>=KSL_COMPOUND&&c<KSL_COMPOUND+p[6])return ksl_compound(p,c,0);
    if((c>=0x2000&&c<=0x3fff&&(c&0x0f00))||(c>=0x4000&&c<=0x4fff))return c&255;
    return c;
}
static bool leader_key(const uint8_t *p,uint16_t c){return sequence_key(kse_leader_tap(p,c));}

/* Bounded UTF-8 decoding shared by validation and the paced text player. */
uint32_t kse_utf8(const uint8_t *p,unsigned n,unsigned *used){
    if(!n)return 0;
    uint32_t c=p[0],min=0;unsigned extra=0;
    if(c<0x80){*used=1;return (c==9||c==10||scalar(c))?c:0;}
    if(c>=0xc2&&c<=0xdf){extra=1;c&=31;min=0x80;}
    else if(c>=0xe0&&c<=0xef){extra=2;c&=15;min=0x800;}
    else if(c>=0xf0&&c<=0xf4){extra=3;c&=7;min=0x10000;}else return 0;
    if(extra>=n)return 0;
    *used=extra+1;
    for(unsigned i=1;i<=extra;i++){if((p[i]&0xc0)!=0x80)return 0;c=(c<<6)|(p[i]&63);}
    return c>=min&&scalar(c)?c:0;
}
static bool leader_output(const uint8_t *v,uint16_t out){
    return (out>=KSE_UNICODE&&out<KSE_UNICODE+v[2])||(ksl_compound_key(out)&&out>1&&!kse_code(out)&&out!=0x7c00&&out!=0x7c01);
}
unsigned kse_program(const uint8_t *v,unsigned index){
    unsigned at=32+v[2]*8+v[3]*14+v[4]*2+v[5];
    for(unsigned i=0;i<index;i++)for(unsigned j=0;j<v[32+v[2]*8+i*14+1];j++)at+=3+(v[at]==3?rd16(v+at+1):0);
    return at;
}

bool kse_valid(const uint8_t *p){
    if(!(p[7]&16))return true;
    unsigned n=kse_size(p);if(n<34||n>3072||n>kse_end(p)-KSL_BINDING_AT)return false;
    const uint8_t *v=kse_options(p);
    if((v[0]!=1&&v[0]!=2)||v[1]>31||v[2]>KSE_UNICODE_COUNT||v[3]>32||v[4]>64||v[5]>96||v[12]>100||v[13]||!zero(v+26,6)||(v[0]==1&&rd16(v+24))||(v[0]==2&&!rd16(v+24))||32+v[2]*8+v[3]*14+v[4]*2+v[5]+rd16(v+24)+2!=n)return false;
    for(unsigned i=6;i<12;i++)if(v[i]>3)return false;
    if(rd16(v+14)>60000||!rd16(v+16)||rd16(v+16)>10000||rd16(v+18)>10000||rd16(v+20)>3600||rd16(v+22)>3600)return false;
    unsigned at=32;
    for(unsigned i=0;i<v[2];i++,at+=8)if(!scalar(rd32(v+at))||(rd32(v+at+4)&&!scalar(rd32(v+at+4))))return false;
    for(unsigned i=0;i<v[3];i++,at+=14){
        const uint8_t *l=v+at;uint16_t out=rd16(l+12);
        if(!l[0]||l[0]>5||l[1]>64||(v[0]==1&&l[1])||!zero(l+2+l[0]*2,10-l[0]*2))return false;
        for(unsigned j=0;j<l[0];j++)if(!leader_key(p,rd16(l+2+j*2)))return false;
        if(l[1]?out!=0:!leader_output(v,out))return false;
        for(unsigned j=0;j<i;j++){
            const uint8_t *other=v+32+v[2]*8+j*14;bool same=l[0]==other[0];
            for(unsigned k=0;k<l[0]&&same;k++){
                uint16_t a=rd16(l+2+k*2),b=rd16(other+2+k*2);
                same=(v[1]&8)?a==b:kse_leader_tap(p,a)==kse_leader_tap(p,b);
            }
            if(same)return false;
        }
    }
    uint8_t used[16]={0};
    for(unsigned i=0;i<v[4];i++,at+=2)for(unsigned j=0;j<2;j++){unsigned pos=v[at+j];if(pos>=128||(used[pos/8]&(1u<<(pos%8))))return false;used[pos/8]|=1u<<(pos%8);}
    for(unsigned i=0;i<v[5];i++)if(v[at+i]<32||v[at+i]>126)return false;
    at+=v[5];
    for(unsigned i=0;i<v[3];i++){
        unsigned delay=0;
        for(unsigned j=0;j<v[32+v[2]*8+i*14+1];j++){
            if(at+3>n-2)return false;
            unsigned tag=v[at],value=rd16(v+at+1);at+=3;
            if(tag==1){if(!leader_output(v,value)||(value>=0x7700&&value<=0x777f))return false;}
            else if(tag==2){if(!value||value>10000)return false;delay+=value;if(delay>60000)return false;}
            else if(tag==3){
                if(at+value>n-2)return false;
                unsigned end=at+value;
                while(at<end){unsigned used=0;if(!kse_utf8(v+at,end-at,&used))return false;at+=used;}
            }else return false;
        }
    }
    return at==n-2;
}
unsigned kse_swap_position(const uint8_t *p,unsigned pos){
    const uint8_t *v=kse_options(p),*pairs=v+32+v[2]*8+v[3]*14;
    for(unsigned i=0;i<v[4];i++){if(pairs[i*2]==pos)return pairs[i*2+1];if(pairs[i*2+1]==pos)return pairs[i*2];}return pos;
}
uint8_t kse_brightness(const uint8_t *p,uint32_t elapsed,bool soon){
    const uint8_t *v=kse_options(p);unsigned secs=soon?rd16(v+22):rd16(v+20),fade=rd16(v+18);
    uint32_t idle=ksl_lighting_idle_seconds(p)*1000U,dim=secs*1000U;
    unsigned target=255;
    if(secs&&elapsed>=dim){unsigned low=v[12]*255/100;target=fade&&elapsed-dim<fade?255-(255-low)*(elapsed-dim)/fade:low;}
    if(idle){unsigned start=idle>fade?idle-fade:0;if(elapsed>=idle)target=0;else if(fade&&elapsed>start)target=target*(idle-elapsed)/(idle-start);}
    return target;
}
