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
#if defined(HITLS_CRYPTO_EAL) && defined(HITLS_CRYPTO_DRBG)

#include <stdint.h>
#include <stdbool.h>
#include <securec.h>
#include "crypt_eal_rand.h"
#include "crypt_errno.h"
#include "bsl_errno.h"
#include "bsl_err_internal.h"
#include "bsl_sal.h"
#include "eal_common.h"
#include "crypt_types.h"
#include "crypt_local_types.h"
#include "crypt_algid.h"
#include "crypt_drbg.h"
#include "eal_drbg_local.h"
#ifdef HITLS_CRYPTO_MD
#include "eal_md_local.h"
#endif
#ifdef HITLS_CRYPTO_MAC
#include "eal_mac_local.h"
#endif
#ifdef HITLS_CRYPTO_CIPHER
#include "eal_cipher_local.h"
#endif
#include "crypt_modes.h"

static EAL_RandUnitaryMethod g_randMethod = {
    .newCtx = (RandNewCtx)DRBG_New,
    .inst = (RandDrbgInst)DRBG_Instantiate,
    .unInst = (RandDrbgUnInst)DRBG_Uninstantiate,
    .gen = (RandDrbgGen)DRBG_Generate,
    .reSeed = (RandDrbgReSeed)DRBG_Reseed,
    .ctrl = (RandDrbgCtrl)DRBG_Ctrl,
    .freeCtx = (RandDrbgFreeCtx)DRBG_Free,
};

/* Mapping between RAND and specific random number generation algorithms */
static const DrbgIdMap DRBG_METHOD_MAP[] = {
#ifdef HITLS_CRYPTO_DRBG_HASH
    { CRYPT_RAND_SHA1, CRYPT_MD_SHA1, RAND_TYPE_MD },
    { CRYPT_RAND_SHA224, CRYPT_MD_SHA224, RAND_TYPE_MD },
    { CRYPT_RAND_SHA256, CRYPT_MD_SHA256, RAND_TYPE_MD },
    { CRYPT_RAND_SHA384, CRYPT_MD_SHA384, RAND_TYPE_MD },
    { CRYPT_RAND_SHA512, CRYPT_MD_SHA512, RAND_TYPE_MD },
#endif
#ifdef HITLS_CRYPTO_DRBG_HMAC
    { CRYPT_RAND_HMAC_SHA1, CRYPT_MAC_HMAC_SHA1, RAND_TYPE_MAC },
    { CRYPT_RAND_HMAC_SHA224, CRYPT_MAC_HMAC_SHA224, RAND_TYPE_MAC },
    { CRYPT_RAND_HMAC_SHA256, CRYPT_MAC_HMAC_SHA256, RAND_TYPE_MAC },
    { CRYPT_RAND_HMAC_SHA384, CRYPT_MAC_HMAC_SHA384, RAND_TYPE_MAC },
    { CRYPT_RAND_HMAC_SHA512, CRYPT_MAC_HMAC_SHA512, RAND_TYPE_MAC },
#endif
#ifdef HITLS_CRYPTO_DRBG_CTR
    { CRYPT_RAND_AES128_CTR, CRYPT_CIPHER_AES128_CTR, RAND_TYPE_AES },
    { CRYPT_RAND_AES192_CTR, CRYPT_CIPHER_AES192_CTR, RAND_TYPE_AES },
    { CRYPT_RAND_AES256_CTR, CRYPT_CIPHER_AES256_CTR, RAND_TYPE_AES },
    { CRYPT_RAND_AES128_CTR_DF, CRYPT_CIPHER_AES128_CTR, RAND_TYPE_AES_DF },
    { CRYPT_RAND_AES192_CTR_DF, CRYPT_CIPHER_AES192_CTR, RAND_TYPE_AES_DF },
    { CRYPT_RAND_AES256_CTR_DF, CRYPT_CIPHER_AES256_CTR, RAND_TYPE_AES_DF }
#endif
};

const DrbgIdMap *GetDrbgIdMap(CRYPT_RAND_AlgId id)
{
    uint32_t num = sizeof(DRBG_METHOD_MAP) / sizeof(DRBG_METHOD_MAP[0]);
    for (uint32_t i = 0; i < num; i++) {
        if (DRBG_METHOD_MAP[i].drbgId == id) {
            return &DRBG_METHOD_MAP[i];
        }
    }

    return NULL;
}

EAL_RandUnitaryMethod* EAL_RandGetMethod()
{
    return &g_randMethod;
}

static int32_t GetRequiredMethod(const DrbgIdMap *map, EAL_RandMethLookup *lu)
{
    switch (map->type) {
#ifdef HITLS_CRYPTO_DRBG_HASH
        case RAND_TYPE_MD: {
            const EAL_MdMethod *md = EAL_MdFindMethod(map->depId);
            if (md == NULL) {
                BSL_ERR_PUSH_ERROR(CRYPT_EAL_ERR_ALGID);
                return CRYPT_EAL_ERR_ALGID;
            }
            lu->methodId = map->depId;
            lu->method = md;
            break;
        }
#endif
#ifdef HITLS_CRYPTO_DRBG_HMAC
        case RAND_TYPE_MAC: {
            EAL_MacMethLookup hmac;
            int32_t ret = EAL_MacFindMethod(map->depId, &hmac);
            if (ret != CRYPT_SUCCESS) {
                BSL_ERR_PUSH_ERROR(CRYPT_EAL_ERR_ALGID);
                return CRYPT_EAL_ERR_ALGID;
            }
            lu->methodId = map->depId;
            lu->method = hmac.macMethod;
            break;
        }
#endif
#ifdef HITLS_CRYPTO_DRBG_CTR
        case RAND_TYPE_AES:
        case RAND_TYPE_AES_DF: {
            const EAL_SymMethod *ciphMeth = MODES_GetSymMethod(map->depId);
            if (ciphMeth == NULL) {
                BSL_ERR_PUSH_ERROR(CRYPT_EAL_ERR_ALGID);
                return CRYPT_EAL_ERR_ALGID;
            }
            lu->methodId = map->depId;
            lu->method = ciphMeth;
            break;
        }
#endif
        default:
            BSL_ERR_PUSH_ERROR(CRYPT_EAL_ERR_ALGID);
            return CRYPT_EAL_ERR_ALGID;
    }
    return CRYPT_SUCCESS;
}

int32_t EAL_RandFindMethod(CRYPT_RAND_AlgId id, EAL_RandMethLookup *lu)
{
    if (lu == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_NULL_INPUT);
        return CRYPT_NULL_INPUT;
    }

    const DrbgIdMap *map = GetDrbgIdMap(id);
    if (map == NULL) {
        BSL_ERR_PUSH_ERROR(CRYPT_EAL_ERR_ALGID);
        return CRYPT_EAL_ERR_ALGID;
    }

    int32_t ret = GetRequiredMethod(map, lu);
    if (ret != CRYPT_SUCCESS) {
        return ret;
    }

    lu->type = map->type;
    return CRYPT_SUCCESS;
}
#endif
