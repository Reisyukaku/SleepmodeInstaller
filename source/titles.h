/*
*   titlekeys.h
*       by Reisyukaku
*/

#ifndef INC_TITLES
#define INC_TITLES

static const Result tkey_OutOfMemoryResult = MAKERESULT(RL_STATUS, RS_OUTOFRESOURCE, RM_APPLICATION, RD_OUT_OF_MEMORY);
static const Result tkey_FileNotFoundResult = MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, 1);
static const Result tkey_JSONParseFailureResult = MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, 2);
static const Result tkey_JSONMissingTitleKeyResult = MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, 3);
static const Result tkey_TitleKeyNotInDBResult = MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, 4);
static const Result tmd_FileNotFoundResult = MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, 5);

Result getTitleKey(u64 tid, unsigned char *key);
Result getTitleVersion(u64 tid, u16 *version);

#endif
