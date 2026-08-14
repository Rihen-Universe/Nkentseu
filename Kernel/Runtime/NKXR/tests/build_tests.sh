set -e
L=Build/Lib/Release-Windows
D="-DNKENTSEU_CORE_STATIC_LIB -DNKENTSEU_MATH_STATIC_LIB -DNKENTSEU_MEMORY_STATIC_LIB -DNKENTSEU_CONTAINERS_STATIC_LIB -DNKENTSEU_PLATFORM_STATIC_LIB -DNKENTSEU_LOGGER_STATIC_LIB -DNKENTSEU_THREADING_STATIC_LIB -DNKENTSEU_TIME_STATIC_LIB -DNKENTSEU_STREAM_STATIC_LIB -DNKENTSEU_FILESYSTEM_STATIC_LIB -DNKENTSEU_SERIALIZATION_STATIC_LIB -DNKENTSEU_REFLECTION_STATIC_LIB -DNKENTSEU_EVENT_STATIC_LIB -DNKENTSEU_WINDOW_STATIC_LIB -DNKENTSEU_XR_STATIC_LIB -DNKENTSEU_IMAGE_STATIC_LIB"
I="-IKernel/Foundation/NKCore/src -IKernel/Foundation/NKMath/src -IKernel/Foundation/NKMemory/src -IKernel/Foundation/NKContainers/src -IKernel/Foundation/NKPlatform/src -IKernel/System/NKLogger/src -IKernel/System/NKThreading/src -IKernel/System/NKTime/src -IKernel/System/NKStream/src -IKernel/System/NKFileSystem/src -IKernel/System/NKSerialization/src -IKernel/System/NKReflection/src -IKernel/Runtime/NKEvent/src -IKernel/Runtime/NKWindow/src -IKernel/Runtime/NKXR/src -IKernel/Runtime/NKImage/src"
LIBS="-Wl,--start-group $L/NKXR.lib $L/NKWindow.lib $L/NKEvent.lib $L/NKImage.lib $L/NKReflection.lib $L/NKSerialization.lib $L/NKFileSystem.lib $L/NKStream.lib $L/NKTime.lib $L/NKThreading.lib $L/NKLogger.lib $L/NKContainers.lib $L/NKMemory.lib $L/NKMath.lib $L/NKPlatform.lib $L/NKCore.lib -Wl,--end-group -luser32 -lgdi32 -lshell32 -lole32 -ladvapi32 -lshcore -ldinput8 -ldxguid -lwinmm -limm32 -lversion -lsetupapi -lcfgmgr32 -ldwmapi -luxtheme -lpropsys -lcomdlg32"
mkdir -p /tmp/nkxrtests
clang++ -std=c++20 -O2 $D $I Kernel/Runtime/NKXR/tests/test_ar.cpp $LIBS -o /tmp/nkxrtests/test_ar.exe
clang++ -std=c++20 -O2 $D $I Kernel/Runtime/NKXR/tests/test_xr.cpp $LIBS -o /tmp/nkxrtests/test_xr.exe
clang++ -std=c++20 -O2 $D $I Kernel/Runtime/NKXR/tests/test_ar_image.cpp $LIBS -o /tmp/nkxrtests/test_ar_image.exe
echo BUILT
