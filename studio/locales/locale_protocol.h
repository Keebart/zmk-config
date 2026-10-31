#pragma once
#include "locale.h"
enum { KSL_OK, KSL_INVALID, KSL_BOUNDS, KSL_BUSY, KSL_IO, KSL_PENDING };
struct ksl_transaction {
    uint8_t active[KSL_BYTES], staged[KSL_BYTES], seen[KSL_BYTES/8];
    uint32_t transaction, crc, generation;
    uint8_t result;
    bool staging, pending;
};
void ksl_transaction_init(struct ksl_transaction *,const uint8_t *);
uint8_t ksl_handle(struct ksl_transaction *,const uint8_t[25],uint8_t[25],bool);
void ksl_commit(struct ksl_transaction *,bool (*)(const uint8_t *));
