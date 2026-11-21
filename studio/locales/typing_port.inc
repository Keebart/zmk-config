/* Included by the two Studio adapters alongside the same QMK typing engines.
 * Keep classification and output hooks identical; only report delivery differs.
 */
static unsigned typing_resolved;
static bool typing_auto_key(uint16_t code){
    if(typing_resolved||TYPING_RESOLVED())return false;
    return ksl_character(code)?ksl_auto_shift_character(TYPING_RECORD(),TYPING_COMPUTER(),code):ksl_auto_shift_key(TYPING_RECORD(),code);
}
bool caps_word_press_user(uint16_t code){
    if(ksl_character(code)){
        if(ksl_letter(TYPING_RECORD(),TYPING_COMPUTER(),code)){
            if(ksl_shifted_character(TYPING_RECORD(),TYPING_COMPUTER(),code)!=code)add_weak_mods(2);
            return true;
        }
        if(code==0x802d){add_weak_mods(2);return true;}
        return (code>=0x8030&&code<=0x8039)||code==0x805f;
    }
    // Upstream Caps Word defaults for ordinary raw actions.
    if((code>=4&&code<=29)||code==45){add_weak_mods(2);return true;}
    return (code>=30&&code<=39)||code==42||code==76||code==0x022d;
}
static bool typing_emit(uint16_t code,bool shifted){
    uint16_t source=code;
    uint8_t mods=TYPING_MODS();
    if(shifted){
        // Letters retain Caps Lock/physical Shift interaction. Symbol pairs
        // are selected explicitly so host layouts cannot change their meaning.
        if(ksl_letter(TYPING_RECORD(),TYPING_COMPUTER(),code))mods|=2;
        else code=ksl_shifted_character(TYPING_RECORD(),TYPING_COMPUTER(),code);
    }
    if(code==0x802d&&is_caps_word_on()&&(get_weak_mods()&0x22))code=0x805f;
#ifdef TYPING_PRESS
    return TYPING_PRESS(source,code,mods);
#else
    (void)source;TYPING_EMIT(code,mods);return false;
#endif
}
void autoshift_press_user(uint16_t code,bool shifted,keyrecord_t *record){
    (void)record;
    if(ksl_character(code)){typing_emit(code,shifted||(get_mods()&0x22));return;}
    if(shifted)add_weak_mods(2);
    register_code16(code);
}
void autoshift_release_user(uint16_t code,bool shifted,keyrecord_t *record){
    (void)shifted;(void)record;
    if(!ksl_character(code))unregister_code16(code);
#ifdef TYPING_RELEASE
    else TYPING_RELEASE(code);
#endif
}
static bool typing_character(void *ctx,uint16_t code,bool down,bool resolved){
    (void)ctx;
    if(down)TYPING_BEGIN();
    keyrecord_t record={.event={.pressed=down}};
    typing_resolved+=resolved;
    bool pass=TYPING_WORD(code,&record)&&process_auto_shift(code,&record);
    typing_resolved-=resolved;
    bool held=false;
    if(pass&&down)held=typing_emit(code,false);
#ifdef TYPING_RELEASE
    if(pass&&!down)TYPING_RELEASE(code);
#endif
    TYPING_SCHEDULE();
    return !pass||held;
}
