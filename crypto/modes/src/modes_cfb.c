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
#ifdef HITLS_CRYPTO_CFB

#include <stdbool.h>
#include "securec.h"
#include "bsl_err_internal.h"
#include "bsl_sal.h"
#include "crypt_modes_cfb.h"
#include "modes_local.h"
#include "crypt_errno.h"
#include "crypt_local_types.h"
#include "crypt_modes.h"

/* 8-bit | 64-bit | 128-bit CFB encryption. Here, len indicates the number of bytes to be processed. */
static int32_t MODES_CFB_BytesEncrypt(MODES_CipherCFBCtx *ctx, const uint8_t *in, uint8_t *out, uint32_t len)
{
    const uint8_t *input = in;
    uint8_t *output = out;
    uint8_t *tmp = ctx->modeCtx.buf;
    uint32_t blockSize = ctx->modeCtx.blockSize;
    uint32_t feedbackBytes = ctx->feedbackBits >> 3;
    uint32_t left = len;
    uint32_t i, k;

    // If the remaining encryption iv is not used up last time, use that part to perform XOR.
    while (left > 0 && ctx->modeCtx.offset > 0) {
        ctx->modeCtx.iv[ctx->modeCtx.offset] ^= *(input++);
        *(output++) = ctx->modeCtx.iv[ctx->modeCtx.offset];
        left--;
        ctx->modeCtx.offset = (ctx->modeCtx.offset + 1) % blockSize;
    }

    while (left > 0) {
        // Encrypt the IV.
        int32_t ret = ctx->modeCtx.ciphMeth->encryptBlock(ctx->modeCtx.ciphCtx, ctx->modeCtx.iv, tmp, blockSize);
        if (ret != CRYPT_SUCCESS) {
            BSL_ERR_PUSH_ERROR(ret);
            return ret;
        }

        i = 0;

        // The first (blockSize - feedbackBytes) bytes are filled with the least significant bytes of the previous IV.
        if (blockSize - feedbackBytes > 0) {
            (void)memmove_s(&ctx->modeCtx.iv[0], blockSize, &ctx->modeCtx.iv[feedbackBytes],
                blockSize - feedbackBytes);
            i = blockSize - feedbackBytes;
        }

        // The input data is XORed with the encrypted IV, and the current ciphertext is sent to the next IV.
        if (left >= feedbackBytes) {
            // Enter the last feedbackBytes in ciphertext.
            for (k = 0; i < blockSize; i++, k++) {
                output[k] = input[k] ^ tmp[k];
                ctx->modeCtx.iv[i] = output[k];
            }
            UPDATE_VALUES(left, input, output, feedbackBytes);
        } else {
            // Enter the last feedbackBytes in ciphertext.
            // The cache with insufficient feedbackBytes is used to encrypt the IV.
            for (k = 0; k < left; k++) {
                output[k] = input[k] ^ tmp[k];
                ctx->modeCtx.iv[i++] = output[k];
            }

            while (i < blockSize) {
                ctx->modeCtx.iv[i++] = tmp[k++];
            }
            ctx->modeCtx.offset = (uint8_t)(blockSize - feedbackBytes + left);
            left = 0;
        }
    }

    return CRYPT_SUCCESS;
}

/* 8-bit | 64-bit | 128-bit CFB decryption. Here, len indicates the number of bytes to be processed. */
static int32_t MODES_CFB_BytesDecrypt(MODES_CipherCFBCtx *ctx, const uint8_t *in, uint8_t *out, uint32_t len)
{
    const uint8_t *input = in;
    uint8_t *output = out;
    uint8_t *tmp = ctx->modeCtx.buf;
    uint32_t blockSize = ctx->modeCtx.blockSize;
    uint32_t feedbackBytes = ctx->feedbackBits >> 3;
    uint32_t left = len;
    uint32_t i, k;

    // If the remaining encryption iv is not used up last time, use that part to perform XOR.
    while (left > 0 && ctx->modeCtx.offset > 0) {
        uint8_t tmpInput = *input;      // To support the same address in and out
        *(output++) = ctx->modeCtx.iv[ctx->modeCtx.offset] ^ *(input++);
        ctx->modeCtx.iv[ctx->modeCtx.offset] = tmpInput;
        left--;
        ctx->modeCtx.offset = (ctx->modeCtx.offset + 1) % blockSize;
    }

    while (left > 0) {
        // Encrypt the IV.
        int32_t ret = ctx->modeCtx.ciphMeth->encryptBlock(ctx->modeCtx.ciphCtx, ctx->modeCtx.iv, tmp, blockSize);
        if (ret != CRYPT_SUCCESS) {
            BSL_ERR_PUSH_ERROR(ret);
            return ret;
        }

        i = 0;

        // The first (blockSize - feedbackBytes) bytes are filled with the least significant bytes of the previous IV.
        if (blockSize - feedbackBytes > 0) {
            (void)memmove_s(&ctx->modeCtx.iv[0], blockSize, &ctx->modeCtx.iv[feedbackBytes],
                blockSize - feedbackBytes);
            i = blockSize - feedbackBytes;
        }

        // The input data is XORed with the encrypted IV, and the current ciphertext is sent to the next IV.
        if (left >= feedbackBytes) {
            // Enter the last feedbackBytes in ciphertext.
            for (k = 0; i < blockSize; i++, k++) {
                ctx->modeCtx.iv[i] = input[k];
                output[k] = input[k] ^ tmp[k];
            }
            UPDATE_VALUES(left, input, output, feedbackBytes);
        } else {
            // Enter the last feedbackBytes in ciphertext.
            // The cache with insufficient feedbackBytes is used to encrypt the IV.
            for (k = 0; k < left; k++) {
                ctx->modeCtx.iv[i++] = input[k];
                output[k] = input[k] ^ tmp[k];
            }

            while (i < blockSize) {
                ctx->modeCtx.iv[i++] = tmp[k++];
            }
            ctx->modeCtx.offset = (uint8_t)(blockSize - feedbackBytes + left);
            left = 0;
        }
    }

    return CRYPT_SUCCESS;
}

static int32_t Cfb1Crypt(MODES_CipherCFBCtx *ctx, const uint8_t *in, uint8_t *out, bool enc)
{
    int32_t ret;
    uint8_t *tmp = ctx->modeCtx.buf;
    uint32_t blockSize = ctx->modeCtx.blockSize;
    uint32_t i;

    // Encrypt the IV.
    ret = ctx->modeCtx.ciphMeth->encryptBlock(ctx->modeCtx.ciphCtx, ctx->modeCtx.iv, tmp, blockSize);
    if (ret != CRYPT_SUCCESS) {
        BSL_ERR_PUSH_ERROR(ret);
        return ret;
    }

    for (i = 0; i < blockSize - 1; i++) {
        // All bytes are shifted left by one bit,
        // and the least significant bits are obtained by shifting right by 7 bits from the next byte.
        ctx->modeCtx.iv[i] = (ctx->modeCtx.iv[i] << 1) | (ctx->modeCtx.iv[i + 1] >> 7);
    }

    if (enc) {
        *out = tmp[0] ^ *in;
        // The last byte is shifted to the left by one bit and then filled in the ciphertext.
        // Shifted to the right by 7 bits to obtain the first bit of the byte.
        ctx->modeCtx.iv[i] = (ctx->modeCtx.iv[i] << 1) | (*out >> 7);
    } else {
        // The last byte is shifted to the left by one bit and then filled in the ciphertext.
        // Shifted to the right by 7 bits to obtain the first bit of the byte.
        ctx->modeCtx.iv[i] = (ctx->modeCtx.iv[i] << 1) | (*in >> 7);
        *out = tmp[0] ^ *in;
    }

    return CRYPT_SUCCESS;
}

/* 1-bit CFB. Here, len indicates the number of bits to be processed. */
int32_t MODES_CFB_BitCrypt(MODES_CipherCFBCtx *ctx, const uint8_t *in, uint8_t *out, uint32_t len, bool enc)
{
    int32_t ret;
    uint8_t tmp[2];
    uint32_t pos;
    for (uint32_t i = 0; i < len; i++) {
        // 7 - i % 8 is used to obtain the number of bits in the byte stream (high bit -> low bit).
        pos = 7 - i % 8;
        // Obtain the bits to be encrypted. 0x80 indicates a byte whose most significant bit is 1.
        tmp[0] = ((in[i / 8] & (1 << pos)) > 0) ? 0x80 : 0;
        ret = Cfb1Crypt(ctx, &tmp[0], &tmp[1], enc);
        if (ret != CRYPT_SUCCESS) {
            return ret;
        }
        // Divide by 8 to obtain the current byte position. Assign the out encryption bit to 0.
        out[i / 8] = out[i / 8] & ~(1u << pos);
        // Divide by 8 to obtain the current byte position. tmpOut[0] >> 7 to obtain the most significant bit.
        out[i / 8] |= (tmp[1] >> 7) << pos; // Assign the out encryption bit to the encrypted/decrypted value.
    }
    (void)memset_s(tmp, sizeof(tmp), 0, sizeof(tmp));
    return CRYPT_SUCCESS;
}

static int32_t CFB_Crypt(MODES_CipherCFBCtx *ctx, const uint8_t *in, uint8_t *out, uint32_t len, bool enc)
{
    if (ctx == NULL || in == NULL || out == NULL || len == 0) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }

    switch (ctx->feedbackBits) {
        case 1:
            return MODES_CFB_BitCrypt(ctx, in, out, len * 8, enc);
        case 8:
        case 64:
        case 128:
            return enc ? MODES_CFB_BytesEncrypt(ctx, in, out, len) : MODES_CFB_BytesDecrypt(ctx, in, out, len);

        default:
            BSL_ERR_PUSH_ERROR(CRYPT_MODES_ERR_FEEDBACKSIZE);
            return CRYPT_MODES_ERR_FEEDBACKSIZE;
    }
}

int32_t MODES_CFB_Encrypt(MODES_CipherCFBCtx *ctx, const uint8_t *in, uint8_t *out, uint32_t len)
{
    return CFB_Crypt(ctx, in, out, len, true);
}

int32_t MODES_CFB_Decrypt(MODES_CipherCFBCtx *ctx, const uint8_t *in, uint8_t *out, uint32_t len)
{
    return CFB_Crypt(ctx, in, out, len, false);
}

static int32_t SetFeedbackSize(MODES_CipherCFBCtx *ctx, const uint32_t *val, uint32_t len)
{
    if (val == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }
    if (len != sizeof(uint32_t)) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODE_ERR_INPUT_LEN);
        return CRYPT_MODE_ERR_INPUT_LEN;
    }
    if (ctx->modeCtx.ciphMeth->algId == CRYPT_SYM_SM4 && *val != 128) { // sm4 set 128 feedbackbits only
        BSL_ERR_PUSH_ERROR(CRYPT_MODES_FEEDBACKSIZE_NOT_SUPPORT);
        return CRYPT_MODES_FEEDBACKSIZE_NOT_SUPPORT;
    }
    if (*val != 1 && *val != 8 && *val != 128) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODES_ERR_FEEDBACKSIZE);
        return CRYPT_MODES_ERR_FEEDBACKSIZE;
    }

    if (*val > (uint32_t)(ctx->modeCtx.blockSize * 8)) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODES_ERR_FEEDBACKSIZE);
        return CRYPT_MODES_ERR_FEEDBACKSIZE;
    }
    ctx->feedbackBits = (uint8_t)*val;
    return CRYPT_SUCCESS;
}

static int32_t GetFeedbackSize(MODES_CipherCFBCtx *ctx, uint32_t *val, uint32_t len)
{
    if (val == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }
    if (len != sizeof(uint32_t)) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODE_ERR_INPUT_LEN);
        return CRYPT_MODE_ERR_INPUT_LEN;
    }
    *val = ctx->feedbackBits;
    return CRYPT_SUCCESS;
}

int32_t MODES_CFB_Ctrl(MODES_CFB_Ctx *modeCtx, int32_t opt, void *val, uint32_t len)
{
    if (modeCtx == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }

    switch (opt) {
        case CRYPT_CTRL_REINIT_STATUS:
            return MODES_SetIv(&modeCtx->cfbCtx.modeCtx, val, len);
        case CRYPT_CTRL_GET_IV:
            return MODES_GetIv(&modeCtx->cfbCtx.modeCtx, (uint8_t *)val, len);
        case CRYPT_CTRL_SET_FEEDBACKSIZE:
            return SetFeedbackSize(&modeCtx->cfbCtx, (uint32_t *)val, len);
        case CRYPT_CTRL_GET_FEEDBACKSIZE:
            return GetFeedbackSize(&modeCtx->cfbCtx, (uint32_t *)val, len);
        case CRYPT_CTRL_GET_BLOCKSIZE:
            if (val == NULL || len != sizeof(uint32_t)) {
                return CRYPT_INVALID_ARG;
            }
            *(int32_t *)val = 1;
            return CRYPT_SUCCESS;
        default:
            BSL_ERR_PUSH_ERROR(CRYPT_MODES_METHODS_NOT_SUPPORT);
            return CRYPT_MODES_METHODS_NOT_SUPPORT;
    }
}

MODES_CFB_Ctx *MODES_CFB_NewCtx(int32_t algId)
{
    const EAL_SymMethod *method = MODES_GetSymMethod(algId);
    if (method == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_INVALID_ARG);
        return NULL;
    }
    MODES_CFB_Ctx *ctx = BSL_SAL_Calloc(1, sizeof(MODES_CFB_Ctx));
    if (ctx == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_MEM_ALLOC_FAIL);
        return ctx;
    }
    ctx->algId = algId;

    ctx->cfbCtx.modeCtx.ciphCtx = BSL_SAL_Calloc(1, method->ctxSize);
    if (ctx->cfbCtx.modeCtx.ciphCtx  == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_MEM_ALLOC_FAIL);
        BSL_SAL_FREE(ctx);
        return NULL;
    }
    uint8_t blockBits = method->blockSize * 8;
    if (blockBits <= 128) {
        ctx->cfbCtx.feedbackBits = blockBits;
    } else {
        ctx->cfbCtx.feedbackBits = 128;
    }
    ctx->cfbCtx.modeCtx.blockSize = method->blockSize;
    ctx->cfbCtx.modeCtx.ciphMeth = method;
    ctx->cfbCtx.modeCtx.offset = 0;
    return ctx;
}

int32_t MODES_CFB_InitCtx(MODES_CFB_Ctx *modeCtx, const uint8_t *key, uint32_t keyLen, const uint8_t *iv,
    uint32_t ivLen, bool enc)
{
    int32_t ret;
    if (ivLen != modeCtx->cfbCtx.modeCtx.blockSize) {
        BSL_ERR_PUSH_ERROR(CRYPT_MODES_IVLEN_ERROR);
        return CRYPT_MODES_IVLEN_ERROR;
    }

    ret = modeCtx->cfbCtx.modeCtx.ciphMeth->setEncryptKey(modeCtx->cfbCtx.modeCtx.ciphCtx, key, keyLen);
    if (ret != CRYPT_SUCCESS) {
        BSL_ERR_PUSH_ERROR(ret);
        return ret;
    }
    (void)memcpy_s(modeCtx->cfbCtx.modeCtx.iv, MODES_MAX_IV_LENGTH, iv, ivLen);
    modeCtx->enc = enc;
    return ret;
}

int32_t MODES_CFB_Update(MODES_CFB_Ctx *modeCtx, const uint8_t *in, uint32_t inLen, uint8_t *out, uint32_t *outLen)
{
    return MODES_CipherStreamProcess(modeCtx->enc ? MODES_CFB_Encrypt : MODES_CFB_Decrypt, &modeCtx->cfbCtx,
        in, inLen, out, outLen);
}

int32_t MODES_CFB_Final(MODES_CFB_Ctx *modeCtx, uint8_t *out, uint32_t *outLen)
{
    (void) modeCtx;
    (void) out;
    *outLen = 0;
    return CRYPT_SUCCESS;
}

int32_t MODES_CFB_DeInitCtx(MODES_CFB_Ctx *modeCtx)
{
    if (modeCtx == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }
    MODES_Clean(&modeCtx->cfbCtx.modeCtx);
    uint8_t blockBits = modeCtx->cfbCtx.modeCtx.blockSize * 8;
    if (blockBits <= 128) {
        modeCtx->cfbCtx.feedbackBits = blockBits;
    } else {
        modeCtx->cfbCtx.feedbackBits = 128;
    }
    return CRYPT_SUCCESS;
}

void MODES_CFB_FreeCtx(MODES_CFB_Ctx *modeCtx)
{
    if (modeCtx == NULL) {
        return ;
    }
    MODES_CFB_DeInitCtx(modeCtx);
    BSL_SAL_Free(modeCtx->cfbCtx.modeCtx.ciphCtx);
    BSL_SAL_Free(modeCtx);
}

int32_t MODES_CFB_InitCtxEx(MODES_CFB_Ctx *modeCtx, const uint8_t *key, uint32_t keyLen, const uint8_t *iv,
    uint32_t ivLen, CRYPT_Param *param, bool enc)
{
    (void) param;
    if (modeCtx == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }
    switch (modeCtx->algId) {
        case CRYPT_CIPHER_SM4_CFB:
            return SM4_CFB_InitCtx(modeCtx, key, keyLen, iv, ivLen, enc);
        default:
            return MODES_CFB_InitCtx(modeCtx, key, keyLen, iv, ivLen, enc);
    }
}

int32_t MODES_CFB_UpdateEx(MODES_CFB_Ctx *modeCtx, const uint8_t *in, uint32_t inLen, uint8_t *out, uint32_t *outLen)
{
    if (modeCtx == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }
    switch (modeCtx->algId) {
        case CRYPT_CIPHER_AES128_CFB:
        case CRYPT_CIPHER_AES192_CFB:
        case CRYPT_CIPHER_AES256_CFB:
            return AES_CFB_Update(modeCtx, in, inLen, out, outLen);
        case CRYPT_CIPHER_SM4_CFB:
            return SM4_CFB_Update(modeCtx, in, inLen, out, outLen);
        default:
            return MODES_CFB_Update(modeCtx, in, inLen, out, outLen);
    }
}

#endif