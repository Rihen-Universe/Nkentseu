#ifndef ASSIMP_REVISION_H_INC
#define ASSIMP_REVISION_H_INC

// #define GitVersion 0x@GIT_COMMIT_HASH@
// #define GitBranch "@GIT_BRANCH@"
#define GitVersion 0x65d366d
#define GitBranch "master"

//#define VER_MAJOR @ASSIMP_VERSION_MAJOR@
//#define VER_MINOR @ASSIMP_VERSION_MINOR@
//#define VER_PATCH @ASSIMP_VERSION_PATCH@
//#define VER_BUILD @ASSIMP_PACKAGE_VERSION@

#define VER_MAJOR 5
#define VER_MINOR 2
#define VER_PATCH 5
#define VER_BUILD 0

#define STR_HELP(x) #x
#define STR(x) STR_HELP(x)

#define VER_FILEVERSION             VER_MAJOR,VER_MINOR,VER_PATCH,VER_BUILD
#if GitVersion == 0
#define VER_FILEVERSION_STR         STR(VER_MAJOR) "." STR(VER_MINOR) "." STR(VER_PATCH) "." STR(VER_BUILD)
#else
#define VER_FILEVERSION_STR         STR(VER_MAJOR) "." STR(VER_MINOR) "." STR(VER_PATCH) "." STR(VER_BUILD) " (Commit <commit_hash>)"
#endif
#define VER_COPYRIGHT_STR           "\xA9 2006-2022"

#ifdef  NDEBUG
#define VER_ORIGINAL_FILENAME_STR   "Assimp.dll"
#else
#define VER_ORIGINAL_FILENAME_STR   "Assimp.dll"
#endif //  NDEBUG

#endif // ASSIMP_REVISION_H_INC
