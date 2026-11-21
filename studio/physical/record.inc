/* Prepared by Studio without touching settings, Bluetooth bonds or flash geometry.
 * A public custom build may set KEEBART_PHYSICAL_FLAGS to the same three bits. */
#ifndef KEEBART_PHYSICAL_FLAGS
#define KEEBART_PHYSICAL_FLAGS 0
#endif
static const volatile uint8_t ksh_record[KSH_BYTES] __attribute__((used)) = {
    'K','S','H','W','0','0','0','1', (uint8_t)KSH_MODEL, (uint8_t)(KSH_MODEL>>8), 1, 0, 0,0,0,0
};
uint8_t ksh_flags(void) {
    uint8_t bytes[KSH_BYTES];
    for (unsigned i=0;i<KSH_BYTES;i++) bytes[i]=ksh_record[i];
    /* Only the untouched placeholder may use the build-time configuration. */
    if (!bytes[11] && !bytes[12] && !bytes[13] && !bytes[14] && !bytes[15])
        return ksh_valid(KSH_MODEL, KEEBART_PHYSICAL_FLAGS) ? KEEBART_PHYSICAL_FLAGS : 0;
    return ksh_decode(bytes,KSH_MODEL);
}
