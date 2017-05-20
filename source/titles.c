/*
*   titlekeys.c
*       by Reisyukaku
*/
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parson.h"
#include "titles.h"

static Result downloadTitleKeys(u64 tid, char **json){
    httpcContext ctx;
    Result res = 0;
    u32 status = 0;
    bool require_close = false;
    u32 nr_total;
    u32 nr = 0;
    size_t allocated;
    unsigned char *buf = NULL;
    unsigned char *newbuf = NULL;

    if(R_FAILED(res = httpcOpenContext(&ctx, HTTPC_METHOD_GET, "http://3ds.titlekeys.gq/json_enc", 1))) goto clean;
    require_close = true;

    if(R_FAILED(res = httpcSetSSLOpt(&ctx, SSLCOPT_DisableVerify))) goto clean;
    if(R_FAILED(res = httpcAddRequestHeaderField(&ctx, "User-Agent", "nimtest/1.0"))) goto clean;
    if(R_FAILED(res = httpcSetKeepAlive(&ctx, HTTPC_KEEPALIVE_DISABLED))) goto clean;

    if(R_FAILED(res = httpcBeginRequest(&ctx))) goto clean;
    if(R_FAILED(res = httpcGetResponseStatusCodeTimeout(&ctx, &status, 30000000000))) goto clean;//30s timeout

    if(status != 200){
        res = tkey_FileNotFoundResult;
        goto clean;
    }

    if((buf = malloc(0x1000)) == NULL){
        res = tkey_OutOfMemoryResult;
        goto clean;
    }
    allocated = 0x1000;

    nr_total = 0;

    do{
        res = httpcDownloadData(&ctx, buf+nr_total, 0x1000, &nr);
        nr_total += nr;

        if(res == HTTPC_RESULTCODE_DOWNLOADPENDING){
            if((newbuf = realloc(buf, nr_total + 0x1000)) == NULL){
                res = tkey_OutOfMemoryResult;
                goto clean;
            }
            buf = newbuf;
            allocated += 0x1000;
        }
    }while(res == HTTPC_RESULTCODE_DOWNLOADPENDING);    

    if(R_FAILED(res)) goto clean;

    if(nr_total == allocated){//no space for the '\0' that parson requires
        if((newbuf = realloc(buf, nr_total + 1)) == NULL){ 
            res = tkey_OutOfMemoryResult;
            goto clean;
        }
        buf = newbuf;
    }

    buf[nr_total] = '\0';

clean:
    if(require_close) httpcCloseContext(&ctx);

    if(R_FAILED(res)){
        if(buf) free(buf);
    }else{
        *json = (char *)buf;
    }

    return res;
}

static inline bool is_valid_hex_char(int c)
{
    return (
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f') ||
            (c >= '0' && c <= '9')
           );
}

static inline u8 get_hex_char_value(int c)
{
    if(c >= 'A' && c <= 'F'){
        return (u8)((c - 'A') + 0xA);
    }else if (c >= 'a' && c <= 'f'){
        return (u8)((c - 'a') + 0xA);
    }else{
        return (u8)(c - '0');
    }
}

static int parse_hex(const char *in, unsigned char *out, size_t outlen)
{
    size_t inlen;

    if((inlen = strlen(in)) & 1) return -1;
    if(outlen < inlen / 2) return -1;

    for(size_t i = 0; i < inlen; i += 2){
        if (!is_valid_hex_char(in[i]) || !is_valid_hex_char(in[i + 1])) return -1;

        out[i / 2] = (unsigned char)((get_hex_char_value(in[i]) << 4) | get_hex_char_value(in[i + 1]));
    }

    return 0;
}

static Result getTitleKeyFromJSON(u64 tid, char *buf, unsigned char *key){
    Result res = 0;
    JSON_Value *root;
    JSON_Array *keyinfos;
    JSON_Object *title;
    u64 tmptid;

    root = json_parse_string(buf);

    if((keyinfos = json_array(root)) == NULL){
        res = tkey_JSONParseFailureResult;
        goto clean;
    }

    for(size_t i = 0; i < json_array_get_count(keyinfos); ++i){
        //skipping error checking for performance
        title = json_array_get_object(keyinfos, i);
        tmptid = strtoull(json_object_get_string(title, "titleID"), NULL, 16);
        if(tmptid != tid)
            continue;

        if(parse_hex(json_object_get_string(title, "encTitleKey"), key, 0x10) != 0){
            res = tkey_JSONMissingTitleKeyResult;
            goto clean;
        }

        goto clean; //success
    }

    res = tkey_TitleKeyNotInDBResult; //failure

clean:
    json_value_free(root);

    return res;
}

Result getTitleKey(u64 tid, unsigned char *key){
    Result res = 0;
    char *buf = NULL;

    if((R_FAILED(res = downloadTitleKeys(tid, &buf)))) goto clean;
    if((R_FAILED(res = getTitleKeyFromJSON(tid, buf, key)))) goto clean;

clean:
    if(buf) free(buf);

    return res;
}

//this is fucking dumb, downloading a tmd to download a tmd, but registertask requires the correct tmd version
Result getTitleVersion(u64 tid, u16 *version){
    static char urlbuf[0x49];
    static unsigned char tmdbuf[0x300]; //Dont need the whole thing
    httpcContext ctx;
    Result res = 0;
    u32 nr;
    u32 status;
    bool require_close = false;

    snprintf(urlbuf, sizeof(urlbuf), "http://ccs.cdn.c.shop.nintendowifi.net/ccs/download/%016llx/tmd", tid);
    if(R_FAILED(res = httpcOpenContext(&ctx, HTTPC_METHOD_GET, urlbuf, 1))) goto clean;
    require_close = true;

    if(R_FAILED(res = httpcAddRequestHeaderField(&ctx, "User-Agent", "CTR/P/1.0.0/r61631"))) goto clean;//taken from nim, seems legit(TM)
    if(R_FAILED(res = httpcSetKeepAlive(&ctx, HTTPC_KEEPALIVE_DISABLED))) goto clean;
    if(R_FAILED(res = httpcBeginRequest(&ctx))) goto clean;
    if(R_FAILED(res = httpcGetResponseStatusCodeTimeout(&ctx, &status, 30000000000))) goto clean;//30s timeout

    if(status != 200){
        res = tmd_FileNotFoundResult;
        goto clean;
    }
    
    httpcDownloadData(&ctx, tmdbuf, sizeof(tmdbuf), &nr); //will hang httpcCloseContext if remote sends > sizeof(tidbuf), but this never happens?
    
    *version = (tmdbuf[0x1dc] << 8) | tmdbuf[0x1dd];
    
    printf("TMD version: %u\n", *version);

clean:
    if(require_close) httpcCloseContext(&ctx);

    return res;
}
