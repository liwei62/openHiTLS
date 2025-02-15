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

#ifndef APP_CTX_H
#define APP_CTX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup hitls_cert_type
 * @brief   Describe the APP cache linked list.
 */
typedef struct BslList AppList;

typedef struct {
    uint8_t *buf;       /* buffer */
    uint32_t bufSize;   /* size of the buffer */
    uint32_t start;     /* start position */
    uint32_t end;       /* end position */
} AppBuf;

/**
 * AppDataCtx struct, used to transfer app data information
 */
struct AppDataCtx {
    AppBuf appReadBuf;      /* buffer received by the app */
    AppList *appList;       /* cache unexpected app messages */
};

#ifdef __cplusplus
}
#endif
#endif