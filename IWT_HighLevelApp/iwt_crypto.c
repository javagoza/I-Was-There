#include "iwt_crypto.h"

#ifdef WOLFSSL_STATIC_MEMORY
static WOLFSSL_HEAP_HINT* HEAP_HINT;
#else
#define HEAP_HINT NULL
#endif /* WOLFSSL_STATIC_MEMORY */
static int devId = INVALID_DEVID;

int iwt_crypto_hmacsha256(uint8_t* hmacDigest,
	const uint8_t* input,
	const uint32_t input_size,
	const uint8_t* key ) {
	Hmac        hmac;
	int ret;
	if (wc_HmacInit(&hmac, HEAP_HINT, devId) != 0)
		return -3500;

	ret = wc_HmacSetKey(&hmac, WC_SHA256, (byte*)key,
		(word32)XSTRLEN(key));
	if (ret != 0)
		return -3510;
	
		ret = wc_HmacUpdate(&hmac, (byte*)input,
			(word32)input_size);
	if (ret != 0)
		return -3520;
	ret = wc_HmacFinal(&hmac, hmacDigest);
	if (ret != 0)
		return -3530;
	return 0;
}

