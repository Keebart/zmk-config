/* The same event engine is compiled by both firmware ports. */
static uint16_t ex16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t ex32(const uint8_t *p){return ex16(p)|(uint32_t)ex16(p+2)<<16;}
static void ex_action(struct ksl_runtime *r,uint16_t c,bool down,const struct ksl_io *io){bool prior=r->extended.emitting;r->extended.emitting=true;io->action(io->context,c,down);r->extended.emitting=prior;}
static void ex_output(struct ksl_runtime *r,const uint8_t *p,uint16_t code,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io);
static void ex_unicode_add(struct kse_runtime *e,uint8_t key,uint8_t mods){if(e->unicode_count<20)e->unicode[e->unicode_count++]=(struct ksl_stroke){key,mods};}
static void ex_text(struct ksl_runtime *r,const uint8_t *p,char c,uint8_t mods){
    struct ksl_output out;
    if(ksl_resolve(p,r->active,0x8000u|(unsigned char)c,0,false,&out))
        for(unsigned j=0;j<out.count;j++)ex_unicode_add(&r->extended,out.strokes[j].key,out.strokes[j].mods|mods);
}
static void ex_hex(struct ksl_runtime *r,const uint8_t *p,uint32_t value,unsigned digits,uint8_t mods,unsigned mode){
    for(int i=(int)digits-1;i>=0;i--){
        unsigned n=(value>>(i*4))&15;
        if(mode==3)ex_unicode_add(&r->extended,n<10?(n?29+n:39):4+n-10,mods);
        else if(mode==1&&n<10)ex_unicode_add(&r->extended,n?88+n:98,mods);
        else ex_text(r,p,n<10?'0'+n:'a'+n-10,mods);
    }
}
static void ex_unicode_value(struct ksl_runtime *r,const uint8_t *p,uint32_t c,unsigned mode,int64_t now,const struct ksl_io *io){
    struct kse_runtime *e=&r->extended;
    if(e->unicode_count||!c||c>0x10ffff||(c>=0xd800&&c<=0xdfff)||(mode==1&&c>0xffff))return;
    e->unicode_at=0;e->unicode_due=now;
    uint8_t locks=io->locks?io->locks(io->context):1;
    if((mode==0||mode==2)&&(locks&2))ex_unicode_add(e,57,0);
    if(mode==1&&!(locks&1))ex_unicode_add(e,83,0);
    if(mode==0){ex_text(r,p,'u',3);ex_hex(r,p,c,c>0xffff?6:4,0,mode);ex_unicode_add(e,40,0);}
    else if(mode==1){ex_unicode_add(e,87,4);ex_hex(r,p,c,4,4,mode);ex_unicode_add(e,0,0);}
    else if(mode==2){ex_unicode_add(e,0,64);ex_unicode_add(e,0,0);ex_text(r,p,'u',0);ex_hex(r,p,c,c>0xffff?6:4,0,mode);ex_unicode_add(e,40,0);}
    else {if(c>0xffff){c-=0x10000;ex_hex(r,p,0xd800+(c>>10),4,4,mode);ex_hex(r,p,0xdc00+(c&1023),4,4,mode);}else ex_hex(r,p,c,4,4,mode);ex_unicode_add(e,0,0);}
    if((mode==0||mode==2)&&(locks&2))ex_unicode_add(e,57,0);
    if(mode==1&&!(locks&1))ex_unicode_add(e,83,0);
}
void ksl_emit_unicode(struct ksl_runtime *r,const uint8_t *p,uint32_t c,int64_t now,const struct ksl_io *io){ex_unicode_value(r,p,c,3,now,io);}
bool ksl_text(struct ksl_runtime *r,const uint8_t *p,uint32_t c,int64_t now,const struct ksl_io *io){
    if(r->extended.unicode_count||r->extended.program_active)return false;
    if(!c)return true;
    ex_unicode_value(r,p,c,r->extended.unicode_mode[r->active],now,io);
    return true;
}
static void ex_unicode(struct ksl_runtime *r,const uint8_t *p,uint16_t code,int64_t now,uint8_t mods,const struct ksl_io *io){
    const uint8_t *v=kse_options(p);unsigned slot=code-KSE_UNICODE;if(slot>=v[2])return;
    const uint8_t *u=v+32+slot*8;uint32_t c=ex32(u),shift=ex32(u+4);if((mods&0x22)&&shift)c=shift;
    ex_unicode_value(r,p,c,r->extended.unicode_mode[r->active],now,io);
}
static void ex_output(struct ksl_runtime *r,const uint8_t *p,uint16_t code,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io){
    if(code>=KSE_UNICODE&&code<KSE_UNICODE+KSE_UNICODE_COUNT){ex_unicode(r,p,code,now,mods,io);return;}
    if(ksl_character(code)){ksl_emit_character(p,r->active,code,mods,caps,io);return;}
    ex_action(r,code,true,io);tap_pause(p,code,io);ex_action(r,code,false,io);
}
static void ex_leader_end(struct ksl_runtime *r,const uint8_t *p,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io){
    struct kse_runtime *e=&r->extended;const uint8_t *v=kse_options(p),*l=v+32+v[2]*8; e->leader=false;
    for(unsigned i=0;i<v[3];i++,l+=14){if(l[0]!=e->leader_count)continue;bool same=true;for(unsigned j=0;j<l[0];j++)same&=((v[1]&8)?ex16(l+2+j*2):kse_leader_tap(p,ex16(l+2+j*2)))==e->leader_keys[j];if(same){
        if(l[1]){e->program_active=true;e->program_at=kse_program(v,i);e->program_steps=l[1];e->text_remaining=0;e->program_due=now;}
        else ex_output(r,p,ex16(l+12),now,mods,caps,io);
        break;
    }}
    e->leader_count=0;
}
/* One output per tick; delays never block input, split transport or lighting.
 * No macro slot, macro buffer, recursion, or persistent held keys are involved. */
static void ex_program_tick(struct ksl_runtime *r,const uint8_t *p,int64_t now,const struct ksl_io *io){
    struct kse_runtime *e=&r->extended;const uint8_t *v=kse_options(p);
    if(e->unicode_count||now<e->program_due||!e->program_active)return;
    if(!e->program_steps&&!e->text_remaining){e->program_active=false;return;}
    e->program_due=now+10;
    if(!e->text_remaining){
        unsigned tag=v[e->program_at],value=ex16(v+e->program_at+1);e->program_at+=3;e->program_steps--;
        if(tag==1){ex_output(r,p,value,now,0,false,io);return;}
        if(tag==2){e->program_due=now+value;return;}
        e->text_remaining=value;
        if(!value)return;
    }
    unsigned used=0;uint32_t c=kse_utf8(v+e->program_at,e->text_remaining,&used);
    if(!c){e->program_active=false;e->program_steps=0;e->text_remaining=0;return;}
    e->program_at+=used;e->text_remaining-=used;
    if(c==9||c==10){ex_output(r,p,c==9?43:40,now,0,false,io);return;}
    struct ksl_output out;
    if(c<0x3000&&ksl_resolve(p,r->active,0x8000|c,0,false,&out)&&!out.unicode)
        ex_output(r,p,0x8000|c,now,0,false,io);
    else ex_unicode_value(r,p,c,e->unicode_mode[r->active],now,io);
}
static struct kse_edge *ex_edge(struct kse_runtime *e,unsigned pos){for(unsigned i=0;i<32;i++)if(!e->edges[i].used){e->edges[i]=(struct kse_edge){.pos=pos,.used=true};return &e->edges[i];}return NULL;}
static bool ex_exception(const uint8_t *v,const struct kse_runtime *e){
    const uint8_t *text=v+32+v[2]*8+v[3]*14+v[4]*2;unsigned start=0;
    for(unsigned i=0;i<=v[5];i++)if(i==v[5]||text[i]=='|'){unsigned n=i-start;if(n&&n<=e->text_len&&!memcmp(text+start,e->text+e->text_len-n,n))return true;start=i+1;}
    return false;
}
static char ex_ascii(uint16_t c,uint8_t mods){
    if(c>=0x8020&&c<=0x807e)return (char)(c&127);
    if(c>=4&&c<=29)return (mods&0x22?'A':'a')+c-4;
    if(c==44)return ' ';
    if(c==40)return '\n';
    if(c==55)return mods&0x22?'>':'.';
    if(c==56&&mods&0x22)return '?';
    if(c==30&&mods&0x22)return '!';
    return 0;
}
// Pair transformed presses with their original source, including held Characters.
static bool ex_release(struct ksl_runtime *r,unsigned pos,const struct ksl_io *io){
    for(unsigned i=0;i<32;i++)if(r->extended.edges[i].used&&r->extended.edges[i].pos==pos){
        struct kse_edge edge=r->extended.edges[i];r->extended.edges[i].used=false;
        if(edge.release){
            if(ksl_character(edge.code))ksl_character_release(r,edge.code,io);
            else ex_action(r,edge.code,false,io);
        }
        return true;
    }
    return false;
}
static void ex_unlock_key(struct ksl_runtime *r,uint16_t code,const struct ksl_io *io){
    if(ksl_character(code))ksl_character_release(r,code,io);
    else ex_action(r,code,false,io);
}
static bool ex_event(struct ksl_runtime *r,const uint8_t *p,unsigned pos,uint16_t code,bool down,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io){
    struct kse_runtime *e=&r->extended;const uint8_t *v=kse_options(p);if(e->emitting)return false;
    unsigned shift=code==0xe1?1:code==0xe5?2:0;
    if(!down&&shift&&e->shifts[shift-1])e->shifts[shift-1]--;
    if(!down&&ex_release(r,pos,io))return true;
    if(down&&code!=KSE_LUMINO)e->dim_soon=false;
    // A momentary layer can disappear while locked. Keep the activating switch
    // usable for unlocking, including when its current binding is transparent.
    if(pos==e->keyboard_lock_position && e->lock_key_down){
        if(!down)e->lock_key_down=false;
        return true;
    }
    if(code==KSE_KEYBOARD_LOCK || (down && e->keyboard_lock && pos==e->keyboard_lock_position)){
        if(down){
            e->keyboard_lock=!e->keyboard_lock;
            e->keyboard_lock_position=pos;
            e->lock_key_down=true;
            e->arm_lock=false;
        }
        return true;
    }
    // Releases still pass so locking cannot strand a previously held key.
    if(e->keyboard_lock&&down){ex_edge(e,pos);return true;}
    if(code>=0x7f90&&code<=0x7f93){if(down)e->unicode_mode[r->active]=code-0x7f90;return true;}
    if(code==KSE_SENTENCE){if(down){e->sentence=!e->sentence;e->capitalize=true;e->terminal=false;e->text_len=0;}return true;}
    if(code==KSE_KEY_LOCK){if(down)e->arm_lock=!e->arm_lock;return true;}
    if(code==KSE_JIGGLER||(code>=0x7011&&code<=0x7013)){ex_action(r,code,down,io);return true;}
    if(code==KSE_LUMINO){if(down)e->dim_soon=true;return true;}
    if(code==KSE_SWAP||(code>=0x56f2&&code<=0x56f6)){
        if(code==0x56f2||code==0x56f3){if(down&&!e->swap_temporary){e->swap_previous=e->swap;e->swap_temporary=true;e->swap=code==0x56f2;}else if(!down&&e->swap_temporary){e->swap=e->swap_previous;e->swap_temporary=false;}}
        else if(down){e->swap=code==KSE_SWAP?!e->swap:code!=0x56f4;e->swap_once=code==0x56f6;}
        return true;
    }
    if(code==KSE_LEADER){if(down){e->program_active=false;e->program_steps=0;e->text_remaining=0;e->leader=true;e->leader_count=0;e->leader_due=now+ex16(v+16);}return true;}
    // Count logical Shift presses, not just HID bits: both home-row keys may
    // intentionally use Left Shift. Caps Word itself clears those HID bits.
    if(down&&shift&&e->shifts[shift-1]<255)e->shifts[shift-1]++;
    if(!e->shifts[0]&&!e->shifts[1]&&!(mods&0x22))e->both_shifts=false;
    if(!down)return false;
    if(e->leader){
        uint16_t tap=kse_leader_tap(p,code);
        if(tap==41){e->leader=false;e->leader_count=0;ex_edge(e,pos);return true;}
        if(((tap>=4&&tap<=0xdf)||(tap>=0x104&&tap<=0x1fff)||ksl_character(tap))&&e->leader_count<5){if(!(v[1]&8)&&(mods&0x22)){
            if(ksl_character(tap))tap=ksl_shifted_character(p,r->active,tap);
            else if(tap<0x2000)tap|=(tap&0x1000)?0x1200:0x0200;
        }e->leader_keys[e->leader_count++]=(v[1]&8)?code:tap;ex_edge(e,pos);if(v[1]&4)e->leader_due=now+ex16(v+16);if(e->leader_count==5)ex_leader_end(r,p,now,mods,caps,io);return true;}
    }
    if(code>=KSL_COMPOUND&&code<KSL_COMPOUND+KSL_ACTIONS)return false;
    if((code>=4&&code<=0x1fff)||ksl_character(code)){
        for(unsigned i=0;i<16;i++)if(e->locked[i]==code){
            if(!ex_edge(e,pos))return false;
            e->arm_lock=false;e->locked[i]=0;ex_unlock_key(r,code,io);return true;
        }
        if(e->arm_lock){
            e->arm_lock=false;
            for(unsigned i=0;i<16;i++)if(!e->locked[i]){
                if(!ex_edge(e,pos))return false;
                if(ksl_character(code)){
                    // Composed sequences complete immediately; only a held stroke needs releasing.
                    if(ksl_character_press(r,p,code,code,mods,caps,io))e->locked[i]=code;
                }else{e->locked[i]=code;ex_action(r,code,true,io);}
                return true;
            }
        }
    }
    bool locked_shift=false;for(unsigned i=0;i<16;i++)locked_shift|=e->locked[i]==0xe1||e->locked[i]==0xe5;
    if(!locked_shift&&(v[1]&2)&&shift&&((unsigned)e->shifts[0]+e->shifts[1]>=2||(mods&(shift==1?0x20:2)))&&!e->both_shifts){
        e->both_shifts=true;
        // These holds performed the gesture. Retro tapping must not type their
        // tap bindings when the switches are released in either order.
        for(unsigned i=0;i<KSL_HELD;i++){
            struct ksl_held *h=&r->held[i];
            if(h->used && h->resolved && (h->hold==0xe1 || h->hold==0xe5))h->interrupted=true;
        }
        ex_action(r,0x7c73,true,io);ex_action(r,0x7c73,false,io);
        ex_edge(e,pos);return true;
    }
    if(code>=KSE_UNICODE&&code<KSE_UNICODE+KSE_UNICODE_COUNT){ex_unicode(r,p,code,now,mods,io);ex_edge(e,pos);return true;}
    // A modified Enter may be a shortcut. It must still cancel any pending
    // capitalization from the previous line when the newline trigger is off.
    if(e->sentence&&code==40&&(v[1]&16)){
        e->capitalize=false;e->terminal=false;e->text_len=0;e->last_text=now;
    }
    if(e->sentence&&!(mods&0xdd)){
        if(ex16(v+14)&&now-e->last_text>ex16(v+14)){e->capitalize=true;e->terminal=false;e->text_len=0;}e->last_text=now;
        char c=ex_ascii(code,mods);bool letter=ksl_letter(p,r->active,code)||(code>=4&&code<=29);
        bool upper=e->capitalize&&letter;
        if(c){if(e->text_len==sizeof(e->text)){memmove(e->text,e->text+1,sizeof(e->text)-1);e->text_len--;}e->text[e->text_len++]=c;}
        if(c=='.'||c=='!'||c=='?')e->terminal=!ex_exception(v,e);
        else if(c=='\n'){e->capitalize=!(v[1]&16);e->terminal=false;e->text_len=0;}
        else if(c==' '){if(e->terminal)e->capitalize=true;}
        else if(letter){e->capitalize=false;e->terminal=false;}
        if(upper){if(ksl_character(code)){
                struct kse_edge *edge=ex_edge(e,pos);if(!edge)return false;
                edge->code=code;edge->release=ksl_character_press(r,p,code,ksl_shifted_character(p,r->active,code),mods,caps,io);
            }
            else {struct kse_edge *edge=ex_edge(e,pos);if(!edge)return false;edge->code=code|0x0200;edge->release=true;ex_action(r,edge->code,true,io);}return true;}
    }
    return false;
}
static void ex_tick(struct ksl_runtime *r,const uint8_t *p,int64_t now,uint8_t mods,bool caps,const struct ksl_io *io){
    struct kse_runtime *e=&r->extended;if(!e->shifts[0]&&!e->shifts[1]&&!(mods&0x22))e->both_shifts=false;
    if(e->leader&&now>=e->leader_due)ex_leader_end(r,p,now,mods,caps,io);
    ex_program_tick(r,p,now,io);
    if(e->unicode_count&&now>=e->unicode_due){
        if(e->unicode_at<e->unicode_count){struct ksl_stroke s=e->unicode[e->unicode_at++];if(s.key==57)e->unicode_restore_locks^=2;else if(s.key==83)e->unicode_restore_locks^=1;if(io->sequence)io->sequence(io->context,s.key,s.mods,false);else if(s.key)io->tap(io->context,s.key,s.mods);e->unicode_due=now+10;}
        else {e->unicode_count=e->unicode_at=0;if(io->sequence)io->sequence(io->context,0,0,true);}
    }
}
static void ex_cancel(struct ksl_runtime *r,const struct ksl_io *io){
    struct kse_runtime *e=&r->extended;
    if(e->unicode_count&&io->sequence){
        if(e->unicode_restore_locks&2)io->sequence(io->context,57,0,false);
        if(e->unicode_restore_locks&1)io->sequence(io->context,83,0,false);
        io->sequence(io->context,0,0,true);
    }
    for(unsigned i=0;i<16;i++)if(e->locked[i])ex_unlock_key(r,e->locked[i],io);
    for(unsigned i=0;i<32;i++)if(e->edges[i].used)ex_release(r,e->edges[i].pos,io);
    bool sentence=e->sentence,capitalize=e->capitalize;int64_t last_text=e->last_text;uint8_t modes[6];memcpy(modes,e->unicode_mode,6);memset(e,0,sizeof(*e));memcpy(e->unicode_mode,modes,6);e->sentence=sentence;e->capitalize=capitalize;e->last_text=last_text;
}
