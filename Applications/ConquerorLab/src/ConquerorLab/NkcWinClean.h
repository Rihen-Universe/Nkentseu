#pragma once
// =============================================================================
// NkcWinClean — desamorce les macros de <windows.h> avant d'inclure NKFileSystem.
//
// LE PIEGE, EN UNE PHRASE
// -----------------------
// `<windows.h>` transforme une trentaine de noms d'API en MACROS qui choisissent
// la variante ANSI ou Unicode (`#define GetCurrentDirectory GetCurrentDirectoryA`).
// Certaines n'ont meme pas de variante : `#define GetFreeSpace(w) (0x100000l)`.
// Des lors, une DECLARATION portant l'un de ces noms — `NkFileSystem::GetFreeSpace`,
// `NkFile::Delete` -> `DeleteFile`, `NkPath::GetCurrentDirectory` — est reecrite
// par le preprocesseur, et clang signale « expected member name or ';' » sur une
// ligne parfaitement correcte, tres loin de la vraie cause.
//
// L'ordre d'inclusion decide donc si le code compile, ce qui est exactement le
// genre de dependance invisible qui coute une demi-journee. Ce fichier la
// supprime : on l'inclut EN PREMIER, et le reste n'a plus a s'en soucier.
//
// C'est le meme geste que `NKPlatform/NkX11Clean.h` fait pour les macros de
// Xlib (`None`, `Bool`, `Status`) devant NKGui.
// =============================================================================

#if defined(_WIN32)

// Si <windows.h> n'est pas encore la, on le fait venir en version minimale :
// mieux vaut le maitriser ici que le subir depuis un en-tete tiers.
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>

// ---- systeme de fichiers ----------------------------------------------------
#	undef GetFreeSpace
#	undef GetDiskFreeSpace
#	undef GetCurrentDirectory
#	undef SetCurrentDirectory
#	undef CreateDirectory
#	undef RemoveDirectory
#	undef DeleteFile
#	undef CopyFile
#	undef MoveFile
#	undef CreateFile
#	undef GetFileAttributes
#	undef SetFileAttributes
#	undef GetFileSize
#	undef GetTempPath
#	undef GetTempFileName
#	undef FindFirstFile
#	undef FindNextFile
#	undef GetFullPathName
#	undef GetLongPathName
#	undef GetShortPathName
#	undef CreateSymbolicLink
#	undef GetVolumeInformation
#	undef GetDriveType
#	undef SearchPath

// ---- divers -----------------------------------------------------------------
#	undef GetModuleFileName
#	undef GetObject
#	undef GetMessage
#	undef LoadLibrary
#	undef GetEnvironmentVariable
#	undef SetEnvironmentVariable

#endif // _WIN32
