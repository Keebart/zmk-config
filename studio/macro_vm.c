#include "macro_vm.h"
#include "locales/locale.h"
#include <string.h>
static bool key_valid(uint16_t key) { return (key <= 0x1fff && (key & 255) >= 4) || ksl_character(key); }
static uint16_t ascii_key(uint8_t c) {
    if (c >= 'a' && c <= 'z') return 4 + c - 'a';
    if (c >= 'A' && c <= 'Z') return 0x204 + c - 'A';
    if (c >= '1' && c <= '9') return 30 + c - '1';
    if (c == '0') return 39;
    if (c == '\n') return 40;
    if (c == '\t') return 43;
    const char plain[] = " -=[]\\;`',./";
    const char shifted[] = "_+{}|:~\"<>?";
    const uint8_t keys[] = {44,45,46,47,48,49,51,53,52,54,55,56};
    for(unsigned i=0;i<sizeof(plain)-1;i++) if(c==(uint8_t)plain[i]) return keys[i];
    for(unsigned i=0;i<sizeof(shifted)-1;i++) if(c==(uint8_t)shifted[i]) return 0x200 | keys[i+1];
    const char numbers[] = "!@#$%^&*()";
    for(unsigned i=0;i<10;i++) if(c==(uint8_t)numbers[i]) return 0x200 | (30+i);
    return 0;
}
/* One bounded instruction; zero is the macro terminator. */
static bool next(const uint8_t *p,size_t len,size_t *at,uint8_t *op,uint16_t *value) {
    if(*at>=len) return false;
    uint8_t c=p[(*at)++];
    if(!c) {*op=0;*value=0;return true;}
    if(c!=1) {*op=1;*value=ascii_key(c);return *value!=0;}
    if(*at+2>len) return false;
    *op=p[(*at)++];uint8_t a=p[(*at)++];
    if(!a || *op<1 || *op>7) return false;
    if(*op>=4) {
        if(*at>=len) return false;
        uint8_t b=p[(*at)++];if(!b)return false;
        *value=*op==4 ? (uint16_t)(a-1)+(uint16_t)(b-1)*255 : b==255 ? (uint16_t)a<<8 : a | (uint16_t)b<<8;
        if(*op!=4)*op-=4;
    } else *value=a;
    return *op==4 || key_valid(*value);
}
bool km_valid(const uint8_t *p,size_t len,unsigned count) {
    if(!p || len< count+1 || len>16384 || !count || count>128 || p[len-1])return false;
    size_t at=0;
    for(unsigned macro=0;macro<count;macro++) {
        uint16_t held[KM_HELD]={0};unsigned n=0,steps=0;uint64_t duration=0;
        for(;;) {
            uint8_t op;uint16_t value;
            if(!next(p,len,&at,&op,&value))return false;
            if(!op){if(n)return false;break;}
            if(++steps>4096)return false;
            duration+=op==4?value:20;
            if(duration>600000)return false;
            unsigned i=0;while(i<n && held[i]!=value)i++;
            if(op==1 && i<n)return false;
            if(op==2) {if(i<n || n==KM_HELD)return false;held[n++]=value;}
            if(op==3) {if(i==n)return false;held[i]=held[--n];}
        }
    }
    while(at<len)if(p[at++])return false;
    return true;
}
void km_cancel(struct km_vm *v,km_emit emit,void *ctx) {
    if(v->tap)emit(ctx,v->tap,false);
    while(v->held_count)emit(ctx,v->held[--v->held_count],false);
    memset(v,0,sizeof(*v));
}
bool km_start(struct km_vm *v,const uint8_t *p,size_t len,unsigned index,int64_t now,km_emit emit,void *ctx) {
    km_cancel(v,emit,ctx);
    if(index>=KM_COUNT || !km_valid(p,len,KM_COUNT))return false;
    size_t at=0;
    for(unsigned i=0;i<index;i++)while(p[at++]){}
    v->bytes=p;v->at=at;v->end=len;v->due=now;v->active=true;return true;
}
void km_tick(struct km_vm *v,int64_t now,km_emit emit,void *ctx) {
    if(!v->active || now<v->due)return;
    if(v->tap) {emit(ctx,v->tap,false);v->tap=0;v->due=now+10;return;}
    uint8_t op;uint16_t value;
    if(!next(v->bytes,v->end,&v->at,&op,&value) || !op) {km_cancel(v,emit,ctx);return;}
    v->due=now+(op==4?value:op==1?10:20);
    if(op==4)return;
    if(op==1)v->tap=value;
    if(op==2) {if(v->held_count==KM_HELD){km_cancel(v,emit,ctx);return;} v->held[v->held_count++]=value;}
    if(op==3) {
        unsigned i=0;while(i<v->held_count && v->held[i]!=value)i++;
        if(i==v->held_count){km_cancel(v,emit,ctx);return;}
        v->held[i]=v->held[--v->held_count];
    }
    emit(ctx,value,op!=3);
}
