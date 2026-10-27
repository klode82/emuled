# eMule Linux — Manual Reference

Glossary and reference manual for the port of eMule v0.72a to Linux (project **eMuleWeb**, id 10 on Bench).
It is updated every time a new term is introduced: if a term, acronym, function or class is not here, it must be added. From now on every acronym or technical term is explained at its first occurrence.

**How to read it.** Part A is a dictionary in alphabetical order: short definitions, written for someone who does not know the term. Each entry also says *where we use it* in the project. Part B is the detailed reference: the eMule profile files, the formats, the code modules, the commands, the tests. Part C explains how we work (phases, tasks, rules).

Status of what the entries mention: **[used]** is already in the code; **[planned]** is in the plan but not built yet; **[not ported]** was dropped on purpose.

---

## Contents

- **Part A** — Glossary A–Z
- **Part B** — Reference
  - B1. The eMule profile files
  - B2. The tag format
  - B3. MFC/Windows → Linux map
  - B4. The modules of our code, class by class
  - B5. The `emuled` command
  - B6. The tests
  - B7. Quality tools: ASan, UBSan, TSan, fuzzing
  - B8. The libraries
  - B9. The project directories
  - B10. Porting rule for every file (checklist)
- **Part C** — How we work

---

# Part A — Glossary A–Z

**AICH** (*Advanced Intelligent Corruption Handling*) — eMule's system for repairing a badly downloaded file without downloading a whole 9.7 MB part again. Besides the MD4 hash, it computes a tree of SHA-1 hashes over the 180 KB blocks: if a part turns out to be corrupt, you can tell *which blocks* are wrong. The root of the tree (20 bytes) is the "master hash". *Here:* the data is in `known2_64.met` (52 MB in the real profile, 333 records); reading and validation are **[used]**, the computation (port of `SHAHashSet.cpp`) is **[planned]** in Phase 5.

**API** (*Application Programming Interface*) — The list of requests that one program can make to another. In this project: the ones the web interface will make to the `emuled` daemon. *Here:* **[planned]** in Phase 6, REST/JSON plus WebSocket. The manual of the calls will be the **APIDoc**.

**APIDoc** — The manual of all the API calls, in plain English, with `curl` examples: [`APIDOC.md`](APIDOC.md). Next to it is the formal description [`openapi.json`](openapi.json) (OpenAPI), which the daemon also serves at `/api/v1/openapi.json`. *Here:* **[written]** for the calls that exist.

**arming / re-arming (of a notification)** — How Windows delivers network events (`WSAAsyncSelect`): one read notification *at a time*; the next one arrives only after the application has called `Receive()`. eMule uses this to **delay reads** and limit the download bandwidth. *Here:* reproduced in `CAsyncSocketEx` by removing the socket from epoll until it is re-armed.

**ASan** (*AddressSanitizer*) — See B7. A tool that makes a program fail immediately when it uses memory wrongly: writing past the end of an array, using memory that was already freed, etc. *Here:* **[used]** on all the tests (`-DEMULE_SANITIZE=address,undefined`).

**ASLR** (*Address Space Layout Randomization*) — An operating system security measure: at every start a program ends up in different memory areas. *Here:* TSan does not work with ASLR enabled on this kernel; it is launched with `setarch "$(uname -m)" -R`.

**ATL** — *Active Template Library*, a Microsoft C++ library for Windows, used by the original source for strings and COM. **[not ported]**.

**atomic (atomic write)** — An operation that either succeeds completely or never happened, with no visible intermediate states. On Linux, a `rename()` that replaces a file is atomic: whoever reads sees the whole old file or the whole new one, never a half file. *Here:* the basis of `SafeWriteMet`.

**backup by hash** — A copy of a healthy `.part.met` called `<hash>.met`, where the hash is the identity of the download. It is overwritten only if the same download moves forward. *Here:* **[used]** by the Guardian.

**.bak** — A safety copy of a `.met` file, next to the original (`known.met.bak`). *Here:* it is created **only if the original is healthy**; eMule instead always creates it, even from a damaged file.

**BOM** (*Byte Order Mark*) — A sequence of bytes at the start of a text file that declares its encoding: `EF BB BF` = UTF-8, `FF FE` = UTF-16 little-endian. *Here:* `shareddir.dat` has the UTF-16 BOM; in the `.met` files, names with non-ASCII characters start with the UTF-8 BOM.

**bootstrap (Kad)** — The first phase, in which a Kademlia client with no contacts looks for some in order to join the network. A bootstrap `nodes.dat` contains hundreds of contacts to try. *Here:* the validator recognises it (version 3, edition 1).

**CEMSocket** — The socket for ed2k packets: it reads the TCP stream and reassembles it into packets (even when split across several segments), queues the packets to send in two queues (control and data) that the throttler empties, and applies the download limit. *Here:* `core/EMSocket.*`. See B4.

**CLI** — Command line. `emuled` is used from the CLI today.

**cmake / CMake** — A tool that reads `CMakeLists.txt` and generates the files to build the project. **[used]**.

**compat** — The `compat/` folder: the layer that replaces the Windows classes (MFC) with Linux equivalents, without changing the eMule code. See B3.

**completion port (IOCP)** — A Windows mechanism for asynchronous I/O: you start a read/write and a thread waits for the "completion" on a queue. *Here:* emulated in `compat/win_iocp.h`: the operation runs immediately (`pread`/`pwrite`) and the completion goes into a queue; the original disk threads do not change.

**cp1252** — Windows encoding for Western Europe (*Windows-1252*): one byte = one character. From byte 0x80 to 0x9F it contains symbols such as € and curly quotes. *Here:* default codepage for the "local" text received from the network, configurable.

**credits** — eMule's system that rewards whoever has given: the more you have uploaded to a client, the higher it puts you in its queue. They are stored in `clients.met`.

**CRLF** — The Windows line ending (`\r\n`, two bytes). Linux uses only `\n`. *Here:* eMule's `.ini` files are CRLF and `IniFile` keeps them that way when writing.

**Crypto++ / cryptopp** — A C++ cryptography library. **[used]** for MD4 and SHA-1; it will also be needed for RSA (secure ident).

**CString** — The MFC string class. *Here:* `compat/CString.h`, on top of UTF-8 `std::string`. See B3.

**CTag** — The eMule class that represents a tag (name, type, value) and reads or writes it in files and packets. *Here:* **[used]**, ported from `Packets.cpp`; 242,146 tags of your `known.met` were read.

**ctest** — CMake's test runner. `ctest --test-dir build` launches the project's 14 tests.

**daemon** — A program that runs in the background, without a window, and is controlled remotely. `emuled` is the daemon: it contains the whole eMule engine; the web interface will be a client. **[partial]**: today it only has profile control commands.

**data race** — Two threads touching the same variable, at least one of them writing, without coordinating. The result is unpredictable. On Windows some cases "work by chance"; for the language they are errors anyway. TSan finds them. *Here:* TSan found three (see B7), all fixed.

**destruction contract** — A rule for the sockets driven by the throttler: the most derived class must call `StopThrottling()` as the first statement of its destructor, because the throttler (another thread) calls virtual functions of the socket as long as it is in its queues.

**Diffie-Hellman** — A method by which two programs agree on a secret key while talking in the clear. eMule uses it in the obfuscation handshake with the servers, with a fixed 768-bit prime number. *Here:* in `EncryptedStreamSocket`, with Crypto++ (`a_exp_b_mod_c` reimplemented with `ModularArithmetic`).

**dry-run** — A "dry run": it computes and reports what it would do, without doing anything. `--check-profile` is a dry-run of the recovery.

**ed2k** (*eDonkey 2000*) — eMule's original sharing protocol: a client connects to a **server** that indexes files and sources, then downloads directly from other clients. A file is identified by its **ed2k hash**.

**edge-triggered / level-triggered** — Two ways of using `epoll`. *Level-triggered*: epoll signals as long as there is something to read. *Edge-triggered*: it signals only when something new arrives. *Here:* level-triggered, with manual re-arming, because it reproduces the Windows semantics.

**EFBIG** — Linux error "file too large": a write exceeds the size limit imposed on the process. *Here:* we use it in the tests to simulate a write that fails (`RLIMIT_FSIZE`).

**EMBLOCKSIZE** — 184,320 bytes (180 KB): the size of the blocks in which data is exchanged and on which the AICH tree is computed.

**endianness / little-endian** — The order in which a multi-byte number is written in memory. eMule always writes *little-endian* (least significant byte first), like today's x86 and ARM processors. *Here:* all the `.met` formats.

**ENOSPC** — "Disk full" error. Not testable here without a virtual disk: postponed to Phase 9.

**epoll** — A Linux mechanism for waiting for events on many network connections together without one thread for each. *Here:* **[used]** by the `EventLoop`, it replaces Windows' `WSAAsyncSelect`.

**EventLoop** — The event loop (`core/net/EventLoop`): a thread that waits with `epoll` for socket activity, runs timers and tasks sent by other threads (`Post`). It replaces the hidden window of Windows. See B4.

**FD_READ, FD_WRITE, FD_ACCEPT, FD_CONNECT, FD_CLOSE** — The Winsock network notifications: data to read, writing is possible, connection to accept, connection completed, connection closed. `FD_FORCEREAD` is an eMule addition to force a read.

**flock** — A file lock between processes, on Linux. *Here:* `ProfileLock` uses it to prevent two `emuled` on the same profile; it releases itself if the process dies.

**fsync** — A call that forces the operating system to write a file's data *really to disk* (otherwise it stays in the cache and a power cut loses it). *Here:* `SafeWriteMet` does `fsync` of the file and of the folder.

**fuzzing** — Trying a program with huge amounts of random or malformed input to bring out errors. *Here:* `tests/fuzz/` feeds the parsers of everything a stranger can send (Kad and ed2k packets, `.met` files, `ed2k://` links, HTTP requests) to the real code under ASan+UBSan; see B7. A light version (20,000 randomly flipped bits) is also in `test_met`, and `fuzz_smoke` repeats a fixed set of mutations in every `ctest`.

**gap / control packet / data packet** — In the protocol, a *control* packet is a short message (login, request, reply) and has priority; a *data* packet carries a piece of a file and can be split over several sends.

**GCC / g++** — The Linux C++ compiler. We use C++20 (`-std=gnu++20`).

**getaddrinfo** — A system function that translates a name (`ed2k.example.org`) into IP addresses. It blocks until the DNS answers, which is why it runs in a thread.

**golden test** — A test that compares the result of our code with a reference result produced by the original program. *Here:* the ed2k hash of your files, computed here, must be equal to the one from eMule Windows in `known.met`.

**Guardian** — The module (`core/guardian`) that watches the Temp folder, saves the healthy `.part.met` files by hash and warns when one becomes damaged. It is the logic of **MetGuardian** inside the daemon.

**hardlink** — A second name for the same file. A `rename` breaks the link. Not handled.

**hash** — A fixed-length "summary" of a file: if a single bit changes, the hash changes completely. It is used to identify a file and to check that it is intact. eMule uses MD4 (16 bytes) and, for AICH, SHA-1 (20 bytes).

**Header_Struct** — The 6-byte header of every ed2k packet over TCP: protocol (1), length (4, includes the opcode), opcode (1). *Here:* `core/Packets.h`.

**HighID / LowID** — A **HighID** client can be reached from outside (other people can connect to it); a **LowID** cannot (it is behind a NAT or a firewall) and must be called back through the server. A LowID has fewer chances to exchange files.

**iconv** — A program that converts files between encodings (e.g. cp1252 → UTF-8). *Here:* 9 `.cpp` and 8 `.h` files of the source are in cp1252 and must be converted when they are ported.

**IniFile** — Our class that reads and writes `.ini` files like Windows, keeping byte for byte whatever is not modified. See B4.

**`init_priority`** — A GCC attribute that fixes the order in which global objects of different files are constructed. *Here:* `thePrefs` and `theStats` are born (and die) after the others, because `theApp` uses them in its constructor and destructor.

**IoSource** — The interface of whoever receives the kernel events for a descriptor (in our code, `CAsyncSocketEx`).

**Kad / Kademlia** — A sharing network **without servers**: each client has a 128-bit identifier and knows some other clients; searches "climb" toward the clients closest to the sought object (distance = XOR of the identifiers). *Here:* the `nodes.dat` file contains the known contacts; the engine is **[planned]** in Phase 4.

**known.met** — The list of all the files eMule knows: name, size, date, hash and part hashes. *Here:* 10 MB, 37,561 records in the real profile. It is read and rewritten byte-identical.

**known2_64.met** — The AICH hashes of all the known files. 52 MB, 333 records in the real profile.

**Latin-1** — ISO 8859-1 encoding: like cp1252 but bytes 0x80–0x9F are invisible control characters. `latin1` option of the codepage.

**libcurl** — A library to download files over HTTP (updating `server.met`, `nodes.dat`, the IP filter). **[used]**.

**libFuzzer** — LLVM's fuzzing tool: it runs a function millions of times with inputs it mutates itself, keeping the ones that reach new code (*coverage-guided*). *Here:* the `fuzz_*` programs of `tests/fuzz/` (clang, `-DEMULED_FUZZ=ON`).

**little-endian** — See *endianness*.

**Log** — The chronological list of what the program does and of the problems it meets ("connecting to the server…", "connection refused", "file completed"). In eMule it is the *Log* tab. In the daemon it is a buffer in memory (the last 3000 lines) read with `GET /log` and followed live by the WebSocket event `log`; each line has a sequence number, the time, the level (debug, info, warning, error) and the source (general, server, api). *Debug* lines are there only with the `verbose_log` preference. With `save_log_to_disk` every line also goes to `logs/emuled.log` (it stays after a restart or a sudden stop; at 20 MB the file becomes `emuled.log.1`), with `save_debug_to_disk` the debug lines too. *Here:* **[written]**.

**loopback** — The address `127.0.0.1`: the machine talking to itself. *Here:* all the network tests use only the loopback, they do not touch the Internet.

**LowID** — See *HighID*.

**mbedtls** — A TLS library (encrypted communications), used by eMule's web server. The system has version 2.28; the source targets 3.x. **[not used yet]**, open decision (Bench #617).

**MD4** — eMule's 16-byte hash algorithm. Old and not secure for cryptography, but it is the one of the protocol. *Here:* class `CMD4` (from `MD4.cpp`, on top of Crypto++).

**MD5** — A 16-byte hash algorithm, like MD4 but newer. eMule uses it to derive the obfuscation keys and for the web server passwords. *Here:* class `MD5Sum`. Beware: the MD5 of a *string* is computed on the **UTF-16** bytes, as on Windows.

**MetGuardian** — The user's project (`/aurigalab/python/metguardian`) that makes backups of healthy `.part.met` files. Its model is the basis of our Guardian and of safe writing.

**MFC** (*Microsoft Foundation Classes*) — The Windows C++ library on which eMule's interface is built (windows, `CString`, `CFile`, ...). It does not exist on Linux: that is why `compat/` exists.

**miniupnpc** — A library to open the router's ports (UPnP). **[used]**.

**MSG_NOSIGNAL / SIGPIPE** — Writing to a closed connection makes the program receive the `SIGPIPE` signal, which normally terminates it. `MSG_NOSIGNAL` avoids that: the send returns an error. *Here:* always on in `Send`.

**mtime** — Date and time of a file's last modification.

**NAT loopback (or *hairpin*)** — A router's ability to let a computer on the local network reach another local computer through the **public address**. Many routers do not do it: they answer "No route to host". *Here:* it prevents two eMules in the same house from connecting through the public IP; it is worked around with an ed2k link that gives the source with the local address (`|sources,192.168.1.128:55662|`).

**nice** — Process priority value on Linux (higher = "nicer", that is, lower priority). *Here:* the hashing threads run with nice 10.

**Ninja** — A program that runs the build very quickly, driven by CMake. **[used]**.

**nodes.dat** — The list of Kademlia contacts. Versions 0, 1, 2, 3. *Here:* 6,812 bytes, version 2, 200 contacts in the real profile.

**obfuscation (protocol encryption)** — eMule can mask its own traffic so that it is not recognisable: a handshake with random numbers and padding, then RC4. *Here:* **[used]** in `CEncryptedStreamSocket`; tested on localhost, also with obfuscation required or turned off.

**OP_EDONKEYPROT / OP_EMULEPROT / OP_PACKEDPROT / Kad** — The bytes that open a packet and say which protocol it is: `0xE3` eDonkey, `0xC5` eMule extensions, `0xD4` zlib-compressed packet, `0xE4` Kademlia, `0xE5` compressed Kademlia.

**OpenAPI** — A standard format to describe an API in a file (paths, parameters, replies), from which the documentation is generated and tests are made. **[planned]**.

**Packet** — The eMule class that represents a packet (header + data) and compresses/decompresses it with zlib. See B4.

**part / .part / .part.met** — A download in progress is made of `NNN.part` (the data, of the final size) and `NNN.part.met` (the list of missing pieces, the "gaps", the name, the size, the hashes). Losing the `.part.met` makes a gigabyte-sized `.part` unusable. See B1.

**part hash** — The MD4 hash of each 9,728,000-byte piece of a file. The file's hash is the MD4 of the concatenation of the part hashes.

**PARTSIZE** — 9,728,000 bytes (9,500 KB): the size of a **part**, the unit in which eMule divides, verifies and shares files.

**Pinia** — A state manager for Vue. **[used]**.

**POSITION** — An MFC type for walking through lists and maps (`GetNext(pos)`). *Here:* reproduced in `compat/collections.h`.

**POSIX** — The standard of Linux and Unix system calls (`open`, `read`, `fsync`, `rename`, `flock`, ...).

**Post** — A function of the `EventLoop`: it makes a task run in the loop's thread. It is the **only** safe thing to call from another thread.

**#pragma pack(1)** — An instruction that tells the compiler not to insert padding bytes in a structure, so that it matches the file format byte for byte. The source has 78 of them: each one must be checked with `static_assert(sizeof...)`.

**profile** — The eMule folder with the configuration and the data: `preferences.ini`, `known.met`, `clients.met`, `server.met`, ... See B1. Yours is in `/home/steveholmes/eMule/config`.

**protocol** — The set of rules by which two programs exchange messages.

**RC4** — A simple stream cipher. eMule uses it to **mask** the traffic (obfuscation), not to protect it: the keys are not secret. After creating the key it discards the first 1,024 bytes. *Here:* `RC4CreateKey` and `RC4Crypt` in `util/Crypto`.

**recovery (RecoverMet)** — A function that, if a `.met` is damaged or gone, restores it from the healthy `.bak` or from another healthy copy, and keeps the damaged one as `.corrupt`.

**RLIMIT_FSIZE** — A system limit on the size of the files a process may write. *Here:* in the tests, to simulate a failed write.

**RSA** — Public-key cryptography. eMule uses it for **secure ident**.

**sanitizer** — Tools inserted by the compiler that check the program while it runs (ASan, UBSan, TSan). See B7.

**secure ident (secure identification)** — A mechanism by which two clients prove their identity with RSA keys, so nobody can steal another's credits. *Here:* the public key (up to 80 bytes, `MAXPUBKEYSIZE`) is in the `clients.met` records: 7,652 out of 8,027 in the real profile.

**server (ed2k)** — A program that indexes files and sources for ed2k clients. `server.met` is your list of servers (7 in the real profile).

**shareddir.dat** — The list of shared folders. In UTF-16 with BOM; in your profile 25 entries, with Windows paths (`Y:\eMule\...`).

**sharedfiles.dat** — The list of files shared one by one (not by folder). In your profile it is empty.

**SIGKILL** — A signal that terminates a process instantly, without giving it a chance to close anything. *Here:* in the tests, 400 times during writing to check file safety.

**slot** — The number of a download in Temp (`001`, `002`, ...). The Guardian tracks the slots; a slot can be taken by another download (state REPLACED).

**SPA** (*Single Page Application*) — A web application that loads once and then updates the page by itself. **[used]**, Vue 3.

**static_assert** — A check done by the compiler: if a condition is false the program does not compile. *Here:* it checks that `DWORD` is 4 bytes and that the structs have the size of the format.

**stub** — A temporary object that takes the place of one from the original that has not been ported yet, with only what the ported files use. *Here:* `theApp`, `thePrefs`, the upload queue, the message log (`core/Emule.h`, `Preferences.h`, `UploadQueue.h`, `Log.h`, ...). See B4.

**symlink** — A symbolic link to another file. *Here:* in `compat/ci/` to resolve includes with wrong capitalisation and in `SafeWriteMet`, which writes to the real file and leaves the link.

**systemd** — The Linux service manager: it starts `emuled` at boot (`packaging/emuled.service`; `emuled --install-service` installs it for your user). **[used]**.

**tag** — A "name = value" pair with a type, used everywhere in the `.met` files and in the ed2k packets (file name, size, gaps, ...). See B2.

**TCHAR / `_T()`** — A Windows type and macro for writing code that is valid with both 8-bit and 16-bit text. *Here:* `TCHAR` = `char` (UTF-8) and `_T("x")` = `"x"`.

**theApp / thePrefs** — The two global objects of eMule: the application (`theApp`, with the queues, the servers, the clients...) and the preferences (`thePrefs`). All the original code calls them directly.

**thread** — A line of execution inside a process. The chosen model (Bench #560): one "logic" thread that touches the data, plus worker threads (hash, disk) that do not touch the shared data.

**throttler** — The upload bandwidth limiter (`UploadBandwidthThrottler`): a thread that decides at every round how many bytes each socket may send, so as to respect the limit and split it equally among the slots. See B4.

**TLS** — The protocol of encrypted communications (https). **[planned]** for the web API.

**TSan** (*ThreadSanitizer*) — See B7. A tool that finds wrong concurrent accesses between threads (*data race*).

**UBSan** (*UndefinedBehaviorSanitizer*) — See B7. A tool that finds constructs whose behaviour is not defined by the language, for example reading an integer from an odd address.

**UDP / TCP** — The two ways of sending data over a network. TCP is reliable and connection-based; UDP is fast, connectionless. eMule uses TCP (port 4662 by default, 24662 in your profile) and UDP (4672, 24672 in yours).

**UInt128** — A 128-bit number used by Kademlia for identifiers. *Here:* class `Kademlia::CUInt128` (ported).

**UPnP** — A protocol by which a program asks the router to open a port. **[used]**.

**UTF-8 / UTF-16** — Two encodings of Unicode text: UTF-8 uses 1–4 bytes per character (ASCII stays 1 byte), UTF-16 uses 2 or 4 bytes. *Here:* all the internal text is UTF-8 (Bench #554); `shareddir.dat` is UTF-16.

**validator** — A function that says whether a file is **healthy** (correct header, all records readable, exact end of file, consistent fields). Outcome: `Valid`, `Invalid`, `Unsupported`, `IoError`.

**verdict** — The outcome of a validator. *Unsupported* = format recognised but not handled (it is not proven to be broken); *IoError* = the file cannot be read, no judgement on its content.

**Vite / Vue 3 / TypeScript** — Tools for the web interface: Vue is the framework, Vite the build tool, TypeScript the typed language. **[used]**.

**WebSocket** — A channel that stays open between browser and server to send events in real time (download progress, server state). *Here:* **[written]** on `/api/v1/events` (token in `?token=`); the daemon compares the state every second and sends `stats`, `server.state`, `kad.state`, `download.*`, `search.results` (see APIDOC section 6).

**Win32** — The Windows programming interface (`CreateFile`, `GetTickCount`, `SendMessage`, ...). Wherever the source uses it, it must be replaced with POSIX.

**write guard (`mtime guard`)** — A rule: a file that cannot be read but was modified only a few seconds ago (10 s) is not "damaged", it is probably being written by another program. See also *eMule Windows active*.

**WSAAsyncSelect** — The Windows call used to ask to receive a socket's events as messages to a window. It does not exist on Linux: `epoll` inside the `EventLoop` replaces it.

**XDG** — A Linux convention for the user's folders: configuration in `~/.config`, data in `~/.local/share`. *Here:* the default profile is `~/.config/emule`, variable `$XDG_CONFIG_HOME`.

**zlib** — A compression library. eMule compresses the larger packets (protocol `0xD4`). *Here:* **[used]** by `Packet::PackPacket` / `UnPackPacket`.

**:Zone.Identifier** — A Windows leftover: a hidden stream that NTFS adds to downloaded files; copied to Linux it becomes a real file (`name.mkv:Zone.Identifier`). In your Incoming tree there are 111. The scanner excludes them.

---

# Part B — Reference

## B1. The eMule profile files

The real profile is in `/home/steveholmes/eMule/config` (63 MB). The **Verified** column says whether the format is tested on your files; rows marked *(to be verified)* are interpretations from the file name, not yet checked against the source.

| File | What it is for | Format | In your profile | Verified |
|---|---|---|---|---|
| `preferences.ini` | Settings (nick, ports, folders, language, network) | INI, CRLF, ASCII | 11 KB, 15 sections; nick `SteveHolmes`, TCP 24662, UDP 24672, language 1040 | read and rewritten identical |
| `preferences.dat` | User identity: 16-byte user hash and window position | 61 fixed bytes | valid | validator + real |
| `preferencesKad.dat` | IP address and Kad identifier (16 bytes) | 23 fixed bytes | valid | validator + real |
| `known.met` | All known files | header 0x0F, count, records | 10 MB, 37,561 records, 199,803 part hashes | identical rewrite |
| `known2_64.met` | AICH hashes of the known files | header 0x02, "master + N hashes" records | 52 MB, 333 records | validator + real |
| `server.met` | List of ed2k servers | header 0xE0, count, servers with tags | 1.2 KB, 7 servers | validator + real |
| `clients.met` | Client credits | header 0x12, count, 119 bytes per client | 955 KB, 8,027 clients; 1,701.6 GB received, 2,316.2 GB sent | identical rewrite |
| `cancelled.met` | Cancelled downloads (hashes "scrambled" with a seed, so they are not proposed again) | header 0x0F, version, seed, records | 979 bytes, 57 records | validator + real |
| `nodes.dat` | Kademlia contacts | versions 0–3 | 6.8 KB, version 2, 200 contacts | validator + real |
| `shareddir.dat` | Shared folders | UTF-16 text with BOM | 25 Windows entries, 23 no longer exist | read |
| `sharedfiles.dat` | Files shared one by one | UTF-16 text | empty | read |
| `Category.ini`, `statistics.ini` | Download categories and statistics | INI | small | — |
| `StoredSearches.met` | Saved searches | binary | 53 KB | not yet (Phase 7) |
| `key_index.dat`, `src_index.dat`, `load_index.dat` | Kademlia indexes (keywords, sources, load) | binary | 1 MB, 232 KB, 792 bytes | not yet (Phase 4) |
| `emfriends.met` | Friends | binary | 5 bytes (empty) | — |
| `staticservers.dat` | Fixed servers | text | 284 bytes | — |
| `downloads.txt` / `downloads.bak` | Text list of the downloads (to restore them) *(to be verified)* | UTF-16 | 880 bytes | only read as text |
| `SearchSpam.met` | Search results marked as spam *(to be verified)* | binary | 185 bytes | — |
| `AC_*.dat` | Histories for the auto-completion of fields *(to be verified)* | text | a few bytes | — |
| `addresses.dat`, `webservices.dat` | Server update addresses; web services of the menu *(to be verified)* | text | 0 and 693 bytes | — |
| `eMule.tmpl` | Template of the old eMule web interface | HTML | 115 KB | — |

**The downloads in progress** are in Temp (`/home/steveholmes/eMule/Temp`):

| File | Content |
|---|---|
| `NNN.part` | The downloaded data, with the final size of the file (missing parts are empty) |
| `NNN.part.met` | Version 0xE0 (or 0xE2 for large files): date, hash, part hashes, tags with name, size and the **gaps** (ranges still to download) |
| `NNN.part.met.bak` | Copy made by eMule after every save, **without checking whether the file is sound** |

If `NNN.part` is shorter than the declared size, eMule considers the rest missing; if it is longer, it truncates it (`PartFile.cpp` lines 1078–1083).

**How eMule saves files — two defects found in the source**
1. `known.met` is written by opening it with `modeCreate`, that is, **truncating it in place**, with no temporary file and no `.bak`: an interruption leaves an empty or broken file.
2. `known2_64.met` is written **by direct append**: an interruption leaves a half record at the end.
Also `clients.met` uses a weak check (it does not redo the `.bak` if this is bigger than the original).

## B2. The tag format

A tag is: **type** (1 byte) · **name** · **value**. If the type has bit 0x80 set, the name is a numeric identifier of 1 byte. Otherwise the name is written as a length (2 bytes) followed by the characters; if the length is 1, that character is a numeric identifier.

| Type | Value | Size of the value |
|---|---|---|
| `HASH` | 0x01 | 16 bytes |
| `STRING` | 0x02 | 2 bytes of length + text |
| `UINT32` | 0x03 | 4 bytes |
| `FLOAT32` | 0x04 | 4 bytes |
| `BOOL` | 0x05 | 1 byte |
| `BOOLARRAY` | 0x06 | 2 bytes of length + `length/8 + 1` bytes |
| `BLOB` | 0x07 | 4 bytes of length + data |
| `UINT16` | 0x08 | 2 bytes |
| `UINT8` | 0x09 | 1 byte |
| `BSOB` | 0x0A | not handled by eMule |
| `UINT64` | 0x0B | 8 bytes |
| `STR1`…`STR16` | 0x11–0x20 | text of 1…16 bytes, without length |

Warning: in `MetGuardian` the types 0x08 and 0x09 are swapped compared to eMule (finding recorded on Bench #632).

Identifiers used in our files: `FT_FILENAME`=0x01, `FT_FILESIZE`=0x02, `FT_LASTSEENCOMPLETE`=0x05, `FT_TRANSFERRED`=0x08, `FT_GAPSTART`=0x09, `FT_GAPEND`=0x0A, `FT_STATUS`=0x14, `FT_DLPRIORITY`=0x18, `FT_KADLASTPUBLISHSRC`=0x21, `FT_AICH_HASH`=0x27, `FT_LASTSHARED`=0x34, `FT_CATEGORY`=0x53; for the servers `ST_SERVERNAME`=0x01, `ST_DESCRIPTION`=0x0B. The **gaps** have a name made of one byte (0x09 or 0x0A) followed by the index as text (`"\x090"`, `"\x091"`, …) and the value is the boundary: for the end, "first byte that is not missing".

The ed2k hash rule: file **< PARTSIZE** → hash = MD4 of the file, no part hashes. Otherwise `size / PARTSIZE + 1` part hashes (the last one is the MD4 of nothing if the size is an exact multiple) and file hash = MD4 of their concatenation.

## B3. MFC/Windows → Linux map

Where the source uses a Windows class, `compat/` provides one with the same interface, so that the eMule code changes as little as possible.

| Original (Windows) | What it is | On Linux | File |
|---|---|---|---|
| `BYTE`, `WORD`, `DWORD`, `UINT`, `ULONG`, `LONG`, `ULONGLONG` | fixed-width integers | `uint8_t` … of exact width. **`LONG`/`ULONG` are 32 bits on Linux too** (there `long` has 64) | `win_types.h` |
| `__int64`, suffix `ui64` | MSVC types and suffixes | `long long` and literal operator `ui64` | `win_types.h` |
| `TCHAR`, `_T()`, `_tcs*` | "wide or narrow" text | `char`, UTF-8 | `win_types.h` |
| `CString`, `CStringA`, `CStringW` | string | three names for a single type (UTF-8). `GetLength()` counts **bytes** | `CString.h` |
| `CException`, `CFileException`, `CMemoryException` | exceptions thrown as a pointer, with `Delete()` | same | `CFile.h` |
| `CFile`, `CMemFile`, `CStdioFile` | file on disk, in memory, with C buffer | POSIX, 64-bit offsets. The `shareDenyWrite` flags are accepted and ignored | `CFile.h` |
| `CFileFind` | list of the files of a folder | `opendir` + `fnmatch` | `CFile.h` |
| `CArray`, `CList`, `CMap`, `CPtrList`, `CTypedPtrList`, `CStringArray`, `CMapStringToString`, `CByteArray`, `CUIntArray` | collections | on STL containers, with the same interface (`POSITION`, `GetNext`, `Lookup`, …) | `collections.h`, `StdAfx.h` |
| `CCriticalSection`, `CMutex`, `CSingleLock` | synchronization | `std::recursive_mutex` (reentrant as in MFC) | `sync.h` |
| `CEvent` | signal between threads | mutex + condition variable, manual or automatic | `time_thread.h` |
| `CWinThread`, `AfxBeginThread`, `RUNTIME_CLASS` | threads | `std::thread`, with `InitInstance/Run/ExitInstance`; priority → nice | `time_thread.h` |
| `CTime`, `CTimeSpan`, `GetTickCount` | time | `time_t`/`strftime`, `steady_clock` | `time_thread.h` |
| `ASSERT`, `VERIFY` | development checks | `assert` | `StdAfx.h` |
| `StdAfx.h` | common header of MFC | our own file that includes everything above | `StdAfx.h` |
| `CAsyncSocket`, `CAsyncSocketEx`, `WSAAsyncSelect` | event-driven sockets, delivered as messages to a hidden window | `epoll` with the same public interface; **[used]**. The layer system (proxy) is **[planned]** | `core/net/` |
| `HANDLE`, `WaitForSingleObject`, `InterlockedExchange64`, `timeGetTime` | Windows wait/atomic objects and functions | `CEvent` as HANDLE, `std::atomic`, `steady_clock` | `win_types.h`, `time_thread.h` |
| `SOCKET`, `SOCKADDR_IN`, `WSAEWOULDBLOCK`, `FD_*` | Winsock types and codes | `int`, `sockaddr_in`, `errno`; `WSAGetLastError()` per thread | `win_net.h` |
| `WM_*`, `SendMessage`, `PostMessage` | messages between windows | **[planned]** bus of typed events | — |
| `theApp.emuledlg->...` (655 calls) | the engine that updates the screen | **[planned]** `IUiEvents` interface | — |
| `CListCtrl`, `CDialog`, `CWnd`, … (116 files) | the graphical interface | **[not ported]**: the UI is web | — |

## B4. The modules of our code, class by class

### `core/met` — validate, write, recover the `.met` files

| Name | What it does |
|---|---|
| `Verdict` | `Valid` / `Invalid` / `Unsupported` / `IoError` |
| `Report` | result of a validator: verdict, reason, version, number of records; for `.part.met` also name, size, missing bytes, hash |
| `ValidatePartMet`, `ValidateKnownMet`, `ValidateServerMet`, `ValidateClientsMet`, `ValidateNodesDat`, `ValidatePreferencesDat`, `ValidatePreferencesKad`, `ValidateCancelledMet`, `ValidateKnown2Met` | one validator per format. "Sound" = header, records all readable, **exact end of file**, consistent fields |
| `ValidateFile(path, validator)` | reads a whole file and validates it; if it cannot be read: `IoError` |
| `Reader`, `Tag`, `ReadTag` (`TagReader.h`) | byte reader with bounds checking; reader of a tag like eMule's `CTag`, which remembers position and length to rewrite it identical |
| `SafeWriteMet(path, data, validator)` | **safe write**: validates the new data; if the current file is sound it copies it to `.bak` (tmp+`fsync`+reread+rename); writes `.tmp`, `fsync`, rereads and validates, atomic rename, `fsync` of the folder. Follows symlinks |
| `SafeWriteOptions` / `SafeWriteResult` | suffixes (`.bak`, `.tmp`), whether to make the backup / result (ok, backup made, reason if skipped, error) |
| `RecoverMet(path, validator, fallback, options, dry_run)` | if the file is `Invalid` or missing: restores from sound `.bak` or fallback; the damaged one becomes `.corrupt`. Does not touch `Unsupported` and `IoError`. `dry_run` reports without acting |
| `FaultHook()` | **for tests only**: allows simulating a crash at a precise point of the write |

### `core/known` — the known and shared files

| Name | What it does |
|---|---|
| `KnownFile` | a `known.met` record: date, hash, part hashes, name, size, and the tags with their original bytes |
| `KnownFileList` | the list: `Load`, `Serialize` (byte-identical), `Save` (safe write), `Find(name, date, size)` like eMule's `FindKnownFile`, `FindByHash`, `Add` |
| `MakeKnownFile` | creates a record as eMule would write it (BOM only if the name is not ASCII, 64-bit size only above ~4 GB) |
| `DecodeTagString` | decodes a tag string: UTF-8 BOM or local codepage |
| `HashFile(path)` | ed2k hash of a file (synchronous, cancellable, with progress); error if the file changes while it is being read |
| `HashPool` | hashing queue with N low-priority threads: `Add`, `Wait`, `Cancel` |
| `ScanShared` / `ShareOptions` / `ScanResult` | reads the shared folders (each without subfolders) and separates the already known files from those to compute; configurable exclusions |
| `SharedIndex` | orchestrator: `Open`, `Rescan`, `Poll`, `WaitIdle`, `Save`. The workers compute, **only the logic thread** applies the results |
| `ClientCreditsFile` | `clients.met` as data: `Load`, `Serialize` (identical), `Save`, `Find`, `RemoveExpired` (150 days) |

### `core/profile` — the profile and the configuration

| Name | What it does |
|---|---|
| `IniFile` | INI read/write like Windows: sections and keys case-insensitive, first duplicate key wins, keeps untouched lines byte for byte, CRLF/LF, BOM, ANSI/UTF-8/UTF-16LE. `Save` uses the safe write |
| `DecodeText` | splits a text file into UTF-8 lines, recognizing the BOM |
| `ResolveProfileDir` | profile folder: argument, `$EMULED_PROFILE`, `$XDG_CONFIG_HOME/emule`, `~/.config/emule` |
| `ProfileLock` | single-instance lock on the profile (`flock` on `emuled.lock`, with the PID) |
| `PathMap` | translates Windows paths to Linux with "Windows prefix = Linux prefix" rules (the longest wins, case ignored) |
| `ResolveCaseInsensitive` | finds a path ignoring case, component by component; if two folders differ only by case it does not guess |
| `DaemonConfig` | `emuled.conf`: `[ed2k] legacy_codepage`, `[pathmap]`, `[share] ignore` and `ignore_add` |
| `CoreConfig` | values of `preferences.ini` that the core needs, shared folders (`shareddir.dat`), single files (`sharedfiles.dat`), already mapped paths |

### `core/guardian` — the Guardian

| Name | What it does |
|---|---|
| `State` | `NEW` (only as a starting point), `OK`, `PENDING`, `DAMAGED`, `UNSUPPORTED`, `REPLACED`, `REMOVED` |
| `Slot` | state of a slot: number, state, hash, name, size, missing bytes, backup present |
| `Guardian::Scan(now)` | one pass over Temp: saves the sound `.part.met` files as `<hash>.met`, returns **only the transitions** of state |
| `Guardian::Restore(slot, force, now)` | restores a `DAMAGED` slot from the most advanced sound copy of the same download; refuses if the file was touched less than 2 minutes ago |
| `Guardian::Load/Save` | persistent state in `guardian.state` (an INI, safe write) |

### `core/net` — the network (Phase 3)

| Name | What it does |
|---|---|
| `EventLoop` | the event loop on `epoll`: `Add`/`Update`/`Del` for the descriptors, `Post(fn)` to run a task in the loop's thread (safe from any thread), `AddTimer`/`CancelTimer`, `ResolveAsync` (name resolution in a thread), `RunOnce`/`Run`/`Stop`. One logic thread owns the loop and all the sockets |
| `ScopedLoop` | makes a loop "current" for the duration of a scope: sockets register on the loop of their thread |
| `IoSource` | interface of whoever receives the kernel events |
| `CAsyncSocketEx` | non-blocking socket with **the same public interface** as the original (`Create`, `Connect`, `Accept`, `Listen`, `Send`, `Receive`, `Close`, `ShutDown`, `Attach`/`Detach`, `OnAccept`, `OnClose`, `OnConnect`, `OnReceive`, `OnSend`, `OnHostNameResolved`, `TriggerEvent`, `AsyncSelect`, `GetPeerName`, `GetSockName`, `Get`/`SetSockOpt`). It reproduces the semantics of `WSAAsyncSelect`: read notification one at a time, re-armed by `Receive()`; `FD_WRITE` after the connection and after a blocked `Send`; `FD_CLOSE` only after the remaining data; destroying the socket inside a callback is safe |
| `CAsyncSocket` | as above plus `SendTo`/`ReceiveFrom` for UDP datagrams |
| `WSAGetLastError()` | last network error, per thread; the `WSAE*` codes are the Linux `errno` values |

Not ported for now: the layer system (`CAsyncSocketExLayer`, used by the SOCKS/HTTP proxy) and the `AF_UNSPEC` family.

### `core/Packets` — packets and tags

| Name | What it does |
|---|---|
| `Header_Struct` | 6-byte header: protocol, length (includes the code), operation code |
| `Packet` | a packet: data, header (`GetHeader`, `GetPacket`), compression with zlib (`PackPacket`, `UnPackPacket`). `UnPackPacket` is valid only for **received** packets |
| `CRawPacket` | already formatted packet, without its own header |
| `CTag` | a tag: constructors for integers, strings, hashes, blobs; `WriteTagToFile` (old format), `WriteNewEd2kTag` (compact), construction from file/packet |

### `core/UploadBandwidthThrottler` — the bandwidth limit

| Name | What it does |
|---|---|
| `UploadBandwidthThrottler` | thread that at every pass decides how many bytes each socket may send to respect the limit: first the control packets, then the slots in equal parts, then the rest from first to last; a socket with no sends for one second receives a "trickle" of data. `AddToStandardList`, `RemoveFromStandardList`, `QueueForSendingControlPacket`, `RemoveFromAllQueues`, `GetNumberOfSentBytesSinceLastCallAndReset`, `GetSlotLimit` |
| `ThrottledControlSocket`, `ThrottledFileSocket` | interfaces that a socket must have to be driven by the throttler (`SendControlData`, `SendFileAndControlData`, `IsBusyQuickCheck`, ...) |

Measured: with a 100 KB/s limit it sends 100.0 KB/s; with 8 KB/s, 8.0 KB/s; the slot shares differ by less than a factor of two.

### `core/EMSocket` — the packet socket

| Name | What it does |
|---|---|
| `CEMSocket` | derives from `CEncryptedStreamSocket` and from `ThrottledFileSocket`. `SendPacket(packet, control?, data bytes)` queues; the throttler calls `SendControlData` / `SendFileAndControlData` from its thread. `OnReceive` reassembles the packets (split header, body in pieces, several packets in one read), rejects unknown protocols (`ERR_WRONGHEADER`) and packets over 2 MB (`ERR_TOOBIG`). `SetDownloadLimit` / `DisableDownloadLimit` delay the reading |
| `PacketReceived`, `OnError` | to be implemented in the derived class (client, server) |
| `StopThrottling()` | **destruction contract**: call it first in the destructor of the most derived class |
| atomic flags | `byConnected`, `lastCalledSend`, `lastSent`, `lastFinishedStandard`, `m_bAccelerateUpload`, `m_bBusy`, `m_hasSent`, and the encryption state: shared between the throttler thread and the loop thread |

### The global objects of the application (`core/Emule.h`, `Preferences.*`, `Statistics.*`)

`thePrefs` (the preferences), `theStats` (the statistics) and `theApp` (the application) are now the ported **original eMule files**: `Preferences.cpp` reads and writes `preferences.ini` and `preferences.dat` (61 bytes, as in Windows), `Statistics.cpp` has the counters (atomic, for the threads). `theApp` (`Emule.h`) holds the pointers to the objects: `clientlist`, `downloadqueue`, `uploadqueue`, `sharedfiles`, `knownfiles`, `listensocket`, `clientcredits`, the two I/O threads. `CreateObjects()` creates them in the order of `InitInstance`, `DestroyObjects()` closes them in the order of `CemuleDlg::OnClose`. The folders (`config`, `Temp`, `Incoming`) come from a single one: `thePrefs.SetConfigDir(config)`, which must be an absolute path.

The graphical windows are no-ops in `GuiStubs.h` (they will become API events); the messages from the threads (hash finished, file completed) go through `CEmuleDlgStub::PostMessage` and run in the event loop (`EmuleDlgStub.cpp`).

### `core/net/Profile` and `core/net/DownloadRun` — the "work profiles"

`ScopedProfile` creates a folder (temporary, or the one given and **persistent**) with `config/`, `Temp/`, `Incoming/`, initializes the preferences and creates the application objects. The tests, the probes and the download use it. `RunDownload` downloads a file by hash: server, search by hash (name and size), download queue, optional Kad, and resumes the `.part` files present in `Temp/` (`CDownloadQueue::Init`).

### The Windows compatibility files (`compat/`)

`win_types.h` (Win32 types and functions), `win_files.h` (file dates, `CopyFile`, `MoveFileWithProgress`, `_stat64`), `win_iocp.h` (emulated completion ports: "overlapped" `ReadFile`/`WriteFile` executed immediately with `pread`/`pwrite`; the code of the disk threads stays the original), `collections.h`, `CFile.h`, `CString.h` (UTF-8).

### Functions ported from `OtherFunctions.cpp`

Divided by topic: `util/Text` (texts, URLs, readable dates, file names), `util/Paths` (paths with `/`, disk space, extensions), `FileTypeEx` (file type from the header), `DbgStrings` (opcode names for the log), `ConnectOptions`.

### `core` (files copied and ported from the Windows source)

`MD4.cpp` (class `CMD4`), `SHA.cpp` (`CSHA`), `Packets.cpp` (`Packet`, `CTag`), `UploadBandwidthThrottler.cpp`, `EncryptedStreamSocket.cpp` (obfuscation), `MD5Sum.cpp`, `SafeFile.cpp` (`CSafeFile`, `CSafeMemFile`, `CSafeBufferedFile`, reading/writing of integers, hashes and strings of the protocol), `StringConversion.*` (rewritten: UTF-8, BOM, local codepage), `util/Hash.*` (hash helpers), `util/Text.h` (`maxi`, `mini`, `sgn`, `inSet`), `util/Errors.*` (`DbgSetThreadName`), `util/Crypto.*` (RC4), `util/Net.*` (`ipstr`), `kademlia/utils/UInt128.*`. The rest of `OtherFunctions` is divided by topic (see the map in `core/OtherFunctions.h`). It is compared with the original with `diff -ru ../srchybrid core`.

### `daemon/emuled.cpp` — see B5.

### `tools/fix_case.py`

Creates in `compat/ci/` the symlinks for includes with wrong case (`StdAfx.h` against `Stdafx.h`) and with backslashes (`miniupnpc\include\...`). It runs by itself at every CMake configuration.

## B5. The `emuled` command

Common to all: `--profile DIR` (the `config` folder; without it, the default one), `--pathmap "Y:\eMule=/home/utente/eMule"` (can be repeated, adds to `[pathmap]`).

| Command | What it does | Writes to the profile? |
|---|---|---|
| `--check-profile` | Validates all files and, for the damaged ones, says what it would recover. Exit code **0** all healthy, **1** damaged, **2** usage error | **no**, it does not even take the lock |
| `--scan-shared` | Lists the shared files: already known, to be computed, ignored, missing folders | no |
| `--verify-hashes [--sample N] [--max-size-mb M]` | **Recomputes the ed2k hash** (of the file and of each 9 728 000-byte block) of a sample of the shared files and compares it with the one written by eMule Windows in `known.met`; first it checks that the rewritten `known.met` is **byte-identical** and that every record is found again by hash. Exits with 1 if anything differs | no Then it also compares the **AICH** (*Advanced Intelligent Corruption Handling*: a second hash, in 180 KB blocks, that makes it possible to repair only the damaged piece of a part): master hash and block hashes against Windows' `known.met` and `known2_64.met`. |
| `--update-known [--force]` | Computes the hash of new files and updates `known.met` safely | yes (lock, guard) |
| `--recover [--force]` | Restores the damaged `.met` files from `.bak`; the damaged one becomes `.corrupt` | yes (lock, guard) |
| `--guard-scan --backup-dir DIR` | The Guardian: reads Temp and saves the healthy `.part.met` files in `DIR` | **no**: writes only in `DIR` |
| `--guard-restore SLOT --backup-dir DIR [--force]` | Restores a damaged slot | yes, only that `.part.met` |


**Network commands** (they go out on the real network, they start only on explicit command; they work on a **copy** of `server.met`):

| Command | What it does |
|---|---|
| `--search "text" --search-server-met FILE (--search-server-index N \| --search-server-address IP:PORT) [--search-all-servers] [--tcp-port P] [--ext pdf] [--type Document] [--min-size B] [--max-size B]` | ed2k search on one server (or on all of them, over UDP); lists the files with sources and hash |
| `--download HASH --search-server-met FILE (--search-server-index N \| --search-server-address IP:PORT) --tcp-port P [--udp-port Q --kad NODES.DAT] [--work DIR] [--out DIR] [--seconds N] [--verbose]` | Downloads the file with that MD4. `--work DIR` is a folder that **stays** between one run and the next: an interrupted download restarts from its `.part`. `--kad` adds Kad. Ctrl-C or `kill` close it cleanly |
| `--setup` (or `emuled` alone, from a terminal, without configuration) | **Terminal wizard at first start** (Bench 871): it asks only what is needed to open the web interface — listen address (127.0.0.1 recommended; 0.0.0.0 with a warning), port (**checked**: if it is busy it says so and proposes the next free one) and profile folder — and writes `~/.emuled/config.yaml`. Then it asks whether to install the **systemd user service**: if yes it writes the unit (or uses the one from the package), runs `daemon-reload` and `enable --now`, waits for `/api/v1/ping` to answer, prints the address and the setup code and exits; if no, the daemon starts in the terminal. It skips itself without a terminal (systemd, Docker, tests), with any argument and with the variables `EMULED_CONF`, `EMULED_CONFIG`, `EMULED_API_PORT`, `EMULED_API_BIND` (they decide what the Wizard would ask; the others, such as `EMULED_TEMPLATES_DIR`, do not prevent it); `--setup` runs it again (it asks for confirmation if a configuration already exists and keeps a `.bak` copy of it). It always starts in English; the Italian texts remain available with `EMULED_LANG=it`. Code: `core/net/Wizard.*`, tested by `tests/test_wizard.cpp` with scripted input and a simulated system |
| `--install-service` / `--uninstall-service` | **Service on and off without questions** (Bench 895): `--install-service` writes the *user* systemd unit `~/.config/systemd/user/emuled.service` (unless the package already provides it), reloads systemd and starts the service (`enable --now`); `--uninstall-service` stops and disables it and removes only the unit written by emuled (never the package's). It never touches the profile or `~/.emuled/config.yaml`. Exit code 0 = done, 1 = a step failed, 2 = systemd is not available. Not tested against a real systemd in this session: only with simulated commands (`tests/test_wizard.cpp`). |
| `--reset-user [--profile CONFIG]` | **Web interface account** (Bench 872): from the terminal, it asks for user name and password (twice, without showing it) and saves them in `emuled.users` in the profile folder (permissions `0600`, password with salt + PBKDF2-HMAC-SHA256). It also changes the session secret: browsers already connected will have to log in again. It also works while the daemon is running. The **first** account is instead created from the web interface with the *setup code*, that is the token in `emuled.token`, which the daemon prints at start-up as long as there is no account. See [APIDOC](APIDOC.md) (Account section) |
| `--run --profile CONFIG [--server IP:PORT] [--tcp-port P] [--udp-port Q] [--no-kad] [--fake-profile] [--add LINK]... [--pathmap "Y:\\eMule=/dir"]... [--status-interval N] [--max-seconds N] [--offline] [--verbose] [--api-port P] [--api-bind IP] [--api-token T] [--web-dir DIR] [--templates-dir DIR] [--no-api]` | **The daemon** (it is the default command: `emuled` alone is enough, `--run` is optional; the tools `--check-profile`, `--probe-*`, `--search`, ... replace it): it stays running, creates the objects once, resumes the `.part` files in `Temp/`, listens, connects to a server and to Kad, prints a status line every N seconds (30 by default) and stops in an orderly way with Ctrl-C or `kill`. It uses the identity and the preferences of the profile; with `--fake-profile` a **temporary** hash and no saving (see "Real profile and fake profile"). Windows paths in the profile without a `--pathmap` rule are ignored (the default folders next to the profile apply). To try it on a copy of someone else's profile use `--fake-profile`. The REST/JSON API starts with it (`127.0.0.1:8765`, token in `emuled.token`; see [APIDOC](APIDOC.md)); `--offline` opens neither servers nor Kad. **Start-up settings** (config folder, API port and address, web UI folder) also from a **file** and from **environment variables**: see below |
| `--probe-kad NODES.DAT`, `--probe-server`, `--check-ports`, `--upnp-discover` | Diagnostic probes |

Note: the server index (`--search-server-index`) is the position in the `server.met` **file**, not in the in-memory list (which eMule keeps in random order).

**Guard.** `--recover` and `--update-known` refuse (exit code 2) if a file of the profile or of the downloads was modified less than **2 minutes** ago: another eMule, for example the Windows one on the same shared folder, might be active. It is overridden with `--force`. A non-healthy file modified less than **10 seconds** ago is "being written", not damaged. A date in the future (different clocks) up to 5 minutes counts as "just modified".

### Start-up settings: file and environment variables

The daemon needs at least the **config folder** (the one with `preferences.dat`). It can be given in three ways; if they appear together, the highest one wins:

1. **command line**: `--profile FOLDER`, `--api-port P`, `--api-bind IP`, `--web-dir FOLDER`, `--templates-dir FOLDER`;
2. **environment variables**: `EMULED_CONFIG`, `EMULED_API_PORT`, `EMULED_API_BIND`, `EMULED_WEB_DIR`;
3. **configuration file** (the daemon's *base* configuration, not eMule's, which lives in the profile): `key: value` lines (simple YAML; `key = value` is fine too; `#` starts a comment, quotes are optional, `~/` means your folder). Keys: `config` (profile folder), `api_port`, `api_bind`, `web_dir` (**a single interface**, served at `/`, with no template choice), `templates_dir` (an **extra** folder where to look for interface templates: it is for development and for Docker; the built-in ones and yours are searched automatically in `/usr/share/emuled/templates` and `~/.emuled/templates`), `mediainfo`. Environment variables: `EMULED_TEMPLATES_DIR` and the other `EMULED_*`; on the command line `--templates-dir`. An unknown key or a wrong line stops the start-up with the line number (better than an ignored typo). The file is searched in this order: `--conf FILE`, `$EMULED_CONF`, **`~/.emuled/config.yaml`**, then the old `$XDG_CONFIG_HOME/emuled/emuled.conf` (or `~/.config/emuled/emuled.conf`) and `/etc/emuled/emuled.conf`. **If it finds none (and you did not give `--conf` or `$EMULED_CONF`) it creates a default one in `~/.emuled/config.yaml`**, with the `~/.emuled` folder and the empty profile `~/.emuled/profile` reserved to your user, and tells you at start-up. **When the file is created** (by the Wizard or by the first start) the values that came from environment variables (`EMULED_TEMPLATES_DIR`, `EMULED_WEB_DIR`, `EMULED_MEDIAINFO`, and also `EMULED_API_BIND`, `EMULED_API_PORT`, `EMULED_CONFIG`) are **written into the file**: a systemd service does not inherit the terminal's environment, so otherwise the service would not see them. Without variables the lines stay commented out, with the default value (the built-in templates, `/usr/share/emuled/templates`, are searched automatically anyway). If the file already exists it is not touched: the variable is valid only for that run.

The default file, `~/.emuled/config.yaml`:

```
config: ~/.emuled/profile
api_bind: 127.0.0.1
api_port: 8765
# web_dir: /percorso/di/web/dist
# templates_dir: /percorso/dei/template
```
To use an already existing profile (for example the copy of the eMule Windows one) just change `config:`.

Without a config folder anywhere the daemon does not start and says so (exit code 2). The `daemon_config` test checks all of this, also with the real process.

## B6. The tests

`ctest --test-dir build --output-on-failure` runs 47 tests. The ones on the real profile work in **read-only** mode and are skipped if the folder is missing.

| Test | What it checks |
|---|---|
| `hash` | MD4 and SHA-1 on known values (RFC 1320, FIPS 180); `Format` with `%I64u`, `%lX` |
| `ed2k_file_carletto` | the ed2k hash of one of your 395 MB files equal to the one from eMule Windows |
| `io` | `CFile` (also beyond 4 GB), exceptions, protocol strings (UTF-8, BOM, cp1252), `UInt128` |
| `met` | validators: every truncated prefix is not healthy, extra bytes, random bits; safe writing with a crash at 5 points; recovery |
| `met_profilo_reale` | all 21 files of your profile are healthy: 8 configuration files, 1 `.part.met` in Temp and 12 backups |
| `profile` | `IniFile` (byte-identical on your `preferences.ini`), `PathMap`, `ProfileLock` between processes, upper/lower case, cp1252 |
| `emuled_check_profilo_reale` | `emuled --check-profile` on your profile: exit code 0 |
| `daemon` | `emuled` on fake profiles: healthy, damaged with and without a copy, lock, guard, `--update-known`, Guardian |
| `known` | `known.met` read again and rewritten identical, ed2k rule on all 1.629 records with parts, identical `clients.met`, hashes at part boundaries, `HashPool`, scanner |
| `known_hash_reali` | 36 of your files recomputed: hash and part hashes equal to eMule Windows (2,19 GB in ~2 s) |
| `compat` | `CString`, collections, `CTime`, `CEvent`, `CWinThread`, `CFileFind` |
| `shared_index` | flow new file → hash → `known.met`; modified, vanished, duplicate file; cancellation |
| `guardian` | states, backup by hash, a single notification, persistence, restore, date in the future |
| `crash` | **400 real kills** during writing: 0 damaged files; failed write; symlink |
| `net` | event loop and sockets on localhost: connection, 24 MB with blocked sending, deferred reading without idle loops, peer closing, refused connection, names, UDP, destruction inside callbacks, 300 connections without descriptor leaks |
| `packets` | header, compression, tags; the real `known.met` read again with the original `CTag` (242.146 tags) equal to our parser |
| `crypto` | RC4 on known vectors, MD5 of strings as UTF-16LE (compatible with Windows), `ipstr`, and the protocol obfuscation on localhost: 60 negotiations, clear data in both directions, no eMule marker on the wire, normal traffic unchanged, obfuscation requested/strict/disabled |
| `emsocket` | the ed2k packet socket with the real throttler: 100 packets in both directions, 300 KB at 150 KB/s in 1,95 s, download limited to 100 KB/s in 1,00 s, compressed packets, obfuscation with packets queued before the handshake, wrong headers, packets split byte by byte, closings and destruction with the throttler active; **long load**: 24 MB in clear and with obfuscation, with the send buffer at 16 KB (continuous partial sends), data and control packets mixed and replies in the opposite direction: everything must arrive whole and in order; **real capture**: two streams of a real session (tcpdump of 25/10/2026, in clear) in `tests/data/`: what the daemon sent to a peer in an upload (OP_SENDINGPART, OP_COMPRESSEDPART, ...) and what the peer sent to it (hello, part requests). Fed back into a socket, cut into segments of 1448, 17 and 60000 bytes, they must give the same 44 and 102 packets, with no errors |
| `credits` | `CClientCredits` read from `clients.met` with the public key length from 0 to 255: above 80 the key is considered absent (before, the `memcpy` went out of the buffers: found with ASan on the synthetic profile of `test_daemon`) |
| `throttler` | bandwidth limit (100,0 and 8,0 KB/s measured), fairness between slots, control queues, removals, busy sockets, clean stop |
| `daemon_api` (preferences/categories) | see the `daemon_api` row: GET/PATCH preferences (all or nothing, 422), categories (create/edit/delete, 0 protected, `Category.ini` written), `category` and `priority` of the downloads |
| `daemon_api` (completed, sources, blocks, servers, folders) | adding and editing servers (priority, static, name; rewriting of `server.met`; local addresses refused), folders (refusal of relative paths, with `..`, duplicates, completed=temporary, sharing of the completed folder; nothing changes if there is an error), a file put in a shared folder that shows up with its hash, blocks (20 000 000 bytes = 3 blocks, the last one of 544 000) and sources |
| `download` (list of completed) | after the real download the file leaves the queue but stays in `GET /downloads` as "complete", `DELETE` removes it from the list without deleting the file |
| `real_profile` | **the real eMule Windows profile** (copy in `/home/steveholmes/eMule`, read-only; it skips itself if missing): byte-identical `known.met`, recomputed ed2k hashes of 8 shared files (up to 400 MB, 41 blocks each) equal to the Windows ones, and the daemon that **resumes the downloads left by Windows** (`.part.met` 0xE0) with the same percentages (0,0 %, 46,7 %, 83,6 %). It works on copies |
| `indexed_load` | **Kad with index files already present** (`load_index.dat`, `key_index.dat`): loading writes no errors and leaves no leftovers. Regression test for the "Unable to replace Kad file" that appeared at every start |
| `eventlog` | **the event log**: numbering that continues after clearing, circular buffer (the oldest lines are lost and it says so), "after the last seen", level filter, debug only with `verbose`, source from the prefix (`[server]`, `[api]`), link to `Log()`/`LogError()`, 8 threads writing together (with TSan) |
| `fuzz_smoke` | **fixed fuzzing of the parsers** (see B7): every target of `tests/fuzz/` runs on its seed corpus (`tests/fuzz/corpus/`, including the inputs that crashed the code before the fixes) and on 300 deterministic mutations of each seed, in an empty network namespace. Skips itself (exit code 77) if the kernel does not allow the namespace (for example a container or Ubuntu 24.04 with AppArmor restricting unprivileged user namespaces) |
| `daemon_config` | **the start-up settings**: reading the file (comments, quotes, unknown keys and wrong lines refused with the line number), invalid ports, precedence command line > environment > file; then the real daemon started only with `EMULED_CONFIG` and `EMULED_API_PORT`, only with `--conf`, and without a config folder (clear error, code 2) |
| `api_http` | the API's HTTP/WebSocket server: token (and `?token=`), paths with parameters, 404/405/413/400, body, static files with attempts to escape the folder (`..`, `%2e%2e`), single page, WebSocket with the RFC 6455 key, masked and unmasked frames, ping/pong, messages from the server, two requests together |
| `daemon_api` | **`emuled --run` as a real process** (offline) and its API: status, adding by hash and by link, duplicate (409), errors, pause, resume, deletion (the `.part` disappears), servers, the token created with 0600 permissions, **every path of the OpenAPI description exists**, clean stop with SIGTERM (exit code 0, `.part.met` saved) |
| `prefs_paths` | a `preferences.ini` with Windows paths (`Y:\eMule\Incoming\`, `shareddir.dat`, skin): without a rule the current folder stays empty and the default folders are used; with a `--pathmap` rule the mapped path is used | **And `shareddir.dat` as eMule Windows writes it** (UTF-16 little endian with BOM, CRLF lines, with accents, Japanese and a surrogate pair): correct reading, saving again in UTF-16 (eMule Windows must be able to read it) and read back identical. Before, each line was read with a one-byte offset and **all the shared folders of the Windows profile were discarded**; and saving wrote UTF-8 after the UTF-16 BOM (a file unreadable for Windows). |
| `download` | **real downloads and uploads** against a fake peer on an address of the machine: a fake ed2k server gives the high ID, the source is added by hand, the fake peer answers like an eMule client (HELLO, name, status, slot, 10 KB blocks). The real `CPartFile` writes the `.part`, checks the MD4 and completes in `Incoming/`: file identical byte for byte. Then the file is shared by itself and a fake client connects to our port and downloads 100.000 bytes from the real `CUploadQueue` |

## B7. Quality tools: ASan, UBSan, TSan, fuzzing

> TSan on this kernel requires turning off address randomization: `setarch x86_64 -R ctest ...`. ASan: `ASAN_OPTIONS=detect_leaks=0`. Builds with a sanitizer leave the assertions (`ASSERT`) active, which in the normal build (NDEBUG) are removed: this is why they find errors that the normal build does not see.

The eMule code is old (2002–2026), written in C++ with a lot of hand-managed memory. These tools do not look for logic errors: they look for *program safety errors*, the ones that sometimes "work by chance" and then one day break everything. They are enabled by recompiling with an option: the programs become 2 to 15 times slower but report the problem right away, with the exact spot.

| Tool | What it finds | Example | What it found in this project |
|---|---|---|---|
| **ASan** (*AddressSanitizer*) | writing or reading **out of the bounds** of an array, use of memory already freed, forgotten memory | a 16-byte buffer where 17 are written | the defect of `CString::Format` that lost the `%` character (the program crashed inside `vsnprintf`) |
| **UBSan** (*UndefinedBehaviorSanitizer*) | constructs whose result **the language does not define**: reading an integer from an odd address, overflow of signed integers, invalid shifts, null pointers | `*(uint16_t*)&buf[3]` | **12 unaligned reads in `SafeFile`** (on x86 they work, on ARM they do not): fixed with `memcpy`; another 40 in the inventory `docs/unaligned-casts.txt` |
| **TSan** (*ThreadSanitizer*) | **races between threads**: two threads touching the same data without coordinating | a counter increased by two threads | in `CEMSocket` and in the encryption: **25 warnings** on the first run, all from eMule's original design (the throttler sends while the main thread negotiates the encryption and queues packets, with flags and pointers shared without a lock): now atomic flags, `sendLocker` in the queue reads, a destruction contract; 0 warnings. Before that, **three races** in the throttler: a non-atomic stop variable, the length of a list read without a lock (a defect of eMule's original code) and a `CEvent` destroyed while another thread was notifying (a defect of our compat layer). All fixed; now 0 warnings on throttler, network, `HashPool`, `SharedIndex`, `CEvent`, `CWinThread` |

How to use them:

```
cmake -S . -B build-asan -G Ninja -DEMULE_SANITIZE=address,undefined -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan && build-asan/test_met
cmake -S . -B build-tsan -G Ninja -DEMULE_SANITIZE=thread -DCMAKE_BUILD_TYPE=Debug
setarch "$(uname -m)" -R build-tsan/test_known      # on this kernel TSan requires ASLR turned off
```

### Fuzzing the parsers (`tests/fuzz/`)

**Why.** Everything a stranger can send is parsed by old code that trusts the lengths written in the data: Kad and ed2k packets (UDP and TCP), server lists, `nodes.dat`, `ed2k://` links, HTTP requests of the API. A fuzzer builds inputs by itself, looks at which lines of the program each one runs, and keeps mutating the ones that reach new lines. With ASan+UBSan active, any out-of-bounds access, use after free or undefined construct stops the run and saves the exact input.

**Targets** (one program each, `fuzz_<name>`):

| Target | What it feeds | Entry point |
|---|---|---|
| `packet` | eDonkey frames (also compressed) and tags | `Packet`, `UnPackPacket`, `CTag` |
| `ed2k_link` | `ed2k://` links | `CED2KLink::CreateLinkFromUrl` |
| `server_tcp` | the stream of an ed2k server | `CServerSocket::PacketReceived` |
| `server_udp` | datagrams from a server (plain and obfuscated) | `CUDPSocket::OnReceive` through a real socket |
| `client_udp` | datagrams from clients and Kad nodes (plain, packed, obfuscated) | `CClientUDPSocket::OnReceive` through a real socket |
| `kad_udp` | Kad packets after decryption | `CKademlia::ProcessPacket` (the listener's dispatcher) |
| `client_tcp` | the stream of a peer that has just connected | `CClientReqSocket::PacketReceived` |
| `met` | the nine `.met`/`.dat` validators of `core/met` | `Validate*` |
| `server_met`, `nodes_dat`, `part_met` | the files a downloaded list, a Kad bootstrap or a damaged Temp folder can hold | `CServerList::AddServerMetToList`, `CRoutingZone::ReadFile`, `CPartFile::LoadPartFile` |
| `http` | raw bytes on the API port: HTTP/1.1, WebSocket, templates, static files, JSON bodies of the real routes | `HttpServer` with `RegisterRoutes`, over loopback |

Not covered: the reassembly of the TCP stream into packets inside `CEMSocket` (it needs a connected socket pair; the `emsocket` test covers it), and the HTTP *client* that downloads lists from the web (`CHttpClientReqSocket`, `HttpFetch`).

**No network, by construction.** A fuzzed packet can make the code connect to a "client", answer a Kad node or ask a server. Every fuzz program first moves itself into an **empty network namespace** (`unshare(CLONE_NEWUSER | CLONE_NEWNET)`: only a loopback and the fake address `10.200.0.1`) and **refuses to run** if the kernel does not allow it. Nothing can leave the machine. The API target replaces the two routes that act on your file system (`/fs/mkdir`, `PUT /directories`) with a refusal.

**Running them** (clang is needed for libFuzzer):

```
cmake -S . -B build-fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DEMULE_SANITIZE=address,undefined -DEMULED_FUZZ=ON
ninja -C build-fuzz fuzz_kad_udp                       # or any fuzz_<target>
mkdir -p /tmp/work/kad_udp
ASAN_OPTIONS=detect_leaks=0 build-fuzz/fuzz_kad_udp /tmp/work/kad_udp tests/fuzz/corpus/kad_udp \
      -max_total_time=300 -detect_leaks=0 -rss_limit_mb=2048 -artifact_prefix=/tmp/work/
```

The first folder is the working corpus (the fuzzer writes what it finds there), the second one holds the seeds of the project (read only). On a crash the input is saved as `/tmp/work/crash-<hash>`; replay it with `build-fuzz/fuzz_kad_udp /tmp/work/crash-<hash>`. After fixing the bug copy the file into `tests/fuzz/corpus/<target>/regress-<what>` so that `fuzz_smoke` keeps checking it. `EMULED_FUZZ=ON` also turns undefined behaviour into a stop (`-fno-sanitize-recover`).

`tests/fuzz/make_corpus.py` rebuilds the hand-made seeds (one well-formed message for every opcode that matters). Without clang the **smoke test** still runs in every `ctest` (`ctest -R fuzz_smoke`); `build/test_fuzz_smoke tests/fuzz/corpus 3000` runs 3000 mutations per seed.

**What it found** (each input is kept in the corpus as `regress-*`, so `fuzz_smoke` checks it at every `ctest`):

| Where | Defect | Why it mattered |
|---|---|---|
| `CUDPSocket::OnReceive` (server UDP) | a datagram of 1 byte (`0xE3`) made the payload size `-1` (4 GB): read past a 5000-byte stack buffer | anyone could crash the daemon with a single UDP packet (the original eMule has the same flaw) |
| `CClientReqSocket::ProcessPacket`, `OP_END_OF_DOWNLOAD` | a packet shorter than 16 bytes made `CheckFailedFileIdReqs` read 16 bytes of hash past it (heap over-read) | a connected peer could make the daemon read outside a heap block (an ASan stop; original flaw) |
| `CEmuleDlgStub::AddServerMessageLine` (`core/Emule.h`) | the text a server sends (`OP_SERVERMESSAGE`) was used as a `printf` format: `%s`, `%n`… in it were interpreted | format-string defect reachable from any ed2k server; now the text is logged as text |
| `CPartFile::LoadPartFile` | `FT_AICH_HASH` tag that is not a string: its integer value was used as a pointer | crash when loading a damaged or foreign `.part.met` |
| `CPartFile::LoadPartFile` | a file size far above the 256 GB limit made it allocate the part tables (hundreds of GB) before the limit check | the daemon dies when it finds such a `.part.met` |
| `CRing::SetCapacity`, `CClientUDPSocket::SendControlData` | pointer `dst - 1` of an empty ring; `memcpy(…, nullptr, 0)` of an empty packet | undefined behaviour, no visible effect on x86 |
| `core/search/Parser.y`, `Scanner.l` | `CString` objects passed through `...` to the error function (the `%s` printed garbage) | wrong message for a search syntax error |
| `compat/win_types.h`, `Opcodes.h`, … | MSVC literal suffixes (`9728000ui64`) that clang does not accept | needed to build with clang at all |

**Known limit, not fixed.** `CServerConnect::ConnectionEstablished` deletes the socket (`DestroySocket(sender)`) when the connection is no longer in the "connecting" state, **from inside** the handler that called it (`OP_IDCHANGE` in `CServerSocket::ProcessPacket`), and the handler goes on using the deleted socket. In practice it needs a server socket that was already dropped to still receive a login answer while another server connected first; it is the original eMule design. The `server_tcp` target keeps the connection in the "connecting" state so it does not stop on this; fixing it means deferring the deletion (as `Safe_Delete` does for client sockets).

ASan+UBSan go together; **TSan goes alone** (it cannot be combined with ASan). Other quality tools: the compiler's `-Wformat` warning (it found the `%lX` defect with `long` of 32 or 64 bits), `-Wall`, **fuzzing** (above), and the **golden tests** against eMule Windows.

## B8. The libraries

| Library | Version on the system | What it is for | Status |
|---|---|---|---|
| Crypto++ (`libcrypto++-dev`) | 8.9 | MD4, SHA-1; later RSA for the secure ident | used |
| zlib | 1.3 | packet compression, `GZipFile` | planned |
| mbedtls | 2.28 (the source targets 3.x) | web server TLS | open decision (#617) |
| miniupnpc | 2.2.6 | opening the router's ports (UPnP) | planned |
| libcurl | 8.5 | downloading the server list from an address (http/https, with verified certificate) | in use |
| libcurl | 8.5 | downloading `server.met`, `nodes.dat`, IP filter | planned |
| flex / bison | 2.6.4 / 3.8.2 | parser of the search expressions (`Scanner.l`, `Parser.y`) | planned |
| pthread (`Threads`) | from the system | threads | used |
| id3lib → TagLib | — | MP3 tags (MediaInfo) | optional |
| ResizableLib | — | layout of MFC windows | **not ported** |

Compiler: g++ 13.3, C++20. Build: CMake 3.28 + Ninja 1.11. List of system packages: `requirements-apt.txt`.

## B9. The project directories

| Folder | Content |
|---|---|
| `compat/` | the MFC/Windows → Linux layer (B3); `ci/` = generated symlinks |
| `core/` | ported eMule sources and new code (`met/`, `known/`, `profile/`, `guardian/`, `net/`, `util/`) and the temporary replacements of the global objects |
| `daemon/` | `emuled.cpp` |
| `web/` | **the web interface** (Vue 3 + Vite + TypeScript + Pinia + Tailwind + vue-i18n): `src/lib` (API client, WebSocket, formats), `src/stores` (state), `src/components`, `src/views` (common screens), `src/i18n` (Italian, English and 12 more languages: es, zh, hi, ar, fr, pt, ru, de, ja, ko, tr, id; Arabic is read from right to left) and `src/template.ts` (the **common kit**), `templates/<id>/` (the four built-in templates: console, dashboard, studio, mobile); `./demo.sh` shows it with sample data; see `web/README.md` |
| `tests/` | the 19 tests and the builders of test files |
| `tools/` | `fix_case.py` |
| `docs/` | this manual, the inventory of unaligned casts |
| `web/` | the web interface (empty until Phase 7) |
| `upstream/emule/0.72a-community/srchybrid/` (path in `EMULED_UPSTREAM`, CMakeLists.txt) | the original Windows source: **not modified** |

---

# Part C — How we work

**Bench** — The project register at `http://localhost:8201`. The **eMuleWeb** project (id 10) holds the roadmap: **phases** (Phase 0–9), **tasks** inside the phases, **notes** on the tasks. It is the source of truth; `ROADMAP.md` is the initial snapshot.

**Phases** — 0 Preliminaries · 1 Foundations · 2 I/O and persistence · 3 Network · 4 ed2k servers and Kademlia · 5 Transfers · 6 Daemon and API · 7 Web UI · 8 Extra features · 9 Quality and release.

**Rules we gave ourselves**
1. Every step, every pro and con, every change decided together is recorded on Bench.
2. The Windows source (`srchybrid/`) is never touched.
3. We write only what is needed: a compatibility method is added when a ported file uses it (or based on a survey of real usage).
4. Before writing a `.met` file we check that the original is healthy; the write is atomic; a damaged file is never deleted.
5. The user's data is read **read-only**; tests never write into their profile.
6. Every ported file passes the checklist: `static_assert` on structs with `pack`, unaligned casts done with `memcpy`, `%l` formats, tests with ASan+UBSan (and TSan if it has threads).
7. Non-obvious decisions are presented with pros and cons and closed with the user.

**Decisions taken** — network: core of `CAsyncSocketEx` rewritten on level-triggered `epoll` with manual re-arming, same interface · temporary stand-ins for `theApp`/`thePrefs` and similar · `AfxBeginThread` with MFC semantics (the thread destroys itself) · C++20 and a daemon with an API · internal text UTF-8 · frontend Vue 3 + TypeScript + Vite + Pinia · `CStringW` = `CString` · configurable local codepage (default cp1252) · a single `emuled` per profile (lock) · thread model: one logical thread and worker threads · UI cut-off with an `IUiEvents` interface · fixed-width types with `static_assert` · native record for `known.met` · `OtherFunctions` split by topic.

**Open** — mbedtls version (decision postponed to Phase 6).

**Documents planned on Bench** — *Manual Reference* (this manual, once the document type exists) and *APIDoc* (manual of the API calls, Phase 6).

**ed2k login probe** (`core/net/ServerProbe.*`, Phase 3) — connects to an ed2k server, sends `OP_LOGINREQUEST` (temporary user hash, no shared files), reads `OP_SERVERMESSAGE`, `OP_IDCHANGE` (assigned ID: HighID = reachable from outside, LowID = not), `OP_SERVERSTATUS` (users and files) and disconnects. It starts only on an explicit command: `emuled --probe-server HOST:PORT` or `emuled --probe-server-met server.met --probe-index N` (option `--probe-serverlist` to also ask for the list of other servers). It touches no profile. The automatic test uses a fake server on localhost. `core/known/ServerMetFile.*` reads `server.met` (IP, port, name, description).

**Kademlia, routing table** (`core/kademlia/`, Phase 4 step 1) — *Kademlia (Kad)* is eMule's distributed network that finds files and sources without servers. Every node has a 128-bit ID (`CUInt128`); the distance between two nodes is the XOR of their IDs. The *routing table* (`CRoutingZone`) is a binary tree: each leaf is a *bin* (`CRoutingBin`) that holds at most 10 contacts (`CContact`: ID, IP, ports, version, UDP key). It is loaded from `nodes.dat` and rewritten at shutdown with the safe write (temporary file, check, atomic rename, healthy `.bak`). `CPrefs` (`kademlia/Prefs.*`) holds the Kad ID, firewall state and statistics, and writes `preferencesKad.dat` the same way.
Parts still *temporary* (stubs, to be replaced with the ported original): `Kademlia.cpp` (only the instance and accessors), `Indexed.h`, `SearchManager.h`, `KademliaUDPListener.h`, `UDPFirewallTester.h`; in `Emule.h` the fake graphical objects (`emuledlg`, `kademliawnd`), `ipfilter` and `serverlist`. Test: `test_kad_routing` (copies the real `nodes.dat` into a temporary folder and checks reading, rewriting and rereading).
New porting rule: in Windows `long` is 32 bits, here 64: a literal `0ul` must be written `(ULONG)0` when the constructor wants a 32-bit `ULONG`.

**UDP obfuscation** (`core/EncryptedDatagramSocket.*`, Phase 4 step 2) — encrypts UDP packets (client, Kad, server) with RC4; the key is an MD5 of data known to both sides plus 2 random bytes written in clear in the header. Four keys: user hash + sender IP (ed2k client), receiver's Kad ID (Kad), receiver's UDP key (Kad), server base key. The client→server and server→client directions use different constants (0x6B and 0xA5). `Statistics.h` is temporary: only the encryption traffic counters (`theStats`). Test: `test_dgram_crypt` (round trip for all types, format checked byte by byte with a decryption written in the test, plain packets that pass through unchanged).

**Complete Kademlia** (`core/kademlia/`, Phase 4 step 3) — ported in full: `Kademlia` (start, `Process()` once per second), `KademliaUDPListener` (all Kad messages), `Search`/`SearchManager` (searches and publications), `Indexed` (index of what other nodes entrust to us; files `src_index.dat`, `key_index.dat`, `load_index.dat`, now written with a temporary file and atomic rename), `Entry`, `UDPFirewallTester`, `PacketTracking`, `io/` (binary reading and writing) and `ClientUDPSocket` (the UDP socket with obfuscation). The socket handles only Kad packets: UDP packets between ed2k clients (upload and download queue) are discarded until Phase 5 arrives.
- *Kad lowercase map*: Kad lowercases keywords with a fixed Unicode table, the same on all nodes (otherwise the word hashes would not match). It is the one from eMule for Windows (`res/WideCharLowerMap.bin`), embedded as a table of differences (`core/kademlia/io/KadLowerMap.inc`, 873 entries; regenerated with `tools/gen_kad_lowermap.py`).
- *Keywords in UTF-8*: the separators are ASCII, so they are searched by byte; the rule "3 characters and 3 bytes = extension" counts UTF-16 units like the original.
- *Bridge with the application* (`AppBridge.h`, temporary): files, download queue, search list, buddy and graphical windows are fakes that answer "nothing found". Kad takes part in the network (routing, node search, firewall) but neither publishes nor searches files until Phases 5 and 6 arrive.
- `util/FileTypes.*`: extension -> ed2k file type (audio, video, ...), table identical to the original (it travels over the network). `util/Hash`: now also `EncodeBase32`/`DecodeBase32`. `AICHHash.cpp`: only the functions of `CAICHHash` (the full AICH tree arrives with Phase 5).
- *Kad probe* (`core/net/KadProbe.*`): `emuled --probe-kad NODES.DAT [--probe-seconds N]`. Starts the real Kad stack on a COPY of nodes.dat in a temporary folder, random Kad ID, random high UDP port, and after N seconds reports contacts before/after, traffic and state. It sends UDP packets to real nodes: it starts only on an explicit command. Test: `test_kad_net` uses a fake Kad node on an address of the machine (no packet leaves): it decrypts with its own NodeID, answers, and checks the whole send and receive path.

**Search on the ed2k server** (`core/net/ServerProbe.*`) — `emuled --probe-server-met FILE --probe-index N --probe-search "words"` (or `--probe-server HOST:PORT`): after login it sends `OP_SEARCHREQUEST` and reads `OP_SEARCHRESULT`. The words are in implicit AND, in the prefix tree that eMule builds (`AND p1 AND p2 p3`, each word = `0x01` + length + UTF-8 text); for each file it reports name, size, sources known to the server (and complete ones), type, hash and one source. So far without quotes, OR/NOT and filters (type, size): they arrive with the porting of `Parser.y`/`Scanner.l`. The test uses a fake server and checks the bytes of the tree and the parsing of the results.

**Open ports, high ID and source search** — `--tcp-port N` (server probe): listens on that TCP port; the server connects to it, sends `OP_HELLO` (with a FIN right after: the answer cannot wait) and wants `OP_HELLOANSWER`; if it receives it, it assigns the **high ID** (the ID is the public IP). The real listening socket for clients belongs to Phase 5: here there is only a minimal listener (`MiniListener`) that answers `OP_HELLO`. `--udp-port N` (Kad probe): UDP port of your choice. `--probe-sources HASH` (server): `OP_GETSOURCES` (large files: `0` + 64-bit size), answer `OP_FOUNDSOURCES`. `--probe-kad NODES.DAT --probe-sources HASH --probe-kad-size BYTE`: source search on Kad (FILE type). In the original the FILE search asks the download queue for the file to read its size: the bridge (`AppBridge.h`) now has `SetProbeFile`, which registers a "paper" file. `EMULED_KAD_DEBUG=1` writes every Kad packet with timings to stderr.

**Search filters and parser** (`core/search/`, task 658) — the search request is built as in eMule. `Scanner.l` and `Parser.y` are the originals (only the messages changed, to English, and the flex ECHO); they are compiled at build time with the system `flex` and `bison` (`requirements-apt.txt`). *Flex* and *bison* generate a scanner (splits the text into pieces: words, operators, `@size`...) and a parser (puts them together into a tree). `SearchBuilder.cpp` contains `GetSearchPacket`, extracted from `SearchResultsWnd.cpp` (the window file): it writes the search tree in the protocol format. Errors: `SearchParseError` exception with the parser's message.
- *Syntax in the search field*: words in AND, `"exact phrase"`, `OR`, `NOT`, parentheses, `@size>10m`, `@type=video`, `@ext=mkv`, `@sources>5`, `@complete>3`, `@length>1:30`, `@bitrate>128`, `@codec=`, `@title=`, `@album=`, `@artist=`, or an `ed2k://|file|...|` link (becomes `ed2k::HASH`). Numeric suffixes: `k`, `m`, `g` = multiples of 1024.
- *Filters as in eMule's panel* (`SSearchParams`): extension, type (Audio, Video, Image, Doc, Pro, Arc, Iso), minimum and maximum size, minimum and complete sources, bitrate, length, codec, title, album, artist. In the probe: `--ext`, `--type`, `--min-size`, `--max-size`, `--min-sources`, `--min-complete`.
- *eMule optimisation*: if there are only words in AND and no filter in the text, the request contains ONE single string (`fight club ita`) and the server splits the words. With a filter, or with OR/NOT, the tree is sent (`AND "fight club" ext=mkv`). For Kad the chosen keyword is removed from the tree and may be replaced by a longer, hence rarer, one.
- *64-bit sizes*: in the packet only if the server declares it (`SRV_TCPFLG_LARGEFILES`).
- Test: `test_search` (bytes of the tree for words, extension, type, size, OR, errors, Kad keyword, ed2k link, 64 bit). Tested on the real server: 65 results without filters, 18 with `--ext mkv`, 7 with `@size>4g`, 6 with `--type Doc`.
- Not ported: `CED2KLink` (448 lines, uses wininet for links with sources). For search the hash is enough, extracted by hand in the scanner.

**Keyword search on Kad** — `emuled --probe-kad NODES.DAT --udp-port N --probe-search "expression" [--ext ...] [--type ...]`. Like eMule's `DoNewKadSearch`: the keyword is the first term (without quotes), the tree is built with `GetSearchPacket` and the chosen keyword is removed from the tree (the node already knows it: it is its index); then `PrepareFindKeywords`. Kad queries the nodes whose ID is close to the MD4 hash of the lowercase word (with the Kad map) and each one answers with the files it has indexed. The same file comes from several nodes: the output merges them by hash and says how many nodes it came from. The number of sources is the one declared by the publishers (`TAG_SOURCES`). The results go through the bridge (`CSearchList::KademliaSearchKeyword`, which now records them instead of discarding them). Test: `test_kad_net` (scenario 2): fake node with an ID very close to the hash of "fight", checks the `KADEMLIA2_SEARCH_KEY_REQ` packet and the reading of the answer.

**Kad UDP firewall** (`core/FwCheckClient.cpp`, `core/net/Hello.*`) — how Kad finds out whether the UDP port is reachable from outside: the node connects over **TCP** to an unknown contact, sends it `OP_HELLO` and then `OP_FWCHECKUDPREQ` (internal UDP port, external port, key); the contact answers with two **UDP** packets `KADEMLIA2_FIREWALLUDP`, one per port. If one arrives, the port is open; if the connection drops with no answer, it is closed (two failed attempts = "firewall"); if it cannot connect, the test is cancelled and does not count. `OP_HELLO` is mandatory: without it, the contact refuses every other packet from an unknown client (and closes at once: that is what happened on the first attempt). `FwCheckSocket` is the only piece of the TCP client that Kad needs; the complete client (`CUpDownClient`, `ClientReqSocket`) arrives with Phase 5. `emuled --probe-kad` now prints the outcome (`VERIFICATO: porta UDP aperta/chiusa` or `NON verificato`). Test: `test_fwcheck` (the fake contact receives OP_HELLO and then OP_FWCHECKUDPREQ with the right ports). Tested on the network with UDP port 55672: open.

**Server list** (`core/Server.*`, `core/ServerList.*`, `core/ServerStrings.h`, task 583) — `CServer` (one server: IP, port, name, users, files, flags, UDP key...) and `CServerList` (the list, `server.met`, static servers) are the ported originals. Only what touched Windows or the window was changed:
- *Saving*: `server.met` is composed in memory and written with the safe write (check, temporary file, atomic rename, `.bak` only if the original is healthy); `staticservers.dat` with a temporary file and rename. The original wrote `server.met.new` and replaced the old one without checking it.
- *Profile folder*: `thePrefs.SetConfigDir(...)` (set by the daemon with `--profile`); empty = no file read or written.
- *Graphical list*: `CServerListCtrl` is a fake in `Emule.h` but `AddServer`/`RemoveServer` act on the real list, as in the original (that function is what inserts the server). The code always works on `theApp.serverlist`, as in eMule.
- *Update from a web address* (`AutoUpdate`): not ported yet (needs libcurl, Phase 8).
- *Messages*: in English (`ServerStrings.h`, text from `emule.rc`; `ResString.h`: `GetResString(IDS_X)` returns the text).
- Random numbers: `util/Random.*` (one generator per thread, instead of `rand()` with `GetTickCount`).
- Bug found by ASan: `CServer::GetAddress()` returned a pointer to a temporary `CString` (`cond ? ipfull : m_strDynIP` converts `ipfull` to `CString`). Porting rule: in the `?:` operator the two branches must have the same type (cast to `LPCTSTR`).
- Test: `test_serverlist` (copy of the real `server.met`: 8 servers read, saved and reread with names included, `.bak`, one static server).

**Server connection** (`core/ServerConnect.*`, `core/ServerSocket.*`, `core/UDPSocket.*`, `core/search/SearchResult.*`, tasks 583 and 659) — the ported originals.
- `CServerConnect`: chooses the server, tries several connections at once, retries every 30 seconds, keeps the assigned ID, keep-alive. `CServerSocket` (a `CEMSocket`): login (`OP_LOGINREQUEST`), reads messages, `OP_IDCHANGE`, status, server identity, search results (`OP_SEARCHRESULT`), sources (`OP_FOUNDSOURCES`), server lists, callback requests. `CUDPSocket`: UDP socket towards the servers (status, global search, sources over UDP), with the servers' UDP encryption.
- *Changed from the original*: the retry timer is an event-loop timer (not a window `SetTimer`); name resolution of servers with a dynamic address no longer uses a hidden window with `WSAAsyncGetHostByName` but `EventLoop::ResolveAsync` (one thread; `alive_` avoids touching the socket if it has already been destroyed); `OP_CALLBACKREQUESTED` (the server asks us to contact a peer behind a firewall) is recorded and ignored: it needs the Phase 5 TCP client; the server messages go to the log (`[server] ...`).
- *Bridge*: the server's search results (TCP and UDP) are recorded by `CSearchList` (`serverHits`), read with `ParseSearchAnswer`/`ParseSearchFile` (a single piece of code, also used by the probe). `theApp` now builds a real `CServerConnect`: `thePrefs` and `theStats` are defined BEFORE `theApp` in `Emule.cpp` (within one file the globals are constructed in the written order; this was the crash of all tests).
- Test: `test_serverconnect`, against a fake ed2k server: login with the eMule marker hash, message in the log, high ID, status (users and files), search with results in the bridge, disconnection. ASan and TSan clean.
- Still to do for the search on all servers: the orchestration that in the original lives in the results window (`SearchResultsWnd`): after the search on the connected server, one `OP_GLOBSEARCHREQ` packet to one server at a time (a timer) with `GetNextSearchServer`.

**Search on all servers** (`core/search/GlobalSearch.*`, `core/net/ServerSearch.*`, `core/net/MiniListener.h`, task 659) — `emuled --search "expression" --search-server-met FILE (--search-server-index N | --search-server-address IP:PORT) [--search-all-servers] [--tcp-port P] [--ext ...] [--type ...] [--min-size ...]`. Without `--search-all-servers` it searches only on the server it connects to; with the option it is eMule's *global search*:
1. login and `OP_SEARCHREQUEST` to the connected server (TCP);
2. when it answers (or after 50 seconds) a 750 ms timer starts;
3. at each tick `OP_GLOBSEARCHREQ` (or `REQ2`/`REQ3`, depending on the capabilities the server declares: large files, extensions) is sent over UDP to the next server in the list (`GetNextSearchServer`, with the priorities), with the same search tree; the results come back as `OP_GLOBSEARCHRES` (one file per packet);
4. when the list is finished, a few seconds for late answers.
The orchestration is `GlobalSearch` (in the original it lives in the results window). *Difference from the original*: the original loop took at most N-1 steps; if the connected server happened to be among the first, skipping it used up a step and the LAST server was never queried. Here every position is visited once. The in-memory list is in random order (as in eMule: `AddServer` puts at random at the head or the tail), so the server to connect to is chosen by address or by index in the FILE. The output merges the files by hash, with the highest sources and how many servers it comes from. `--tcp-port` makes it listen for the high ID (`MiniListener`). The UDP socket towards the servers uses a random port: the answers come back through the router mapping created by the outgoing packet.
- Test: `test_globalsearch` (one fake TCP server and two UDP-only: the tree arrives identical to all, the results come back with the originating server), ASan and TSan clean. Lesson: the servers' UDP socket lives inside `theApp`, a global object that lives until exit: it must be destroyed before closing the event loop that hosts it (`RecreateUDPSocket()` with port 0).
- On the real network, 8 servers in the list, connected to one and UDP to the other 7: 95 answers, 70 distinct files (65 with the connected server alone).

**UPnP and port check** (`core/UPnPImpl.*`, `core/UPnPImplMiniLib.*`, `core/net/UpnpProbe.*`, tasks 587 and 588)
- *UPnP* is the protocol with which a program asks the router to open a port (a "mapping"), via SSDP announcements on the local network. `CUPnPImplMiniLib` is the ported original (it uses the `miniupnpc` library): it finds the router in a thread, opens the TCP and UDP ports (and the web one), renews them and closes them at exit. The "Windows service" implementation and the selector (`UPnPImplWinServ`, `UPnPImplWrapper`) are not needed on Linux. Changed: the result arrives through a function (`SetOnResult`) instead of a window message; `TerminateThread` does not exist in a safe form (the thread is left to finish); the system `miniupnpc` (API 17) does not give the WAN IP from `UPNP_GetValidIGD`, which is read afterwards with `UPNP_GetExternalIPAddress`.
- `emuled --upnp-discover`: READ-ONLY. It finds the router, reads its external IP and the mappings already present; it adds and removes nothing. The real opening of the ports (`StartDiscovery`) modifies the router and is not connected to any automatic command.
- `emuled --check-ports --probe-server-met FILE --probe-index N --probe-kad NODES.DAT --tcp-port P --udp-port Q`: checks the two ports from outside, using the means of the protocol. *TCP*: login to a server, listening on the port; if the server manages to connect (and receives the answer to `OP_HELLO`) it assigns the high ID, so the port is open. *UDP*: Kad UDP firewall test (two nodes send us a packet at the port). Result per port: open, closed or not verifiable. Exit code 0 only if both are open.

## B10. Porting rule for every file (checklist)

Every eMule file ported from `upstream/emule/0.72a-community/srchybrid/` to `core/` goes through these points. `srchybrid/` is never modified: it is imported with `tools/import_src.py` (removes the CRs and converts from cp1252 to UTF-8) and then the copy is fixed.

1. **Structs with `#pragma pack`** (those that end up on disk or on the network): `static_assert` on the size. Example: `Preferences_Ext_Struct` of `preferences.dat` must stay 61 bytes.
2. **Unaligned reads**: `memcpy`, never a pointer cast to `uint32*` on a byte buffer.
3. **Print formats**: Windows `%ls`/`%hs` are not valid on Linux. Our `CString` is UTF-8: use `%s`.
4. **Windows functions**: replaced with `compat/` (`win_files.h`, `win_iocp.h`...); `#ifdef` is not scattered through the files.
5. **Graphical interface**: calls to windows go to the no-ops of `GuiStubs.h`. They will become API events (Phase 6).
6. **Cut-off parts**: every function replaced by a stub carries a `LINUX:` comment saying why and where work resumes, and has an entry on Bench.
7. **Writing `.met`/`.dat` files**: always with `SafeWriteMet` (temporary file, `fsync`, rename). Never overwrite a profile under test: work on a copy (`ScopedProfile`).
8. **Tests**: ASan and UBSan on every file; if it has threads, TSan too.
9. **Manual**: every new acronym, class or command enters this manual in the same step.

**Search tabs and global search** (`core/api/Search.cpp`) — Every search in the interface is a *tab* tied to the searched word. The tab stays, with its results, until you click the X: even after restarting the daemon (the tabs are in the file `searches.json` in the configuration folder; it is our own file, eMule for Windows does not use it). One search runs at a time, but the other tabs stay available to browse. Where to search: *Global* = ed2k (all servers) **plus** Kad together, with the results merged by hash (it is enough that one of the two networks is connected); *Connected server*; *All servers*; *Kad*. API: `GET /searches`, `POST /searches/{id}/cancel` (stops), `DELETE /searches/{id}` (removes the tab).

**Real profile and "fake" profile** — Normally `emuled --run` uses the profile's identity (name, user hash, credits: how the ed2k network recognises you) and saves there the changes made from the interface and API (preferences, folders). With **`--fake-profile`** it instead uses a random hash and the name "eMuleLinux" and does **not** save the preferences: it is for tests and for copies of other eMules' profiles, because two clients with the same hash disturb each other. The ports passed with `--tcp-port`/`--udp-port` are never saved in the profile.

**UPnP, server list from an address and "Refresh" of searches** — *UPnP* (`core/net/Daemon.cpp`, `CUPnPImplMiniLib`): with the `upnp_enabled` preference the daemon asks the router to forward the TCP and UDP ports (at start, or right away when you turn it on from the Kad page or from Preferences, without a restart); it renews the mappings every 30 minutes and, with `upnp_close_on_exit`, removes them at shutdown. The state (`off`, `pending`, `forwarded`, `failed`, `offline`) is in `GET /status`, field `upnp`, and in the Log. *Server list from an address* (`core/api/ServerUpdate.cpp`, `core/net/HttpFetch.cpp`, libcurl): `POST /servers/update` downloads a `server.met` in a separate thread, merges it into the list in the main loop and remembers the address in `AC_ServerMetURLs.dat`; on the Servers page there is the field with the list of addresses already used. *Refresh*: `POST /searches/{id}/refresh` runs the same search again (word, where, and filters, which the tab also remembers after a restart) in the same tab.

**Interface: multiple choice, categories and delete confirmation** — *Search*: every result has a checkbox; with several files ticked a bar appears with the target category and "Download the N chosen files" (those already downloading cannot be chosen). *Transfers*: the **Category options** menu (like the one in eMule's category tabs) has three parts: move the selected files to a category (or to a new one, created there) — actions on all downloads in view (pause, resume, resume the next one, stop, cancel) and the category's priority — management (new, edit, delete the category in view). Every download deletion asks for confirmation **with the list of files** (name, size, percentage). *Kad*: the page also shows how many of your files have been announced to Kad and how many indexes (words, sources, comments) you host for others (`GET /kad`, fields `published` and `indexed`).

**Safe profile: start-up check, guardian and all-or-nothing writes** (Bench 626, 629, 630) — Before reading any file, the daemon checks the configuration files (`preferences.ini`, `known.met`, `server.met`, `clients.met`, `known2_64.met`, `cancelled.met`, `nodes.dat`, `preferences.dat`, `preferencesKad.dat`; `core/profile/ProfileCheck.*`, the same code as `--check-profile`). A **damaged** file with a healthy `.bak` comes back from the `.bak` (the damaged one stays as `<name>.corrupt`); a damaged file **without** a healthy copy is moved to `<name>.corrupt` and the daemon starts from scratch on that file instead of reading garbage. A file modified less than 10 seconds ago is not judged (it may be being written). Everything ends up in the Log with the prefix "Profile:"; healthy files make no noise. With `--fake-profile` nothing is touched. Then **the guardian** (`core/guardian/`): every 5 minutes and at shutdown it copies every healthy `.part.met` into `guardian/<hash>.met` ("guardian" folder next to the profile, or `--guardian-dir`); the copy is overwritten only if the same download has advanced and it is never deleted. If at start-up a `.part.met` is broken, it comes back from the most advanced copy between eMule's `.bak` and the guardian's one (tested: restored identical). Finally **the all-or-nothing writes** (`compat/CFile.h`): a `.met`, `.dat`, `.ini` or `.txt` file rewritten from scratch is written to a file beside it (`<name>.tmpPID_N`) and on close, even implicit, `fsync` and `rename` are done over the original: if the process dies or the power goes out halfway, either the old file or the whole new one remains, never a half one. It does not apply to appending writes (`known2_64.met`) nor to the data of the `.part` files.

**Downloads without sources: watchdog** (`core/net/SourceWatchdog.*`, Bench 677) — Every minute the daemon looks at the active downloads (not paused, not stopped). One that stays at **0 sources for 10 minutes** is picked up again at once: eMule's waits (15 minutes with the server, hours with Kad) are reset and the sources are requested again, then again every 10 minutes until they arrive. If at least one download stays at 0 sources for **30 minutes** and the server is connected, the daemon **also retries the connection to the servers** (another server has another memory of who has the file), but at most once an hour. The server accepts a group of requests every 5 minutes anyway: the requests are queued, not lost. Everything appears in the Log with the prefix "Sources:". The decision is tested with a fake clock (`sourcewatchdog`); the real test is seen from the Log after 10 minutes with a download without sources.

**Shared folders that update by themselves, SIGHUP and systemd service** (Bench 653, 595) — The daemon watches the shared folders and the completed-files folder with `inotify` (`core/net/DirWatcher.*`): when a file arrives, disappears or is moved, after **10 seconds of quiet** (a big copy makes many events) it rechecks the folders and computes the hash of the new files; in the Log "Shared: rechecking the folders (…)" appears. Changing the folders from the Preferences page also updates the watches. **`SIGHUP`** (`kill -HUP <pid>`, or `systemctl --user reload emuled`) does the same on command. The `auto_connect` preference (now saved in the profile, default yes) decides whether the daemon connects to a server by itself at start. To make it start with the session there is `packaging/emuled.service`, a systemd *user* service: the instructions are in the file (`systemctl --user enable --now emuled`; `loginctl enable-linger` to start even without logging in); with `Restart=on-failure` it restarts if it stops by itself and it gives 60 seconds for an orderly shutdown. When a newer `.deb`/`.rpm` is installed over an old one, the package script `packaging/postinst` runs `systemctl --user try-restart emuled` for each logged-in user (only if the service is running, never on a first install, and it never fails the installation).

**Idle CPU** (Bench 693) — The bandwidth limiter thread, when there was nothing to send, woke up every millisecond (as in eMule for Windows): about 1.6% of a core with the daemon idle. Now it waits up to 100 ms and wakes up at once when data arrives (the event is cleared before looking at the queues, so nothing is lost): 16 times less work at rest (from 16 to 1 CPU "ticks" in 10 seconds). Be careful how you read the CPU: the `%CPU` column of `ps` is the average since the process was born (it includes the hash computation at start); for the current usage you need `top`.

**API errors with a stable code** (Bench 689) — Every error now has `{"error": "text in English", "code": "code"}` (list in APIDOC). The code is derived from the **beginning of the English message** (`core/api/HttpServer.cpp`, `ErrorCode` table: a new message must either start with one of its prefixes or have no code of its own) or, if none matches, from the HTTP status. The interface translates the codes that have a translation (`apiErrors` in `it.ts`/`en.ts`); for the others it shows the daemon's text.

**Repopulating Kad** (Bench 692) — `POST /kad/bootstrap`: with `url` it downloads a `nodes.dat` (http/https) and adds its contacts to the table (starting Kad if needed); with `address` (ip:UDP port) it asks that node for the first contacts. On the Kad page there are the two fields.

**Blocks being downloaded now** (`GET /downloads/{hash}/blocks`, `core/api/Api.cpp` `BlocksJson`, panel in the *Progress* tab) — In eMule a file is divided into *blocks* (**chunks**, 9.28 MB: those of the map) and each block is downloaded in **pieces** of 180 KB (`EMBLOCKSIZE`) requested from the sources. For every source that is transferring, the daemon knows which pieces it has requested (`m_PendingBlocks_list`, only those whose request has already gone out) and how many bytes of them have arrived (`transferred`). The **"Downloading now"** panel lists the blocks that are arriving, with their percentage, the sources transferring them (name, program, speed) and the pieces on the way; on the **map** the blocks being downloaded have an orange frame (solid = **receiving**, light = **requested** and waiting). **Map legend** (all states, in all templates): *complete*; *complete, not verified* (light tint: the file has no hashset, so the block cannot be checked); *partial* (filled in proportion); *missing*; *rare* (yellow background: 1-2 sources, at risk); *without sources* (grey border); *to redownload* (red, corrupted); *receiving* and *requested*. The optional entries appear only if there are blocks in that state; hovering the mouse over a cell tells its state. For colours that are CSS variables, transparency is obtained with stacked layers and `opacity`, not with `/50` (Tailwind does not apply it to `var(--colour)`). It refreshes every 3 seconds together with the rest of the detail. *Here:* **[written; tested with API tests (empty list, 404) and in the browser with a simulated answer; not yet seen with real sources downloading]**.

**File details in tabs, with `mediainfo`** (Bench 755) — The download panel has eMule's tabs: **General** (block map, then the "General", "Transfers" and "Dates" boxes: part.met, ed2k hash and **AICH with its state**, size and space on disk, parts, useful sources, compression, corrupted, **recovered parts**, dates), **Sources**, **Content** and **eD2K link** (with the options IP as source and HTML tag). The data comes from `GET /downloads/{hash}/details`. The **Content** tab uses the external program `mediainfo` (`core/media/MediaProbe.*`, `core/api/Content.*`): it launches it without a shell, with a 10-second limit, and reads the JSON; it also works on an incomplete `.part` as long as the container header is there. If it is not installed the tab says so (`sudo apt install mediainfo`); the path can be given with the `mediainfo` key of `config.yaml` or with `EMULED_MEDIAINFO`. **Interface models (shells)** (Bench 683, 684, 685) — A *shell* is the frame of the interface (sidebar or top bar, search box, status bar); everything else (download states, detail panel, Search, Servers screens...) is common. Besides A *Console* there are B *Dashboard* (summary tiles and cards with a ring), C *Studio* (list and detail side by side, command box with Ctrl+K) and D *Mobile first* (big search box, rows with a ring, detail sheet from the bottom). For now it is chosen with `?skin=a|b|c|d` in the address; the colours are CSS tokens per model (`data-skin`). The selector in Preferences is task 686. *Here:* **[written, tested in the browser with the test daemon; not yet with real data in motion]**.

**Interface templates and the `template.yaml` manifest** (`core/templates/Manifest.*`, Bench 840, Phase 11) — A *template* is a folder with a small web application that talks only to the daemon's API; it is the way to change the look of the interface without touching the program (A, B, C and D will become the four built-in ones). The *manifest* is the `template.yaml` file that describes it, in **flat YAML** (`key: value` lines, comments with `#`, no lists or nesting, like `config.yaml`). Keys: `id` (2-32 characters: lowercase letters, digits, hyphen), `name`, `version` (1.2.3) — required; `description` (300 characters), `author`, `license`, `api` (version of the API required, default 1: the daemon refuses a number higher than its own), `entry` (entry page, default `index.html`), `preview` (image). `entry` and `preview` are safe relative paths: no `..`, no leading `/`, only letters, digits and `. _ - /`. Unknown keys are ignored; the file does not exceed 8 KB. The check returns **all** the errors together, in Italian and with the line number. Tested with `template_manifest` (test). **Template registry** (`core/templates/Registry.*`, Bench 841) — Looks for templates in three folders, in this order of importance: **system** `/usr/share/emuled/templates/` (the four built-in ones, installed by the package), **user** `~/.emuled/templates/`, **extra** (`templates_dir`, for development and Docker). Every subfolder with a `template.yaml` is a candidate. Rules: the folder name must equal the `id`; the built-in names (`console`, `dashboard`, `studio`, `mobile`) are **reserved** (fine from system or extra, never from the user folder); the same `id` in two folders: the more important one wins and the other stays in the *invalid* list; the entry page (`entry`) must exist and be a file inside the folder (a symbolic link that leaves the folder is not valid). Folders that start with a dot and loose files are ignored. Invalid templates **stay in the list with the reason** (so the interface can explain why they do not appear) and the daemon's Log has a summary ("Template: 6 loaded, 8 invalid") and a warning for each one. It can be reread as many times as you like. Tested with `template_registry` (test). **Installing the built-in templates** (`CMakeLists.txt`, `core/templates/Registry.*`, Bench 849; the real package is task 615) — `cmake --install build` installs the daemon in `<prefix>/bin` and the four built templates (`web/dist/templates/`, from `npm run build`) in **`<prefix>/share/emuled/templates/<id>/`**; with `-DCMAKE_INSTALL_PREFIX=/usr` (as packages do) it is `/usr/share/emuled/templates/`. The folder the daemon searches as "system" is decided **at compile time** from the same prefix (`EMULED_SYSTEM_TEMPLATES`), so installation and search coincide even with `/usr/local`. The installed files belong to the administrator and are read-only for the user: the built-in templates are not touched. The user folder **`~/.emuled/templates/`** is created by the daemon at first start (only if missing) with a `README.txt` that explains how to add a template; if it already exists it touches nothing. If there is no template the daemon starts anyway and `/t/<id>/` gives the help page. **Broken choice**: if the browser had chosen a template that no longer exists or is broken, the help page shows the reason and **removes the choice by itself** from the browser (only if the id has the form of a template id: no free text in the script); the next time the default opens. Tested: `cmake --install` with a fake `DESTDIR` (structure and permissions), `template_registry` (creation, README, existing folder not touched) and the daemon with a new `HOME` (folder and README created, line in the Log). Without `web/dist/templates` CMake warns and does not install the templates. *Here:* **[written and tested; only the .deb/AppImage/Flatpak/Docker of 615 are missing]**.

**Choosing the template in Preferences** (`web/src/stores/templates.ts`, `PrefsView.vue`, Bench 848) — In Preferences there is the **Look** section: the templates found (from `GET /api/v1/templates`) as cards with preview, name, version, author, description, the label *Built-in* / *Yours* / *Development* and "In use" for the one the page is opened from; "Use this one" switches to that template **staying on the same screen** (`/t/<id>/#/prefs`). The choice is remembered **only in the browser** (`localStorage`, key `emuled.template`); choosing *Console* removes the key (= no choice). For a template that is not built-in (the name is not reserved) a **warning** appears first ("a template is a program that runs in the page and can read the access code: only use templates you trust"). At the bottom, **Not loaded**: the templates that are in the folders but cannot be used, each with the reason. "Rescan folders" calls `POST /templates/rescan`. The address `/?template=console` remains the emergency exit. Tested with `npm run e2e:templates` (31 checks: choice, persistence, warning, cancel, emergency, broken templates, built-in name refused, working example template with the same token, template vanished from disk, previews, no CSP violation) and with 3 unit tests. *Here:* **[written and tested]**.

**Building the templates and the common kit** (`web/`, Bench 846) — Before, the page was *one* application that contained A, B, C and D and chose one; now every built-in template is a **separate application**: `web/templates/<id>/` contains its shell (`Shell.vue`), its Transfers screen, its colours (`tokens.css`), the entry point (`main.ts`, `index.html`), the manifest and the preview. The **common kit** (`web/src/`: data, screens, components, translations) is **included in every template at build time** (there is no shared code at runtime, so no SDK to keep stable). `npm run build` builds all the templates in `web/dist/templates/<id>/` (one `vite build` each, with `TEMPLATE=<id>`; Tailwind scans only the kit and the template being built); file addresses are **relative** (`base: './'`) because the template lives at `/t/<id>/`. The logo is imported as a module (no longer an absolute `/assets/...`). **The fonts belong to the template**: `main.ts` imports its `@fontsource` packages (Console: IBM Plex Sans; Dashboard: Manrope; Studio: Public Sans; Mobile first: DM Sans; all with IBM Plex Mono for fixed-width characters) and `tokens.css` gives the family to the variables `--font-sans` and `--font-mono`, which Tailwind reads; the `.woff2` files end up in `dist/templates/<id>/assets/`, so at runtime nothing is downloaded from the internet (the templates' security policy forbids it anyway). A third-party template can bring its own font the same way. `vitest.config.ts` is separate from `vite.config.ts` (which changes root per template). `?skin=` removed: the choice of the template will be in Preferences (task 848). **Warning**: `web/dist/` no longer contains `index.html` at its root: whoever used `web_dir: …/web/dist` switches to `templates_dir: …/web/dist/templates`. *Here:* **[written; tested with the tests (vue-tsc, vitest, full e2e against /t/console/); the rest of the templates at 847 and tasks 683-685]**.

**Template security** (Bench 851) — A template is code that runs in the page and sees the token. The daemon: serves only regular files inside the folder (at most **32 MB** each), never writes into the template folders, accepts a manifest of at most 8 KB and a preview of at most 2 MB, and puts on the template pages the **Content-Security-Policy** (CSP: a list that tells the browser where the page may load things from): network requests, scripts, styles, fonts, images and media only from the **same address** as the daemon (plus `data:` and `blob:`), no forms towards other sites, no framing (`frame-ancestors 'none'`), `Referrer-Policy: no-referrer`. Tested in the browser with a test template: the script written in the page runs, `fetch` and WebSocket towards the daemon pass, `fetch`, `<script>` and `<img>` towards another address are blocked (violations `connect-src`, `script-src-elem`, `img-src`). **Honest limit**: it does not stop a template from moving the page to another site; for this reason only templates you trust should be installed (Preferences will say so: task 848). The cost: templates cannot use fonts or scripts taken from the internet.

**Serving the templates and the entry page** (`core/api/HttpServer.*`, Bench 842) — `/t/<id>/…` serves the template's folder, without a token, with the same protection as static files (no `..`, no symbolic links that leave the folder: 403). `/` is a small **entry page** built into the daemon (it depends on no template, so it works even if they are all broken): it reads `localStorage` (`emuled.template`), and leads to `/t/<id>/` (with no choice, `console`), keeping the `#/…` anchor. `/?template=<id>` sets the choice; **`/?template=console` is the emergency address**. If the template is not there, a help page in plain HTML (without CSS: it must work even without templates) says where to put them and points to the emergency address; the id is protected against injected HTML. With `web_dir` set, `/` stays the web folder (a single interface, behaviour as before). **`templates_dir`** can be set in `config.yaml`, with `EMULED_TEMPLATES_DIR` or with `--templates-dir` (precedence: command line, environment, file). **`web_dir` does not change**: it serves a single folder at `/`, as before, and in that case there is neither the entry page nor the choice of template (a simplicity decision: whoever uses `web_dir` has chosen a single interface). Tested with `api_http` (test: ranges, exits from the folder, links, help page, launcher) and in the browser with a test daemon: no choice → console, choice remembered, broken template → help page, emergency and strange `?template=` → console. **Templates API** (Bench 843) — `GET /api/v1/templates` lists the templates found (also the invalid ones, with the reason) with `dirs` (where to look for them) and `api_version`; `POST /api/v1/templates/rescan` rereads the folders. The daemon passes them to the API with a "hook" (`DaemonHooks::templates`), as it already does for UPnP and Kad. Tested with `daemon_api` (user, extra, broken, reserved name, rereading after adding a template, 401 without token, lines in the Log). *Here:* **[written and tested]**.

**Streaming** (Bench 607) — The download panel has "Open in a player" and "Copy address". The daemon creates a **temporary link** (`POST …/stream`: valid for 6 hours, for a single file, without the Bearer token) and serves it with range requests (**HTTP Range**: the player asks "from byte A to byte B" and can jump forwards and backwards). "Open in a player" downloads an **`.m3u` playlist** (a text file with the address) that the system opens with the default player; "Copy address" is for pasting it into VLC (Open Network Stream) or mpv. For a download in progress only what is already on disk is served: if the player asks for a part not yet downloaded it receives **503** and tries again; the daemon turns on the *preview priority* (first and last part first). The code is in `core/api/HttpServer.*` (file answers, read in 256 KB chunks so as not to fill the memory) and in `core/api/Api.cpp` (ticket, range, bytes available with `IsCompleteBD`). There is no format conversion: the player must be able to read the file. *Here:* **[written, tested with tests; not yet with a real download]**.

The **Name** tab (`GET …/names`) lists the names with which the sources call the file (how many sources per name) and lets you **rename** the file (`PATCH …/{hash}` with `name`); a click on a name puts it in the field, "Clear" empties it. The **All comments** tab (`GET …/comments`) shows comments and ratings (from 1 *Fake* to 5 *Excellent*) that arrived from the sources (eD2K network) and the **notes** found on Kad: "Search on Kad" asks the nodes for them (`POST …/comments/search-kad`) and "Edit the spam filter" changes the `comment_filter` preference (words separated by `|`: a comment that contains one of them is not shown; it applies to comments that arrive from now on). **Green verification bar** (Bench 752): when the daemon is doing something on the file (verifying the hashes, copying it, decompressing it) the API sends `file_op` and `file_op_percent` and the Download list draws a green bar above the completion one. In the Download list the **Size** column has on its second line how much is **missing** (Bench 754).

**"Choose a folder" window** (`web/src/components/FolderPicker.vue`, `core/api/FsApi.cpp`, `GET /fs`, `POST /fs/mkdir`, Bench 874) — For Incoming, Temp and the shared folders, in the Wizard and in Preferences, next to the field there is "Browse…": it opens a window with the path as clickable "breadcrumbs", the subfolders (with "Parent folder"), the free space, the option "Show hidden" and "New folder". **It shows the folders of the computer where the daemon runs**, not those of the PC from which you view the page: with Docker they are those of the container and of the mounted volumes. It needs the token, sees only what the daemon's user can see and never shows files. For Incoming and Temp the "Choose this folder" button is active only where it is possible to write. *Here:* **[written; backend tested by the C++ tests (`daemon_api`: listing, hidden, `..`, errors, creation); window tested in the real browser: opened from the Wizard and from Preferences, breadcrumb path, creation of a new folder, choice that fills the field, cancel, no console errors]**.

**Restart from the interface and port test** (`POST /system/restart`, `GET /ports/test-url`, "The emuled service" section of Preferences, Bench 878) — **New ports only take effect after a restart** of emuled. Now one click is enough: in Preferences there is "Restart emuled" (with confirmation) and, when a change requires the restart, the "Restart now" button inside the notice; in the last step of the Wizard, if you changed the ports, "Finish and restart". The daemon saves everything and starts again by itself, **in the same process and with the same arguments** (`execv`), so it works with systemd, with Docker and from a terminal; the page waits for it to come back and reloads. Also in Preferences there is "Test the ports from the internet": it opens in a new tab `porttest.emule-project.net` (the same test as the old eMule), which tries to connect to your TCP and UDP ports **in use right now** (if you have just changed them, restart first); the daemon answers the test (`OP_PORTTEST`). With obfuscation on, the address contains your user hash. In the Wizard, if you type the port that emuled is already using, the check says "This is the port emuled is already using: fine": it is not a conflict, it is emuled itself listening there. *Here:* **[written; backend tested by the C++ tests (`daemon_api` for the test address); the real restart tested in a terminal and from the import; NOT tested: the button from the browser with a systemd service, the real test towards emule-project.net (external network: you do it)]**

**Initial language and old token.** The interface **always starts in English**, even with an Italian browser; the chosen language is remembered in the browser. If the browser held a token saved by a previous installation (same address), it is now forgotten **without showing "Invalid token"** (before, it appeared on the first-start page as a false error). In the Wizard's import step the folders found on the computer appear (also hidden ones like `~/.emuled.old/profile`) and the picker opens with hidden folders visible.

**Start-up banner and icons** (`core/net/Banner.*`, `core/net/BannerArt.inc`, `tools/gen_icons.py`) — When `emuled` starts from the command line it shows a header with the **emuled version**, the **eMule version it derives from** and the **project link** on GitHub. On a terminal with colours and UTF-8 it also shows the icon made of block characters and the stylised "emuled" lettering; elsewhere (a service's journal, Docker logs, a pipe, `TERM=dumb`, a non-UTF-8 locale) only two lines of plain text come out. `NO_COLOR=1` removes the colours. It does not appear with `--reset-user`. The main icon is `docs/images/emuled_icon.png` (1024x1024): `python3 tools/gen_icons.py` derives from it the templates' favicon (`web/public/emuled_favicon.png`, 64x64), the interface logo (`web/src/assets/emuled_logo_transparent.png`, 512x512) and the character icon (`BannerArt.inc`). The README screenshots (`docs/images/frontend-*.png`) and the previews of the 4 templates (`web/templates/*/preview.png`) are redone with `npm run screenshots` in `web/`: the script starts an **offline** test daemon with a fake profile and four sample downloads, photographs the templates in English and shuts it down (no personal data, no network). Then `npm run build` brings the new previews into `dist/`. *Here:* **[written; tested by the C++ test `banner` and in a real terminal (with and without UTF-8/colours); interface with the new logo seen in the browser in light and dark theme; screenshots and previews redone with the new logo and checked by eye]**

**Importing an existing eMule** (`core/profile/Import.*`, `core/api/ImportApi.cpp`, "Existing eMule" step of the Wizard, Bench 875) — If you already have eMule (usually on Windows), the Wizard offers, right after the welcome, to **bring your profile here**: the user hash (your identity on the network), the encryption key, the **credits** (`clients.met`: whoever gave you files is owed something in the queue), the known files (`known.met`, `known2_64.met`), the servers, the Kad nodes and the preferences. You give the configuration folder of the old eMule (the one with `preferences.ini`), which must be reachable from the computer where emuled runs (copied, shared or mounted). The program **only reads it, never modifies it**. Windows paths (`Z:\eMule\Incoming\`) do not exist on Linux: emuled looks above the given folder for where they really are (e.g. `Z:\eMule` = `/mnt/z/eMule`), shows you the rules and, for each folder, whether it found it; you can correct them by hand or with "Browse…". Folders without a rule stay as they are (you correct them later, at the Folders step). Then the files are prepared and **emuled restarts by itself**: at start-up it puts them in the right place before reading the profile (the ones replaced go to `pre-import-DATA/` in the profile folder), and the Wizard starts again with name, ports and limits already filled in from the old profile. The user name and the interface password stay the ones just created. The downloads in progress are **not** copied (`Temp/`: the imported Temp folder points to them where they are) and neither are the Kad indexes (they are rebuilt). *Warning:* if the old eMule is still running with the same identity and the same ports, close it first. It can only be done at the first start and only once. *Here:* **[written; tested by the C++ tests `import` (scan, rules, copy, UTF-16, replacement and backup, source intact) and `daemon_api`; the full round in the browser with a real restart: see the verification below]**

**First-start web Wizard** (`web/src/views/WizardView.vue`, `web/src/stores/setup.ts`, `GET /setup`, `POST /setup/done`, `GET /ports/check`, Bench 873) — After the first account is created the interface shows, full screen and before everything else, a procedure in 7 steps (8 if the import of an existing eMule is offered, see above): welcome, **name on the network** (`nick`), **TCP and UDP ports** (with the check that says free / already in use by emuled / taken by another program / not valid: it is a **warning that does not block**, because the ports do not prevent using the interface but downloading, and they can be corrected later; plus "Use the standard ports" 55662/55672 and UPnP), **obfuscation** yes/no (`crypt_supported` and `crypt_requested`), **speed limits** (kB/s or MB/s, 0 = unlimited), **folders** Incoming and Temp (absolute paths) and the "ENJOY" summary. It saves everything at the end with `PATCH /preferences` and `PUT /directories`; the ports take effect at restart and the Wizard says so. The "to be completed" state is in `emuled.setup` in the profile (`pending` after the first account, `done` at the end; a profile without the file does not show the Wizard). The daemon **connects by itself**, but without servers or Kad nodes it stays waiting: the final step points to the Servers page. *Here:* **[written; backend tested by the C++ tests (`daemon_api`); interface tested in the real browser with the test daemon: first account, 7 steps, entry into the interface, no Wizard after reloading, 2 MB/s = 2048 kB/s]**. *Not done:* importing an existing eMule profile (from Windows), Bench 875; applying the new ports requires restarting the daemon.

**Web interface account** (`core/api/Auth.*`, `core/api/AuthApi.cpp`, `emuled --reset-user`, Bench 872) — You log in with **user name and password**, no longer only with the token. The account is in `emuled.users` (profile folder, `0600`): name, a random 16-byte **salt**, the **PBKDF2-HMAC-SHA256** hash (200000 rounds) of the password and a **secret** that signs the sessions. The password cannot be recovered: it is reset with `emuled --reset-user`, which also changes the secret (open sessions lapse) and works with the daemon running, because the file is reread when it changes. The **session** is a token signed with HMAC (`s1.<name>|<expiry>.<signature>`, 30 days) that the browser keeps at the key `localStorage['emuled.token']` and that counts like the file's token (Bearer, or `?token=` for the WebSocket): sessions are not remembered on the server, so they survive a restart. The **first account** is created only with the *setup code* (the token in `emuled.token`, printed at start-up as long as there is no account), so whoever reaches the port without having it cannot take over the interface. Five wrong login attempts from the same address block it for one minute (`429`); the daemon does not say whether the name or the password is wrong. The file's token remains valid for scripts and tools. *Here:* **[written; tested with C++ tests (`auth`, `daemon_api`), vitest and in the real browser: first start, error on mismatching passwords, re-entry after logout, session that stays after reload; `--reset-user` tested with a simulated terminal]**. *Not tested:* Docker/systemd with the account, third-party templates with the new login.
