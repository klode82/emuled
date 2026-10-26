// LINUX: vedi util/Crypto.h. Codice identico all'originale.
#include "StdAfx.h"
#include "util/Crypto.h"

static inline void swap_byte(uint8 *a, uint8 *b)
{
	uint8 bySwap = *a;
	*a = *b;
	*b = bySwap;
}

RC4_Key_Struct* RC4CreateKey(const uchar *pachKeyData, uint32 nLen, RC4_Key_Struct *key, bool bSkipDiscard)
{
	if (key == NULL)
		key = new RC4_Key_Struct;

	uint8 *pabyState = key->abyState;
	for (uint32 i = 0; i < sizeof key->abyState; ++i)
		pabyState[i] = (uint8)i;

	key->byX = 0;
	key->byY = 0;
	uint8 index1 = 0;
	uint8 index2 = 0;
	for (uint32 i = 0; i < sizeof key->abyState; ++i) {
		index2 += pachKeyData[index1] + pabyState[i];
		swap_byte(&pabyState[i], &pabyState[index2]);
		index1 = (uint8)((index1 + 1) % nLen);
	}
	if (!bSkipDiscard)
		RC4Crypt(NULL, NULL, 1024, key);
	return key;
}

void RC4Crypt(const uchar *pachIn, uchar *pachOut, uint32 nLen, RC4_Key_Struct *key)
{
	ASSERT(key != NULL && nLen > 0);
	if (key == NULL)
		return;

	uint8 byX = key->byX;
	uint8 byY = key->byY;
	uint8 *pabyState = key->abyState;
	uint8 byXorIndex;

	for (uint32 i = 0; i < nLen; ++i) {
		byY += pabyState[++byX];
		swap_byte(&pabyState[byX], &pabyState[byY]);
		byXorIndex = (pabyState[byX] + pabyState[byY]);

		if (pachIn != NULL)
			pachOut[i] = pachIn[i] ^ pabyState[byXorIndex];
	}
	key->byX = byX;
	key->byY = byY;
}

void RC4Crypt(uchar *pach, uint32 nLen, RC4_Key_Struct *key)
{
	RC4Crypt(pach, pach, nLen, key);
}
