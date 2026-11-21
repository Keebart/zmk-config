// Included by locale.c, sharing its audited character recipes and metadata.
#include <stdio.h>
#include "display_data.h"
static void label_copy(char *out,size_t size,const char *text){
    if(!size)return;
    size_t n=strlen(text);if(n>=size)n=size-1;
    while(n&&((unsigned char)text[n]&0xc0)==0x80)n--;
    memcpy(out,text,n);out[n]=0;
}
static void label_add(char *out,size_t size,const char *text){size_t n=strlen(out);if(n<size)label_copy(out+n,size-n,text);}
static void label_scalar(char *out,size_t size,uint32_t c){
    if(c>='a'&&c<='z')c-=32;
    if(c==32){label_copy(out,size,"Space");return;}
    char text[5]={0};
    if(c<128)text[0]=c;
    else if(c<2048){text[0]=0xc0|(c>>6);text[1]=0x80|(c&63);}
    else if(c<65536){text[0]=0xe0|(c>>12);text[1]=0x80|((c>>6)&63);text[2]=0x80|(c&63);}
    else {text[0]=0xf0|(c>>18);text[1]=0x80|((c>>12)&63);text[2]=0x80|((c>>6)&63);text[3]=0x80|(c&63);}
    label_copy(out,size,text);
}
static uint32_t dead_scalar(uint16_t code){
    if(code>=0xb020)return code&0xfff;
    unsigned index=code-0xb001;
    return index<sizeof(ksl_dead_extra)/sizeof(ksl_dead_extra[0])?ksl_dead_extra[index]:0;
}
static uint32_t label_interpret(unsigned layout,uint16_t code){
    if(layout>=KSL_LAYOUT_COUNT||code>=0x2000)return 0;
    for(unsigned cur=layout;cur!=255;cur=ksl_label_override_parent[cur]){
        unsigned lo=ksl_label_override_ranges[cur][0],hi=ksl_label_override_ranges[cur][1];
        while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(ksl_label_overrides[mid].raw<code)lo=mid+1;else hi=mid;}
        if(lo<ksl_label_override_ranges[cur][1]&&ksl_label_overrides[lo].raw==code){
            uint16_t value=ksl_label_overrides[lo].value;
            if(value==0xffffu)break;
            return (value&0x8000u)?((uint32_t)(value&0x7fffu)|0x80000000u):(uint32_t)value;
        }
    }
    unsigned mods=(code>>8)&15;if(code&0x1000)mods<<=4;
    uint16_t best=0xffffu;
    for(unsigned cur=layout;cur!=255;cur=ksl_layout_parent[cur]){
        unsigned lo=ksl_layouts[cur][0],hi=lo+ksl_layouts[cur][1];
        for(unsigned i=lo;i<hi;i++){
            uint16_t idx=ksl_recipe_indices[i];
            if(idx>=KSL_RECIPES1_COUNT)continue;
            const struct ksl_recipe1 *r=&ksl_recipes1[idx];
            uint16_t rc=r->lo|((uint16_t)r->hi<<8);
            if(rc<best&&r->key==(code&255)&&r->mods==mods){
                struct ksl_recipe eff;
                if(recipe(layout,rc,&eff)&&!(eff.flags&1)&&eff.key==r->key&&eff.mods==r->mods)best=rc;
            }
        }
    }
    if(best!=0xffffu)return best>=0xb000?dead_scalar(best)|0x80000000u:best&0x7fff;
    return 0;
}
static const char *label_gui(unsigned layout){unsigned os=ksl_layout_os[layout]&3;return os==2?"Cmd":os==1?"Win":"Super";}
static void label_mods(char *out,size_t size,unsigned mods,unsigned layout){
    const char *names[]={"Ctrl","Shift","Alt",label_gui(layout)};
    if(mods&16)label_add(out,size,"R ");
    bool first=true;
    for(unsigned i=0;i<4;i++)if(mods&(1u<<i)){if(!first)label_add(out,size,"+");label_add(out,size,names[i]);first=false;}
}
static void key_label(const uint8_t *p,unsigned computer,uint16_t code,const uint16_t *dance,char *out,size_t size,unsigned depth){
    if(!size)return;
    out[0]=0;
    if(depth>3){label_copy(out,size,"Key");return;}
    unsigned layout=computer<p[4]?rd16(p+8+computer*36):KSL_STOCK_LAYOUT;
    code=ksl_binding(p,computer,code);
    uint16_t tap=0,hold=0;
    if(code>=KSL_COMPOUND&&code<KSL_COMPOUND+p[6]){tap=ksl_compound(p,code,0);hold=ksl_compound(p,code,1);}
    else if(code>=0x2000&&code<0x4000){tap=code&255;hold=code&0x1f00;}
    else if(code>=0x4000&&code<0x5000){tap=code&255;hold=0x5220|((code>>8)&15);}
    if(hold){
        char text[96];key_label(p,computer,tap,NULL,out,size,depth+1);label_add(out,size," / ");
        key_label(p,computer,hold,NULL,text,sizeof(text),depth+1);label_add(out,size,text);return;
    }
    if(code>=0x5700&&code<0x5740){
        snprintf(out,size,"TD %u",code-0x5700+1);
        if(dance){char text[96];if(dance[0]>1){key_label(p,computer,dance[0],NULL,text,sizeof(text),depth+1);label_add(out,size,": ");label_add(out,size,text);}if(dance[1]>1){key_label(p,computer,dance[1],NULL,text,sizeof(text),depth+1);label_add(out,size," / ");label_add(out,size,text);}}
        return;
    }
    if(code>=0x7700&&code<0x7720){
        char name[161];ksl_macro_name(p,code-0x7700,name,sizeof(name));
        char *start=name;while(*start==' ')start++;size_t end=strlen(start);while(end&&start[end-1]==' ')start[--end]=0;
        if(start[0]){label_copy(out,size,"M ");label_add(out,size,start);}else snprintf(out,size,"Macro %u",code-0x7700+1);return;
    }
    if(ksl_character(code)){label_scalar(out,size,code>=0xb000?dead_scalar(code):code&0x7fff);if(code>=0xb000)label_add(out,size," ◌");return;}
    if(code>=KSE_UNICODE&&code<KSE_UNICODE+kse_options(p)[2]){label_scalar(out,size,rd32(kse_options(p)+32+(code-KSE_UNICODE)*8));return;}
    uint32_t character=label_interpret(layout,code);
    if(character){label_scalar(out,size,character&0x7fffffff);if(character&0x80000000u)label_add(out,size," ◌");return;}
    if(code>=0x0100&&code<0x2000){
        label_mods(out,size,code>>8,layout);
        if(code&255){char base[64];key_label(p,computer,code&255,NULL,base,sizeof(base),depth+1);label_add(out,size,"+");label_add(out,size,base);}return;
    }
    if(code==227||code==231){if(code==231)label_copy(out,size,"R ");label_add(out,size,label_gui(layout));return;}
    if(code>=0x5220&&code<0x5240){snprintf(out,size,"Layer %u",code&31);return;}
    if(code>=KSL_SELECT&&code<KSL_NEXT){snprintf(out,size,"PC %u",code-KSL_SELECT+1);return;}
    if(code==KSL_NEXT){label_copy(out,size,"PC →");return;}
    if(code==KSL_PREVIOUS){label_copy(out,size,"← PC");return;}
    unsigned lo=0,hi=sizeof(ksl_labels)/sizeof(*ksl_labels);
    while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(ksl_labels[mid].code<code)lo=mid+1;else hi=mid;}
    label_copy(out,size,lo<sizeof(ksl_labels)/sizeof(*ksl_labels)&&ksl_labels[lo].code==code?&ksl_label_pool[ksl_labels[lo].offset]:"Custom key");
}
void ksl_key_label(const uint8_t *p,unsigned computer,uint16_t code,const uint16_t *dance,char *out,size_t size){key_label(p,computer,code,dance,out,size,0);}
