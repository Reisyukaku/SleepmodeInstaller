/*
*   ticket.c
*       by Reisyukaku
*
*   Handles tickets and things.
*/

#include "ticket.h"

void BuildCetk(unsigned char *out, const u8 *sig, const unsigned long long titleID, const u8 *tikID, const u8 *encKey, const u8 *eccPubKey, const u16 version){        
    Cetk *tik = calloc(1, sizeof(Cetk));
    tik->sigType = 0x04000100U;
    memcpy(&tik->signature, sig, 0x13C);
    u64 tidBE = ENDIAN_SWAP_u64(titleID);
    
    TikData *tikData = calloc(1, sizeof(TikData));
    memcpy(&tikData->issuer, "Root-CA00000003-XS0000000c", 0x1B);
    memcpy(&tikData->eccPubKey, eccPubKey, 0x3C);
    tikData->version = 1;
    tikData->tikTitleVersion = ((version & 0xFF00) >> 8) | (version << 8);
    tikData->audit = 1;
    memcpy(&tikData->contIndex, contIndex, 0xAC); //Get contindex
    memcpy(&tikData->titleKey, encKey, 0x10);
    memcpy(&tikData->titleID, &tidBE, 8);
    memcpy(&tikData->ticketID, tikID, 8);
    tikData->commKeyIndex = 0;
    tik->ticketData = *tikData;
    memcpy(&tik->chainSig, chainSig, 0x700);
    
    memcpy(out, tik, sizeof(Cetk));
    free(tikData);
    free(tik);
}