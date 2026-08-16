# =============================================================================
# Banc NKXR — construit les trois auto-tests.
#
# ⚠️ DIALECTE : C++17, celui du depot (cppdialect("C++17"), 205 projets).
# Ce script a compile en C++20 jusqu'au 2026-08-17 (le nom du drapeau n'est pas
# ecrit en toutes lettres ici : un sed sur le drapeau reecrirait cette phrase et
# la documentation se mettrait a corroborer l'inverse de ce qui s'est passe --
# c'est arrive en ecrivant ce commentaire). Un banc qui n'est pas
# compile comme le code qu'il juge ne mesure pas le meme code -- c'est ce qui
# avait piege le temoin NkToWide (char16 = uint16 en C++17, char16_t en C++20).
#
# La bascule a ete MESUREE avant d'etre faite, pas supposee :
#     test_xr        C++20 : 66 OK, 0 ECHECS   |  C++17 : 66 OK, 0 ECHECS
#     test_ar        C++20 : 82 OK, 0 ECHECS   |  C++17 : 82 OK, 0 ECHECS
#     journaux horodatage retire : IDENTIQUES sur les trois binaires
# Donc : ZERO controle tombait. La dette etait un reglage de script, pas un
# probleme de portabilite -- et on le sait, au lieu de l'esperer.
#
# CE QUE CE SCRIPT CONSTRUIT — deux bancs et un instrument, pas trois tests :
#     test_xr.exe          AUTO-TEST, a un verdict.  66 controles.
#     test_ar.exe          AUTO-TEST, a un verdict.  82 controles.
#     outil_ar_image.exe   INSTRUMENT. AUCUN verdict : il rend 0 qu'il detecte
#                          cinq marqueurs ou zero, donc il ne peut pas echouer
#                          et ne compte dans AUCUN total. Renomme le 2026-08-17
#                          (il s'appelait test_ar_image) : son nom le faisait
#                          passer pour un test, et son code 1 quand on l'appelle
#                          sans image ressemblait alors a un echec.
#
# TOTAL REEL DES AUTO-TESTS : 66 + 82 = 148. La ROADMAP annoncait 66 jusqu'au
# 2026-08-17 : elle ne comptait qu'un banc sur deux.
# =============================================================================
set -e
L=Build/Lib/Release-Windows
D="-DNKENTSEU_CORE_STATIC_LIB -DNKENTSEU_MATH_STATIC_LIB -DNKENTSEU_MEMORY_STATIC_LIB -DNKENTSEU_CONTAINERS_STATIC_LIB -DNKENTSEU_PLATFORM_STATIC_LIB -DNKENTSEU_LOGGER_STATIC_LIB -DNKENTSEU_THREADING_STATIC_LIB -DNKENTSEU_TIME_STATIC_LIB -DNKENTSEU_STREAM_STATIC_LIB -DNKENTSEU_FILESYSTEM_STATIC_LIB -DNKENTSEU_SERIALIZATION_STATIC_LIB -DNKENTSEU_REFLECTION_STATIC_LIB -DNKENTSEU_EVENT_STATIC_LIB -DNKENTSEU_WINDOW_STATIC_LIB -DNKENTSEU_XR_STATIC_LIB -DNKENTSEU_IMAGE_STATIC_LIB"
I="-IKernel/Foundation/NKCore/src -IKernel/Foundation/NKMath/src -IKernel/Foundation/NKMemory/src -IKernel/Foundation/NKContainers/src -IKernel/Foundation/NKPlatform/src -IKernel/System/NKLogger/src -IKernel/System/NKThreading/src -IKernel/System/NKTime/src -IKernel/System/NKStream/src -IKernel/System/NKFileSystem/src -IKernel/System/NKSerialization/src -IKernel/System/NKReflection/src -IKernel/Runtime/NKEvent/src -IKernel/Runtime/NKWindow/src -IKernel/Runtime/NKXR/src -IKernel/Runtime/NKImage/src"
LIBS="-Wl,--start-group $L/NKXR.lib $L/NKWindow.lib $L/NKEvent.lib $L/NKImage.lib $L/NKReflection.lib $L/NKSerialization.lib $L/NKFileSystem.lib $L/NKStream.lib $L/NKTime.lib $L/NKThreading.lib $L/NKLogger.lib $L/NKContainers.lib $L/NKMemory.lib $L/NKMath.lib $L/NKPlatform.lib $L/NKCore.lib -Wl,--end-group -luser32 -lgdi32 -lshell32 -lole32 -ladvapi32 -lshcore -ldinput8 -ldxguid -lwinmm -limm32 -lversion -lsetupapi -lcfgmgr32 -ldwmapi -luxtheme -lpropsys -lcomdlg32"
mkdir -p /tmp/nkxrtests
clang++ -std=c++17 -O2 $D $I Kernel/Runtime/NKXR/tests/test_ar.cpp $LIBS -o /tmp/nkxrtests/test_ar.exe
clang++ -std=c++17 -O2 $D $I Kernel/Runtime/NKXR/tests/test_xr.cpp $LIBS -o /tmp/nkxrtests/test_xr.exe
clang++ -std=c++17 -O2 $D $I Kernel/Runtime/NKXR/tests/outil_ar_image.cpp $LIBS -o /tmp/nkxrtests/outil_ar_image.exe
echo BUILT
