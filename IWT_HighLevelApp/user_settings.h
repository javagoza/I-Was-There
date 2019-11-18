#ifndef _USER_SETTINGS_H_
#define _USER_SETTINGS_H_

#define WOLFSSL_AZSPHERE

/* Math: Normal (!USE_FAST_MATH) */
#define SIZEOF_LONG_LONG 8
#define WC_RSA_BLINDING
#define ECC_TIMING_RESISTANT

/* Enable options */
#define HAVE_CHACHA
#define HAVE_POLY1305
#define HAVE_ECC
#define HAVE_SUPPORTED_CURVES
#define HAVE_TLS_EXTENSIONS
#define HAVE_ONE_TIME_AUTH
#define HAVE_TRUNCATED_HMAC
#define HAVE_EXTENDED_MASTER
#define HAVE_ALPN
#define HAVE_SNI
#define HAVE_OCSP
#define HAVE_AESGCM

/* Disable options */
#define NO_PWDBASED
#define NO_DSA
#define NO_DES3
#define NO_RABBIT
#define NO_RC4
#define NO_MD4

/* Benchmark / Testing */
#define BENCH_EMBEDDED
#define USE_CERT_BUFFERS_2048
#define USE_CERT_BUFFERS_256

/* OS */
#define SINGLE_THREADED

/* Filesystem */
#define NO_FILESYSTEM

/* Debug */
#define printf Log_Debug
#define WOLFIO_DEBUG

#endif /* _USER_SETTINGS_H_ */
