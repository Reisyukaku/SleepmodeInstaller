/*
*   nimtest.c
*       by Reisyukaku
*
*   NIM test application for downloading titles in sleep mode.
*/

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <3ds.h>
#include "am.h"
#include "ticket.h"
#include "titles.h"

static Result installTicket(u64 tid, unsigned char *encTitleKey){
    unsigned char *cetk = malloc(sizeof(Cetk));
    unsigned char dummySig[0x13C] = {0}; //doesnt matter
    unsigned char tikID[8] = {0}; //doesnt matter
    unsigned char eccPubKey[0x3C] = {0}; //doesnt matter
    BuildCetk(cetk, dummySig, tid, tikID, encTitleKey, eccPubKey, 0);
    
    Handle tik;
    Result res;
    u32 wb;
    res = AM_InstallTicketBegin(&tik);
    FSFILE_Write(tik, &wb, 0, cetk, sizeof(Cetk), 0);
    res |= AM_InstallTicketFinish(tik);
    
    return res;
}

static Result startInstall(u64 tid){
    Result res;
    NIM_TitleConfig tc;
    u16 version;
    
    res = getTitleVersion(tid, &version);
    printf("Getting TMD version: %s (%lx)\n", R_FAILED(res) ? "\x1b[31mfailure\x1b[0m" : "\x1b[32msuccess\x1b[0m", res);
            
    NIMS_MakeTitleConfig(&tc, tid, version, 0, MEDIATYPE_SD);
    res = NIMS_RegisterTask(&tc, "(game name, read from samurai if available)", "(publisher)");
    printf("Registering task: %s (%lx)\n", R_FAILED(res) ? "\x1b[31mfailure\x1b[0m" : "\x1b[32msuccess\x1b[0m", res);
    return res;
}

static SwkbdCallbackResult getTitleIDFromUser_cb(void *unused, const char **ppMessage, const char *text, size_t textlen){
    static char errbuf[SWKBD_MAX_CALLBACK_MSG_LEN];
    u64 tid;
    char *ep;

    tid = strtoull(text, &ep, 16);

    if(*ep != '\0' || errno != 0){
        snprintf(errbuf, sizeof(errbuf), "Invalid title ID %s.", text);
        *ppMessage = errbuf;
        return SWKBD_CALLBACK_CONTINUE;
    }

    //tid must start with 0004
    if((tid & 0xFFFF000000000000ULL) >> 48 != 4){
        snprintf(errbuf, sizeof(errbuf), "Invalid platform (TID did not start with 0004).");
        *ppMessage = errbuf;
        return SWKBD_CALLBACK_CONTINUE;
    }

    //must be mediatype_sd: !(tidhigh & 0x10 [system bit])
    if((tid >> 32) & 0x10){
        snprintf(errbuf, sizeof(errbuf), "You cannot download system titles.");
        *ppMessage = errbuf;
        return SWKBD_CALLBACK_CONTINUE;
    }

    return SWKBD_CALLBACK_OK;
}

static int getTitleIDFromUser(u64 *tid){
    static SwkbdState swkbd;//rip stack
    char tidbuf[17];
    SwkbdButton button;

    swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, 16);
    swkbdSetValidation(&swkbd, SWKBD_FIXEDLEN, SWKBD_FILTER_CALLBACK, 16);
    swkbdSetFeatures(&swkbd, SWKBD_DARKEN_TOP_SCREEN|SWKBD_FIXED_WIDTH);
    swkbdSetHintText(&swkbd, "000400000fffff00");
    swkbdSetInitialText(&swkbd, "000400000fffff00");
    swkbdSetFilterCallback(&swkbd, getTitleIDFromUser_cb, NULL);
    //Defaults are: Left button is "Cancel", right button is "OK"

    button = swkbdInputText(&swkbd, tidbuf, sizeof(tidbuf));

    if(button == SWKBD_BUTTON_NONE){
        switch(swkbdGetResult(&swkbd)){
        case SWKBD_INVALID_INPUT:
            printf("SWKBD LIBRARY USAGE ERROR!\n");
            return -1;
        case SWKBD_OUTOFMEM:
            printf("SWKBD OOM!\n");
            return -1;
        case SWKBD_POWERPRESSED:
            printf("Power button pressed; aborting.\n");
            return -1;
        case SWKBD_BANNED_INPUT:
            printf("Invalid input; aborting.\n");
            return -1;
        default:
            printf("UNEXPECTED SWKBD FAILURE!\n");
            return -1;
        }
    }else if(button == SWKBD_BUTTON_LEFT){
        //Cancel
        printf("Cancelled by user.\n");
        return 1;
    }

    //Filter callback ensures we have valid input
    *tid = strtoull(tidbuf, NULL, 16);

    return 0;
}

static bool hasTicket(u64 tid)
{
    u32 ntickets;
    Result res;

    if(R_FAILED(res = AM_GetNumTicketsOfProgram(tid, &ntickets))){
        if(res == (Result)0xD8A083FAU) return false; //Title not found

        ERRF_ThrowResultWithMessage(res, "AM_GetNumTicketsOfProgram failed");
        return false;//never reached
    }

    return (ntickets > 0);
}

void runInstaller(void){
    Result res;
    static u8 nimBuf[0x20000];
    u64 tid;
    
    printf(
        "sleep mode installer\n"
        "--------------------\n"
    );

    //Get TID from user
    if(getTitleIDFromUser(&tid) == 0){
        //Obtain ticket if needed
        if(!hasTicket(tid)){
            unsigned char key[0x10];
            printf("Ticket not found!\nTrying to download title key for %016llx\n", tid);
            
            res = getTitleKey(tid, key);
            printf("Obtaining titlekey: %s (%lx)\n", R_FAILED(res) ? "\x1b[31mfailure\x1b[0m" : "\x1b[32msuccess\x1b[0m", res);
            if(R_FAILED(res)) return;
            
            res = installTicket(tid, key);
            printf("Installing generated ticket: %s (%lx)\n", R_FAILED(res) ? "\x1b[31mfailure\x1b[0m" : "\x1b[32msuccess\x1b[0m", res);
            if(R_FAILED(res)) return;
        }else{
            printf("Ticket found!\n");
        }

        //Start install
        if(R_SUCCEEDED(res = nimsInit(nimBuf, sizeof(nimBuf)))){
            switch(startInstall(tid)){
                case 0xd8808040U:
                    printf("\x1b[31mError: missing ticket\x1b[0m");
                    break;
                case 0xc820d005U:
                    printf("\x1b[31mError: title already installed\x1b[0m");
                    break;
                case 0xc920d008U:
                    printf("\x1b[31mError: TMD not found (wrong version?)\x1b[0m");
                    break;
                default:
                    break;
            }
        }else{
            printf("Unable to access nim:s: %08lx!\n", res);
        }
    }
}

int main(void){
    //Init
    amInit();
    httpcInit(0);
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    
    //Run main routine
    runInstaller();

    //Loop
    printf("Press START to exit.\n");
    while(aptMainLoop()){
        gspWaitForVBlank();
        hidScanInput();

        if(hidKeysDown() & KEY_START) break;

        gfxFlushBuffers();
        gfxSwapBuffers();
    }

    //Exit
    httpcExit();
    amExit();
    gfxExit();
    nimsExit();
    return 0;
}
