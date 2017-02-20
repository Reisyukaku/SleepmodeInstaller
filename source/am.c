/*
*   am.c
*       by Reisyukaku
*/

#include <3ds.h>
#include "am.h"

Result AM_GetNumTicketsOfProgram(u64 titleId, u32 *ntickets){
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x81f, 2, 0); // 0x81f0080
    cmdbuf[1] = (u32)(titleId & 0xFFFFFFFF);
    cmdbuf[2] = (u32)((titleId >> 32) & 0xFFFFFFFF);

    if(R_FAILED(ret = svcSendSyncRequest(*(amGetSessionHandle())))) return ret;
    if(R_FAILED(ret = (Result)cmdbuf[1])) return ret;

    if(ntickets) *ntickets = cmdbuf[2];

    return ret;
}
