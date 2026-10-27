// LINUX: the map-key comparison operators (OtherFunctions.cpp lines 2716-2724). CAICHHash and the AICH tree are in SHAHashSet.cpp.
#include "StdAfx.h"
#include "SHAHashSet.h"
#include "MapKey.h"
#include "SafeFile.h"
#include "OtherFunctions.h"

bool operator==(const CCKey &k1, const CCKey &k2)
{
	return md4equ(k1.m_key, k2.m_key);
}

bool operator==(const CSKey &k1, const CSKey &k2)
{
	return md4equ(k1.m_key, k2.m_key);
}
