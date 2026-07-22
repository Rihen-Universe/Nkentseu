#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MakeNkCodeDist.py — assemble une DISTRIBUTION AUTONOME de NKCode pour testeurs
externes (Phase 5 ROADMAP / Phase 12 « zéro-dépendance ») :

    dist/NKCode/
      NKCode.exe                      (build Release ou Debug)
      python312.dll, python3.dll,     (DLLs exigees AU DEMARRAGE : NKCode.exe
      vcruntime140*.dll                lie python312 -> loader Windows)
      tools/
        python-embed/                 (runtime CPython 3.12 embeddable complet)
        jenga-src/Jenga/              (sources Jenga, sans Docs/Exemples/tests)
        compilers/llvm-mingw/         (Clang autonome par defaut, telecharge)

Usage :
    python scripts/MakeNkCodeDist.py [--config Release] [--skip-compiler]
                                     [--jenga-repo D:/chemin/vers/Jenga] [--zip]

Le compilateur par defaut = llvm-mingw (clang+mingw autonome, aucun autre
prerequis) — correspond exactement a la preference par defaut de Jenga sur
Windows (`clang-mingw` en tete de liste dans Core/Builder._ResolveToolchain).
Telecharge une fois puis mis en cache dans .cache/ (pas re-telecharge).

PascalCase (convention Rihen). Zero dependance pip (stdlib uniquement).
"""

import argparse
import os
import shutil
import sys
import urllib.request
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LLVM_MINGW_URL = ("https://github.com/mstorsjo/llvm-mingw/releases/download/"
                  "20240619/llvm-mingw-20240619-ucrt-x86_64.zip")

# Exclusions pour l'arbre Jenga embarque (sources utiles uniquement).
JENGA_EXCLUDE_DIRS = {"Docs", "Exemples", "Unitest", "unused", "__pycache__",
                      ".git", ".github", "Captures", "Tools"}
JENGA_EXCLUDE_TOP = {"Tools"}  # Tools/Installer & co : packaging Jenga, pas le build

# Polices REELLEMENT chargees par NkAppFonts/NkFontPrefs (verifie par grep du
# code) — les 9 autres graisses NotoSansSC (11 Mo chacune !) + la variable
# (17 Mo) ne sont referencees nulle part : ~95 Mo economises.
KEEP_FONTS = {"NotoSans-Regular.ttf", "NotoSansSC-Regular.ttf",
              "NotoEmoji-Regular.ttf", "NotoSansCJKsc-Regular.otf",
              "CascadiaCode.ttf", "CascadiaMono.ttf"}

# llvm-mingw multi-cible : on ne compile QUE pour x86_64 -> les triplets
# i686/ARM (sysroots + wrappers bin/) representent plus de la moitie du
# dossier pour zero utilite ici.
COMPILER_EXCLUDE_DIRS = {"i686-w64-mingw32", "armv7-w64-mingw32",
                         "aarch64-w64-mingw32", "arm64ec-w64-mingw32"}
COMPILER_EXCLUDE_PREFIXES = ("i686-", "armv7-", "aarch64-", "arm64ec-")


def Log(msg):
    print(f"[dist] {msg}", flush=True)


def CopyTree(src: Path, dst: Path, exclude_dirs=None, file_filter=None):
    exclude_dirs = exclude_dirs or set()
    for root, dirs, files in os.walk(src):
        rel = Path(root).relative_to(src)
        dirs[:] = [d for d in dirs if d not in exclude_dirs]
        out = dst / rel
        out.mkdir(parents=True, exist_ok=True)
        for f in files:
            if f.endswith((".pyc", ".pyo")):
                continue
            if file_filter and not file_filter(rel, f):
                continue
            shutil.copy2(Path(root) / f, out / f)


def DownloadCompiler(cache: Path) -> Path:
    """Telecharge (avec cache) puis extrait llvm-mingw. Retourne le dossier racine."""
    cache.mkdir(parents=True, exist_ok=True)
    zip_path = cache / Path(LLVM_MINGW_URL).name
    if not zip_path.exists():
        Log(f"telechargement du compilateur par defaut : {LLVM_MINGW_URL}")
        tmp = zip_path.with_suffix(".part")
        urllib.request.urlretrieve(LLVM_MINGW_URL, tmp)
        tmp.rename(zip_path)
    else:
        Log(f"compilateur deja en cache : {zip_path.name}")
    marker = cache / (zip_path.stem + ".extracted")
    ext_dir = cache / zip_path.stem
    if not marker.exists():
        Log("extraction du compilateur...")
        with zipfile.ZipFile(zip_path) as z:
            z.extractall(cache)
        marker.write_text("ok", encoding="utf-8")
    # l'archive contient un dossier racine du meme nom que le zip
    return ext_dir


def Main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="Release", choices=["Release", "Debug"])
    ap.add_argument("--jenga-repo", default=os.environ.get(
        "NKCODE_JENGA_SRC", "D:/Projets/MacShared/Projets/Jenga"))
    ap.add_argument("--skip-compiler", action="store_true",
                    help="n'embarque pas Clang (distribution plus legere, "
                         "suppose un compilateur deja present chez le testeur)")
    ap.add_argument("--zip", action="store_true", help="zippe la distribution")
    ap.add_argument("--xz", action="store_true",
                    help="archive .tar.xz (LZMA, bien plus petite que zip)")
    ap.add_argument("--out", default=str(REPO / "dist"))
    args = ap.parse_args()

    exe = REPO / f"Build/Bin/{args.config}-Windows/NKCode/NKCode.exe"
    if not exe.exists():
        Log(f"ERREUR : {exe} introuvable — builder d'abord :")
        Log(f"  jenga build --target NKCode --config {args.config}")
        return 1
    jenga_repo = Path(args.jenga_repo)
    if not (jenga_repo / "Jenga" / "__init__.py").exists():
        Log(f"ERREUR : package Jenga introuvable dans {jenga_repo}")
        return 1
    pyembed = REPO / "Externals/Libs/PythonEmbed/runtime"
    if not pyembed.exists():
        Log(f"ERREUR : {pyembed} introuvable (PythonEmbed vendorise manquant)")
        return 1

    out = Path(args.out) / "NKCode"
    if out.exists():
        Log(f"nettoyage de {out}")
        shutil.rmtree(out)
    tools = out / "tools"
    tools.mkdir(parents=True)

    Log("copie de NKCode.exe")
    shutil.copy2(exe, out / "NKCode.exe")

    # Ressources de l'IDE (logos, icones SVG, polices, langues, icons.cfg) :
    # NkAppFonts/NkAppIcons cherchent notamment "data/..." RELATIF au dossier
    # de lancement -> un dossier data/ A COTE de l'exe suffit (double-clic =
    # CWD = dossier de l'exe).
    data = REPO / "Applications/NKCode/data"
    if data.exists():
        Log("copie des ressources (data/ : polices UTILES, textures, logos, langues)")

        def DataFilter(rel: Path, f: str) -> bool:
            # fonts/ : ne garde que les polices reellement chargees par le code.
            if rel.parts and rel.parts[0] == "fonts":
                return f in KEEP_FONTS
            return True

        CopyTree(data, out / "data", exclude_dirs={"__pycache__"}, file_filter=DataFilter)
    else:
        Log("ATTENTION : Applications/NKCode/data introuvable — dist sans ressources !")

    Log("copie du runtime Python embarque (tools/python-embed)")
    CopyTree(pyembed, tools / "python-embed")
    # DLLs exigees au DEMARRAGE (import table de NKCode.exe) -> a cote de l'exe.
    for dll in ("python312.dll", "python3.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
        shutil.copy2(pyembed / dll, out / dll)

    Log("copie des sources Jenga (tools/jenga-src/Jenga)")
    CopyTree(jenga_repo / "Jenga", tools / "jenga-src" / "Jenga",
             exclude_dirs=JENGA_EXCLUDE_DIRS)
    # VERSION du Jenga embarque (mises a jour independantes, Phase 13)
    ver = jenga_repo / "Jenga" / "_version.py"
    if ver.exists():
        shutil.copy2(ver, tools / "jenga-src" / "Jenga" / "_version.py")

    if not args.skip_compiler:
        comp = DownloadCompiler(REPO / ".cache")
        Log("copie du compilateur par defaut (tools/compilers/llvm-mingw, x86_64 uniquement)")

        def CompFilter(rel: Path, f: str) -> bool:
            # bin/ : retire les wrappers des cibles i686/ARM (on ne vise que x86_64).
            if rel.parts and rel.parts[0] == "bin" and f.startswith(COMPILER_EXCLUDE_PREFIXES):
                return False
            return True

        CopyTree(comp, tools / "compilers" / "llvm-mingw",
                 exclude_dirs=COMPILER_EXCLUDE_DIRS, file_filter=CompFilter)
    else:
        Log("compilateur SAUTE (--skip-compiler)")

    if args.zip:
        zpath = Path(args.out) / f"NKCode-{args.config}-win64"
        Log(f"zip -> {zpath}.zip")
        shutil.make_archive(str(zpath), "zip", Path(args.out), "NKCode")
    if args.xz:
        xpath = Path(args.out) / f"NKCode-{args.config}-win64"
        Log(f"tar.xz (LZMA, ~2x plus petit que zip ; Windows 11/7-Zip l'ouvrent) -> {xpath}.tar.xz")
        shutil.make_archive(str(xpath), "xztar", Path(args.out), "NKCode")

    Log(f"OK : {out}")
    Log("Test : lancer dist/NKCode/NKCode.exe sur une machine SANS Python ni compilateur.")
    return 0


if __name__ == "__main__":
    sys.exit(Main())
