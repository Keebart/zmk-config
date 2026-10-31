#include "locale_runtime.h"
#include <string.h>
// Only momentary holds participate. Toggle/default/one-shot state cannot
// accidentally activate a combined layer. Both ports execute these same edges.
static bool managed_layer(const struct ksl_runtime *r,uint16_t code){return r->layer_inputs[0]&&code>=0x5221&&code<=0x522f;}
static void layer_sync(struct ksl_runtime *r,const struct ksl_io *io){
    uint16_t held=0;
    for(unsigned i=0;i<KSL_HELD;i++){
        const struct ksl_held *h=&r->held[i];if(!h->used)continue;
        uint16_t code=h->tap_active?h->tap:h->hold?(h->resolved?h->hold:0):h->code;
        if(managed_layer(r,code))held|=1u<<(code&15);
    }
    uint16_t desired=held;
    for(unsigned i=0;i<KSL_LAYER_RULES;i++)if(r->layer_inputs[i]&&(held&r->layer_inputs[i])==r->layer_inputs[i])desired|=1u<<r->layer_targets[i];
    uint16_t previous=r->layer_state;r->layer_state=desired;
    // Record ownership before callbacks: synthetic output may re-enter ports.
    for(unsigned i=1;i<16;i++)if((previous&~desired)&(1u<<i))io->action(io->context,0x5220+i,false);
    for(unsigned i=1;i<16;i++)if((desired&~previous)&(1u<<i))io->action(io->context,0x5220+i,true);
}
static void output(const uint8_t *p,unsigned active,uint16_t code,uint8_t mods,bool caps,const struct ksl_io *io){
    if(!ksl_character(code)){
        io->action(io->context,code,true);
        if(io->pause)io->pause(io->context);
        io->action(io->context,code,false);return;
    }
    struct ksl_output out;
    if(io->prepare)io->prepare(io->context,code,true);
    if(io->mods)mods=io->mods(io->context);
    if(!ksl_resolve(p,active,code,mods,caps,&out)){if(io->prepare)io->prepare(io->context,code,false);return;}
    for(unsigned i=0;i<out.count;i++)io->tap(io->context,out.strokes[i].key,(mods&~0x22U)|out.strokes[i].mods);
    if(io->prepare)io->prepare(io->context,code,false);
}
static bool busy(const struct ksl_runtime *r){if(r->guarded||r->deferred_count)return true;for(unsigned i=0;i<KSL_HELD;i++)if(r->held[i].used)return true;return false;}
static void switch_ready(struct ksl_runtime *r){if(r->switching&&!busy(r)){r->active=r->requested;r->switching=false;}}
int64_t ksl_deadline(const struct ksl_runtime *r){int64_t due=INT64_MAX;for(unsigned i=0;i<KSL_HELD;i++)if(r->held[i].used&&r->held[i].due<due)due=r->held[i].due;return due;}
void ksl_runtime_init(struct ksl_runtime *r,const uint8_t *p){
    memset(r,0,sizeof(*r));r->active=p[5];
    for(unsigned i=0;i<KSL_LAYER_RULES;i++){const uint8_t *v=p+KSL_LAYER_RULE_AT+i*4;r->layer_inputs[i]=v[0]|(uint16_t)v[1]<<8;r->layer_targets[i]=v[2];}
}
bool ksl_select(struct ksl_runtime *r,const uint8_t *p,unsigned selected){
    if(selected>=p[4])return false;
    r->requested=selected;r->switching=true;switch_ready(r);return true;
}
static void hold_start(struct ksl_runtime *r,struct ksl_held *h,const uint8_t *p,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io){
    h->resolved=true;h->due=INT64_MAX;
    if(ksl_character(h->hold)){output(p,r->active,h->hold,mods,caps,io);h->due=now+500;}
    else if(h->hold>=KSL_SELECT&&h->hold<=KSL_PREVIOUS){if(p[4])ksl_select(r,p,h->hold==KSL_NEXT?(r->active+1)%p[4]:h->hold==KSL_PREVIOUS?(r->active+p[4]-1)%p[4]:h->hold-KSL_SELECT);}
    else if(managed_layer(r,h->hold))layer_sync(r,io);
    else io->action(io->context,h->hold,true);
}
static void hold_end(struct ksl_runtime *r,struct ksl_held *h,const struct ksl_io *io){
    if(managed_layer(r,h->hold)){layer_sync(r,io);return;}
    if(!ksl_character(h->hold)&&!(h->hold>=KSL_SELECT&&h->hold<=KSL_PREVIOUS))io->action(io->context,h->hold,false);
}
static void tap_start(struct ksl_runtime *r,struct ksl_held *h,const uint8_t *p,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io){
    h->tap_active=true;h->due=INT64_MAX;
    if(ksl_character(h->tap)){output(p,r->active,h->tap,mods,caps,io);h->due=now+500;}
    else if(h->tap>=KSL_SELECT&&h->tap<=KSL_PREVIOUS){
        if(p[4])ksl_select(r,p,h->tap==KSL_NEXT?(r->active+1)%p[4]:h->tap==KSL_PREVIOUS?(r->active+p[4]-1)%p[4]:h->tap-KSL_SELECT);
    }else if(managed_layer(r,h->tap))layer_sync(r,io);
    else io->action(io->context,h->tap,true);
}
static void tap_end(struct ksl_runtime *r,const struct ksl_held *h,const struct ksl_io *io){
    if(managed_layer(r,h->tap))layer_sync(r,io);
    else if(!ksl_character(h->tap)&&!(h->tap>=KSL_SELECT&&h->tap<=KSL_PREVIOUS))io->action(io->context,h->tap,false);
}
static void replay(const struct ksl_io *io,unsigned pos,uint16_t code,bool down,int64_t now){
    if(io->replay)io->replay(io->context,pos,code,down,now);else io->action(io->context,code,down);
}
static struct ksl_held *pending(struct ksl_runtime *r){
    struct ksl_held *first=NULL;
    for(unsigned i=0;i<KSL_HELD;i++){struct ksl_held *h=&r->held[i];if(h->used&&h->hold&&!h->tap_active&&!h->resolved&&(!first||h->started<first->started))first=h;}
    return first;
}
static void drain(struct ksl_runtime *,const uint8_t *,int64_t,uint8_t,bool,const struct ksl_io *);
void ksl_cancel(struct ksl_runtime *r,const struct ksl_io *io){
    r->draining=true;
    for(unsigned i=0;i<KSL_HELD;i++){
        const struct ksl_held *h=&r->held[i];
        // Raw keys may already be down before a permissive hold starts.
        // Their buffered releases must survive cancellation too.
        bool released=false;
        for(unsigned j=0;j<r->deferred_count;j++)if(!r->deferred[j].down&&r->deferred[j].position==h->position)released=true;
        if(h->used&&(h->forwarded||(released&&!h->hold&&h->code>=4&&h->code<0x2000)))replay(io,h->position,h->code,false,0);
    }
    for(unsigned i=0;i<KSL_HELD;i++)if(r->held[i].used){
        if(r->held[i].tap_active)tap_end(r,&r->held[i],io);
        else if(r->held[i].hold&&r->held[i].resolved&&!managed_layer(r,r->held[i].hold))hold_end(r,&r->held[i],io);
    }
    r->last_press_valid=r->last_tap_valid=false;
    r->deferred_count=0;memset(r->held,0,sizeof(r->held));r->draining=false;layer_sync(r,io);switch_ready(r);
}
void ksl_tick(struct ksl_runtime *r,const uint8_t *p,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io){
    for(unsigned i=0;i<KSL_HELD;i++){
        struct ksl_held *h=&r->held[i];if(!h->used||now<h->due)continue;
        if(h->tap_active){
            if(ksl_character(h->tap)){output(p,r->active,h->tap,mods,caps,io);h->due=now+33;}
        }else if(h->hold){
            if(!h->resolved)hold_start(r,h,p,now,mods,caps,io);
            else if(ksl_character(h->hold)){output(p,r->active,h->hold,mods,caps,io);h->due=now+33;}
        }
        else if(ksl_character(h->code)){output(p,r->active,h->code,mods,caps,io);h->due=now+33;}
    }drain(r,p,now,mods,caps,io);switch_ready(r);
}
static bool event(struct ksl_runtime *r,const uint8_t *p,unsigned pos,uint16_t code,bool down,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io,bool force_tap){
    struct ksl_held *found=NULL,*free=NULL;
    for(unsigned i=0;i<KSL_HELD;i++){
        struct ksl_held *h=&r->held[i];
        if(h->used&&h->position==pos)found=h;
        if(!h->used&&!free)free=h;
        if(down&&h->used&&h->hold&&h->position!=pos)h->interrupted=true;
    }
    if(io->mods)mods=io->mods(io->context);
    if(!down){
        if(found){
            // Synthetic macro output can re-enter the runtime. Remove this
            // released position first so it cannot resolve its own hold.
            struct ksl_held released=*found;memset(found,0,sizeof(*found));
            if(released.hold){
                bool tapped=released.tap_active||!released.resolved||((released.flags&KSL_RETRO)&&!released.interrupted);
                if(released.tap_active)tap_end(r,&released,io);
                else {
                    if(released.resolved)hold_end(r,&released,io);
                    if(tapped){
                        uint16_t tap=released.tap;
                        if(tap>=KSL_SELECT&&tap<=KSL_PREVIOUS){if(p[4])ksl_select(r,p,tap==KSL_NEXT?(r->active+1)%p[4]:tap==KSL_PREVIOUS?(r->active+p[4]-1)%p[4]:tap-KSL_SELECT);}
                        else output(p,r->active,tap,mods,caps,io);
                    }
                }
                if(tapped){r->last_tap_valid=true;r->last_tap_position=pos;r->last_tap_code=released.code;r->last_tap_time=now;}
            }
            layer_sync(r,io);
            switch_ready(r);
            if(released.forwarded){replay(io,pos,released.code,false,now);return true;}
            if(managed_layer(r,released.code))return true;
        }return ksl_supported(code)||managed_layer(r,code);
    }
    if(code>=KSL_SELECT&&code<=KSL_PREVIOUS){if(p[4])ksl_select(r,p,code==KSL_NEXT?(r->active+1)%p[4]:code==KSL_PREVIOUS?(r->active+p[4]-1)%p[4]:code-KSL_SELECT);return true;}
    if(found)return ksl_supported(code)||managed_layer(r,code);
    if(!free)return ksl_supported(code)||managed_layer(r,code);
    *free=(struct ksl_held){.position=pos,.code=code,.due=INT64_MAX,.started=now,.used=true};
    if(managed_layer(r,code)){layer_sync(r,io);return true;}
    if(ksl_character(code)){output(p,r->active,code,mods,caps,io);free->due=now+500;return true;}
    if(code>=KSL_COMPOUND&&code<KSL_COMPOUND+KSL_ACTIONS){
        free->tap=ksl_compound(p,code,0);free->hold=ksl_compound(p,code,1);free->due=now+ksl_compound(p,code,2);free->flags=ksl_compound(p,code,3);
        if(force_tap)tap_start(r,free,p,now,mods,caps,io);
        return true;
    }
    return false;
}

static void remove_deferred(struct ksl_runtime *r,unsigned at){
    r->deferred_count--;memmove(r->deferred+at,r->deferred+at+1,(r->deferred_count-at)*sizeof(*r->deferred));
}
static void drain(struct ksl_runtime *r,const uint8_t *p,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io){
    if(r->draining)return;
    r->draining=true;
    while(r->deferred_count){
        struct ksl_held *h=pending(r);
        if(h){
            bool decided=false;
            for(unsigned i=0;i<r->deferred_count;i++){
                struct ksl_deferred e=r->deferred[i];
                if(e.time>=h->due){hold_start(r,h,p,h->due,mods,caps,io);decided=true;break;}
                if(!e.down&&e.position==h->position){
                    remove_deferred(r,i);event(r,p,e.position,e.code,false,e.time,e.mods,e.caps,io,false);decided=true;break;
                }
                if(e.down&&e.position!=h->position){
                    h->interrupted=true;
                    unsigned first=io->hand?io->hand(io->context,h->position):0;
                    unsigned second=io->hand?io->hand(io->context,e.position):0;
                    if((h->flags&KSL_CHORDAL)&&first&&first==second){tap_start(r,h,p,e.time,mods,caps,io);decided=true;break;}
                    if(!(h->flags&(KSL_PERMISSIVE|KSL_TAP_PREFERRED))){hold_start(r,h,p,e.time,mods,caps,io);decided=true;break;}
                }
                if(!e.down&&(h->flags&KSL_PERMISSIVE)){
                    bool nested=false;
                    for(unsigned j=0;j<i;j++)if(r->deferred[j].position==e.position)nested=r->deferred[j].down;
                    if(nested){hold_start(r,h,p,e.time,mods,caps,io);decided=true;break;}
                }
            }
            if(!decided&&now>=h->due){hold_start(r,h,p,h->due,mods,caps,io);decided=true;}
            if(!decided)break;
            continue;
        }
        struct ksl_deferred e=r->deferred[0];remove_deferred(r,0);
        if(!event(r,p,e.position,e.code,e.down,e.time,e.mods,e.caps,io,e.tap)){
            if(e.down)for(unsigned i=0;i<KSL_HELD;i++)if(r->held[i].used&&r->held[i].position==e.position)r->held[i].forwarded=true;
            replay(io,e.position,e.code,e.down,e.time);
        }
    }
    r->draining=false;switch_ready(r);
}
static bool flow_key(const uint8_t *p,unsigned active,uint16_t code,uint8_t mods,bool caps){
    if(ksl_character(code)){
        struct ksl_output out;if(!ksl_resolve(p,active,code,mods,caps,&out)||out.count!=1||(out.strokes[0].mods&0xdd))return false;
        code=out.strokes[0].key;
    }
    return (code>=4&&code<=29)||code==44||code==51||(code>=54&&code<=56);
}
bool ksl_event(struct ksl_runtime *r,const uint8_t *p,unsigned pos,uint16_t code,bool down,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io){
    if(r->draining)return event(r,p,pos,code,down,now,mods,caps,io,false);
    ksl_tick(r,p,now,mods,caps,io);
    bool force_tap=false;
    if(down){
        uint16_t tap=code;
        if(code>=KSL_COMPOUND&&code<KSL_COMPOUND+p[6]){
            tap=ksl_compound(p,code,0);
            unsigned quick=ksl_compound(p,code,4),term=ksl_compound(p,code,2),flow=ksl_compound(p,code,5);
            if(quick>term)quick=term;
            force_tap=quick&&r->last_tap_valid&&r->last_tap_position==pos&&r->last_tap_code==code&&now-r->last_tap_time<quick;
            if(flow&&!pending(r)&&r->last_press_valid&&now-r->last_press_time<flow&&!(mods&0xdd)&&flow_key(p,r->active,tap,mods,caps)&&flow_key(p,r->active,r->last_press_code,mods,caps))force_tap=true;
        }
        r->last_press_valid=true;r->last_press_time=now;r->last_press_code=tap;
        if(r->last_tap_position!=pos||r->last_tap_code!=code)r->last_tap_valid=false;
        for(unsigned i=0;i<KSL_HELD;i++)if(r->held[i].used&&r->held[i].hold&&r->held[i].position!=pos)r->held[i].interrupted=true;
    }
    if(pending(r)||r->deferred_count){
        // Resolve an overloaded queue as a hold; never drop a release edge.
        while(r->deferred_count==KSL_DEFERRED){
            struct ksl_held *h=pending(r);if(h)hold_start(r,h,p,now,mods,caps,io);
            drain(r,p,now,mods,caps,io);
        }
        r->deferred[r->deferred_count++]=(struct ksl_deferred){.position=pos,.code=code,.down=down,.time=now,.mods=mods,.caps=caps,.tap=force_tap};
        drain(r,p,now,mods,caps,io);return true;
    }
    return event(r,p,pos,code,down,now,mods,caps,io,force_tap);
}

bool ksl_usb_select(struct ksl_runtime *r,const uint8_t *p,const uint8_t *request,uint8_t *reply,bool usb,bool authorized){
    if(memcmp(request+1,p+KSL_CRC_AT,4)||(request[0]!=255&&request[0]>=p[4]))return false;
    if(request[0]!=255&&usb&&(!authorized||!ksl_select(r,p,request[0])))return false;
    reply[0]=r->active;reply[1]=r->switching;reply[2]=usb;memcpy(reply+3,p+KSL_CRC_AT,4);
    return true;
}
