#include "locale_protocol.h"
#include <string.h>
static uint16_t rd16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t rd32(const uint8_t *p){return rd16(p)|(uint32_t)rd16(p+2)<<16;}
static void wr32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=n>>(8*i);}
void ksl_transaction_init(struct ksl_transaction *s,const uint8_t *saved){
    memset(s,0,sizeof(*s));s->generation=1;
    if(!ksl_upgrade(s->active,saved))ksl_default(s->active);
}
uint8_t ksl_handle(struct ksl_transaction *s,const uint8_t p[25],uint8_t out[25],bool authorized){
    memset(out,0,25);
    if(p[0]==0){
        unsigned at=rd16(p+1);if(at>=KSL_BYTES)return KSL_BOUNDS;
        unsigned n=KSL_BYTES-at;if(n>18)n=18;
        wr32(out,s->generation);out[4]=at;out[5]=at>>8;out[6]=n;memcpy(out+7,s->active+at,n);return KSL_OK;
    }
    uint32_t tx=rd32(p+1);
    if(p[0]==4){wr32(out,s->transaction);out[4]=s->pending?KSL_PENDING:s->result;return KSL_OK;}
    if(!authorized||s->pending)return KSL_BUSY;
    if(p[0]==1){if(!tx)return KSL_INVALID;s->transaction=tx;s->crc=rd32(p+5);s->staging=true;s->result=KSL_PENDING;memset(s->seen,0,sizeof(s->seen));memset(s->staged,0,sizeof(s->staged));return KSL_OK;}
    if(!s->staging||tx!=s->transaction)return KSL_INVALID;
    if(p[0]==2){
        unsigned at=rd16(p+5),n=p[7];if(!n||n>16||at+n>KSL_BYTES)return KSL_BOUNDS;
        for(unsigned i=0;i<n;i++)if((s->seen[(at+i)/8]&(1U<<((at+i)%8)))&&s->staged[at+i]!=p[8+i])return KSL_INVALID;
        memcpy(s->staged+at,p+8,n);for(unsigned i=0;i<n;i++)s->seen[(at+i)/8]|=1U<<((at+i)%8);return KSL_OK;
    }
    if(p[0]!=3)return KSL_INVALID;
    unsigned end=KSL_BINDING_AT+rd16(s->staged+480)*12U;if(end>KSL_LAYER_RULE_AT)return KSL_INVALID;
    for(unsigned i=0;i<KSL_BYTES;i++)if((i<end||i>=KSL_LAYER_RULE_AT)&&!(s->seen[i/8]&(1U<<(i%8))))return KSL_INVALID;
    if(!ksl_valid(s->staged)||ksl_crc(s->staged,KSL_CRC_AT)!=s->crc)return KSL_INVALID;
    s->staging=false;s->pending=true;return KSL_PENDING;
}
void ksl_commit(struct ksl_transaction *s,bool (*persist)(const uint8_t *)){
    if(!s->pending)return;
    if(persist(s->staged)){memcpy(s->active,s->staged,KSL_BYTES);s->generation++;s->result=KSL_OK;}else s->result=KSL_IO;
    s->pending=false;
}
