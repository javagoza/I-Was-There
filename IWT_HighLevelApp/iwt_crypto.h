#pragma once

#include <stdint.h>
#include "user_settings.h"
#include "wolfssl/wolfcrypt/sha256.h"
#include "wolfssl/wolfcrypt/hmac.h"

int iwt_crypto_hmacsha256(uint8_t* hmacDigest,	const uint8_t* buffer,	const uint32_t src_buf_size, const uint8_t* key);
