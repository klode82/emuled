// LINUX: vedi FileTypes.h. Origine: srchybrid/OtherFunctions.cpp righe 1303-1760.
#include "StdAfx.h"
#include "util/FileTypes.h"
#include <cstdlib>
#include <cstring>

// equivalente di ::PathFindExtension (Windows): punta all'ultimo '.' del nome senza percorso, oppure alla fine
static LPCTSTR FindExtension(LPCTSTR pszPath)
{
	LPCTSTR pszDot = nullptr;
	for (LPCTSTR p = pszPath; *p; ++p) {
		if (*p == '/' || *p == '\\') pszDot = nullptr;
		else if (*p == '.') pszDot = p;
	}
	return pszDot ? pszDot : pszPath + strlen(pszPath);
}

static struct SED2KFileType
{
	LPCTSTR pszExt;
	EED2KFileType iFileType;
} g_aED2KFileTypes[] =
{
	{ _T(".669"),	ED2KFT_AUDIO },		// Unis Composer Music File
	{ _T(".aa"),	ED2KFT_AUDIO },		// Audible Audiobook File
	{ _T(".aac"),	ED2KFT_AUDIO },		// Advanced Audio Coding File
	{ _T(".aax"),	ED2KFT_AUDIO },		// Audible Enhanced Audiobook File
	{ _T(".ac3"),	ED2KFT_AUDIO },		// Audio Codec 3 File
	{ _T(".aif"),	ED2KFT_AUDIO },		// Audio Interchange File Format
	{ _T(".aifc"),	ED2KFT_AUDIO },		// Audio Interchange File Format
	{ _T(".aiff"),	ED2KFT_AUDIO },		// Audio Interchange File Format
	{ _T(".amf"),	ED2KFT_AUDIO },		// DSMI Advanced Module Format
	{ _T(".amr"),	ED2KFT_AUDIO },		// Adaptive Multi-Rate Codec File
	{ _T(".ams"),	ED2KFT_AUDIO },		// Extreme Tracker Module
	{ _T(".ape"),	ED2KFT_AUDIO },		// Monkey's Audio Lossless Audio File
	{ _T(".au"),	ED2KFT_AUDIO },		// Audio File (Sun, Unix)
	{ _T(".aud"),	ED2KFT_AUDIO },		// General Audio File
	{ _T(".audio"),	ED2KFT_AUDIO },		// General Audio File
	{ _T(".caf"),	ED2KFT_AUDIO },		// Apple Core Audio Format File
	{ _T(".cda"),	ED2KFT_AUDIO },		// CD Audio Track
	{ _T(".dbm"),	ED2KFT_AUDIO },		// DigiBooster 3
	{ _T(".dff"),	ED2KFT_AUDIO },		// DSDIFF Audio File (high-res)
	{ _T(".dmf"),	ED2KFT_AUDIO },		// Delusion Digital Music File
	{ _T(".dsf"),	ED2KFT_AUDIO },		// DSD Stream Audio File (high-res)
	{ _T(".dsm"),	ED2KFT_AUDIO },		// Digital Sound Module
	{ _T(".dts"),	ED2KFT_AUDIO },		// DTS Encoded Audio File
	{ _T(".far"),	ED2KFT_AUDIO },		// Farandole Composer Module
	{ _T(".flac"),	ED2KFT_AUDIO },		// Free Lossless Audio Codec File
	{ _T(".it"),	ED2KFT_AUDIO },		// Impulse Tracker Module
	{ _T(".kar"),	ED2KFT_AUDIO },		// Karaoke MIDI File
	{ _T(".m1a"),	ED2KFT_AUDIO },		// MPEG-1 Audio File
	{ _T(".m2a"),	ED2KFT_AUDIO },		// MPEG-2 Audio File
	{ _T(".m3u"),	ED2KFT_AUDIO },		// MP3 Playlist File
	{ _T(".m4a"),	ED2KFT_AUDIO },		// MPEG-4 Audio File
	{ _T(".m4b"),	ED2KFT_AUDIO },		// MPEG-4 Audiobook File (M4A audio with chapters)
	{ _T(".mdl"),	ED2KFT_AUDIO },		// DigiTrakker Module
	{ _T(".med"),	ED2KFT_AUDIO },		// Amiga MED Sound File
	{ _T(".mid"),	ED2KFT_AUDIO },		// MIDI File
	{ _T(".midi"),	ED2KFT_AUDIO },		// MIDI File
	{ _T(".mka"),	ED2KFT_AUDIO },		// Matroska Audio File
	{ _T(".mod"),	ED2KFT_AUDIO },		// Amiga Music Module File
	{ _T(".mol"),	ED2KFT_AUDIO },		// Module Playlist files
	{ _T(".mp1"),	ED2KFT_AUDIO },		// MPEG-1 Audio File
	{ _T(".mp2"),	ED2KFT_AUDIO },		// MPEG-2 Audio File
	{ _T(".mp3"),	ED2KFT_AUDIO },		// MPEG-1/2 Audio Layer 3 File
	{ _T(".mpa"),	ED2KFT_AUDIO },		// MPEG Audio File
	{ _T(".mpc"),	ED2KFT_AUDIO },		// Musepack Compressed Audio File
	{ _T(".mpp"),	ED2KFT_AUDIO },		// Musepack Audio File
	{ _T(".mtm"),	ED2KFT_AUDIO },		// MultiTracker Module
	{ _T(".nst"),	ED2KFT_AUDIO },		// Noise Tracker Music Module
	{ _T(".oga"),	ED2KFT_AUDIO },		// Ogg Audio File
	{ _T(".ogg"),	ED2KFT_AUDIO },		// Ogg Vorbis Compressed Audio File
	{ _T(".okt"),	ED2KFT_AUDIO },		// Oktalyzer Module (Amiga)
	{ _T(".opus"),	ED2KFT_AUDIO },		// Opus Audio File
	{ _T(".psm"),	ED2KFT_AUDIO },		// Protracker Studio Module
	{ _T(".ptm"),	ED2KFT_AUDIO },		// PolyTracker Module
	{ _T(".ra"),	ED2KFT_AUDIO },		// Real Audio File
	{ _T(".rmi"),	ED2KFT_AUDIO },		// MIDI File
	{ _T(".s3m"),	ED2KFT_AUDIO },		// Scream Tracker 3 Module
	{ _T(".snd"),	ED2KFT_AUDIO },		// Audio File (Sun, Unix)
	{ _T(".stm"),	ED2KFT_AUDIO },		// Scream Tracker 2 Module
	{ _T(".ult"),	ED2KFT_AUDIO },		// UltraTracker Music Module
	{ _T(".umx"),	ED2KFT_AUDIO },		// Unreal Music Package
	{ _T(".wav"),	ED2KFT_AUDIO },		// WAVE Audio File
	{ _T(".weba"),	ED2KFT_AUDIO },		// WebM Audio File
	{ _T(".wma"),	ED2KFT_AUDIO },		// Windows Media Audio File
	{ _T(".wow"),	ED2KFT_AUDIO },		// Grave Composer audio tracker
	{ _T(".wv"),	ED2KFT_AUDIO },		// WavPack Lossless Audio File
	{ _T(".xm"),	ED2KFT_AUDIO },		// Fasttracker 2 Extended Module

	{ _T(".3g2"),	ED2KFT_VIDEO },		// 3GPP Multimedia File
	{ _T(".3gp"),	ED2KFT_VIDEO },		// 3GPP Multimedia File
	{ _T(".3gp2"),	ED2KFT_VIDEO },		// 3GPP Multimedia File
	{ _T(".3gpp"),	ED2KFT_VIDEO },		// 3GPP Multimedia File
	{ _T(".amv"),	ED2KFT_VIDEO },		// Anime Music Video File
	{ _T(".asf"),	ED2KFT_VIDEO },		// Advanced Systems Format File
	{ _T(".avi"),	ED2KFT_VIDEO },		// Audio Video Interleave File
	{ _T(".bik"),	ED2KFT_VIDEO },		// BINK Video File
	{ _T(".divx"),	ED2KFT_VIDEO },		// DivX-Encoded Movie File
	{ _T(".dvr-ms"),ED2KFT_VIDEO },		// Microsoft Digital Video Recording
	{ _T(".f4v"),	ED2KFT_VIDEO },		// Flash MP4 Video File
	{ _T(".flc"),	ED2KFT_VIDEO },		// FLIC Video File
	{ _T(".fli"),	ED2KFT_VIDEO },		// FLIC Video File
	{ _T(".flic"),	ED2KFT_VIDEO },		// FLIC Video File
	{ _T(".flv"),	ED2KFT_VIDEO },		// Flash Video File
	{ _T(".h264"),	ED2KFT_VIDEO },		// H.264 Raw Video Stream
	{ _T(".h265"),	ED2KFT_VIDEO },		// H.265 Raw Video Stream
	{ _T(".hdmov"),	ED2KFT_VIDEO },		// High-Definition QuickTime Movie
	{ _T(".hevc"),	ED2KFT_VIDEO },		// HEVC Raw Video Stream
	{ _T(".ifo"),	ED2KFT_VIDEO },		// DVD-Video Disc Information File
	{ _T(".m1v"),	ED2KFT_VIDEO },		// MPEG-1 Video File
	{ _T(".m2t"),	ED2KFT_VIDEO },		// MPEG-2 Video Transport Stream
	{ _T(".m2ts"),	ED2KFT_VIDEO },		// MPEG-2 Video Transport Stream
	{ _T(".m2v"),	ED2KFT_VIDEO },		// MPEG-2 Video File
	{ _T(".m4v"),	ED2KFT_VIDEO },		// MPEG-4 Video File
	{ _T(".mkv"),	ED2KFT_VIDEO },		// Matroska Video File
	{ _T(".mov"),	ED2KFT_VIDEO },		// QuickTime Movie File
	{ _T(".movie"),	ED2KFT_VIDEO },		// QuickTime Movie File
	{ _T(".mp1v"),	ED2KFT_VIDEO },		// MPEG-1 Video File
	{ _T(".mp2v"),	ED2KFT_VIDEO },		// MPEG-2 Video File
	{ _T(".mp4"),	ED2KFT_VIDEO },		// MPEG-4 Video File
	{ _T(".mpe"),	ED2KFT_VIDEO },		// MPEG-1 Video File
	{ _T(".mpeg"),	ED2KFT_VIDEO },		// MPEG-1 or MPEG-2 Video File
	{ _T(".mpg"),	ED2KFT_VIDEO },		// MPEG-1 or MPEG-2 Video File
	{ _T(".mps"),	ED2KFT_VIDEO },		// MPEG-1 Video File
	{ _T(".mpv"),	ED2KFT_VIDEO },		// MPEG Video File
	{ _T(".mpv1"),	ED2KFT_VIDEO },		// MPEG-1 Video File
	{ _T(".mpv2"),	ED2KFT_VIDEO },		// MPEG-2 Video File
	{ _T(".mts"),	ED2KFT_VIDEO },		// AVCHD Video File
	{ _T(".mxf"),	ED2KFT_VIDEO },		// Material Exchange Format File
	{ _T(".ogm"),	ED2KFT_VIDEO },		// Ogg Media File
	{ _T(".ogv"),	ED2KFT_VIDEO },		// Ogg Theora Video File
	{ _T(".pva"),	ED2KFT_VIDEO },		// MPEG Video File
	{ _T(".qt"),	ED2KFT_VIDEO },		// QuickTime Movie
	{ _T(".ram"),	ED2KFT_VIDEO },		// Real Audio Media
	{ _T(".ratdvd"),ED2KFT_VIDEO },		// RatDVD Disk Image
	{ _T(".rm"),	ED2KFT_VIDEO },		// Real Media File
	{ _T(".rmm"),	ED2KFT_VIDEO },		// Real Media File
	{ _T(".rmvb"),	ED2KFT_VIDEO },		// Real Video Variable Bit Rate File
	{ _T(".rv"),	ED2KFT_VIDEO },		// Real Video File
	{ _T(".rv9"),	ED2KFT_VIDEO },		// Real Video File
	{ _T(".smil"),	ED2KFT_VIDEO },		// SMIL Presentation File
	{ _T(".smk"),	ED2KFT_VIDEO },		// Smacker Compressed Movie File
	{ _T(".swf"),	ED2KFT_VIDEO },		// Macromedia Flash Movie
	{ _T(".tp"),	ED2KFT_VIDEO },		// Video Transport Stream File
	{ _T(".ts"),	ED2KFT_VIDEO },		// Video Transport Stream File
	{ _T(".vid"),	ED2KFT_VIDEO },		// General Video File
	{ _T(".video"),	ED2KFT_VIDEO },		// General Video File
	{ _T(".vivo"),	ED2KFT_VIDEO },		// VivoActive Movies and Audio
	{ _T(".vob"),	ED2KFT_VIDEO },		// DVD Video Object File
	{ _T(".vp6"),	ED2KFT_VIDEO },		// TrueMotion VP6 Video File
	{ _T(".webm"),	ED2KFT_VIDEO },		// Web Media File
	{ _T(".wm"),	ED2KFT_VIDEO },		// Windows Media Video File
	{ _T(".wmv"),	ED2KFT_VIDEO },		// Windows Media Video File
	{ _T(".xvid"),	ED2KFT_VIDEO },		// Xvid-Encoded Video File

	{ _T(".apng"),	ED2KFT_IMAGE },		// Animated PNG File
	{ _T(".arw"),	ED2KFT_IMAGE },		// Sony Alpha RAW Image
	{ _T(".avif"),	ED2KFT_IMAGE },		// AV1 Image File
	{ _T(".bmp"),	ED2KFT_IMAGE },		// Bitmap Image File
	{ _T(".cr2"),	ED2KFT_IMAGE },		// Canon RAW 2 Image
	{ _T(".cr3"),	ED2KFT_IMAGE },		// Canon RAW 3 Image
	{ _T(".dcx"),	ED2KFT_IMAGE },		// Zsoft Multi-Page Paintbrush file
	{ _T(".dng"),	ED2KFT_IMAGE },		// Digital Negative RAW Image
	{ _T(".emf"),	ED2KFT_IMAGE },		// Enhanced Windows Metafile
	{ _T(".gif"),	ED2KFT_IMAGE },		// Graphical Interchange Format File
	{ _T(".heic"),	ED2KFT_IMAGE },		// High Efficiency Image File
	{ _T(".heif"),	ED2KFT_IMAGE },		// High Efficiency Image Format File
	{ _T(".ico"),	ED2KFT_IMAGE },		// Icon File
	{ _T(".j2k"),	ED2KFT_IMAGE },		// JPEG 2000 Code Stream
	{ _T(".jfif"),	ED2KFT_IMAGE },		// JPEG File Interchange Format
	{ _T(".jp2"),	ED2KFT_IMAGE },		// JPEG 2000 Image File
	{ _T(".jpe"),	ED2KFT_IMAGE },		// JPEG Image File
	{ _T(".jpeg"),	ED2KFT_IMAGE },		// JPEG Image File
	{ _T(".jpg"),	ED2KFT_IMAGE },		// JPEG Image File
	{ _T(".jxl"),	ED2KFT_IMAGE },		// JPEG XL Image File
	{ _T(".nef"),	ED2KFT_IMAGE },		// Nikon Electronic Format RAW Image
	{ _T(".orf"),	ED2KFT_IMAGE },		// Olympus RAW Image
	{ _T(".pbk"),	ED2KFT_IMAGE },		// Adobe Pixel Bender Kernel file
	{ _T(".pct"),	ED2KFT_IMAGE },		// PICT Picture File
	{ _T(".pcx"),	ED2KFT_IMAGE },		// Paintbrush Bitmap Image File
	{ _T(".pic"),	ED2KFT_IMAGE },		// PICT Picture File
	{ _T(".pict"),	ED2KFT_IMAGE },		// PICT Picture File
	{ _T(".png"),	ED2KFT_IMAGE },		// Portable Network Graphic
	{ _T(".psd"),	ED2KFT_IMAGE },		// Photoshop Document
	{ _T(".psp"),	ED2KFT_IMAGE },		// Paint Shop Pro Image File
	{ _T(".raf"),	ED2KFT_IMAGE },		// Fujifilm RAW Image
	{ _T(".rw2"),	ED2KFT_IMAGE },		// Panasonic RAW Image
	{ _T(".svg"),	ED2KFT_IMAGE },		// Scalable Vector Graphics File
	{ _T(".tga"),	ED2KFT_IMAGE },		// Targa Graphic
	{ _T(".tif"),	ED2KFT_IMAGE },		// Tagged Image File
	{ _T(".tiff"),	ED2KFT_IMAGE },		// Tagged Image File
	{ _T(".wbmp"),	ED2KFT_IMAGE },		// Wireless Application Protocol Bitmap Format
	{ _T(".webp"),	ED2KFT_IMAGE },		// Weppy Photo File
	{ _T(".wmf"),	ED2KFT_IMAGE },		// Windows Metafile
	{ _T(".wmp"),	ED2KFT_IMAGE },		// Windows Media Photo File
	{ _T(".xcf"),	ED2KFT_IMAGE },		// GIMP Image File
	{ _T(".xif"),	ED2KFT_IMAGE },		// ScanSoft Pagis Extended Image Format File
	{ _T(".xpm"),	ED2KFT_IMAGE },		// X-Windows Pixmap

	{ _T(".7z"),	ED2KFT_ARCHIVE },	// 7-Zip Compressed File
	{ _T(".ace"),	ED2KFT_ARCHIVE },	// WinAce Compressed File
	{ _T(".alz"),	ED2KFT_ARCHIVE },	// ALZip Archive
	{ _T(".arc"),	ED2KFT_ARCHIVE },	// Compressed File Archive
	{ _T(".arj"),	ED2KFT_ARCHIVE },	// ARJ Compressed File Archive
	{ _T(".bz2"),	ED2KFT_ARCHIVE },	// Bzip Compressed File
	{ _T(".cab"),	ED2KFT_ARCHIVE },	// Cabinet File
	{ _T(".cb7"),	ED2KFT_ARCHIVE },	// Comic Book 7-Zip Archive
	{ _T(".cba"),	ED2KFT_ARCHIVE },	// Comic Book ACE Archive
	{ _T(".cbr"),	ED2KFT_ARCHIVE },	// Comic Book RAR Archive
	{ _T(".cbt"),	ED2KFT_ARCHIVE },	// Comic Book Tarball
	{ _T(".cbz"),	ED2KFT_ARCHIVE },	// Comic Book ZIP Archive
	{ _T(".gz"),	ED2KFT_ARCHIVE },	// Gnu Zipped File
	{ _T(".hqx"),	ED2KFT_ARCHIVE },	// BinHex 4.0 Encoded File
	{ _T(".lha"),	ED2KFT_ARCHIVE },	// LHARC Compressed Archive
	{ _T(".lz"),	ED2KFT_ARCHIVE },	// Lzip Compressed File
	{ _T(".lz4"),	ED2KFT_ARCHIVE },	// LZ4 Compressed File
	{ _T(".lzh"),	ED2KFT_ARCHIVE },	// LZH Compressed File
	{ _T(".lzma"),	ED2KFT_ARCHIVE },	// LZMA Compressed File
	{ _T(".pak"),	ED2KFT_ARCHIVE },	// PAK (Packed) File
	{ _T(".par"),	ED2KFT_ARCHIVE },	// Parchive Index File
	{ _T(".par2"),	ED2KFT_ARCHIVE },	// Parchive 2 Index File
	{ _T(".rar"),	ED2KFT_ARCHIVE },	// WinRAR Compressed Archive
	{ _T(".sea"),	ED2KFT_ARCHIVE },	// Stuffit Self-Extracting Archive
	{ _T(".sit"),	ED2KFT_ARCHIVE },	// Stuffit Archive
	{ _T(".sitx"),	ED2KFT_ARCHIVE },	// Stuffit X Archive
	{ _T(".tar"),	ED2KFT_ARCHIVE },	// Consolidated Unix File Archive
	{ _T(".tbz2"),	ED2KFT_ARCHIVE },	// Tar BZip 2 Compressed File
	{ _T(".tgz"),	ED2KFT_ARCHIVE },	// Gzipped Tar File
	{ _T(".tlz"),	ED2KFT_ARCHIVE },	// Tar LZMA Compressed File
	{ _T(".txz"),	ED2KFT_ARCHIVE },	// Tar XZ Compressed File
	{ _T(".uc2"),	ED2KFT_ARCHIVE },	// UltraCompressor 2 Archive
	{ _T(".xz"),	ED2KFT_ARCHIVE },	// XZ Compressed File
	{ _T(".z"),		ED2KFT_ARCHIVE },	// Unix Compressed File
	{ _T(".zip"),	ED2KFT_ARCHIVE },	// Zipped File
	{ _T(".zoo"),	ED2KFT_ARCHIVE },	// Zoo Archive
	{ _T(".zst"),	ED2KFT_ARCHIVE },	// Zstandard Compressed File

	{ _T(".apk"),	ED2KFT_PROGRAM },	// Android Application Package
	{ _T(".app"),	ED2KFT_PROGRAM },	// macOS Application Bundle
	{ _T(".appimage"),ED2KFT_PROGRAM },	// Linux AppImage Application
	{ _T(".appx"),	ED2KFT_PROGRAM },	// Windows App Package
	{ _T(".bat"),	ED2KFT_PROGRAM },	// Batch File
	{ _T(".cmd"),	ED2KFT_PROGRAM },	// Command File
	{ _T(".com"),	ED2KFT_PROGRAM },	// Executable COM File
	{ _T(".cpl"),	ED2KFT_PROGRAM },	// Windows Control Panel Applet
	{ _T(".deb"),	ED2KFT_PROGRAM },	// Debian Software Package
	{ _T(".dll"),	ED2KFT_PROGRAM },	// Dynamic Link Library File
	{ _T(".exe"),	ED2KFT_PROGRAM },	// Executable File
	{ _T(".flatpak"), ED2KFT_PROGRAM },	// Flatpak Application Package
	{ _T(".hta"),	ED2KFT_PROGRAM },	// HTML Application
	{ _T(".jar"),	ED2KFT_PROGRAM },	// Java Archive (application/library)
	{ _T(".js"),	ED2KFT_PROGRAM },	// Java Script
	{ _T(".jse"),	ED2KFT_PROGRAM },	// Encoded Java Script
	{ _T(".msc"),	ED2KFT_PROGRAM },	// Microsoft Common Console File
	{ _T(".msi"),	ED2KFT_PROGRAM },	// Microsoft Installer File
	{ _T(".msix"),	ED2KFT_PROGRAM },	// Windows App Package
	{ _T(".ps1"),	ED2KFT_PROGRAM },	// PowerShell Script
	{ _T(".psd1"),	ED2KFT_PROGRAM },	// PowerShell Data File
	{ _T(".psm1"),	ED2KFT_PROGRAM },	// PowerShell Module File
	{ _T(".rpm"),	ED2KFT_PROGRAM },	// RPM Software Package
	{ _T(".scr"),	ED2KFT_PROGRAM },	// Windows Screensaver Executable
	{ _T(".sh"),	ED2KFT_PROGRAM },	// Unix Shell Script
	{ _T(".snap"),	ED2KFT_PROGRAM },	// Snap Application Package
	{ _T(".vbe"),	ED2KFT_PROGRAM },	// Encoded Visual Basic Script File
	{ _T(".vbs"),	ED2KFT_PROGRAM },	// Visual Basic Script File
	{ _T(".wsf"),	ED2KFT_PROGRAM },	// Windows Script File
	{ _T(".wsh"),	ED2KFT_PROGRAM },	// Windows Scripting Host File
	{ _T(".sys"),	ED2KFT_PROGRAM },	// Microsoft system file
	{ _T(".xpi"),	ED2KFT_PROGRAM },	// Mozilla eXtension Installer Package

	{ _T(".b5t"),	ED2KFT_CDIMAGE },	// BlindWrite 5 Disc Image
	{ _T(".b6t"),	ED2KFT_CDIMAGE },	// BlindWrite 6 Disc Image
	{ _T(".bin"),	ED2KFT_CDIMAGE },	// CD Image
	{ _T(".bwa"),	ED2KFT_CDIMAGE },	// BlindWrite Disk Information File
	{ _T(".bwi"),	ED2KFT_CDIMAGE },	// BlindWrite CD/DVD Disc Image
	{ _T(".bws"),	ED2KFT_CDIMAGE },	// BlindWrite Sub Code File
	{ _T(".bwt"),	ED2KFT_CDIMAGE },	// BlindWrite 4 Disk Image
	{ _T(".ccd"),	ED2KFT_CDIMAGE },	// CloneCD Disk Image
	{ _T(".cdi"),	ED2KFT_CDIMAGE },	// DiscJuggler Disc Image
	{ _T(".cue"),	ED2KFT_CDIMAGE },	// Cue Sheet File
	{ _T(".daa"),	ED2KFT_CDIMAGE },	// PowerISO Disc Image
	{ _T(".dmg"),	ED2KFT_CDIMAGE },	// Mac OS X Disk Image
	{ _T(".dmz"),	ED2KFT_CDIMAGE },	// Disk Masher Z?
	{ _T(".img"),	ED2KFT_CDIMAGE },	// Disk Image Data File
	{ _T(".iso"),	ED2KFT_CDIMAGE },	// Disc Image File
	{ _T(".isz"),	ED2KFT_CDIMAGE },	// Compressed ISO Disc Image
	{ _T(".mdf"),	ED2KFT_CDIMAGE },	// Media Disc Image File
	{ _T(".mds"),	ED2KFT_CDIMAGE },	// Media Descriptor File
	{ _T(".mdx"),	ED2KFT_CDIMAGE },	// DAEMON Tools Disc Image
	{ _T(".nra"),	ED2KFT_CDIMAGE },	// Nero Audio-CD Compilation
	{ _T(".nrg"),	ED2KFT_CDIMAGE },	// Nero CD/DVD Image File
	{ _T(".qcow2"),	ED2KFT_CDIMAGE },	// QEMU Copy-On-Write Disk Image
	{ _T(".sub"),	ED2KFT_CDIMAGE },	// Subtitle file
	{ _T(".toast"),	ED2KFT_CDIMAGE },	// Toast Disc Image
	{ _T(".uif"),	ED2KFT_CDIMAGE },	// MagicISO Disc Image
	{ _T(".vcd"),	ED2KFT_CDIMAGE },	// Virtual CD Disc Image
	{ _T(".vdi"),	ED2KFT_CDIMAGE },	// VirtualBox Virtual Disk Image
	{ _T(".vhd"),	ED2KFT_CDIMAGE },	// Virtual Hard Disk Image
	{ _T(".vhdx"),	ED2KFT_CDIMAGE },	// Hyper-V Virtual Hard Disk Image
	{ _T(".vmdk"),	ED2KFT_CDIMAGE },	// VMware Virtual Disk Image
	{ _T(".wim"),	ED2KFT_CDIMAGE },	// Windows Imaging Format File

	{ _T(".azw"),	ED2KFT_DOCUMENT },	// EBook File
	{ _T(".azw3"),	ED2KFT_DOCUMENT },	// Amazon Kindle KF8 E-Book
	{ _T(".chm"),	ED2KFT_DOCUMENT },	// Compiled HTML Help File
	{ _T(".css"),	ED2KFT_DOCUMENT },	// Cascading Style Sheet
	{ _T(".csv"),	ED2KFT_DOCUMENT },	// Comma-Separated Values File
	{ _T(".diz"),	ED2KFT_DOCUMENT },	// Description in Zip File
	{ _T(".djvu"),	ED2KFT_DOCUMENT },	// DjVu Document
	{ _T(".doc"),	ED2KFT_DOCUMENT },	// Microsoft Word Document
	{ _T(".docm"),	ED2KFT_DOCUMENT },	// Microsoft Word Macro-Enabled Document
	{ _T(".docx"),	ED2KFT_DOCUMENT },	// Microsoft Word Document (OOXML)
	{ _T(".dot"),	ED2KFT_DOCUMENT },	// Document Template File
	{ _T(".dotx"),	ED2KFT_DOCUMENT },	// Microsoft Word Template (OOXML)
	{ _T(".epub"),	ED2KFT_DOCUMENT },	// EBook File
	{ _T(".fb2"),	ED2KFT_DOCUMENT },	// FictionBook 2 E-Book
	{ _T(".hlp"),	ED2KFT_DOCUMENT },	// Help File
	{ _T(".htm"),	ED2KFT_DOCUMENT },	// HTML File
	{ _T(".html"),	ED2KFT_DOCUMENT },	// HTML File
	{ _T(".ini"),	ED2KFT_DOCUMENT },	// Initialisation file
	{ _T(".json"),	ED2KFT_DOCUMENT },	// JSON File
	{ _T(".key"),	ED2KFT_DOCUMENT },	// Apple Keynote Presentation
	{ _T(".kfx"),	ED2KFT_DOCUMENT },	// Amazon Kindle KFX E-Book
	{ _T(".markdown"),ED2KFT_DOCUMENT },// Markdown Document
	{ _T(".md"),	ED2KFT_DOCUMENT },	// Markdown Document
	{ _T(".mobi"),	ED2KFT_DOCUMENT },	// EBook File
	{ _T(".nfo"),	ED2KFT_DOCUMENT },	// Warez Information File
	{ _T(".numbers"),ED2KFT_DOCUMENT },	// Apple Numbers Spreadsheet
	{ _T(".odp"),	ED2KFT_DOCUMENT },	// OpenDocument Presentation
	{ _T(".ods"),	ED2KFT_DOCUMENT },	// OpenDocument Spreadsheet
	{ _T(".odt"),	ED2KFT_DOCUMENT },	// OpenDocument File
	{ _T(".otp"),	ED2KFT_DOCUMENT },	// OpenDocument Presentation Template
	{ _T(".ots"),	ED2KFT_DOCUMENT },	// OpenDocument Spreadsheet Template
	{ _T(".ott"),	ED2KFT_DOCUMENT },	// OpenDocument Template File
	{ _T(".oxps"),	ED2KFT_DOCUMENT },	// OpenXPS Document
	{ _T(".pages"),	ED2KFT_DOCUMENT },	// Apple Pages Document
	{ _T(".pdf"),	ED2KFT_DOCUMENT },	// Portable Document Format File
	{ _T(".potx"),	ED2KFT_DOCUMENT },	// PowerPoint Template (OOXML)
	{ _T(".pps"),	ED2KFT_DOCUMENT },	// PowerPoint Slide Show
	{ _T(".ppsx"),	ED2KFT_DOCUMENT },	// PowerPoint Slide Show (OOXML)
	{ _T(".ppt"),	ED2KFT_DOCUMENT },	// PowerPoint Presentation
	{ _T(".pptm"),	ED2KFT_DOCUMENT },	// PowerPoint Macro-Enabled Presentation
	{ _T(".pptx"),	ED2KFT_DOCUMENT },	// PowerPoint Presentation (OOXML)
	{ _T(".ps"),	ED2KFT_DOCUMENT },	// PostScript File
	{ _T(".rtf"),	ED2KFT_DOCUMENT },	// Rich Text Format File
	{ _T(".srt"),	ED2KFT_DOCUMENT },	// SubRip Text
	{ _T(".stc"),	ED2KFT_DOCUMENT },	// OpenOffice.org 1.0 Spreadsheet Template
	{ _T(".sti"),	ED2KFT_DOCUMENT },	// OpenOffice.org 1.0 Presentation Template
	{ _T(".stw"),	ED2KFT_DOCUMENT },	// OpenOffice.org 1.0 Document Template File
	{ _T(".sxc"),	ED2KFT_DOCUMENT },	// OpenOffice.org 1.0 Spreadsheet
	{ _T(".sxi"),	ED2KFT_DOCUMENT },	// OpenOffice.org 1.0 Presentation
	{ _T(".sxw"),	ED2KFT_DOCUMENT },	// OpenOffice.org 1.0 Document File
	{ _T(".tex"),	ED2KFT_DOCUMENT },	// LaTeX Source Document
	{ _T(".text"),	ED2KFT_DOCUMENT },	// General Text File
	{ _T(".txt"),	ED2KFT_DOCUMENT },	// Text File
	{ _T(".wri"),	ED2KFT_DOCUMENT },	// Windows Write Document
	{ _T(".xls"),	ED2KFT_DOCUMENT },	// Excel Spreadsheet
	{ _T(".xlsm"),	ED2KFT_DOCUMENT },	// Excel Macro-Enabled Spreadsheet
	{ _T(".xlsx"),	ED2KFT_DOCUMENT },	// Excel Spreadsheet (OOXML)
	{ _T(".xlt"),	ED2KFT_DOCUMENT },	// Excel Template
	{ _T(".xltx"),	ED2KFT_DOCUMENT },	// Excel Template (OOXML)
	{ _T(".xml"),	ED2KFT_DOCUMENT },	// XML File
	{ _T(".xps"),	ED2KFT_DOCUMENT },	// XML Paper Specification Document
	{ _T(".yaml"),	ED2KFT_DOCUMENT },	// YAML File
	{ _T(".yml"),	ED2KFT_DOCUMENT },	// YAML File

	{ _T(".emulecollection"), ED2KFT_EMULECOLLECTION }
};

static int CompareE2DKFileType(const void *p1, const void *p2) noexcept
{
	return _tcscmp(reinterpret_cast<const SED2KFileType*>(p1)->pszExt, reinterpret_cast<const SED2KFileType*>(p2)->pszExt);
}

EED2KFileType GetED2KFileTypeID(LPCTSTR pszFileName)
{
	LPCTSTR const pszExt = FindExtension(pszFileName);
	if (!pszExt[0] || !pszExt[1])
		return ED2KFT_ANY; //no extension
	CString strExt(pszExt);
	SED2KFileType ft;
	ft.pszExt = strExt.MakeLower();
	ft.iFileType = ED2KFT_ANY;
	const SED2KFileType *pFound = (SED2KFileType*)bsearch(&ft, g_aED2KFileTypes, _countof(g_aED2KFileTypes), sizeof g_aED2KFileTypes[0], CompareE2DKFileType);
	return pFound ? pFound->iFileType : ED2KFT_ANY;
}

// Returns the ed2k file type string ID which is to be used for publishing+searching
LPCTSTR GetED2KFileTypeSearchTerm(EED2KFileType iFileID)
{
	switch (iFileID) {
	case ED2KFT_AUDIO:
		return _T(ED2KFTSTR_AUDIO);
	case ED2KFT_VIDEO:
		return _T(ED2KFTSTR_VIDEO);
	case ED2KFT_IMAGE:
		return _T(ED2KFTSTR_IMAGE);
	// NOTE: Archives and CD-Images are published+searched with file type "Pro"
	// NOTE: If this gets changed, the function 'GetED2KFileTypeSearchID' also needs to get updated!
	case ED2KFT_ARCHIVE:
	case ED2KFT_CDIMAGE:
	case ED2KFT_PROGRAM:
		return _T(ED2KFTSTR_PROGRAM);
	case ED2KFT_DOCUMENT:
		return _T(ED2KFTSTR_DOCUMENT);
	case ED2KFT_EMULECOLLECTION:
		return _T(ED2KFTSTR_EMULECOLLECTION);
	// LINUX: gli altri valori non fanno nulla (elencati per far segnalare a -Wswitch i valori nuovi)
	case ED2KFT_ANY:
		break;
	}
	return _T(ED2KFTSTR_ANY);
}

// Returns the ed2k file type integer ID which is to be used for publishing+searching
EED2KFileType GetED2KFileTypeSearchID(EED2KFileType iFileID)
{
	switch (iFileID) {
	case ED2KFT_AUDIO:
	case ED2KFT_VIDEO:
	case ED2KFT_IMAGE:
	case ED2KFT_PROGRAM:
	case ED2KFT_DOCUMENT:
		return iFileID;
	// NOTE: Archives and CD-Images are published+searched with file type "Pro"
	// NOTE: If this gets changed, the function 'GetED2KFileTypeSearchTerm' also needs to get updated!
	case ED2KFT_ARCHIVE:
	case ED2KFT_CDIMAGE:
		return ED2KFT_PROGRAM;
	// LINUX: gli altri valori non fanno nulla (elencati per far segnalare a -Wswitch i valori nuovi)
	case ED2KFT_ANY:
	case ED2KFT_EMULECOLLECTION:
		break;
	}
	return ED2KFT_ANY;
}

// Returns a file type which is used eMule internally only, examining the extension of the given filename
CString GetFileTypeByName(LPCTSTR pszFileName)
{
	LPCTSTR p;
	switch (GetED2KFileTypeID(pszFileName)) {
	case ED2KFT_AUDIO:
		p = _T(ED2KFTSTR_AUDIO);
		break;
	case ED2KFT_VIDEO:
		p = _T(ED2KFTSTR_VIDEO);
		break;
	case ED2KFT_IMAGE:
		p = _T(ED2KFTSTR_IMAGE);
		break;
	case ED2KFT_DOCUMENT:
		p = _T(ED2KFTSTR_DOCUMENT);
		break;
	case ED2KFT_PROGRAM:
		p = _T(ED2KFTSTR_PROGRAM);
		break;
	case ED2KFT_ARCHIVE:
		p = _T(ED2KFTSTR_ARCHIVE);
		break;
	case ED2KFT_CDIMAGE:
		p = _T(ED2KFTSTR_CDIMAGE);
		break;
	case ED2KFT_EMULECOLLECTION:
		p = _T(ED2KFTSTR_EMULECOLLECTION);
		break;
	default:
		p = _T("");
	}
	return CString(p);
}
