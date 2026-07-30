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
# ⚠️ N'exclure QUE des dossiers que `Jenga/__init__.py` n'importe PAS. `Unitest`
# etait exclu a tort : `__init__.py` fait `from . import Unitest`, donc son
# absence faisait echouer TOUT `import Jenga` dans la distribution
# (« cannot import name 'Unitest' ... partially initialized module ») —
# l'interpreteur embarque comme le shim en ligne de commande. Il ne pese que
# 273 Ko : l'exclusion ne gagnait rien et cassait le paquet.
# Verifier avec : grep "^from \. import" Jenga/__init__.py
JENGA_EXCLUDE_DIRS = {"Docs", "Exemples", "unused", "__pycache__",
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


def FindIscc() -> Path | None:
    """Localise ISCC.exe (compilateur Inno Setup). Mêmes emplacements que la
    détection de Jenga (Commands/Package.py) : installation winget en portée
    utilisateur d'abord, puis installation MSI classique."""
    candidates = [
        Path(os.path.expandvars(r"%LOCALAPPDATA%\Programs\Inno Setup 6")),
        Path(os.path.expandvars(r"%LOCALAPPDATA%\Programs\Inno Setup 5")),
        Path(os.path.expandvars(r"%ProgramFiles(x86)%\Inno Setup 6")),
        Path(os.path.expandvars(r"%ProgramFiles%\Inno Setup 6")),
        Path(os.path.expandvars(r"%ProgramFiles(x86)%\Inno Setup 5")),
    ]
    for d in candidates:
        exe = d / "ISCC.exe"
        if exe.exists():
            return exe
    found = shutil.which("ISCC")
    return Path(found) if found else None


def ReadNkCodeVersion() -> str:
    """Version affichée par l'IDE — lue dans la SOURCE UNIQUE
    (NkUi.h::NkCodeVersion) pour que l'installeur ne puisse pas en diverger."""
    src = REPO / "Applications/NKCode/src/NKCode/Shell/NkUi.h"
    try:
        txt = src.read_text(encoding="utf-8", errors="ignore")
        i = txt.find("NkCodeVersion()")
        if i >= 0:
            j = txt.find('return "', i)
            if j >= 0:
                k = txt.find('"', j + 8)
                if k > 0:
                    return txt[j + 8:k]
    except OSError:
        pass
    return "0.0.0"


def MakeInnoInstaller(distDir: Path, outDir: Path, config: str) -> None:
    """Écrit un script Inno Setup pour la distribution assemblée, puis le
    compile si ISCC est disponible.

    Choix : Inno Setup plutôt que l'installeur maison (Jenga Tools/Installer),
    conformément à l'exigence « de vrais outils ». Ce qu'un vrai installeur
    apporte et qu'une archive ne peut pas : entrée « Programmes et
    fonctionnalités », désinstalleur, raccourcis Menu Démarrer/Bureau,
    association du dossier d'installation par utilisateur (aucun droit
    administrateur requis), et signature Authenticode possible ensuite.
    """
    version = ReadNkCodeVersion()
    iss = outDir / "NKCode.iss"
    setupBase = f"NKCode-{version}-win64-setup"
    # PrivilegesRequired=lowest : installation par UTILISATEUR (pas d'UAC) ->
    # un testeur sans droits admin peut installer. DisableProgramGroupPage :
    # moins de questions, l'installation doit rester triviale.
    iss.write_text(f"""; Script Inno Setup GENERE par scripts/MakeNkCodeDist.py — ne pas editer a la main.
; NKCode {version} ({config}) — editeur : Rihen
[Setup]
AppId={{{{B7E4B4C1-3F2A-4E77-9C2E-6E4B3D1A7C05}}}}
AppName=NKCode
AppVersion={version}
AppPublisher=Rihen
AppPublisherURL=https://github.com/Rihen-Universe/NKCode-Beta
DefaultDirName={{autopf}}\\NKCode
DefaultGroupName=NKCode
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={outDir.as_posix()}
OutputBaseFilename={setupBase}
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayName=NKCode {version}
UninstallDisplayIcon={{app}}\\NKCode.exe

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{{cm:CreateDesktopIcon}}"; GroupDescription: "{{cm:AdditionalIcons}}"; Flags: unchecked

[Files]
Source: "{distDir.as_posix()}\\*"; DestDir: "{{app}}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{{group}}\\NKCode"; Filename: "{{app}}\\NKCode.exe"; WorkingDir: "{{app}}"
Name: "{{group}}\\{{cm:UninstallProgram,NKCode}}"; Filename: "{{uninstallexe}}"
Name: "{{autodesktop}}\\NKCode"; Filename: "{{app}}\\NKCode.exe"; WorkingDir: "{{app}}"; Tasks: desktopicon

[Run]
; WorkingDir explicite : les ressources data/ et tools/ sont resolues a cote de
; l'executable, mais on lance depuis {{app}} par coherence.
Filename: "{{app}}\\NKCode.exe"; WorkingDir: "{{app}}"; Description: "{{cm:LaunchProgram,NKCode}}"; Flags: nowait postinstall skipifsilent
""", encoding="utf-8")
    Log(f"script Inno Setup -> {iss}")

    iscc = FindIscc()
    if not iscc:
        Log("ISCC.exe introuvable : script .iss ecrit, installeur NON compile.")
        Log("  Installer Inno Setup : winget install --id JRSoftware.InnoSetup --scope user")
        return
    Log(f"compilation de l'installeur avec {iscc}")
    import subprocess
    rc = subprocess.call([str(iscc), str(iss)])
    if rc == 0:
        Log(f"OK : {outDir / (setupBase + '.exe')}")
    else:
        Log(f"ERREUR : ISCC a retourne {rc}")


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
    ap.add_argument("--installer", action="store_true",
                    help="genere un VRAI installeur Windows (Inno Setup) : "
                         "raccourcis, desinstalleur, entree Programmes et "
                         "fonctionnalites. Compile avec ISCC s'il est trouve, "
                         "sinon ecrit seulement le script .iss.")
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

    # ── Shim `jenga` en LIGNE DE COMMANDE ────────────────────────────────────
    # NKCode route la plupart des commandes vers l'interpreteur embarque, mais
    # certaines ont besoin d'un VRAI terminal (`jenga gdb`, session interactive)
    # et l'utilisateur peut vouloir taper `jenga ...` lui-meme dans le terminal
    # integre. Ce shim appelle le Python EMBARQUE avec les sources embarquees :
    # aucun Python systeme requis. NkEmbeddedJenga::Configure prefixe `tools/`
    # au PATH du process, donc le terminal integre le trouve.
    Log("shim ligne de commande (tools/jenga.cmd + tools/jenga)")
    # Le chemin des sources Jenga est declare dans le fichier `._pth` de la
    # distribution *embeddable* : c'est LE mecanisme prevu par CPython pour ce
    # cas. PYTHONPATH ne conviendrait pas — la presence d'un `._pth` fait
    # ignorer les variables d'environnement (et `-I` les ignore de toute facon).
    for pth in (tools / "python-embed").glob("python*._pth"):
        txt = pth.read_text(encoding="ascii", errors="ignore")
        if "jenga-src" not in txt:
            pth.write_text(txt.rstrip("\n") + "\n../jenga-src\n", encoding="ascii", newline="")
            Log(f"  {pth.name} : ajout de ../jenga-src")
    (tools / "jenga.cmd").write_text(
        "@echo off\r\n"
        "rem Shim GENERE par scripts/MakeNkCodeDist.py : Jenga via le Python embarque.\r\n"
        'rem (le chemin des sources vient du fichier python*._pth, pas de PYTHONPATH)\r\n'
        '"%~dp0python-embed\\python.exe" -m Jenga %*\r\n', encoding="ascii", newline="")
    # Variante POSIX (Phase 6 Linux/macOS) : ecrite des maintenant pour que le
    # pipeline soit identique quand le runtime non-Windows arrivera.
    sh = tools / "jenga"
    sh.write_text(
        "#!/bin/sh\n"
        "# Shim GENERE par scripts/MakeNkCodeDist.py : Jenga via le Python embarque.\n"
        'DIR="$(cd "$(dirname "$0")" && pwd)"\n'
        'PYTHONPATH="$DIR/jenga-src" exec "$DIR/python-embed/python" -m Jenga "$@"\n',
        encoding="ascii", newline="\n")
    try:
        sh.chmod(0o755)
    except OSError:
        pass

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
    if args.installer:
        MakeInnoInstaller(out, Path(args.out), args.config)

    Log(f"OK : {out}")
    Log("Test : lancer dist/NKCode/NKCode.exe sur une machine SANS Python ni compilateur.")
    return 0


if __name__ == "__main__":
    sys.exit(Main())
