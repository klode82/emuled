// LINUX: the "simple cryptography" topic of OtherFunctions.h: RC4 used by the protocol obfuscation.
// Origine: srchybrid/OtherFunctions.h righe 519-530 e OtherFunctions.cpp righe 3670-3726 (RC4CreateKey, RC4Crypt, swap_byte).
// Note: eMule uses RC4 to MASK the traffic, not to protect it (the keys are not secret).
#pragma once
#include "StdAfx.h"
#include <cryptopp/integer.h>
#include <cryptopp/modarith.h>

// Crypto++ 8.9 no longer has CryptoPP::a_exp_b_mod_c (used by the obfuscation Diffie-Hellman agreement): same function with ModularArithmetic.
namespace CryptoPP {
inline Integer a_exp_b_mod_c(const Integer &x, const Integer &e, const Integer &m) { return ModularArithmetic(m).Exponentiate(x, e); }
}

struct RC4_Key_Struct
{
	uint8 abyState[256];
	uint8 byX;
	uint8 byY;
};

RC4_Key_Struct* RC4CreateKey(const uchar *pachKeyData, uint32 nLen, RC4_Key_Struct *key = NULL, bool bSkipDiscard = false);
void RC4Crypt(const uchar *pachIn, uchar *pachOut, uint32 nLen, RC4_Key_Struct *key);
void RC4Crypt(uchar *pach, uint32 nLen, RC4_Key_Struct *key); //in-place
