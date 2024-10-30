/*
 * This file is part of the openHiTLS project.
 *
 * openHiTLS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *     http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/* BEGIN_HEADER */
#include "hitls_csr_local.h"
#include "hitls_x509.h"
#include "bsl_list.h"
#include "sal_file.h"
#include "bsl_obj_internal.h"
#include "hitls_x509_errno.h"
#include "crypt_types.h"
#include "crypt_errno.h"
#include "crypt_encode.h"
#include "crypt_eal_encode.h"
#include "crypt_eal_rand.h"
#include "eal_pkey_local.h"
#include "bsl_list_internal.h"

/* END_HEADER */
#define MAX_DATA_LEN 128

void *TestMallocErr(uint32_t len)
{
    (void)len;
    return NULL;
}

static void *TestMalloc(uint32_t len)
{
    return malloc((size_t)len);
}

static void TestMemInitErr()
{
    BSL_SAL_MemCallback cb = {TestMallocErr, free};
    BSL_SAL_RegMemCallback(&cb);
}

static void TestMemInitCorrect()
{
    static BSL_SAL_MemCallback cb = {TestMalloc, free};
    BSL_SAL_RegMemCallback(&cb);
}

/* BEGIN_CASE */
void SDV_X509_CSR_New_FUNC_TC001(void)
{
    TestMemInitErr();
    HITLS_X509_Csr *csr = HITLS_X509_CsrNew();
    ASSERT_EQ(csr, NULL);

    TestMemInitCorrect();
    csr = HITLS_X509_CsrNew();
    ASSERT_NE(csr, NULL);

exit:
    HITLS_X509_CsrFree(csr);
}
/* END_CASE */

/* BEGIN_CASE */
void SDV_X509_CSR_Free_FUNC_TC001(void)
{
    TestMemInit();
    HITLS_X509_Csr *csr = HITLS_X509_CsrNew();
    ASSERT_NE(csr, NULL);
    HITLS_X509_CsrFree(csr);

    HITLS_X509_CsrFree(NULL);

exit:
   return;
}
/* END_CASE */

/**
 * parse csr file api test
*/
/* BEGIN_CASE */
void SDV_X509_CSR_PARSE_API_TC001(void)
{
    TestMemInit();
    HITLS_X509_Csr *csr = NULL;
    const char *path = "../testdata/cert/pem/csr/csr.pem";
    ASSERT_NE(HITLS_X509_CsrParseFile(BSL_FORMAT_PEM, path, NULL), HITLS_X509_SUCCESS);

    ASSERT_NE(HITLS_X509_CsrParseFile(BSL_FORMAT_UNKNOWN, path, &csr), HITLS_X509_SUCCESS);

    ASSERT_NE(HITLS_X509_CsrParseFile(BSL_FORMAT_PEM, "/errPath/csr.pem", &csr), HITLS_X509_SUCCESS);

    ASSERT_NE(HITLS_X509_CsrParseFile(BSL_FORMAT_PEM, NULL, &csr), HITLS_X509_SUCCESS);

    /* the csr file don't have read permission */

exit:
    HITLS_X509_CsrFree(csr);
}
/* END_CASE */

/**
 * parse csr buffer api test
*/
/* BEGIN_CASE */
void SDV_X509_CSR_PARSE_API_TC002(void)
{
    TestMemInit();
    HITLS_X509_Csr *csr = NULL;
    uint8_t data[MAX_DATA_LEN] = {};
    BSL_Buffer buffer = {data, sizeof(data)};
    BSL_Buffer ori = {NULL, 0};
    ASSERT_EQ(HITLS_X509_CsrParseBuff(BSL_FORMAT_ASN1, &buffer, NULL), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_CsrParseBuff(BSL_FORMAT_ASN1, NULL, NULL), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_CsrParseBuff(BSL_FORMAT_ASN1, &ori, &csr), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_CsrParseBuff(BSL_FORMAT_ASN1, &ori, &csr), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_CsrParseBuff(BSL_FORMAT_UNKNOWN, &buffer, &csr), HITLS_X509_ERR_NOT_SUPPORT_FORMAT);
exit:
    return;
}
/* END_CASE */


/* BEGIN_CASE */
void SDV_X509_CSR_PARSE_FUNC_TC001(int format, char *path, int expRawDataLen, int expSignAlg, Hex *expectedSign,
    int expectUnusedbits)
{
    TestMemInit();
    HITLS_X509_Csr *csr = NULL;
    uint32_t rawDataLen = 0;
    ASSERT_EQ(HITLS_X509_CsrParseFile(format, path, &csr), HITLS_X509_SUCCESS);

    ASSERT_EQ(HITLS_X509_CsrVerify(csr), HITLS_X509_SUCCESS);

    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_ENCODELEN, &rawDataLen, sizeof(rawDataLen)), 0);
    ASSERT_EQ(rawDataLen, expRawDataLen);

    uint8_t *rawData = NULL;
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_ENCODE, &rawData, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(rawData, NULL);

    CRYPT_EAL_PkeyCtx *publicKey = NULL;
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_PUBKEY, &publicKey, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(publicKey, NULL);
    CRYPT_EAL_PkeyFreeCtx(publicKey);

    int32_t alg = 0;
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_SIGNALG, &alg, sizeof(alg)), HITLS_X509_SUCCESS);
    ASSERT_EQ(alg, expSignAlg);

    int32_t ref = 0;
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_REF_UP, &ref, sizeof(ref)), HITLS_X509_SUCCESS);
    ASSERT_EQ(ref, 2);
    HITLS_X509_CsrFree(csr);

    ASSERT_NE(csr->signature.buff, NULL);
    ASSERT_EQ(csr->signature.len, expectedSign->len);
    ASSERT_EQ(memcmp(csr->signature.buff, expectedSign->x, expectedSign->len), 0);
    ASSERT_EQ(csr->signature.unusedBits, expectUnusedbits);

exit:
    HITLS_X509_CsrFree(csr);
}
/* END_CASE */

/**
 * Test parse csr: check subject name
*/
/* BEGIN_CASE */
void SDV_X509_CSR_PARSE_FUNC_TC002(int format, char *path, int expectedNum, char *dnType1,
    char *dnName1, char *dnType2, char *dnName2, char *dnType3, char *dnName3, char *dnType4, char *dnName4,
    char *dnType5, char *dnName5)
{
    TestMemInit();
    HITLS_X509_Csr *csr = NULL;
    ASSERT_EQ(HITLS_X509_CsrParseFile(format, path, &csr), HITLS_X509_SUCCESS);

    BslList *rawSubject = NULL;
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_SUBJECT_DNNAME, &rawSubject, sizeof(BslList *)), 0);
    ASSERT_NE(rawSubject, NULL);
    int count = BSL_LIST_COUNT(rawSubject);
    ASSERT_EQ(count, expectedNum);
    char *dnTypes[5] = {dnType1, dnType2, dnType3, dnType4, dnType5};
    char *dnName[5] = {dnName1, dnName2, dnName3, dnName4, dnName5};
    HITLS_X509_NameNode *nameNode = BSL_LIST_GET_FIRST(rawSubject);
    for (int i = 0; i < count && count <= 10 && nameNode != NULL; i++, nameNode = BSL_LIST_GET_NEXT(rawSubject)) {
        if (nameNode->layer == 1) {
            continue;
        }
        BSL_ASN1_Buffer nameType = nameNode->nameType;
        BSL_ASN1_Buffer nameValue = nameNode->nameValue;
        BslOidString typeOid = {
            .octs = (char *)nameType.buff,
            .octetLen = nameType.len,
        };
        const char *oidName = BSL_OBJ_GetOidNameFromOid(&typeOid);
        ASSERT_NE(oidName, NULL);
        ASSERT_EQ(strcmp(dnTypes[i / 2], oidName), 0);
        ASSERT_EQ(memcmp(dnName[i / 2], nameValue.buff, strlen(dnName[i / 2])), 0);
    }

exit:
    HITLS_X509_CsrFree(csr);
}
/* END_CASE */

/**
 * Test parse csr: check the count of the attribute list
*/
/* BEGIN_CASE */
void SDV_X509_CSR_PARSE_FUNC_TC003(int format, char *path, int attrNum, int attrCid, Hex *attrValue)
{
    TestMemInit();
    HITLS_X509_Csr *csr = NULL;
    BslList *rawAttrs = NULL;

    ASSERT_EQ(HITLS_X509_CsrParseFile(format, path, &csr), HITLS_X509_SUCCESS);

    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_CSR_GET_ATTRIBUTES, &rawAttrs, sizeof(BslList *)), HITLS_X509_SUCCESS);
    ASSERT_NE(rawAttrs, NULL);
    ASSERT_EQ(attrNum, BSL_LIST_COUNT(rawAttrs));
    if (attrNum == 0) {
        goto exit;
    }

    HITLS_X509_AttrEntry *entry = BSL_LIST_GET_FIRST(rawAttrs);
    ASSERT_EQ(attrCid, entry->cid);
    BslOidString *oid = BSL_OBJ_GetOidFromCID(entry->cid);
    ASSERT_NE(oid, NULL);
    ASSERT_COMPARE("csr attr oid", entry->attrId.buff, entry->attrId.len, (uint8_t *)oid->octs, oid->octetLen);
    ASSERT_COMPARE("csr attr value", entry->attrValue.buff, entry->attrValue.len, attrValue->x, attrValue->len);

exit:
    HITLS_X509_CsrFree(csr);
}
/* END_CASE */

/**
 * encode csr buffer api test
*/
/* BEGIN_CASE */
void SDV_X509_CSR_GEN_API_TC001(void)
{
    TestMemInit();

    HITLS_X509_Csr *csr = NULL;
    const char *path = "../testdata/cert/pem/csr/csr.pem";
    const char *writePath = "../testdata/cert/pem/csr/genCsr.pem";
    int32_t ret = HITLS_X509_CsrParseFile(BSL_FORMAT_PEM, path, &csr);
    ASSERT_EQ(ret, HITLS_X509_SUCCESS);

    ASSERT_EQ(HITLS_X509_CsrGenFile(NULL, BSL_FORMAT_PEM, writePath), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_CsrGenFile(csr, BSL_FORMAT_UNKNOWN, writePath), HITLS_X509_ERR_NOT_SUPPORT_FORMAT);
    ASSERT_EQ(HITLS_X509_CsrGenFile(csr, BSL_FORMAT_PEM, NULL), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_NE(HITLS_X509_CsrGenFile(csr, BSL_FORMAT_PEM, "/errPath/csr.pem"), HITLS_X509_SUCCESS);
exit:
    HITLS_X509_CsrFree(csr);
    return;
}
/* END_CASE */

/**
 * encode csr buffer api test
*/
/* BEGIN_CASE */
void SDV_X509_CSR_GEN_API_TC002(void)
{
    TestMemInit();
    HITLS_X509_Csr *csr = HITLS_X509_CsrNew();
    ASSERT_NE(csr, NULL);
    uint8_t data[MAX_DATA_LEN] = {};
    BSL_Buffer buffer = {NULL, 0};
    BSL_Buffer buffErr = {data, sizeof(data)};
    ASSERT_EQ(HITLS_X509_CsrGenBuff(csr, BSL_FORMAT_UNKNOWN, &buffer), HITLS_X509_ERR_NOT_SUPPORT_FORMAT);
    ASSERT_EQ(HITLS_X509_CsrGenBuff(NULL, BSL_FORMAT_PEM, &buffer), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_CsrGenBuff(csr, BSL_FORMAT_PEM, NULL), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_CsrGenBuff(csr, BSL_FORMAT_PEM, &buffErr), HITLS_X509_ERR_INVALID_PARAM);
exit:
    HITLS_X509_CsrFree(csr);
    return;
}
/* END_CASE */

/**
 * 1. transform format
*/
/* BEGIN_CASE */
void SDV_X509_CSR_GEN_FUNC_TC001(int inFormat, char *csrPath, int outFormat)
{
    TestMemInit();
    TestRandInit();
    HITLS_X509_Csr *csr = NULL;
    BSL_Buffer encode = {NULL, 0};
    uint8_t *data = NULL;
    uint32_t dataLen = 0;
    BSL_Buffer asnEncode = {NULL, 0};

    ASSERT_EQ(BSL_SAL_ReadFile(csrPath, &data, &dataLen), BSL_SUCCESS);

    BSL_Buffer ori = {data, dataLen};
    ASSERT_EQ(HITLS_X509_CsrParseBuff(inFormat, &ori, &csr), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrGenBuff(csr, outFormat, &encode), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_ENCODELEN, &asnEncode.dataLen, sizeof(asnEncode.dataLen)),
        HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_ENCODE, &asnEncode.data, 0), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrVerify(csr), HITLS_X509_SUCCESS);

    if (inFormat == outFormat) {
        ASSERT_EQ(dataLen, encode.dataLen);
        ASSERT_EQ(memcmp(encode.data, data, dataLen), 0);
    } else if (inFormat == BSL_FORMAT_ASN1 && outFormat == BSL_FORMAT_PEM) {
        ASSERT_EQ(dataLen, asnEncode.dataLen);
        ASSERT_EQ(memcmp(asnEncode.data, data, dataLen), 0);
    } else {
        ASSERT_EQ(csr->rawDataLen, encode.dataLen);
        ASSERT_EQ(memcmp(encode.data, csr->rawData, encode.dataLen), 0);
    }
exit:
    BSL_SAL_FREE(data);
    BSL_SAL_FREE(encode.data);
    HITLS_X509_CsrFree(csr);
}
/* END_CASE */


/**
 * 1. parse csr
 * 2. not set some value after parse
*/
/* BEGIN_CASE */
void SDV_X509_CSR_GEN_FUNC_TC002(int format, char *csrPath, int keyFormat, char *privPath, int keyType, int pkeyId,
    int pad)
{
    TestMemInit();
    HITLS_X509_Csr *csr = NULL;
    uint8_t *data = NULL;
    uint32_t dataLen = 0;
    CRYPT_EAL_PkeyCtx *privKey = NULL;
    int32_t mdId = CRYPT_MD_SHA256;

    ASSERT_EQ(BSL_SAL_ReadFile(csrPath, &data, &dataLen), BSL_SUCCESS);

    BSL_Buffer ori = {data, dataLen};
    ASSERT_EQ(HITLS_X509_CsrParseBuff(format, &ori, &csr), HITLS_X509_SUCCESS);
    ASSERT_EQ(CRYPT_EAL_DecodeFileKey(keyFormat, keyType, privPath, NULL, 0, &privKey), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_PRIVKEY, privKey, sizeof(CRYPT_EAL_PkeyCtx *)),
        HITLS_X509_ERR_SET_AFTER_PARSE);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_PUBKEY, privKey, sizeof(CRYPT_EAL_PkeyCtx *)),
        HITLS_X509_ERR_SET_AFTER_PARSE);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_MD_ID, &mdId, sizeof(int32_t)),
        HITLS_X509_ERR_SET_AFTER_PARSE);
    if (pkeyId == CRYPT_PKEY_RSA) {
        ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_RSA_PADDING, &pad, sizeof(int32_t)),
            HITLS_X509_ERR_SET_AFTER_PARSE);
        if (pad == CRYPT_PKEY_EMSA_PSS) {
            CRYPT_RSA_PssPara para = {20, mdId, CRYPT_MD_SHA256}; // 20 is salt len
            ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_RSA_PSS_PARAM, &para, sizeof(CRYPT_RSA_PssPara)),
                HITLS_X509_ERR_SET_AFTER_PARSE);
        }
    }

exit:
    BSL_SAL_FREE(data);
    HITLS_X509_CsrFree(csr);
    CRYPT_EAL_PkeyFreeCtx(privKey);
}
/* END_CASE */

static void ResetCsrNameList(HITLS_X509_Csr *raw)
{
    BslList *newSubject = NULL;
    (void)HITLS_X509_CsrCtrl(raw, HITLS_X509_GET_SUBJECT_DNNAME, &newSubject, sizeof(BslList **));
    newSubject->curr = NULL;
    newSubject->last = NULL;
    newSubject->first = NULL;
    newSubject->dataSize = sizeof(HITLS_X509_NameNode);
    newSubject->count = 0;
}

static void ResetCsrAttrsList(HITLS_X509_Csr *raw)
{
    BslList *newAttrs = NULL;
    (void)HITLS_X509_CsrCtrl(raw, HITLS_X509_CSR_GET_ATTRIBUTES, &newAttrs, sizeof(BslList **));
    newAttrs->curr = NULL;
    newAttrs->last = NULL;
    newAttrs->first = NULL;
    newAttrs->dataSize = sizeof(HITLS_X509_NameNode);
    newAttrs->count = 0;
}

static int32_t SetCsr(HITLS_X509_Csr *raw, HITLS_X509_Csr *new, CRYPT_EAL_PkeyCtx *priv, int32_t pad, int32_t mdId,
    int mgfId, int saltLen)
{
    int32_t ret = 1;
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_SET_PUBKEY, raw->reqInfo.ealPubKey, sizeof(CRYPT_EAL_PkeyCtx *)), 0);
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_SET_PRIVKEY, priv, sizeof(void *)), 0);
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_SET_SIGN_MD_ID, &mdId, sizeof(int32_t)), 0);
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_SET_SIGN_RSA_PADDING, &pad, sizeof(int32_t)), 0);
    if (pad == CRYPT_PKEY_EMSA_PSS) {
        CRYPT_RSA_PssPara para = {saltLen, mdId, mgfId};
        ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_SET_SIGN_RSA_PSS_PARAM, &para, sizeof(CRYPT_RSA_PssPara)), 0);
    }
    BslList *rawSubject = NULL;
    BslList *newSubject = NULL;
    ASSERT_EQ(HITLS_X509_CsrCtrl(raw, HITLS_X509_GET_SUBJECT_DNNAME, &rawSubject, sizeof(BslList *)), 0);
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_GET_SUBJECT_DNNAME, &newSubject, sizeof(BslList *)), 0);
    ASSERT_NE(rawSubject, NULL);
    ASSERT_NE(newSubject, NULL);
    ASSERT_NE(BSL_LIST_Concat(newSubject, rawSubject), NULL);

    BslList *rawAttrs = NULL;
    BslList *newAttrs = NULL;
    ASSERT_EQ(HITLS_X509_CsrCtrl(raw, HITLS_X509_CSR_GET_ATTRIBUTES, &rawAttrs, sizeof(BslList *)), 0);
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_CSR_GET_ATTRIBUTES, &newAttrs, sizeof(BslList *)), 0);
    ASSERT_NE(rawAttrs, NULL);
    ASSERT_NE(newAttrs, NULL);
    if (BSL_LIST_COUNT(rawAttrs) > 0) {
        ASSERT_NE(BSL_LIST_Concat(newAttrs, rawAttrs), NULL);
    }

    ret = 0;
exit:
    return ret;
}

/**
 * 1. set subject name, private key, public key, mdId, padding
 * 2. generate csr
 * 3. compare the generated csr buff
*/
/* BEGIN_CASE */
void SDV_X509_CSR_GEN_FUNC_TC003(int csrFormat, char *csrPath, int keyFormat, char *privPath, int keyType, int pad,
    int mdId, int mgfId, int saltLen)
{
    TestMemInit();
    HITLS_X509_Csr *raw = NULL;
    HITLS_X509_Csr *new = NULL;
    CRYPT_EAL_PkeyCtx *privKey = NULL;
    BSL_Buffer encode = {NULL, 0};
    uint8_t *newCsrEncode = NULL;
    uint32_t newCsrEncodeLen = 0;
    uint8_t *rawCsrEncode = NULL;
    uint32_t rawCsrEncodeLen = 0;

    ASSERT_EQ(CRYPT_EAL_DecodeFileKey(keyFormat, keyType, privPath, NULL, 0, &privKey), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrParseFile(csrFormat, csrPath, &raw), HITLS_X509_SUCCESS);
    new = HITLS_X509_CsrNew();
    ASSERT_NE(new, NULL);
    ASSERT_EQ(SetCsr(raw, new, privKey, pad, mdId, mgfId, saltLen), 0);
    ASSERT_EQ(HITLS_X509_CsrGenBuff(new, csrFormat, &encode), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_GET_ENCODELEN, &newCsrEncodeLen, sizeof(newCsrEncodeLen)),
        HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_GET_ENCODE, &newCsrEncode, 0), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(raw, HITLS_X509_GET_ENCODELEN, &rawCsrEncodeLen, sizeof(rawCsrEncodeLen)),
        HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(raw, HITLS_X509_GET_ENCODE, &rawCsrEncode, 0), HITLS_X509_SUCCESS);

    if (pad == CRYPT_PKEY_EMSA_PSS) {
        ASSERT_EQ(raw->reqInfo.reqInfoRawDataLen, new->reqInfo.reqInfoRawDataLen);
        ASSERT_EQ(memcmp(raw->reqInfo.reqInfoRawData, new->reqInfo.reqInfoRawData, raw->reqInfo.reqInfoRawDataLen), 0);
    } else {
        ASSERT_EQ(newCsrEncodeLen, rawCsrEncodeLen);
        ASSERT_EQ(memcmp(newCsrEncode, rawCsrEncode, rawCsrEncodeLen), 0);
    }
exit:
    HITLS_X509_CsrFree(raw);
    ResetCsrNameList(new);
    ResetCsrAttrsList(new);
    HITLS_X509_CsrFree(new);
    BSL_SAL_FREE(encode.data);
    CRYPT_EAL_PkeyFreeCtx(privKey);
}
/* END_CASE */

void SetRsaPara(CRYPT_EAL_PkeyPara *para, uint8_t *e, uint32_t eLen, uint32_t bits)
{
    para->id = CRYPT_PKEY_RSA;
    para->para.rsaPara.e = e;
    para->para.rsaPara.eLen = eLen;
    para->para.rsaPara.bits = bits;
}

/**
 * 1. csr ctrl interface test
*/
/* BEGIN_CASE */
void SDV_X509_CSR_CTRL_SET_API_TC001(char *csrPath)
{
    TestMemInit();

    BSL_Buffer encodeRaw = { NULL, 0};
    HITLS_X509_Csr *csr = NULL;
    uint8_t *csrEncode = NULL;
    uint32_t csrEncodeLen = 0;
    CRYPT_EAL_PkeyCtx *pkey = NULL;

    ASSERT_EQ(BSL_SAL_ReadFile(csrPath, &encodeRaw.data, &encodeRaw.dataLen), HITLS_X509_SUCCESS);
    ASSERT_NE(encodeRaw.data, NULL);
    ASSERT_EQ(HITLS_X509_CsrParseBuff(BSL_FORMAT_ASN1, &encodeRaw, &csr), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_GET_ENCODE, &csrEncode, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, 0xFFFF, &csrEncode, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_ENCODE, NULL, 0), HITLS_X509_SUCCESS);

    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_ENCODELEN, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_GET_ENCODELEN, &csrEncodeLen, sizeof(csrEncodeLen)),
        HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_ENCODELEN, &csrEncodeLen, 0), HITLS_X509_SUCCESS);

    int ref = 0;
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_REF_UP, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_REF_UP, &ref, 0), HITLS_X509_SUCCESS);

    int padding = CRYPT_PKEY_EMSA_PKCSV15;
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_RSA_PADDING, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_RSA_PADDING, &padding, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_SET_SIGN_RSA_PADDING, &padding, 0), HITLS_X509_SUCCESS);

    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_PUBKEY, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_GET_PUBKEY, &pkey, 0), HITLS_X509_SUCCESS);
    int32_t signAlg = 0;
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_SIGNALG, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_GET_SIGNALG, &signAlg, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_SIGNALG, &signAlg, 0), HITLS_X509_SUCCESS);

    BslList *subjectName = 0;
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_SUBJECT_DNNAME, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_GET_SUBJECT_DNNAME, &subjectName, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_SUBJECT_DNNAME, &subjectName, 0), HITLS_X509_SUCCESS);

    BslList *attrs = 0;
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_CSR_GET_ATTRIBUTES, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_CSR_GET_ATTRIBUTES, &attrs, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_CSR_GET_ATTRIBUTES, &attrs, 0), HITLS_X509_SUCCESS);

exit:
    BSL_SAL_FREE(encodeRaw.data);
    HITLS_X509_CsrFree(csr);
}
/* END_CASE */

/**
 * 1. csr ctrl interface test
*/
/* BEGIN_CASE */
void SDV_X509_CSR_CTRL_SET_API_TC002(char *csrPath)
{
    TestMemInit();
    TestRandInit();
    HITLS_X509_Csr *csr = NULL;
    CRYPT_EAL_PkeyCtx *rsaPkey = NULL;
    CRYPT_EAL_PkeyCtx *eccPkey = NULL;
    uint8_t e[] = {1, 0, 1};

    int32_t ret = HITLS_X509_CsrParseFile(BSL_FORMAT_ASN1, csrPath, &csr);
    ASSERT_EQ(ret, HITLS_X509_SUCCESS);

    rsaPkey = CRYPT_EAL_PkeyNewCtx(CRYPT_PKEY_RSA);
    ASSERT_NE(rsaPkey, NULL);
    CRYPT_EAL_PkeyPara rsaPara = {0};
    SetRsaPara(&rsaPara, e, sizeof(e), 2048); // 2048 is rsa key bits
    ASSERT_EQ(CRYPT_EAL_PkeySetPara(rsaPkey, &rsaPara), CRYPT_SUCCESS);
    ASSERT_EQ(CRYPT_EAL_PkeyGen(rsaPkey), CRYPT_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_PUBKEY, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_SET_PUBKEY, rsaPkey, 0), HITLS_X509_SUCCESS);

    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_PRIVKEY, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_SET_PRIVKEY, rsaPkey, 0), HITLS_X509_SUCCESS);

    int32_t mdId = CRYPT_MD_SHA256;
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_MD_ID, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_SET_SIGN_MD_ID, &mdId, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_MD_ID, &mdId, 0), HITLS_X509_SUCCESS);

    CRYPT_RSA_PssPara para = {20, CRYPT_MD_SHA256, CRYPT_MD_SHA256};
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_RSA_PSS_PARAM, NULL, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(NULL, HITLS_X509_SET_SIGN_RSA_PSS_PARAM, &para, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_RSA_PSS_PARAM, &para, 0), HITLS_X509_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_RSA_PSS_PARAM, &para, sizeof(CRYPT_RSA_PssPara)),
        HITLS_X509_SUCCESS);

    eccPkey = CRYPT_EAL_PkeyNewCtx(CRYPT_PKEY_ECDSA);
    ASSERT_NE(eccPkey, NULL);
    ASSERT_EQ(CRYPT_EAL_PkeySetParaById(eccPkey, CRYPT_ECC_NISTP256), CRYPT_SUCCESS);
    ASSERT_EQ(CRYPT_EAL_PkeyGen(eccPkey), CRYPT_SUCCESS);
    ASSERT_NE(HITLS_X509_CsrCtrl(csr, HITLS_X509_SET_SIGN_RSA_PSS_PARAM, &para, sizeof(CRYPT_RSA_PssPara)),
        HITLS_X509_SUCCESS);
exit:
    HITLS_X509_CsrFree(csr);
    CRYPT_EAL_PkeyFreeCtx(rsaPkey);
    CRYPT_EAL_PkeyFreeCtx(eccPkey);
    CRYPT_EAL_RandDeinit();
}
/* END_CASE */

/**
 * 1. csr ctrl interface test
*/
/* BEGIN_CASE */
void SDV_X509_CSR_CTRL_FUNC_TC001(char *csrPath)
{
    TestMemInit();
    TestRandInit();
    BSL_Buffer encodeRaw = { NULL, 0};
    HITLS_X509_Csr *csr = NULL;
    uint8_t *csrEncode = NULL;
    uint32_t csrEncodeLen = 0;
    CRYPT_EAL_PkeyCtx *pkey = NULL;
    uint8_t e[] = {1, 0, 1};
    HITLS_X509_Csr *newCsr = NULL;

    ASSERT_EQ(BSL_SAL_ReadFile(csrPath, &encodeRaw.data, &encodeRaw.dataLen), HITLS_X509_SUCCESS);
    ASSERT_NE(encodeRaw.data, NULL);
    ASSERT_EQ(HITLS_X509_CsrParseBuff(BSL_FORMAT_ASN1, &encodeRaw, &csr), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_ENCODE, &csrEncode, 0), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_GET_ENCODELEN, &csrEncodeLen, sizeof(csrEncodeLen)),
        HITLS_X509_SUCCESS);
    ASSERT_EQ(csrEncodeLen, encodeRaw.dataLen);
    ASSERT_EQ(memcmp(encodeRaw.data, csrEncode, encodeRaw.dataLen), 0);

    int32_t ref = 0;
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_REF_UP, &ref, sizeof(ref)), HITLS_X509_SUCCESS);
    ASSERT_EQ(ref, 2);
    HITLS_X509_CsrFree(csr);

    newCsr = HITLS_X509_CsrNew();
    ASSERT_NE(newCsr, NULL);
    pkey = CRYPT_EAL_PkeyNewCtx(CRYPT_PKEY_RSA);
    ASSERT_NE(pkey, NULL);
    CRYPT_EAL_PkeyPara para = {0};
    SetRsaPara(&para, e, sizeof(e), 2048); // 2048 is rsa key bits
    ASSERT_EQ(CRYPT_EAL_PkeySetPara(pkey, &para), CRYPT_SUCCESS);
    ASSERT_EQ(CRYPT_EAL_PkeyGen(pkey), CRYPT_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(newCsr, HITLS_X509_SET_PUBKEY, pkey, 0), HITLS_X509_SUCCESS);
    ASSERT_EQ(HITLS_X509_CsrCtrl(newCsr, HITLS_X509_SET_PRIVKEY, pkey, 0), HITLS_X509_SUCCESS);

    ASSERT_EQ(HITLS_X509_CsrCtrl(newCsr, HITLS_X509_GET_ENCODELEN, &csrEncodeLen, sizeof(csrEncodeLen)),
        HITLS_X509_SUCCESS);

exit:
    BSL_SAL_FREE(encodeRaw.data);
    HITLS_X509_CsrFree(csr);
    HITLS_X509_CsrFree(newCsr);
    CRYPT_EAL_PkeyFreeCtx(pkey);
    CRYPT_EAL_RandDeinit();
}
/* END_CASE */

/* BEGIN_CASE */
void SDV_X509_CSR_AttrCtrl_API_TC001(void)
{
    TestMemInit();
    HITLS_X509_Attr attr = {0};
    HITLS_X509_Attr getAttr = {0};
    HITLS_X509_Ext ext = {0};
    HITLS_X509_ExtKeyUsage ku = {0, HITLS_X509_EXT_KU_NON_REPUDIATION};
    int32_t cmd = HITLS_X509_ATTR_SET_REQUESTED_EXTENSIONS;
    BslList *attrList = NULL;

    HITLS_X509_Csr *csr = HITLS_X509_CsrNew();
    ASSERT_NE(csr, NULL);
    ASSERT_EQ(HITLS_X509_ExtCtrl(&ext, HITLS_X509_EXT_SET_KUSAGE, &ku, sizeof(HITLS_X509_ExtKeyUsage)), 0);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_CSR_GET_ATTRIBUTES, &attrList, sizeof(BslList *)), 0);

    // invalid param
    ASSERT_EQ(HITLS_X509_AttrCtrl(NULL, cmd, &attr, sizeof(HITLS_X509_Attr)), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, -1, &attr, sizeof(HITLS_X509_Attr)), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, cmd, NULL, sizeof(HITLS_X509_Attr)), HITLS_X509_ERR_INVALID_PARAM);
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, cmd, &attr, 0), HITLS_X509_ERR_INVALID_PARAM);

    // attr.cid is unknown
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, cmd, &attr, sizeof(HITLS_X509_Attr)), CRYPT_ERR_ALGID);

    // attr.value is null
    attr.cid = BSL_CID_REQ_EXTENSION;
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, cmd, &attr, sizeof(HITLS_X509_Attr)), HITLS_X509_ERR_INVALID_PARAM);

    attr.value = &ext;

    // encode ext failed
    ext.list->count = 2;
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, cmd, &attr, sizeof(HITLS_X509_Attr)), BSL_INVALID_ARG);
    ext.list->count = 1;

    // success
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, cmd, &attr, sizeof(HITLS_X509_Attr)), 0);

    // repeat
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, cmd, &attr, sizeof(HITLS_X509_Attr)), HITLS_X509_ERR_SET_ATTR_REPEAT);

    // get attr
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, HITLS_X509_ATTR_GET_REQUESTED_EXTENSIONS,
        &getAttr, sizeof(HITLS_X509_Attr)), HITLS_X509_SUCCESS);
    ASSERT_NE(getAttr.value, NULL);
    ASSERT_EQ(getAttr.cid, BSL_CID_REQ_EXTENSION);
    HITLS_X509_Ext *getExt = getAttr.value;
    ASSERT_EQ(getExt->keyUsage, HITLS_X509_EXT_KU_NON_REPUDIATION);

    // not found
    HITLS_X509_ExtFree(getExt);
    getExt = NULL;
    getAttr.value = NULL;
    BSL_LIST_DeleteAll(attrList, (BSL_LIST_PFUNC_FREE)HITLS_X509_AttrEntryFree);
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, HITLS_X509_ATTR_GET_REQUESTED_EXTENSIONS,
        &getAttr, sizeof(HITLS_X509_Attr)), HITLS_X509_ERR_ATTR_NOT_FOUND);

exit:
    HITLS_X509_CsrFree(csr);
    BSL_LIST_FREE(ext.list, (BSL_LIST_PFUNC_FREE)HITLS_X509_ExtEntryFree);
    HITLS_X509_ExtFree(getExt);
}
/* END_CASE */

/* BEGIN_CASE */
void SDV_X509_CSR_EncodeAttrList_FUNC_TC001(int critical1, int maxPath, int critical2, int keyUsage, Hex *expect)
{
    TestMemInit();

    HITLS_X509_Ext ext = {0};
    BslList *attrList = NULL;
    HITLS_X509_ExtBCons bCons = {critical1, false, maxPath};
    HITLS_X509_ExtKeyUsage ku = {critical2, keyUsage};
    BSL_ASN1_Buffer encode = {0};

    HITLS_X509_Csr *csr = HITLS_X509_CsrNew();
    ASSERT_NE(csr, NULL);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_CSR_GET_ATTRIBUTES, &attrList, sizeof(BslList *)), 0);
    ASSERT_NE(attrList, NULL);

    // Generate ext
    ASSERT_EQ(HITLS_X509_ExtCtrl(&ext, HITLS_X509_EXT_SET_KUSAGE, &ku, sizeof(HITLS_X509_ExtKeyUsage)), 0);
    ASSERT_EQ(HITLS_X509_ExtCtrl(&ext, HITLS_X509_EXT_SET_BCONS, &bCons, sizeof(HITLS_X509_ExtBCons)), 0);

    // Set ext into attr
    HITLS_X509_Attr attr = {BSL_CID_REQ_EXTENSION, &ext};
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, HITLS_X509_ATTR_SET_REQUESTED_EXTENSIONS, &attr, sizeof(HITLS_X509_Attr)),
              0);

    // Test: Encode and check
    ASSERT_EQ(HITLS_X509_EncodeAttrList(1, attrList, &encode), 0);
    ASSERT_COMPARE("Encode attrs", expect->x, expect->len, encode.buff, encode.len);

exit:
    HITLS_X509_CsrFree(csr);
    BSL_SAL_Free(encode.buff);
    BSL_LIST_FREE(ext.list, (BSL_LIST_PFUNC_FREE)HITLS_X509_ExtEntryFree);
}
/* END_CASE */

/* BEGIN_CASE */
void SDV_X509_CSR_EncodeAttrList_FUNC_TC002(void)
{
    TestMemInit();

    HITLS_X509_Ext ext = {0};
    BslList *attrList = NULL;
    HITLS_X509_ExtKeyUsage ku = {0, HITLS_X509_EXT_KU_NON_REPUDIATION};
    BSL_ASN1_Buffer encode = {0};

    HITLS_X509_Csr *csr = HITLS_X509_CsrNew();
    ASSERT_NE(csr, NULL);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_CSR_GET_ATTRIBUTES, &attrList, sizeof(BslList *)), 0);
    ASSERT_NE(attrList, NULL);
    ASSERT_EQ(HITLS_X509_ExtCtrl(&ext, HITLS_X509_EXT_SET_KUSAGE, &ku, sizeof(HITLS_X509_ExtKeyUsage)), 0);

    // Test 1: no attr
    ASSERT_EQ(HITLS_X509_EncodeAttrList(1, attrList, &encode), 0);
    ASSERT_EQ(encode.buff, NULL);
    ASSERT_EQ(encode.len, 0);

    // Test 2: encode attr entry failed
    attrList->count = 1;
    ASSERT_EQ(HITLS_X509_EncodeAttrList(1, attrList, &encode), BSL_INVALID_ARG);

    // Set ext into attr
    HITLS_X509_Attr attr = {BSL_CID_REQ_EXTENSION, &ext};
    ASSERT_EQ(HITLS_X509_AttrCtrl(attrList, HITLS_X509_ATTR_SET_REQUESTED_EXTENSIONS, &attr, sizeof(HITLS_X509_Attr)),
              0);

    // Test 3: encode list item failed
    ASSERT_EQ(HITLS_X509_EncodeAttrList(1, attrList, &encode), BSL_INVALID_ARG);

exit:
    HITLS_X509_CsrFree(csr);
    BSL_LIST_FREE(ext.list, (BSL_LIST_PFUNC_FREE)HITLS_X509_ExtEntryFree);
}
/* END_CASE */

/* BEGIN_CASE */
void SDV_X509_CSR_ParseAttrList_FUNC_TC001(Hex *encode, int ret)
{
    TestMemInit();

    BSL_ASN1_Buffer attrs = {0, encode->len, encode->x};
    BslList *attrList = NULL;

    HITLS_X509_Csr *csr = HITLS_X509_CsrNew();
    csr->flag = 0x01; // HITLS_X509_CSR_PARSE_FLAG
    ASSERT_NE(csr, NULL);
    ASSERT_EQ(HITLS_X509_CsrCtrl(csr, HITLS_X509_CSR_GET_ATTRIBUTES, &attrList, sizeof(BslList *)), 0);
    ASSERT_NE(attrList, NULL);

    attrs.tag = BSL_ASN1_TAG_CONSTRUCTED | BSL_ASN1_TAG_SEQUENCE;
    ASSERT_EQ(HITLS_X509_ParseAttrList(&attrs, attrList), ret);

exit:
    HITLS_X509_CsrFree(csr);
}
/* END_CASE */

static void SetX509Dn(HITLS_X509_DN *dnName, int dnType, char *dnNameStr)
{
    dnName->cid = (BslCid)dnType;
    dnName->data = (uint8_t *)dnNameStr;
    dnName->dataLen = strlen(dnNameStr);
}

static int32_t SetNewCsrInfo(HITLS_X509_Csr *new, CRYPT_EAL_PkeyCtx *key, int mdId, int dnType1,
    char *dnName1, int dnType2, char *dnName2, int dnType3, char *dnName3)
{
    int32_t ret = 1;
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_SET_PUBKEY, key, sizeof(CRYPT_EAL_PkeyCtx *)), 0);
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_SET_PRIVKEY, key, sizeof(void *)), 0);
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_SET_SIGN_MD_ID, &mdId, sizeof(int32_t)), 0);

    HITLS_X509_DN dnName[3] = {0};
    int dnTypes[3] = {dnType1, dnType2, dnType3};
    char *dnNameStr[3] = {dnName1, dnName2, dnName3};
    for (int i = 0; i < 3; i++) {
        SetX509Dn(&dnName[i], dnTypes[i], dnNameStr[i]);
        ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_ADD_SUBJECT_NAME, &dnName[i], 1), HITLS_X509_SUCCESS);
    }
    BslList *subjectName = 0;
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_GET_SUBJECT_DNNAME, &subjectName, sizeof(BslList *)),
        HITLS_X509_SUCCESS);
    ASSERT_EQ(BSL_LIST_COUNT(subjectName), 6);
    
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_ADD_SUBJECT_NAME, dnName, 3), HITLS_X509_SUCCESS);
    ASSERT_EQ(BSL_LIST_COUNT(subjectName), 10);

    ret = 0;
exit:
    return ret;
}

/* BEGIN_CASE */
void SDV_X509_CSR_AddSubjectName_FUNC_TC001(int keyFormat, int keyType, char *privPath,
    int mdId, int dnType1, char *dnName1, int dnType2, char *dnName2, int dnType3, char *dnName3, Hex *expectedReqInfo)
{
    TestMemInit();
    TestRandInit();
    HITLS_X509_Csr *new = NULL;
    CRYPT_EAL_PkeyCtx *privKey = NULL;
    BSL_Buffer encode = {NULL, 0};

    ASSERT_EQ(CRYPT_EAL_DecodeFileKey(keyFormat, keyType, privPath, NULL, 0, &privKey), HITLS_X509_SUCCESS);
    new = HITLS_X509_CsrNew();
    ASSERT_NE(new, NULL);

    ASSERT_EQ(SetNewCsrInfo(new, privKey, mdId, dnType1, dnName1, dnType2, dnName2, dnType3, dnName3), 0);
    ASSERT_EQ(HITLS_X509_CsrGenBuff(new, BSL_FORMAT_PEM, &encode), HITLS_X509_SUCCESS);
    ASSERT_EQ(new->reqInfo.reqInfoRawDataLen, expectedReqInfo->len);
    ASSERT_EQ(memcmp(new->reqInfo.reqInfoRawData, expectedReqInfo->x, expectedReqInfo->len), 0);

    // error length
    HITLS_X509_DN dnNameErr[1] = {{BSL_CID_COUNTRYNAME, (uint8_t *)"CNNN", strlen("CNNN")}};
    ASSERT_EQ(HITLS_X509_CsrCtrl(new, HITLS_X509_ADD_SUBJECT_NAME, dnNameErr, 1),
        HITLS_X509_ERR_SET_DNNAME_INVALID_LEN);
exit:
    HITLS_X509_CsrFree(new);
    BSL_SAL_FREE(encode.data);
    CRYPT_EAL_PkeyFreeCtx(privKey);
}
/* END_CASE */

/* BEGIN_CASE */
void SDV_X509_CSR_PARSE_FUNC_TC004(int format, char *path, int expectedRet)
{
    TestMemInit();
    HITLS_X509_Csr *csr = NULL;
    ASSERT_EQ(HITLS_X509_CsrParseFile(format, path, &csr), expectedRet);

exit:
    HITLS_X509_CsrFree(csr);
}
/* END_CASE */
