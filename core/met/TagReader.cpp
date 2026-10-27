// LINUX (codice nuovo): vedi TagReader.h
#include "StdAfx.h"
#include "met/TagReader.h"
#include "Opcodes.h"

namespace emule::met {

bool ReadTag(Reader &r, Tag &t, std::string &why)
{
	t = Tag();
	t.off = r.pos;
	t.type = r.u8();
	if (t.type & 0x80) { t.type &= 0x7F; t.id = r.u8(); }
	else {
		uint16_t len = r.u16();
		if (len == 1) t.id = r.u8();
		else { if (len > r.left()) { why = "tag name past the end"; return false; } t.name.assign((const char*)r.p + r.pos, len); r.skip(len); }
	}
	if (r.bad) { why = "tag truncated"; return false; }
	switch (t.type) {
	case TAGTYPE_STRING: { uint16_t l = r.u16(); if (l > r.left()) { why = "string past the end"; return false; } t.str.assign((const char*)r.p + r.pos, l); r.skip(l); t.isStr = true; break; }
	case TAGTYPE_UINT32: t.num = r.u32(); t.isInt = true; break;
	case TAGTYPE_UINT64: t.num = r.u64(); t.isInt = true; break;
	case TAGTYPE_UINT16: t.num = r.u16(); t.isInt = true; break;
	case TAGTYPE_UINT8:  t.num = r.u8();  t.isInt = true; break;
	case TAGTYPE_FLOAT32: r.skip(4); break;
	case TAGTYPE_HASH: r.skip(16); break;
	case TAGTYPE_BOOL: r.skip(1); break;
	case TAGTYPE_BOOLARRAY: { uint16_t l = r.u16(); r.skip((size_t)l / 8 + 1); break; }
	case TAGTYPE_BLOB: { uint32_t l = r.u32(); if (l > r.left()) { why = "blob past the end"; return false; } r.skip(l); break; }
	default:
		if (t.type >= TAGTYPE_STR1 && t.type <= TAGTYPE_STR16) {
			size_t l = t.type - TAGTYPE_STR1 + 1;
			if (l > r.left()) { why = "short string past the end"; return false; }
			t.str.assign((const char*)r.p + r.pos, l); r.skip(l); t.isStr = true; break;
		}
		why = "unknown tag type 0x" + std::string(1, "0123456789ABCDEF"[t.type >> 4]) + "0123456789ABCDEF"[t.type & 15];
		return false;
	}
	if (r.bad) { why = "tag value truncated"; return false; }
	t.len = r.pos - t.off;
	return true;
}

} // namespace emule::met
