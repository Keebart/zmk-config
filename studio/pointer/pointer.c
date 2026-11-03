#include "pointer.h"
#include <string.h>
static bool moving(const struct ksp_state *s){return s->held[0]||s->held[1]||s->held[2]||s->held[3]||s->held[12]||s->held[13]||s->held[14]||s->held[15];}
static struct ksp_report report(struct ksp_state *s,uint32_t now,bool move){
    struct ksp_report r={0};for(unsigned i=0;i<8;i++)if(s->held[i+4])r.buttons|=1u<<i;
    if(!move)return r;
    int step=s->held[16]?1:s->held[17]?4:s->held[18]?12:1+(now-s->started)/160;
    if(step>12)step=12;
    r.x=((s->held[3]!=0)-(s->held[2]!=0))*step;
    r.y=((s->held[1]!=0)-(s->held[0]!=0))*step;
    if(r.x&&r.y){r.x=r.x*181/256;r.y=r.y*181/256;if(!r.x)r.x=s->held[3]?1:-1;if(!r.y)r.y=s->held[1]?1:-1;}
    // Wheel cadence is independent of pointer acceleration.
    if((int32_t)(now-s->wheel_due)>=0){s->wheel_due=now+100;r.v=(s->held[12]!=0)-(s->held[13]!=0);r.h=(s->held[15]!=0)-(s->held[14]!=0);}
    return r;
}
bool ksp_key(struct ksp_state *s,uint16_t code,bool down,uint32_t now,ksp_send send,void *ctx){
    if(code<0xcd||code>0xdf)return false;
    unsigned i=code-0xcd;bool was=moving(s);
    if(down){if(s->held[i]<255)s->held[i]++;}else if(s->held[i])s->held[i]--;
    bool active=moving(s);if(!was&&active){s->started=now;s->due=now+10;s->wheel_due=now;}
    send(ctx,report(s,now,!was&&active));return true;
}
void ksp_tick(struct ksp_state *s,uint32_t now,ksp_send send,void *ctx){
    if(!moving(s)||(int32_t)(now-s->due)<0)return;
    s->due=now+10;send(ctx,report(s,now,true));
}
void ksp_clear(struct ksp_state *s,ksp_send send,void *ctx){memset(s,0,sizeof(*s));send(ctx,(struct ksp_report){0});}
