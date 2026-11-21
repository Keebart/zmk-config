// Included by the Sharp MiP status widget only in Studio builds.
// Fit text in the label's own area and avoid missing-glyph boxes.
static void display_key_text(char out[192],const char *input,const lv_font_t *font,unsigned height) {
    unsigned i=0,n=0;
    while(input[i]&&n<180){
        unsigned start=i;uint32_t c=(unsigned char)input[i++];
        if(c>=128){unsigned count=(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:3;c&=(1u<<(6-count))-1;while(count--&&input[i])c=(c<<6)|((unsigned char)input[i++]&63);}
        lv_font_glyph_dsc_t glyph;
        if(lv_font_get_glyph_dsc(font,&glyph,c,0)){memcpy(out+n,input+start,i-start);n+=i-start;}
        else if(c==0x2212)out[n++]='-';
        else n+=snprintf(out+n,192-n,"U+%04lX",(unsigned long)c);
    }
    out[n]=0;
    char full[192];memcpy(full,out,n+1);
    for(unsigned at=n;;){
        memcpy(out,full,at);out[at]=0;
        if(at<n)strcat(out,"...");
        lv_point_t size;lv_txt_get_size(&size,out,font,0,0,68,LV_TEXT_FLAG_NONE);
        if(size.y<=height||!at)break;
        at--;while(at&&((unsigned char)full[at]&0xc0)==0x80)at--;
    }
}
static void last_key_update(struct k_work *work) {
    (void)work;
    extern bool ks_display_last_key(char *,size_t);
    char text[192], fitted[192];
    if(!ks_display_last_key(text,sizeof(text)))return;
    display_key_text(fitted,text,&lv_font_montserrat_12,24);
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets,widget,node){
        memcpy(widget->state.last_key,fitted,sizeof(fitted));
        draw_bottom(widget->obj,widget->cbuf3,&widget->state);
    }
}
static K_WORK_DEFINE(widget_last_key_work, last_key_update);
static int last_key_cb(const zmk_event_t *eh) {
    (void)eh;
    if(zmk_display_is_initialized()){
        k_work_submit_to_queue(zmk_display_work_q(),&widget_last_key_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(widget_last_key, last_key_cb);
ZMK_SUBSCRIPTION(widget_last_key, ks_display_key_changed);
