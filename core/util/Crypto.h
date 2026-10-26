// LINUX: argomento "crittografia semplice" di OtherFunctions.h: RC4 usato dall'obfuscation del protocollo.
// Origine: srchybrid/OtherFunctions.h righe 519-530 e OtherFunctions.cpp righe 3670-3726 (RC4CreateKey, RC4Crypt, swap_byte).
// Nota: eMule usa RC4 per MASCHERARE il traffico, non per proteggerlo (le chiavi non sono segrete).
#pragma once
#include "StdAfx.h"
#include <cryptopp/integer.h>
#include <cryptopp/modarith.h>

// Crypto++ 8.9 non ha piu' CryptoPP::a_exp_b_mod_c (usata dall'accordo Diffie-Hellman dell'obfuscation): stessa funzione con ModularArithmetic.
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
