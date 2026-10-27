// LINUX: original OtherFunctions.h (529 lines, ~100 functions, MFC and Win32 mixed) SPLIT BY TOPIC (Bench #619).
// This file is only the entry point: the core files keep writing #include "OtherFunctions.h".
// Each function is ported to the file of its topic, with "Origin: OtherFunctions.cpp:line" at the top. Ported on request.
//
//   util/Hash.*      DONE      hashes and hex encodings: md4equ, md4cpy, md4str, strmd4, EncodeBase16, toHex
//   util/Text.*      IN PROGRESS  (done: ROUND, maxi, mini, sgn, inSet, nstrdup) pure utilities: maxi/mini/sgn, EncodeBase32/DecodeBase32/DecodeBase16, URLEncode/URLDecode,
//                              CastItoXBytes, CastItoIShort, CastSecondsToHM, GetNextString, StripInvalidFilenameChars,
//                              LevenshteinDistance, ExtensionIs, CleanupFilename, ValidFilename
//   util/Crypto.*    DONE      RC4 (RC4CreateKey, RC4Crypt) for protocol obfuscation
//   util/Net.*       DONE      IPv4 addresses as text: ipstr, ipstrA
//   util/Paths.*     TO DO     paths, rewritten for Linux ('/' instead of '\\'): slosh, unslosh, canonical, EqualPaths,
//                              makepathabs, RemoveFileExtension, HasSubdirectories, GetFreeTempSpace, GetShareName
//   util/Errors.*    IN PROGRESS  (done: DbgSetThreadName, InitThreadLocale) error messages without Windows FormatMessage: GetErrorMessage, CExceptionStr, DbgGetHexDump
//   (no file)    NOT PORTED  Shell*, BrowserOpen, SelectDir, DialogBrowseFile, clipboard, icons, fonts, tabs (UI only);
//                              GetResString/LocMessageBox (translations move to the Web UI i18n files)
#pragma once
#include "util/Hash.h"
#include "util/Text.h"
#include "util/Errors.h"
#include "util/Crypto.h"
#include "util/Net.h"
#include "util/FileTypes.h"
#include "util/Random.h"
#include "util/Text.h"
#include "util/Paths.h"
#include "DbgStrings.h"
#include "FileTypeEx.h"
#include "ConnectOptions.h"
