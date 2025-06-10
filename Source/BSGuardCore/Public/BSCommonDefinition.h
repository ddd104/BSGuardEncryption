#pragma once

THIRD_PARTY_INCLUDES_START

#define UI UI_ST
#include <openssl/hmac.h>
#undef UI

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

#ifdef OPENSSL_API_COMPAT
#undef OPENSSL_API_COMPAT
#define OPENSSL_API_COMPAT 0x10100000L
#endif


#define BSGE_META_ENCRYPT_KEY TEXT("BSGE_EncryptTag")

namespace BSGE
{
	constexpr uint8  CryptoMagic[4]   = { 'B', 'S', 'G', 'E' };
	constexpr uint8  CryptoVersion    = 0x01;          // 方便后续协议升级
	constexpr int32  GcmNonceSize     = 12;            // 96 bit  GCM 标准
	constexpr int32  GcmTagSize       = 16;            // 128 bit Auth Tag
}


DECLARE_LOG_CATEGORY_EXTERN(LogBSGE,    Log, All);     // Core 运行时 & 加解密
DECLARE_LOG_CATEGORY_EXTERN(LogBSGEEd,  Log, All);     // Editor 扩展


#ifndef UE_NEWER_THAN
#define UE_NEWER_THAN(Major,Minor) \
(ENGINE_MAJOR_VERSION >  (Major) || \
(ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)))
#endif


#if WITH_EDITOR
#define BSGE_WITH_EDITOR  1
#else
#define BSGE_WITH_EDITOR  0
#endif