cmake_minimum_required(VERSION 3.20)
project(emule_linux CXX)

set(EMULED_VERSION 0.2.0)		# versione di emuled (non quella di eMule): il workflow GitHub ne ricava il tag v<versione>
set(EMULED_UPSTREAM "upstream/emule/0.72a-community")	# sorgenti originali di eMule (sola lettura); gli script in tools/ la leggono da qui
set(EMULED_BASE_VERSION "eMule 0.72a-community")	# il codice da cui deriva
add_compile_definitions(EMULED_VERSION="${EMULED_VERSION}" EMULED_BASE_VERSION="${EMULED_BASE_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Sanitizer: cmake -DEMULE_SANITIZE=address,undefined (or thread)
set(EMULE_SANITIZE "" CACHE STRING "valori per -fsanitize=")
if(EMULE_SANITIZE)
  add_compile_options(-fsanitize=${EMULE_SANITIZE} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${EMULE_SANITIZE})
endif()

# eMule includes with different case and backslashes: the symlinks in compat/ci are regenerated at every configuration.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/fix_case.py RESULT_VARIABLE FIXCASE_RC)
if(NOT FIXCASE_RC EQUAL 0)
  message(FATAL_ERROR "tools/fix_case.py e' fallito")
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(CRYPTOPP REQUIRED IMPORTED_TARGET libcrypto++ libcryptopp)		# Debian: libcrypto++.pc, Fedora: libcryptopp.pc
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
find_package(ZLIB REQUIRED)

# The eMule code is written for MSVC. The workarounds are here, in one place only.
add_compile_options(-Wall -Wno-unknown-pragmas -Wno-reorder -Wno-sign-compare -Wno-parentheses -Wno-misleading-indentation -Wno-write-strings -Wno-int-to-pointer-cast)

# Core library. Sources are added as they get ported (Bench, phases 1-5).
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
# The search parser is generated from the original sources (Parser.y, Scanner.l) with the system flex/bison.
set(GEN_DIR ${CMAKE_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN_DIR})
BISON_TARGET(SearchParser core/search/Parser.y ${GEN_DIR}/Parser.cpp DEFINES_FILE ${GEN_DIR}/parser.hpp)
FLEX_TARGET(SearchScanner core/search/Scanner.l ${GEN_DIR}/Scanner.cpp)
ADD_FLEX_BISON_DEPENDENCY(SearchScanner SearchParser)

add_library(emulecore STATIC
  core/MD4.cpp
  core/SHA.cpp
  core/util/Hash.cpp
  core/util/Errors.cpp
  core/util/Crypto.cpp
  core/util/Net.cpp
  core/util/FileTypes.cpp
  core/util/Random.cpp
  core/util/Text.cpp
  core/util/Paths.cpp
  core/MD5Sum.cpp
  core/EncryptedStreamSocket.cpp
  core/EncryptedDatagramSocket.cpp
  core/ClientUDPSocket.cpp
  core/Server.cpp
  core/ServerList.cpp
  core/ServerSocket.cpp
  core/ServerConnect.cpp
  core/UDPSocket.cpp
  core/EMSocket.cpp
  core/net/ServerProbe.cpp
  core/net/Hello.cpp
  core/search/SearchBuilder.cpp
  core/search/SearchResult.cpp
  core/search/GlobalSearch.cpp
  ${BISON_SearchParser_OUTPUTS}
  ${FLEX_SearchScanner_OUTPUTS}
  core/net/KadProbe.cpp
  core/net/DaemonConfig.cpp
  core/net/Wizard.cpp
  core/net/Banner.cpp
  core/templates/Manifest.cpp
  core/templates/Registry.cpp
  core/CompletedDownloads.cpp
  core/EventLog.cpp
  core/net/Profile.cpp
  core/net/DownloadRun.cpp
  core/net/Daemon.cpp
  core/api/HttpServer.cpp
  core/api/Api.cpp
  core/api/Auth.cpp
  core/api/AuthApi.cpp
  core/api/FsApi.cpp
  core/api/ImportApi.cpp
  core/api/Search.cpp
  core/api/ServerUpdate.cpp
  core/api/Content.cpp
  core/net/HttpFetch.cpp
  core/media/MediaProbe.cpp
  core/net/DirWatcher.cpp
  core/net/SourceWatchdog.cpp
  core/api/Transfers.cpp
  core/api/Prefs.cpp
  core/api/LogApi.cpp
  core/net/ServerSearch.cpp
  core/net/UpnpProbe.cpp
  core/UPnPImpl.cpp
  core/Ini2.cpp
  core/AbstractFile.cpp
  core/FileIdentifier.cpp
  core/ShareableFile.cpp
  core/StatisticFile.cpp
  core/KnownFile.cpp
  core/KnownFileList.cpp
  core/SharedFileList.cpp
  core/BaseClient.cpp
  core/DownloadClient.cpp
  core/UploadClient.cpp
  core/ClientList.cpp
  core/ClientCredits.cpp
  core/PartFile.cpp
  core/PartFileWriteThread.cpp
  core/CorruptionBlackBox.cpp
  core/DownloadQueue.cpp
  core/UploadQueue.cpp
  core/UploadDiskIOThread.cpp
  core/ListenSocket.cpp
  core/SearchFile.cpp
  core/Friend.cpp
  core/FriendList.cpp
  core/Collection.cpp
  core/CollectionFile.cpp
  core/DeadSourceList.cpp
  core/SHAHashSet.cpp
  core/UPnPImplMiniLib.cpp
  core/known/ServerMetFile.cpp
  core/StringConversion.cpp
  core/SafeFile.cpp
  core/kademlia/utils/UInt128.cpp
  core/kademlia/utils/MiscUtils.cpp
  core/kademlia/routing/Contact.cpp
  core/kademlia/routing/RoutingBin.cpp
  core/kademlia/routing/RoutingZone.cpp
  core/kademlia/kademlia/Prefs.cpp
  core/kademlia/kademlia/Kademlia.cpp
  core/kademlia/kademlia/Entry.cpp
  core/kademlia/kademlia/Indexed.cpp
  core/kademlia/kademlia/Search.cpp
  core/kademlia/kademlia/SearchManager.cpp
  core/kademlia/kademlia/UDPFirewallTester.cpp
  core/kademlia/utils/LookupHistory.cpp
  core/kademlia/net/KademliaUDPListener.cpp
  core/kademlia/net/PacketTracking.cpp
  core/kademlia/io/ByteIO.cpp
  core/kademlia/io/BufferedFileIO.cpp
  core/kademlia/io/FileIO.cpp
  core/AICHHash.cpp
  core/kademlia/io/DataIO.cpp
  core/kademlia/io/IOException.cpp
  core/met/TagReader.cpp
  core/met/MetFormat.cpp
  core/met/SafeMet.cpp
  core/known/KnownFile.cpp
  core/known/ClientCredits.cpp
  core/known/Hasher.cpp
  core/known/SharedScanner.cpp
  core/known/SharedIndex.cpp
  core/Packets.cpp
  core/Log.cpp
  core/Emule.cpp
  core/Preferences.cpp
  core/Statistics.cpp
  core/DbgStrings.cpp
  core/FileTypeEx.cpp
  core/ConnectOptions.cpp
  core/URLClient.cpp
  core/ED2KLink.cpp
  core/HttpClientReqSocket.cpp
  core/EmuleDlgStub.cpp
  core/UploadBandwidthThrottler.cpp
  core/guardian/Guardian.cpp
  core/net/EventLoop.cpp
  core/net/AsyncSocketEx.cpp
  core/profile/Import.cpp
  core/profile/IniFile.cpp
  core/profile/Profile.cpp
  core/profile/ProfileCheck.cpp
)
target_include_directories(emulecore PUBLIC compat compat/ci core ${CMAKE_BINARY_DIR}/gen)
# built-in templates are looked up where `cmake --install` puts them (<prefix>/share/emuled/templates; /usr/share/... with -DCMAKE_INSTALL_PREFIX=/usr)
include(GNUInstallDirs)
target_compile_definitions(emulecore PRIVATE EMULED_SYSTEM_TEMPLATES="${CMAKE_INSTALL_FULL_DATADIR}/emuled/templates")
# the API's OpenAPI description goes into the binary (served at /api/v1/openapi.json)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/gen/openapi_json.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/gen
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/embed.py ${CMAKE_SOURCE_DIR}/docs/openapi.json ${CMAKE_BINARY_DIR}/gen/openapi_json.h kOpenApiJson
  DEPENDS ${CMAKE_SOURCE_DIR}/docs/openapi.json ${CMAKE_SOURCE_DIR}/tools/embed.py)
target_sources(emulecore PRIVATE ${CMAKE_BINARY_DIR}/gen/openapi_json.h)
# miniupnpc.h includes miniupnpc_declspec.h from its own folder: with the symlink in compat/ci it must be on the search path.
find_path(MINIUPNPC_INCLUDE_DIR miniupnpc.h PATH_SUFFIXES miniupnpc)
if(MINIUPNPC_INCLUDE_DIR)
  target_include_directories(emulecore SYSTEM PUBLIC ${MINIUPNPC_INCLUDE_DIR})
endif()
find_package(Threads REQUIRED)
# For a single .deb installable on several distributions (Ubuntu 24.04, Debian 13): miniupnpc changes library version between them
# (libminiupnpc17 / 18), so it is embedded in the binary. The other packages have the same name on both.
option(EMULED_STATIC_MINIUPNPC "incorpora miniupnpc nel binario (pacchetto .deb unico)" OFF)
if(EMULED_STATIC_MINIUPNPC)
  find_library(MINIUPNPC_LIB NAMES libminiupnpc.a REQUIRED)
else()
  find_library(MINIUPNPC_LIB miniupnpc REQUIRED)
endif()
target_link_libraries(emulecore PUBLIC PkgConfig::CRYPTOPP PkgConfig::CURL ZLIB::ZLIB Threads::Threads ${MINIUPNPC_LIB})

enable_testing()
add_executable(test_hash tests/test_hash.cpp)
target_link_libraries(test_hash emulecore)
add_test(NAME hash COMMAND test_hash)

add_executable(test_ed2k_file tests/test_ed2k_file.cpp)
target_link_libraries(test_ed2k_file emulecore)
# Real file with a hash computed by eMule Windows. Skipped if the file is missing.
add_test(NAME ed2k_file_carletto COMMAND test_ed2k_file
  "/home/steveholmes/eMule/Incoming/CARLETTO/S01/Carletto il Principe dei Mostri - S01E001 - Diventiamo amici - Carletto cambia casa.mkv"
  335EB15C3BB9A53EDEF5B1FCC441CFA1)

add_executable(test_io tests/test_io.cpp)
target_link_libraries(test_io emulecore)
add_test(NAME io COMMAND test_io)

add_executable(test_met tests/test_met.cpp)
target_link_libraries(test_met emulecore)
add_test(NAME met COMMAND test_met)

add_executable(test_met_real tests/test_met_real.cpp)
target_link_libraries(test_met_real emulecore)
# The user's real profile, READ-ONLY. Skipped if the folder is missing.
add_test(NAME met_profilo_reale COMMAND test_met_real /home/steveholmes/eMule/config /home/steveholmes/eMule/Temp /home/steveholmes/eMule/Backup)

add_executable(test_profile tests/test_profile.cpp)
target_link_libraries(test_profile emulecore)
add_test(NAME profile COMMAND test_profile /home/steveholmes/eMule/config)

add_executable(emuled daemon/emuled.cpp)
target_link_libraries(emuled emulecore)
# Check of the real profile, READ-ONLY: must end with 0 (all healthy).
add_test(NAME emuled_check_profilo_reale COMMAND emuled --profile /home/steveholmes/eMule/config --pathmap "Y:\\eMule=/home/steveholmes/eMule" --check-profile)
set_tests_properties(emuled_check_profilo_reale PROPERTIES SKIP_REGULAR_EXPRESSION "non trovato")

add_executable(test_daemon tests/test_daemon.cpp)
target_link_libraries(test_daemon emulecore)
add_dependencies(test_daemon emuled)
add_test(NAME daemon COMMAND test_daemon $<TARGET_FILE:emuled>)

add_executable(test_known tests/test_known.cpp)
target_link_libraries(test_known emulecore)
add_test(NAME known COMMAND test_known /home/steveholmes/eMule/config)

add_executable(test_known_real tests/test_known_real.cpp)
target_link_libraries(test_known_real emulecore)
# Recomputes the hashes of real files (READ-ONLY, up to 3 GB) and compares them with eMule Windows' known.met.
add_test(NAME known_hash_reali COMMAND test_known_real /home/steveholmes/eMule/config 3000000000 /home/steveholmes/eMule/Incoming /home/steveholmes/eMule/Incoming/CARLETTO)
set_tests_properties(known_hash_reali PROPERTIES SKIP_REGULAR_EXPRESSION "^SKIP")

add_executable(test_compat tests/test_compat.cpp)
target_link_libraries(test_compat emulecore)
add_test(NAME compat COMMAND test_compat)

add_executable(test_shared_index tests/test_shared_index.cpp)
target_link_libraries(test_shared_index emulecore)
add_test(NAME shared_index COMMAND test_shared_index)

add_executable(test_guardian tests/test_guardian.cpp)
target_link_libraries(test_guardian emulecore)
add_test(NAME guardian COMMAND test_guardian)

add_executable(test_crash tests/test_crash.cpp)
target_link_libraries(test_crash emulecore)
add_test(NAME crash COMMAND test_crash)

add_executable(test_net tests/test_net.cpp)
target_link_libraries(test_net emulecore)
add_test(NAME net COMMAND test_net)

add_executable(test_packets tests/test_packets.cpp)
target_link_libraries(test_packets emulecore)
add_test(NAME packets COMMAND test_packets /home/steveholmes/eMule/config)

add_executable(test_throttler tests/test_throttler.cpp)
target_link_libraries(test_throttler emulecore)
add_test(NAME throttler COMMAND test_throttler)

add_executable(test_crypto tests/test_crypto.cpp)
target_link_libraries(test_crypto emulecore)
add_test(NAME crypto COMMAND test_crypto)

add_executable(test_emsocket tests/test_emsocket.cpp)
target_link_libraries(test_emsocket emulecore)
add_test(NAME emsocket COMMAND test_emsocket ${CMAKE_SOURCE_DIR}/tests/data)

add_executable(test_probe tests/test_probe.cpp)
target_link_libraries(test_probe emulecore)
add_test(NAME probe COMMAND test_probe)

add_executable(test_kad_routing tests/test_kad_routing.cpp)
target_link_libraries(test_kad_routing emulecore)
add_test(NAME kad_routing COMMAND test_kad_routing /home/steveholmes/eMule/config)
set_tests_properties(kad_routing PROPERTIES SKIP_REGULAR_EXPRESSION "^SKIP")

add_executable(test_dgram_crypt tests/test_dgram_crypt.cpp)
target_link_libraries(test_dgram_crypt emulecore)
add_test(NAME dgram_crypt COMMAND test_dgram_crypt)

add_executable(test_auth tests/test_auth.cpp)
target_link_libraries(test_auth emulecore)
add_test(NAME auth COMMAND test_auth)

add_executable(test_wizard tests/test_wizard.cpp)
target_link_libraries(test_wizard emulecore)
add_test(NAME wizard COMMAND test_wizard)
add_executable(test_banner tests/test_banner.cpp)
target_link_libraries(test_banner emulecore)
add_test(NAME banner COMMAND test_banner)
add_executable(test_import tests/test_import.cpp)
target_link_libraries(test_import emulecore)
add_test(NAME import COMMAND test_import)

add_executable(test_kad_net tests/test_kad_net.cpp)
target_link_libraries(test_kad_net emulecore)
add_test(NAME kad_net COMMAND test_kad_net)
set_tests_properties(kad_net PROPERTIES SKIP_REGULAR_EXPRESSION "^SKIP")

add_executable(test_search tests/test_search.cpp)
target_link_libraries(test_search emulecore)
add_test(NAME search COMMAND test_search)

add_executable(test_fwcheck tests/test_fwcheck.cpp)
target_link_libraries(test_fwcheck emulecore)
add_test(NAME fwcheck COMMAND test_fwcheck)

add_executable(test_api_http tests/test_api_http.cpp)
target_link_libraries(test_api_http emulecore)
add_test(NAME api_http COMMAND test_api_http)

add_executable(test_daemon_api tests/test_daemon_api.cpp)
add_dependencies(test_daemon_api emuled)
add_test(NAME daemon_api COMMAND test_daemon_api $<TARGET_FILE:emuled>)

add_executable(test_sourcewatchdog tests/test_sourcewatchdog.cpp)
target_link_libraries(test_sourcewatchdog emulecore)
add_test(NAME sourcewatchdog COMMAND test_sourcewatchdog)

add_executable(test_dirwatcher tests/test_dirwatcher.cpp)
target_link_libraries(test_dirwatcher emulecore)
add_test(NAME dirwatcher COMMAND test_dirwatcher)

add_executable(test_mediaprobe tests/test_mediaprobe.cpp)
target_link_libraries(test_mediaprobe emulecore)
add_test(NAME mediaprobe COMMAND test_mediaprobe ${CMAKE_SOURCE_DIR}/tests/data)

add_executable(test_credits tests/test_credits.cpp)
target_link_libraries(test_credits emulecore)
add_test(NAME credits COMMAND test_credits)

add_executable(test_prefs_paths tests/test_prefs_paths.cpp)
target_link_libraries(test_prefs_paths emulecore)
add_test(NAME prefs_paths COMMAND test_prefs_paths)

add_executable(test_download tests/test_download.cpp)
target_link_libraries(test_download emulecore)
add_test(NAME download COMMAND test_download)

add_executable(test_serverlist tests/test_serverlist.cpp)
target_link_libraries(test_serverlist emulecore)
add_test(NAME serverlist COMMAND test_serverlist /home/steveholmes/eMule/config)
set_tests_properties(serverlist PROPERTIES SKIP_REGULAR_EXPRESSION "^SKIP")

add_executable(test_serverconnect tests/test_serverconnect.cpp)
target_link_libraries(test_serverconnect emulecore)
add_test(NAME serverconnect COMMAND test_serverconnect)
set_tests_properties(serverconnect PROPERTIES SKIP_REGULAR_EXPRESSION "^SKIP")

add_executable(test_globalsearch tests/test_globalsearch.cpp)
target_link_libraries(test_globalsearch emulecore)
add_test(NAME globalsearch COMMAND test_globalsearch)
set_tests_properties(globalsearch PROPERTIES SKIP_REGULAR_EXPRESSION "^SKIP")

add_executable(test_daemon_config tests/test_daemon_config.cpp)
target_link_libraries(test_daemon_config emulecore)
add_dependencies(test_daemon_config emuled)
add_test(NAME daemon_config COMMAND test_daemon_config $<TARGET_FILE:emuled>)

add_executable(test_template_manifest tests/test_template_manifest.cpp)
target_link_libraries(test_template_manifest emulecore)
add_test(NAME template_manifest COMMAND test_template_manifest)

add_executable(test_template_registry tests/test_template_registry.cpp)
target_link_libraries(test_template_registry emulecore)
add_test(NAME template_registry COMMAND test_template_registry)

add_executable(test_eventlog tests/test_eventlog.cpp)
target_link_libraries(test_eventlog emulecore)
add_test(NAME eventlog COMMAND test_eventlog)

# tests on the real eMule Windows profile (copy in /home/steveholmes/eMule): read-only, skipped if the folder is missing
add_test(NAME real_profile COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/real_profile.sh $<TARGET_FILE:emuled>)
set_tests_properties(real_profile PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)

add_executable(test_indexed_load tests/test_indexed_load.cpp)
target_link_libraries(test_indexed_load emulecore)
add_test(NAME indexed_load COMMAND test_indexed_load)

# ---- installation (Bench 849; the real package is task 615): the daemon and the built-in templates, built with `npm run build` in web/
install(TARGETS emuled RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(EXISTS ${CMAKE_SOURCE_DIR}/web/dist/templates)
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/web/dist/templates/ DESTINATION ${CMAKE_INSTALL_DATADIR}/emuled/templates)
else()
  message(WARNING "web/dist/templates non c'e': `cmake --install` non installera' i template di serie. Costruirli con: cd web && npm install && npm run build")
endif()
install(FILES packaging/emuled-pkg.service DESTINATION lib/systemd/user RENAME emuled.service)
install(FILES README.md NOTICE.md LICENSE docs/MANUAL.md docs/TEMPLATES.md DESTINATION share/doc/emuled)

# ---- packages (task 615): `cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr && cmake --build build --target emuled && cpack --config build/CPackConfig.cmake -G DEB` (or -G RPM, needs rpmbuild)
set(CPACK_PACKAGE_NAME emuled)
set(CPACK_STRIP_FILES ON)
set(CPACK_PACKAGE_VERSION ${EMULED_VERSION})
set(CPACK_PACKAGE_CONTACT "klode82@gmail.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/klode82/emuled")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "emuled - eMule per Linux (demone con API REST e interfaccia web)")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION net)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPLv2+")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION /usr/lib/systemd /usr/lib/systemd/user /usr/share/doc)
include(CPack)
