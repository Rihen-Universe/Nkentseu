#!/usr/bin/env bash
# Construit et lance le banc NKLogger — coût d'une ligne de journal, en ns.
#
# Pourquoi un script et pas `jenga test` : l'exécution des tests est désactivée
# par politique de workspace (`disableunittestexecution`), décision délibérée de
# Rodolf que `jenga test` annonce clairement. Le cadre `Unitest` des deux autres
# fichiers de ce dossier existe bien — il est fourni par JENGA, pas par ce dépôt.
# Ce banc est autonome pour rester lançable quelle que soit cette politique.
# Même modèle que Kernel/Runtime/NKXR/tests/.
#
# Prérequis : `jenga build --target NKLogger --config Release` (les .lib).
# Usage     : bash Kernel/System/NKLogger/tests/build_bench.sh
set -e

L=Build/Lib/Release-Windows
D="-DNKENTSEU_CORE_STATIC_LIB -DNKENTSEU_MATH_STATIC_LIB -DNKENTSEU_MEMORY_STATIC_LIB -DNKENTSEU_CONTAINERS_STATIC_LIB -DNKENTSEU_PLATFORM_STATIC_LIB -DNKENTSEU_LOGGER_STATIC_LIB -DNKENTSEU_THREADING_STATIC_LIB -DNKENTSEU_TIME_STATIC_LIB"
I="-IKernel/Foundation/NKCore/src -IKernel/Foundation/NKMath/src -IKernel/Foundation/NKMemory/src -IKernel/Foundation/NKContainers/src -IKernel/Foundation/NKPlatform/src -IKernel/System/NKLogger/src -IKernel/System/NKThreading/src -IKernel/System/NKTime/src"
LIBS="-Wl,--start-group $L/NKLogger.lib $L/NKTime.lib $L/NKThreading.lib $L/NKContainers.lib $L/NKMemory.lib $L/NKMath.lib $L/NKPlatform.lib $L/NKCore.lib -Wl,--end-group -luser32 -ladvapi32 -lwinmm"

mkdir -p /tmp/nklogbench
clang++ -std=c++20 -O2 $D $I Kernel/System/NKLogger/tests/benchmark_smoke.cpp $LIBS -o /tmp/nklogbench/bench_nklogger.exe
echo "BUILT -> /tmp/nklogbench/bench_nklogger.exe"
