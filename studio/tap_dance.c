#include "tap_dance.h"
#include <string.h>
static void output(kd_emit emit, void *ctx, const struct kd_state *s, uint16_t code, enum kd_action action) {
    if (code > 1) emit(ctx, s->position, code, action);
}
static void resolve(struct kd_state *s, bool interrupted, kd_emit emit, void *ctx) {
    uint16_t code = 0;
    if (s->entry.term & KD_GUARD) {
        // Upstream Miryoku's ACTION_TAP_DANCE_FN fires only for count == 2,
        // including an interruption or a held second press.
        code = s->count == 2 ? s->entry.twice : 0;
    } else if (s->count == 1) {
        code = s->down && !interrupted && s->entry.hold ? s->entry.hold : s->entry.tap;
    } else if (s->count == 2) {
        if (interrupted) {
            output(emit,ctx,s,s->entry.tap,KD_TAP);code=s->entry.tap;
        } else if (s->down) {
            if (s->entry.tap_hold) code=s->entry.tap_hold;
            else {
                output(emit,ctx,s,s->entry.tap,KD_TAP);
                code=s->entry.hold ? s->entry.hold : s->entry.tap;
            }
        } else if (s->entry.twice) code=s->entry.twice;
        else { output(emit,ctx,s,s->entry.tap,KD_TAP);code=s->entry.tap; }
    }
    if (s->entry.term & KD_GUARD) {
        s->resolved=true;s->output=0;
        output(emit,ctx,s,code,KD_TAP);
        if (!s->down) memset(s,0,sizeof(*s));
        return;
    }
    s->resolved=true;s->output=code;
    output(emit,ctx,s,code,s->down ? KD_DOWN : KD_TAP);
    if (!s->down) memset(s,0,sizeof(*s));
}
void kd_tick(struct kd_engine *e, int64_t now, kd_emit emit, void *ctx) {
    for (unsigned i=0;i<KD_ACTIVE;i++) {
        struct kd_state *s=&e->states[i];
        if (s->used && !s->resolved && now>=s->due) resolve(s,false,emit,ctx);
    }
}
bool kd_input(struct kd_engine *e, unsigned position, uint16_t code, bool down, int64_t now,
              const struct kd_entry *entries, unsigned count, kd_emit emit, void *ctx) {
    kd_tick(e,now,emit,ctx);
    if (down) for(unsigned i=0;i<KD_ACTIVE;i++) {
        struct kd_state *s=&e->states[i];
        if(s->used && !s->resolved && s->position!=position) resolve(s,true,emit,ctx);
    }
    struct kd_state *s=NULL,*free=NULL;
    for(unsigned i=0;i<KD_ACTIVE;i++) {
        if(e->states[i].used && e->states[i].position==position) s=&e->states[i];
        if(!e->states[i].used && !free)free=&e->states[i];
    }
    if (s && !down) {
        s->down=false;
        if(s->resolved) { output(emit,ctx,s,s->output,KD_UP);memset(s,0,sizeof(*s)); }
        // Match Vial's early release decisions: no further tap can change a
        // tap/hold-only dance, or a completed configured double tap. Guarded
        // Miryoku dances must still wait so a third press can cancel them.
        else if (!(s->entry.term & KD_GUARD) &&
                 ((s->count==1 && s->entry.tap && s->entry.hold && !s->entry.twice && !s->entry.tap_hold) ||
                  (s->count==2 && s->entry.twice))) resolve(s,false,emit,ctx);
        return true;
    }
    if(code<KD_BASE || code>=KD_BASE+count || count>KD_COUNT)return false;
    if(!down)return true;
    if(!s){s=free;if(!s)return true;s->used=true;s->position=position;s->entry=entries[code-KD_BASE];}
    if(s->down || s->resolved)return true;
    s->down=true;s->due=now+((s->entry.term & KD_TERM_MASK) ? (s->entry.term & KD_TERM_MASK) : 200);
    if(s->count<255)s->count++;
    if(s->entry.term & KD_GUARD)return true;
    if(s->count==3)for(unsigned i=0;i<3;i++)output(emit,ctx,s,s->entry.tap,KD_TAP);
    else if(s->count>3)output(emit,ctx,s,s->entry.tap,KD_TAP);
    return true;
}
void kd_cancel(struct kd_engine *e,kd_emit emit,void *ctx) {
    for(unsigned i=0;i<KD_ACTIVE;i++)if(e->states[i].used && e->states[i].resolved)
        output(emit,ctx,&e->states[i],e->states[i].output,KD_UP);
    memset(e,0,sizeof(*e));
}
bool kd_busy(const struct kd_engine *e) {
    for(unsigned i=0;i<KD_ACTIVE;i++)if(e->states[i].used)return true;
    return false;
}
int64_t kd_deadline(const struct kd_engine *e) {
    int64_t at=INT64_MAX;
    for(unsigned i=0;i<KD_ACTIVE;i++)if(e->states[i].used && !e->states[i].resolved && e->states[i].due<at)at=e->states[i].due;
    return at;
}
