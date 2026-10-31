#include "store.h"
#include <stdio.h>
#include <string.h>
void ks_store_init(struct ks_store *s) {
    memset(s, 0, sizeof(*s));
    s->selected = -1;
}
bool ks_store_receive(struct ks_store *s, const char *name, const void *data, size_t len) {
    if ((strlen(name) != 5 && strlen(name) != 6) || name[0] != 'b' || name[1] < '0' || name[1] > '1' || name[2] != '/')
        return false;
    unsigned bank = name[1] - '0';
    if (!strcmp(name + 3, "hd") && len == 8) {
        memcpy(s->heads[bank], data, len);
        return true;
    }
    unsigned i = 0;
    for (unsigned j = 4; name[j]; j++) {
        unsigned digit = name[j] >= '0' && name[j] <= '9' ? name[j] - '0'
                         : name[j] >= 'a' && name[j] <= 'f' ? name[j] - 'a' + 10 : 32;
        if (digit >= 16) return false;
        i = i * 16 + digit;
    }
    if (name[3] != 'c' || i >= KS_STORE_CHUNKS) return false;
    unsigned at = i * KS_STORE_CHUNK;
    unsigned size = sizeof(struct ks_config) - at;
    if (size > KS_STORE_CHUNK)
        size = KS_STORE_CHUNK;
    if (!len || len > size)
        return false;
    memcpy((uint8_t *)&s->banks[bank] + at, data, len);
    s->received[bank] |= 1U << i;
    s->lengths[bank][i] = len;
    return true;
}
bool ks_store_load(struct ks_store *s, struct ks_state *state) {
    s->selected = -1;
    for(int b=0;b<2;b++) {
        const struct ks_config *c=&s->banks[b];
        bool old=c->version==3;
        bool v4=c->version==4;
        bool v5=c->version==5;
        bool v6=c->version==6;
        bool v7=c->version==7;
        size_t bytes=old ? offsetof(struct ks_config,dances)+4 : v4 ? offsetof(struct ks_config,overrides)+4 : v5 ? offsetof(struct ks_config,locales)+4 : v6 ? offsetof(struct ks_config,locales)+512+4 : v7 ? offsetof(struct ks_config,locales)+512+sizeof(c->repeats)+4 : sizeof(*c);
        unsigned chunks=(bytes+KS_STORE_CHUNK-1)/KS_STORE_CHUNK;
        if(s->received[b] < ((1U<<chunks)-1))continue;
        bool complete=true;
        for(unsigned i=0;i<chunks;i++) {
            unsigned n=bytes-i*KS_STORE_CHUNK;if(n>KS_STORE_CHUNK)n=KS_STORE_CHUNK;
            if(!(s->received[b] & (1U<<i)) || s->lengths[b][i]!=n)complete=false;
        }
        if(!complete)continue;
        uint32_t stored_crc=0;memcpy(&stored_crc,(uint8_t *)c+bytes-4,4);
        if(stored_crc!=ks_crc(c,bytes-4) || s->heads[b][0]!=c->generation || s->heads[b][1]!=stored_crc)continue;
        if(old || v4 || v5 || v6 || v7) {
            // Boot-time conversion uses existing transaction scratch. Keep the
            // raw bank intact so repeated loads and interrupted upgrades are safe.
            state->staged=*c;
            if(old){memset(state->staged.dances,0,sizeof(state->staged.dances));
            for(unsigned i=0;i<KD_COUNT;i++)state->staged.dances[i].term=200;}
            if(old||v4)memset(state->staged.overrides,0,sizeof(state->staged.overrides));
            if(v7)memmove(state->staged.repeats,(const uint8_t *)c+offsetof(struct ks_config,locales)+512,sizeof(state->staged.repeats));
            else memset(state->staged.repeats,0,sizeof(state->staged.repeats));
            if(v6||v7){if(!ksl_upgrade(state->staged.locales,c->locales))continue;}else ksl_default(state->staged.locales);
            state->staged.version=8;
            state->staged.crc=ks_crc(&state->staged,offsetof(struct ks_config,crc));
            c=&state->staged;
        }
        if(!ks_config_valid(c))continue;
        if(s->selected<0 || (int32_t)(c->generation-state->active.generation)>0) {state->active=*c;s->selected=b;}
    }
    return s->selected>=0;
}
int ks_store_save(struct ks_store *s, const struct ks_config *c, ks_write_record write) {
    if (!ks_config_valid(c))
        return -1;
    int bank = s->selected == 0 ? 1 : 0;
    char name[24];
    for (unsigned i = 0; i < KS_STORE_CHUNKS; i++) {
        unsigned at = i * KS_STORE_CHUNK, n = sizeof(*c) - at;
        if (n > KS_STORE_CHUNK)
            n = KS_STORE_CHUNK;
        snprintf(name, sizeof(name), "keebart/b%d/c%x", bank, i);
        int err = write(name, (const uint8_t *)c + at, n);
        if (err)
            return err;
    }
    uint32_t head[2] = {c->generation, c->crc};
    snprintf(name, sizeof(name), "keebart/b%d/hd", bank);
    int err = write(name, head, sizeof(head));
    if (!err)
        s->selected = bank;
    return err;
}
