// LINUX: OtherFunctions.h originale (529 righe, ~100 funzioni, MFC e Win32 mescolati) DIVISO PER ARGOMENTO (Bench #619).
// Questo file e' solo il punto d'ingresso: i file del core continuano a scrivere #include "OtherFunctions.h".
// Ogni funzione si porta nel file del suo argomento, con in testa "Origine: OtherFunctions.cpp:riga". Si porta a richiesta.
//
//   util/Hash.*      FATTO     hash e codifiche esadecimali: md4equ, md4cpy, md4str, strmd4, EncodeBase16, toHex
//   util/Text.*      IN CORSO  (fatte: ROUND, maxi, mini, sgn, inSet, nstrdup) utilita' pure: maxi/mini/sgn, EncodeBase32/DecodeBase32/DecodeBase16, URLEncode/URLDecode,
//                              CastItoXBytes, CastItoIShort, CastSecondsToHM, GetNextString, StripInvalidFilenameChars,
//                              LevenshteinDistance, ExtensionIs, CleanupFilename, ValidFilename
//   util/Crypto.*    FATTO     RC4 (RC4CreateKey, RC4Crypt) per l'obfuscation del protocollo
//   util/Net.*       FATTO     indirizzi IPv4 come testo: ipstr, ipstrA
//   util/Paths.*     DA FARE   percorsi, riscritti per Linux ('/' al posto di '\\'): slosh, unslosh, canonical, EqualPaths,
//                              makepathabs, RemoveFileExtension, HasSubdirectories, GetFreeTempSpace, GetShareName
//   util/Errors.*    IN CORSO  (fatte: DbgSetThreadName, InitThreadLocale) messaggi d'errore senza FormatMessage di Windows: GetErrorMessage, CExceptionStr, DbgGetHexDump
//   (nessun file)    NON SI PORTA  Shell*, BrowserOpen, SelectDir, DialogBrowseFile, appunti, icone, font, schede (solo UI);
//                              GetResString/LocMessageBox (le traduzioni passano a file i18n della UI Web)
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
