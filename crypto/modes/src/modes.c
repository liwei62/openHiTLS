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

#include "hitls_build.h"
#ifdef HITLS_CRYPTO_MODES

#include "securec.h"
#include "bsl_sal.h"
#include "bsl_err_internal.h"
#include "crypt_errno.h"
#include "modes_local.h"
#include "crypt_aes.h"
#include "crypt_sm4.h"
#include "crypt_chacha20.h"
#include "crypt_modes_gcm.h"
#include "crypt_modes_cbc.h"

#ifdef HITLS_CRYPTO_AES
static const EAL_SymMethod AES128_METHOD = {
    (SetEncryptKey)CRYPT_AES_SetEncryptKey128,
    (SetDecryptKey)CRYPT_AES_SetDecryptKey128,
    (EncryptBlock)CRYPT_AES_Encrypt,
    (DecryptBlock)CRYPT_AES_Decrypt,
    (DeInitBlockCtx)CRYPT_AES_Clean,
    NULL,
    16,
    sizeof(CRYPT_AES_Key),
    CRYPT_SYM_AES128
};

static const EAL_SymMethod AES192_METHOD = {
    (SetEncryptKey)CRYPT_AES_SetEncryptKey192,
    (SetDecryptKey)CRYPT_AES_SetDecryptKey192,
    (EncryptBlock)CRYPT_AES_Encrypt,
    (DecryptBlock)CRYPT_AES_Decrypt,
    (DeInitBlockCtx)CRYPT_AES_Clean,
    NULL,
    16,
    sizeof(CRYPT_AES_Key),
    CRYPT_SYM_AES192
};

static const EAL_SymMethod AES256_METHOD = {
    (SetEncryptKey)CRYPT_AES_SetEncryptKey256,
    (SetDecryptKey)CRYPT_AES_SetDecryptKey256,
    (EncryptBlock)CRYPT_AES_Encrypt,
    (DecryptBlock)CRYPT_AES_Decrypt,
    (DeInitBlockCtx)CRYPT_AES_Clean,
    NULL,
    16,
    sizeof(CRYPT_AES_Key),
    CRYPT_SYM_AES256
};
#endif

#ifdef HITLS_CRYPTO_CHACHA20
static const EAL_SymMethod CHACHA20_METHOD = {
    (SetEncryptKey)CRYPT_CHACHA20_SetKey,
    (SetDecryptKey)CRYPT_CHACHA20_SetKey,
    (EncryptBlock)CRYPT_CHACHA20_Update,
    (DecryptBlock)CRYPT_CHACHA20_Update,
    (DeInitBlockCtx)CRYPT_CHACHA20_Clean,
    (CipherCtrl)CRYPT_CHACHA20_Ctrl,
    1,
    sizeof(CRYPT_CHACHA20_Ctx),
    CRYPT_SYM_CHACHA20
};
#endif

#ifdef HITLS_CRYPTO_SM4
static const EAL_SymMethod SM4_METHOD = {
    (SetEncryptKey)CRYPT_SM4_SetKey,
    (SetDecryptKey)CRYPT_SM4_SetKey,
    (EncryptBlock)CRYPT_SM4_Encrypt,
    (DecryptBlock)CRYPT_SM4_Decrypt,
    (DeInitBlockCtx)CRYPT_SM4_Clean,
    NULL,
    16,
    sizeof(CRYPT_SM4_Ctx),
    CRYPT_SYM_SM4
};
#endif

const EAL_SymMethod *MODES_GetSymMethod(int32_t algId)
{
    switch (algId) {
        case CRYPT_CIPHER_AES128_CBC:
        case CRYPT_CIPHER_AES128_ECB:
        case CRYPT_CIPHER_AES128_CTR:
        case CRYPT_CIPHER_AES128_CCM:
        case CRYPT_CIPHER_AES128_GCM:
        case CRYPT_CIPHER_AES128_CFB:
        case CRYPT_CIPHER_AES128_OFB:
            return &AES128_METHOD;
        case CRYPT_CIPHER_AES192_CBC:
        case CRYPT_CIPHER_AES192_ECB:
        case CRYPT_CIPHER_AES192_CTR:
        case CRYPT_CIPHER_AES192_CCM:
        case CRYPT_CIPHER_AES192_GCM:
        case CRYPT_CIPHER_AES192_CFB:
        case CRYPT_CIPHER_AES192_OFB:
            return &AES192_METHOD;
        case CRYPT_CIPHER_AES256_CBC:
        case CRYPT_CIPHER_AES256_ECB:
        case CRYPT_CIPHER_AES256_CTR:
        case CRYPT_CIPHER_AES256_CCM:
        case CRYPT_CIPHER_AES256_GCM:
        case CRYPT_CIPHER_AES256_CFB:
        case CRYPT_CIPHER_AES256_OFB:
            return &AES256_METHOD;
        case CRYPT_CIPHER_SM4_XTS:
        case CRYPT_CIPHER_SM4_CBC:
        case CRYPT_CIPHER_SM4_ECB:
        case CRYPT_CIPHER_SM4_CTR:
        case CRYPT_CIPHER_SM4_GCM:
        case CRYPT_CIPHER_SM4_CFB:
        case CRYPT_CIPHER_SM4_OFB:
            return &SM4_METHOD;
        case CRYPT_CIPHER_CHACHA20_POLY1305:
            return &CHACHA20_METHOD;
        default:
            return NULL;
    }
}

int32_t MODE_NewCtxInternal(MODES_CipherCtx *ctx, const EAL_SymMethod *method)
{
    ctx->commonCtx.ciphCtx = BSL_SAL_Calloc(1, method->ctxSize);
    if (ctx->commonCtx.ciphCtx  == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_MEM_ALLOC_FAIL);
        return CRYPT_MEM_ALLOC_FAIL;
    }
    // block mode blockSize equal symm blockSize
    ctx->commonCtx.blockSize = method->blockSize;
    ctx->commonCtx.ciphMeth = method;
    ctx->commonCtx.offset = 0;

    return CRYPT_SUCCESS;
}

MODES_CipherCtx *MODES_CipherNewCtx(int32_t algId)
{
    const EAL_SymMethod *method = MODES_GetSymMethod(algId);
    if (method == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_INVALID_ARG);
        return NULL;
    }
    
    MODES_CipherCtx *ctx = BSL_SAL_Calloc(1, sizeof(MODES_CipherCtx));
    if (ctx == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_MEM_ALLOC_FAIL);
        return ctx;
    }
    ctx->algId = algId;
    int32_t ret = MODE_NewCtxInternal(ctx, method);
    if (ret != CRYPT_SUCCESS) {
        BSL_SAL_Free(ctx);
        return NULL;
    }
    return ctx;
}

int32_t MODES_CipherInitCtx(MODES_CipherCtx *ctx, void *setSymKey, void *keyCtx, const uint8_t *key, uint32_t keyLen,
    const uint8_t *iv, uint32_t ivLen, bool enc)
{
    int32_t ret;
    MODES_CipherCtx *modeCtx = ctx;
    if (iv == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }
    
    if (ivLen != modeCtx->commonCtx.blockSize) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODES_IVLEN_ERROR);
        return CRYPT_MODES_IVLEN_ERROR;
    }

    ret = ((SetKey)setSymKey)(keyCtx, key, keyLen);
    if (ret != CRYPT_SUCCESS) {
        BSL_ERR_PUSH_ERROR(ret);
        return ret;
    }
    if (ivLen > 0) {
        (void)memcpy_s(modeCtx->commonCtx.iv, MODES_MAX_IV_LENGTH, iv, ivLen);
    }

    modeCtx->enc = enc;
    return ret;
}

int32_t MODE_CheckUpdateParam(uint8_t blockSize, uint32_t cacheLen, uint32_t inLen, uint32_t *outLen)
{
    if (inLen + cacheLen < inLen) { // Integer flipping
        BSL_ERR_PUSH_ERROR(CRYPT_EAL_BUFF_LEN_NOT_ENOUGH);
        return CRYPT_EAL_BUFF_LEN_NOT_ENOUGH;
    }
    if ((*outLen) < ((inLen + cacheLen) / blockSize * blockSize)) {
        BSL_ERR_PUSH_ERROR(CRYPT_EAL_BUFF_LEN_NOT_ENOUGH);
        return CRYPT_EAL_BUFF_LEN_NOT_ENOUGH;
    }
    return CRYPT_SUCCESS;
}

static bool IfXts(CRYPT_CIPHER_AlgId id)
{
    CRYPT_CIPHER_AlgId XTS_list[] = {
        CRYPT_CIPHER_SM4_XTS,
    };
    for (uint32_t i = 0; i < sizeof(XTS_list) / sizeof(XTS_list[0]); i++) {
        if (id == XTS_list[i]) {
            return true;
        }
    }
    return false;
}

int32_t UnPaddingISO7816(const uint8_t *pad, uint32_t padLen, uint32_t *finLen)
{
    uint32_t len;
    const uint8_t *p = pad;
    len = padLen - 1;
    while (*(p + len) == 0 && len > 0) {
        len--;
    }
    len = (*(p + len) == 0x80) ? len : padLen;

    if (len == padLen) {
        BSL_ERR_PUSH_ERROR(CRYPT_EAL_CIPHER_DATA_ERROR);
        return CRYPT_EAL_CIPHER_DATA_ERROR;
    }
    (*finLen) = len;
    return CRYPT_SUCCESS;
}

int32_t UnPaddingX923(const uint8_t *pad, uint32_t padLen, uint32_t *finLen)
{
    uint32_t len, pos, i;
    uint32_t check = 0;
    len = pad[padLen - 1];

    check |= (uint32_t)(len > padLen);

    pos = padLen - len;
    for (i = 0; i < padLen - 1; i++) {
        check |= (pad[i] * (uint32_t)(i >= pos));
    }

    if (check != 0) {
        BSL_ERR_PUSH_ERROR(CRYPT_EAL_CIPHER_DATA_ERROR);
        return CRYPT_EAL_CIPHER_DATA_ERROR;
    }

    (*finLen) = padLen - len;
    return CRYPT_SUCCESS;
}

int32_t UnPaddingPkcs(const uint8_t *pad, uint32_t padLen, uint32_t *finLen)
{
    uint32_t len, pos, i;
    uint32_t check = 0;

    len = pad[padLen - 1];
    check |= (uint32_t)((len == 0) || (len > padLen));

    pos = padLen - len;
    for (i = 0; i < padLen; i++) {
        check |= ((pad[i] ^ len) * (uint32_t)(i >= pos));
    }

    if (check != 0) {
        BSL_ERR_PUSH_ERROR(CRYPT_EAL_CIPHER_DATA_ERROR);
        return CRYPT_EAL_CIPHER_DATA_ERROR;
    }

    (*finLen) = padLen - len;
    return CRYPT_SUCCESS;
}

int32_t Mode_BlockUnPadding(int32_t padding, const uint8_t *pad, uint32_t padLen, uint32_t *dataLen)
{
    int32_t ret = 0;
    uint32_t len = *dataLen;
    switch (padding) {
        case CRYPT_PADDING_ISO7816:
            ret = UnPaddingISO7816(pad, padLen, &len);
            break;
        case CRYPT_PADDING_X923:
            ret = UnPaddingX923(pad, padLen, &len);
            break;
        case CRYPT_PADDING_PKCS5:
        case CRYPT_PADDING_PKCS7:
            ret = UnPaddingPkcs(pad, padLen, &len);
            break;
        default:
            break;
    }

    *dataLen = len;
    return ret;
}

int32_t MODES_BlockPadding(int32_t algId, int32_t padding, uint8_t blockSize, uint8_t *data, uint8_t *dataLen)
{
    uint8_t tempLen = *dataLen;
    uint8_t *pad = data + tempLen;
    uint8_t padLen = blockSize - tempLen;
    uint8_t i;
    *dataLen += padLen;
    switch (padding) {
        case CRYPT_PADDING_NONE:
            *dataLen = tempLen;
            if (tempLen % blockSize != 0) {
                return IfXts(algId) ? CRYPT_SUCCESS : CRYPT_MODE_ERR_INPUT_LEN;
            }
            break;
        case CRYPT_PADDING_ZEROS:
            for (i = 0; i < padLen; i++) {
                pad[i] = 0x00L;
            }
            break;
        case CRYPT_PADDING_ISO7816:
            pad[0] = 0x80;
            for (i = 1; i < padLen; i++) {
                pad[i] = 0x00L;
            }
            break;
        case CRYPT_PADDING_X923:
            for (i = 0; i < padLen - 1; i++) {
                pad[i] = 0x00L;
            }
            pad[padLen - 1] = padLen;
            break;
        case CRYPT_PADDING_PKCS5:
        case CRYPT_PADDING_PKCS7:
            for (i = 0; i < padLen; i++) {
                pad[i] = padLen;
            }
            break;
        default:
            *dataLen = tempLen;
            return CRYPT_INVALID_ARG;
    }
    return CRYPT_SUCCESS;
}

int32_t MODES_CipherUpdateCache(MODES_CipherCtx *ctx,  void *blockUpdate, const uint8_t **in, uint32_t *inLen,
    uint8_t **out, uint32_t *outLen)
{
    int32_t ret;
    uint8_t blockSize = ctx->commonCtx.blockSize;
    // Process the cache. If there is cached data, the cache data is padded into a block first.
    if (ctx->dataLen > 0) {
        uint8_t padding = blockSize - ctx->dataLen;
        padding = (*inLen) > (padding) ? padding : (uint8_t)(*inLen);
        if (padding != 0) {
            (void)memcpy_s(ctx->data + ctx->dataLen, (EAL_MAX_BLOCK_LENGTH - ctx->dataLen), (*in), padding);
            (*inLen) -= padding;
            (*in) += padding;
            ctx->dataLen += padding;
        }
    }
    // No block is formed, return.
    if (ctx->dataLen != blockSize) {
        return CRYPT_SUCCESS;
    }

    // If the block is padded, perform operations on this block first.
    if (ctx->enc) {
        ret = ((EncryptBlock)blockUpdate)(&ctx->commonCtx, ctx->data, *out, blockSize);
    } else {
        // If it's decryption and the cached data + input data is equal to blockSize,
        // may be it's the last piece of data with padding, the data is cached in the ctx and left for final processing.
        if ((*inLen) == 0 && (ctx->pad != CRYPT_PADDING_NONE)) {
            (*outLen) = 0;
            return CRYPT_SUCCESS;
        }
        ret = ((DecryptBlock)blockUpdate)(&ctx->commonCtx, ctx->data, *out, blockSize);
    }

    if (ret != CRYPT_SUCCESS) {
        BSL_ERR_PUSH_ERROR(ret);
        return ret;
    }
    ctx->dataLen = 0;
    (*outLen) = blockSize;
    (*out) += blockSize;

    return CRYPT_SUCCESS;
}

int32_t MODES_CipherFinal(MODES_CipherCtx *modeCtx, void *blockUpdate, uint8_t *out, uint32_t *outLen)
{
    int32_t ret;
    if (out == NULL || outLen == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }
    if (modeCtx->pad != CRYPT_PADDING_NONE && (*outLen) < modeCtx->commonCtx.blockSize) {
        BSL_ERR_PUSH_ERROR(CRYPT_EAL_BUFF_LEN_NOT_ENOUGH);
        return CRYPT_EAL_BUFF_LEN_NOT_ENOUGH;
    }

    if (modeCtx->enc) {
        ret = MODES_BlockPadding(modeCtx->algId, modeCtx->pad,
            modeCtx->commonCtx.blockSize, modeCtx->data, &modeCtx->dataLen);
        if (ret != CRYPT_SUCCESS) {
            BSL_ERR_PUSH_ERROR(ret);
            return ret;
        }
        if (modeCtx->dataLen == 0) {
            *outLen = modeCtx->dataLen;
            return CRYPT_SUCCESS;
        }
        ret = ((EncryptBlock)blockUpdate)(&modeCtx->commonCtx, modeCtx->data, out, modeCtx->dataLen);
        if (ret != CRYPT_SUCCESS) {
            BSL_ERR_PUSH_ERROR(ret);
            return ret;
        }
        *outLen = modeCtx->dataLen;
    } else {
        if (modeCtx->dataLen == 0) {
            *outLen = modeCtx->dataLen;
            return CRYPT_SUCCESS;
        }
        
        ret = ((DecryptBlock)blockUpdate)(&modeCtx->commonCtx, modeCtx->data, out, modeCtx->dataLen);
        if (ret != CRYPT_SUCCESS) {
            BSL_ERR_PUSH_ERROR(ret);
            return ret;
        }
        ret = Mode_BlockUnPadding(modeCtx->pad, out, modeCtx->dataLen, outLen);
        if (ret != CRYPT_SUCCESS) {
            BSL_ERR_PUSH_ERROR(ret);
            return ret;
        }
    }
    return ret;
}

int32_t MODES_CipherUpdate(MODES_CipherCtx *modeCtx, void *blockUpdate, const uint8_t *in, uint32_t inLen,
    uint8_t *out, uint32_t *outLen)
{
    int32_t ret;
    uint32_t tmpLen = inLen;
    const uint8_t *tmpIn = in;
    uint8_t *tmpOut = (uint8_t *)out;
    
    ret = MODE_CheckUpdateParam(modeCtx->commonCtx.blockSize, modeCtx->dataLen, inLen, outLen);
    if (ret != CRYPT_SUCCESS) {
        BSL_ERR_PUSH_ERROR(ret);
        return ret;
    }
    *outLen = 0;
    ret = MODES_CipherUpdateCache(modeCtx, blockUpdate, &tmpIn, &tmpLen, &tmpOut, outLen);
    if (ret != CRYPT_SUCCESS) {
        BSL_ERR_PUSH_ERROR(ret);
        return ret;
    }
    if (tmpLen == 0) {
        return CRYPT_SUCCESS;
    }

    uint8_t left = tmpLen % modeCtx->commonCtx.blockSize;
    uint32_t len = tmpLen - left;
    if (len > 0) {
        if (modeCtx->enc) {
            ret = ((EncryptBlock)blockUpdate)(&modeCtx->commonCtx, tmpIn, tmpOut, len);
        } else {
            // If it's decryption and the cached data + input data is equal to blockSize,
            // may be it's the last piece of data with padding, the data is cached in the ctx and left for final processing.
            if ((modeCtx->pad != CRYPT_PADDING_NONE) && left == 0) {
                left = modeCtx->commonCtx.blockSize;
                len -= modeCtx->commonCtx.blockSize;
            }
            if (len > 0) {
                ret = ((DecryptBlock)blockUpdate)(&modeCtx->commonCtx, tmpIn, tmpOut, len);
            }
        }
        if (ret != CRYPT_SUCCESS) {
            BSL_ERR_PUSH_ERROR(ret);
            return ret;
        }
    }
    // Process the new cache.
    if (left > 0) {
        (void)memcpy_s(modeCtx->data, modeCtx->commonCtx.blockSize, tmpIn + len, left);
        modeCtx->dataLen = left;
    }

    // The encryption/decryption is successful. OutLen is updated.
    (*outLen) += len;
    return CRYPT_SUCCESS;
}

void MODES_Clean(MODES_CipherCommonCtx *ctx)
{
    if (ctx == NULL) {
        return;
    }
    BSL_SAL_CleanseData((void *)(ctx->buf), MODES_MAX_BUF_LENGTH);
    BSL_SAL_CleanseData((void *)(ctx->iv), MODES_MAX_IV_LENGTH);
    ctx->ciphMeth->cipherDeInitCtx(ctx->ciphCtx);
    ctx->offset = 0;
}

int32_t MODES_CipherDeInitCtx(MODES_CipherCtx *modeCtx)
{
    (void)memset_s(modeCtx->data, EAL_MAX_BLOCK_LENGTH, 0, EAL_MAX_BLOCK_LENGTH);
    modeCtx->dataLen = 0;
    modeCtx->pad = CRYPT_PADDING_NONE;
    MODES_Clean(&modeCtx->commonCtx);
    return CRYPT_SUCCESS;
}

void MODES_CipherFreeCtx(MODES_CipherCtx *modeCtx)
{
    MODES_CipherDeInitCtx(modeCtx);
    BSL_SAL_FREE(modeCtx->commonCtx.ciphCtx);
    BSL_SAL_Free(modeCtx);
}


int32_t MODES_SetIv(MODES_CipherCommonCtx *ctx, uint8_t *val, uint32_t len)
{
    if (val == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }

    if (len != ctx->blockSize) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODES_IVLEN_ERROR);
        return CRYPT_MODES_IVLEN_ERROR;
    }

    if (memcpy_s(ctx->iv, MODES_MAX_IV_LENGTH, (uint8_t*)val, len) != EOK) {
        BSL_ERR_PUSH_ERROR(CRYPT_SECUREC_FAIL);
        return CRYPT_SECUREC_FAIL;
    }
    ctx->offset = 0;    // If the IV value is changed, the original offset is useless.
    return CRYPT_SUCCESS;
}

int32_t MODES_GetIv(MODES_CipherCommonCtx *ctx, uint8_t *val, uint32_t len)
{
    if (val == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }

    uint32_t ivLen = ctx->blockSize;

    if (len != ivLen) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODE_ERR_INPUT_LEN);
        return CRYPT_MODE_ERR_INPUT_LEN;
    }

    if (memcpy_s(val, len, ctx->iv, ivLen) != EOK) {
        BSL_ERR_PUSH_ERROR(CRYPT_SECUREC_FAIL);
        return CRYPT_SECUREC_FAIL;
    }
    return CRYPT_SUCCESS;
}

int32_t MODES_CipherCtrl(MODES_CipherCtx *ctx, int32_t opt, void *val, uint32_t len)
{
    switch (opt) {
        case CRYPT_CTRL_REINIT_STATUS:
            return MODES_SetIv(&ctx->commonCtx, val, len);
        case CRYPT_CTRL_GET_IV:
            return MODES_GetIv(&ctx->commonCtx, (uint8_t *)val, len);
        default:
            BSL_ERR_PUSH_ERROR(CRYPT_MODES_CTRL_TYPE_ERROR);
            return CRYPT_MODES_CTRL_TYPE_ERROR;
    }
}

int32_t MODES_CipherStreamProcess(void *processFuncs, void *ctx, const uint8_t *in, uint32_t inLen,
    uint8_t *out, uint32_t *outLen)
{
    int32_t ret;
    if (inLen == 0) {
        *outLen = 0;
        return CRYPT_SUCCESS;
    }
    if (inLen > *outLen) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODE_BUFF_LEN_NOT_ENOUGH);
        return CRYPT_MODE_BUFF_LEN_NOT_ENOUGH;
    }
    
    ret = ((CipherStreamProcess)(processFuncs))(ctx, in, out, inLen);
    if (ret != CRYPT_SUCCESS) {
        BSL_ERR_PUSH_ERROR(ret);
        return ret;
    }
    *outLen = inLen;
    return CRYPT_SUCCESS;
}

// Note that CRYPT_PADDING_ZEROS cannot restore the plaintext length.
// If uses it, need to maintain the length themselves
int32_t MODES_SetPaddingCheck(int32_t pad)
{
    if (pad >= CRYPT_PADDING_MAX_COUNT || pad < CRYPT_PADDING_NONE) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODES_PADDING_NOT_SUPPORT);
        return CRYPT_MODES_PADDING_NOT_SUPPORT;
    }
    return CRYPT_SUCCESS;
}

#endif // HITLS_CRYPTO_MODES
