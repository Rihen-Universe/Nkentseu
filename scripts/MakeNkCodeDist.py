#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
"""
MakeNkCodeDist.py — assemble une DISTRIBUTION AUTONOME de NKCode pour testeurs
externes (Phase 5 ROADMAP / Phase 12 « zéro-dépendance ») :

    dist/NKCode/
      NKCode.exe                      (build Release ou Debug)
      python312.dll, python3.dll,     (DLLs exigees AU DEMARRAGE : NKCode.exe
      vcruntime140*.dll                lie python312 -> loader Windows)
      tools/
        python-embed/                 (runtime CPython 3.12 embeddable COMPLET :
                                       runtime vendorise + python.exe/pythonw.exe/
                                       python312.zip du paquet officiel python.org,
                                       telecharge et verifie par SHA-256)
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
import hashlib
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LLVM_MINGW_URL = ("https://github.com/mstorsjo/llvm-mingw/releases/download/"
                  "20240619/llvm-mingw-20240619-ucrt-x86_64.zip")

# ── Python embarque : le paquet « Windows embeddable package (64-bit) » ──────
# Le runtime vendorise (Externals/Libs/PythonEmbed/runtime) est INCOMPLET par
# construction : `.gitignore` ignore `*.exe` et `*.zip`, donc `python.exe`,
# `pythonw.exe` et `python312.zip` (TOUTE la bibliotheque standard) n'entrent
# jamais dans le depot. Mesure le 2026-09-05 sur les betas 5, 7 et 8 publiees :
# aucune ne portait ces trois fichiers. Consequences chez un testeur sans
# Python : l'interpreteur IN-PROCESS de NKCode ne demarre pas (`stdlib dir =
# ''`, « No module named 'encodings' ») -> la liste des projets reste vide, le
# bouton Construire ne construit rien ; et le shim `tools/jenga.cmd` du
# terminal integre appelle un `python.exe` absent.
#
# Remede ICI, pas dans git : la recette telecharge le paquet officiel de
# python.org pour la version EPINGLEE ci-dessous, verifie sa somme SHA-256
# (celle publiee par python.org dans le SBOM SPDX du fichier,
# python-<v>-embed-amd64.zip.spdx.json ; recoupee avec le MD5 de la page de
# release et la signature GPG `.asc` — cle « Steve Dower (Python Release
# Signing) », empreinte 7ED1 0B65 31D7 C8E1 BC29 6021 FC62 4643 4870 34E5),
# puis l'extrait EN ENTIER dans tools/python-embed/. Pas de somme connue pour
# la version demandee = pas de distribution.
#
# La version vient de Externals/Libs/PythonEmbed/VERSION (source unique : c'est
# aussi celle des en-tetes contre lesquels NkEmbeddedJenga est compile — le
# `python312.dll` livre a cote de l'exe DOIT etre celui de ce paquet).
PYEMBED_URL = "https://www.python.org/ftp/python/{v}/python-{v}-embed-amd64.zip"
PYEMBED_SHA256 = {
    "3.12.7": "0d57bb6cb078b74d23dbfe91f77d6780d45bed328911609f1f7ee2ba1606bf44",
}


def ReadPyEmbedVersion() -> str:
    f = REPO / "Externals/Libs/PythonEmbed/VERSION"
    try:
        return f.read_text(encoding="ascii").strip()
    except OSError:
        return ""


def Sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fp:
        for chunk in iter(lambda: fp.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def DownloadPythonEmbed(cache: Path, version: str) -> Path:
    """Telecharge (avec cache) le paquet embeddable officiel et VERIFIE sa somme.

    Retourne le chemin du zip verifie. Sort du programme si la version n'a pas
    de somme epinglee ou si la somme ne correspond pas : un zip qu'on ne sait
    pas verifier ne part pas chez un testeur.
    """
    expected = PYEMBED_SHA256.get(version)
    if not expected:
        Log(f"ERREUR : aucune somme SHA-256 epinglee pour Python {version} "
            f"(Externals/Libs/PythonEmbed/VERSION). Ajouter l'entree dans "
            f"PYEMBED_SHA256 depuis python-{version}-embed-amd64.zip.spdx.json "
            f"de python.org — pas de somme, pas de distribution.")
        sys.exit(1)
    cache.mkdir(parents=True, exist_ok=True)
    url = PYEMBED_URL.format(v=version)
    zip_path = cache / Path(url).name
    if zip_path.exists() and Sha256(zip_path) != expected:
        Log(f"cache corrompu ({zip_path.name} : somme differente) -> re-telechargement")
        zip_path.unlink()
    if not zip_path.exists():
        Log(f"telechargement du Python embarque : {url}")
        tmp = zip_path.with_suffix(".part")
        urllib.request.urlretrieve(url, tmp)
        tmp.rename(zip_path)
    else:
        Log(f"Python embarque deja en cache : {zip_path.name}")
    got = Sha256(zip_path)
    if got != expected:
        Log(f"ERREUR : somme SHA-256 de {zip_path.name} inattendue :")
        Log(f"    attendue {expected}")
        Log(f"    obtenue  {got}")
        Log("  Le fichier telecharge n'est pas celui que python.org publie : distribution refusee.")
        sys.exit(1)
    Log(f"SHA-256 verifiee : {got}")
    return zip_path


def InstallPythonEmbed(zip_path: Path, dest: Path) -> None:
    """Extrait le paquet officiel dans dest (par-dessus le runtime vendorise).

    Garde-fou : un fichier present des deux cotes doit etre IDENTIQUE octet
    pour octet — sinon le runtime vendorise (celui dont NKCode.exe prend son
    python312.dll) et le paquet telecharge ne sont pas la meme version, et
    l'IDE et le terminal tourneraient sur deux Python differents.
    """
    # Fichiers TEXTE : compares apres normalisation des fins de ligne. Mesure le
    # 2026-09-05 : LICENSE.txt du zip officiel a des fins de ligne MIXTES (639 CR
    # pour 702 LF) et git (`autocrlf`) a tout mis en CRLF dans le runtime
    # vendorise — 63 octets d'ecart pour un contenu identique. Les binaires
    # (.dll, .pyd, .exe, .zip, .cat) restent compares octet pour octet.
    TEXTE = (".txt", "._pth")

    def _norm(b: bytes) -> bytes:
        return b.replace(b"\r\n", b"\n").replace(b"\r", b"\n")

    with zipfile.ZipFile(zip_path) as z:
        differents = []
        for info in z.infolist():
            if info.is_dir():
                continue
            target = dest / info.filename
            data = z.read(info)
            if target.exists():
                if info.filename.lower().endswith(TEXTE):
                    same = _norm(data) == _norm(target.read_bytes())
                else:
                    same = hashlib.sha256(data).hexdigest() == Sha256(target)
                if not same:
                    differents.append(info.filename)
                continue  # identique : rien a faire
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        if differents:
            Log("ERREUR : le runtime vendorise differe du paquet python.org epingle :")
            for d in differents:
                Log(f"    - {d}")
            Log("  Mettre a jour Externals/Libs/PythonEmbed (runtime + VERSION + somme) ensemble.")
            sys.exit(1)
    for needed in ("python.exe", "pythonw.exe"):
        if not (dest / needed).exists():
            Log(f"ERREUR : {needed} absent apres extraction de {zip_path.name}")
            sys.exit(1)
    if not list(dest.glob("python*.zip")):
        Log("ERREUR : bibliotheque standard (python*.zip) absente apres extraction")
        sys.exit(1)
    Log("runtime Python complet : python.exe, pythonw.exe, bibliotheque standard (python*.zip)")


def VerifierRuntimePython(tools: Path) -> None:
    """Fait tourner le Python EMBARQUE de la distribution, environnement vide
    (aucune variable Python, PATH = System32 seul), et exige `import Jenga`.

    C'est le chemin exact du shim tools/jenga.cmd chez un testeur sans Python :
    si ceci echoue, le terminal integre echouera pareil. Refuse la distribution.
    """
    py = tools / "python-embed" / "python.exe"
    sysroot = os.environ.get("SystemRoot", r"C:\Windows")
    # USERPROFILE : `import Jenga` appelle Path.home() des l'import
    # (Core/Api.py, cache des outils) ; toute session Windows le definit, le
    # temoin le garde donc — sans lui, l'import echoue pour une raison qui
    # n'existe chez aucun testeur (mesure le 2026-09-05).
    profil = os.environ.get("USERPROFILE", sysroot + "\\Temp")
    env = {
        "PATH": f"{sysroot}\\System32;{sysroot}",
        "SystemRoot": sysroot,
        "TEMP": os.environ.get("TEMP", sysroot + "\\Temp"),
        "TMP": os.environ.get("TEMP", sysroot + "\\Temp"),
        "USERPROFILE": profil,
        "HOMEDRIVE": profil[:2],
        "HOMEPATH": profil[2:],
    }
    try:
        res = subprocess.run(
            [str(py), "-I", "-c",
             "import sys, Jenga; print(Jenga.__version__); print(sys.prefix)"],
            capture_output=True, text=True, timeout=120, env=env,
            cwd=str(tools.parent))
    except Exception as err:
        Log(f"ERREUR : le Python embarque ne demarre pas ({err}) : distribution refusee.")
        sys.exit(1)
    if res.returncode != 0:
        Log("ERREUR : `python.exe -I -c \"import Jenga\"` echoue dans la distribution :")
        for ligne in (res.stderr or res.stdout).splitlines()[-8:]:
            Log(f"    {ligne}")
        sys.exit(1)
    lignes = res.stdout.strip().splitlines()
    Log(f"Python embarque OK sans Python systeme : Jenga {lignes[0] if lignes else '?'} "
        f"(prefix {lignes[1] if len(lignes) > 1 else '?'})")

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


# DLL fournies par Windows lui-meme (ou par le pilote GPU pour vulkan-1) : on
# ne les livre pas, et leur absence n'est pas une erreur de distribution.
DLLS_SYSTEME = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
    "ole32.dll", "oleaut32.dll", "ws2_32.dll", "winmm.dll", "comdlg32.dll",
    "shlwapi.dll", "version.dll", "dbghelp.dll", "imm32.dll", "setupapi.dll",
    "cfgmgr32.dll", "crypt32.dll", "iphlpapi.dll", "secur32.dll", "userenv.dll",
    "psapi.dll", "ucrtbase.dll", "msvcrt.dll", "opengl32.dll", "dwmapi.dll",
    "uxtheme.dll", "bcrypt.dll", "ntdll.dll", "rpcrt4.dll", "mswsock.dll",
    "winhttp.dll", "urlmon.dll", "wininet.dll", "dxgi.dll", "d3d11.dll",
    "d3d12.dll", "d3dcompiler_47.dll", "dinput8.dll", "dxguid.dll", "avrt.dll",
    "mf.dll", "mfplat.dll", "mfreadwrite.dll", "mfuuid.dll", "xinput1_3.dll",
    "comctl32.dll", "gdiplus.dll", "oleacc.dll", "propsys.dll", "powrprof.dll",
    "hid.dll", "vulkan-1.dll",
}


def VerifierImports(exe, out):
    """Refuse de livrer un exe dont une DLL importee n'est pas a cote de lui.

    Ecrit apres l'issue beta #15 : NKCode.exe importait libstdc++-6.dll,
    libgcc_s_seh-1.dll et libwinpthread-1.dll, qu'aucune ligne de ce script ne
    copiait. Le chargeur allait donc les prendre DANS LE PATH DU TESTEUR — chez
    l'un d'eux, le msys64 installe pour la toolchain portait une autre version,
    d'ou « NKCode.exe - Point d'entree introuvable : clock_gettime64 ».
    La liste des DLL a livrer etait tenue A LA MAIN ; elle est desormais
    CONFRONTEE a la table d'imports reelle du binaire.
    """
    objdump = shutil.which("objdump") or shutil.which("llvm-objdump")
    if not objdump:
        Log("ATTENTION : objdump introuvable — table d'imports NON verifiee.")
        return
    try:
        res = subprocess.run([objdump, "-p", str(exe)], capture_output=True,
                             text=True, timeout=180)
    except Exception as err:  # outil present mais inutilisable : on previent, on ne bloque pas
        Log(f"ATTENTION : verification des imports impossible ({err}).")
        return

    manquantes = []
    for ligne in res.stdout.splitlines():
        ligne = ligne.strip()
        if not ligne.startswith("DLL Name:"):
            continue
        dll = ligne.split(":", 1)[1].strip()
        bas = dll.lower()
        if bas.startswith("api-ms-") or bas.startswith("ext-ms-") or bas in DLLS_SYSTEME:
            continue
        if (out / dll).exists():
            continue
        manquantes.append(dll)

    if manquantes:
        Log("ERREUR : DLL importees par NKCode.exe mais ABSENTES de la distribution :")
        for d in manquantes:
            Log(f"    - {d}")
        Log("  L'IDE chargerait celles du PATH de l'utilisateur, ou refuserait de")
        Log("  demarrer (issue beta #15). Livrer ces DLL, ou lier en statique.")
        sys.exit(1)
    Log("table d'imports verifiee : aucune DLL non-systeme manquante")


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


def MakeInnoInstaller(distDir: Path, outDir: Path, config: str, withCompiler: bool = True) -> None:
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
    # Chemins ABSOLUS : le .iss est ecrit dans outDir, et Inno resout les
    # chemins relatifs de [Files] PAR RAPPORT AU .iss. Avec un --out relatif
    # (« --out dist-beta4 »), la source devenait « dist-beta4/dist-beta4/NKCode »
    # et la compilation s'arretait sur « No files found matching ». On resout
    # une fois ici plutot que d'imposer un chemin absolu a l'appelant.
    distDir = distDir.resolve()
    outDir = outDir.resolve()
    iss = outDir / "NKCode.iss"
    # Les DEUX variantes (avec/sans compilateur embarque) ecrivaient le MEME
    # nom : construire la complete apres la legere ECRASAIT silencieusement
    # cette derniere, et le suffixe « -complet » n'existait que par renommage
    # manuel. Le nom porte desormais la difference.
    setupBase = f"NKCode-{version}-win64-setup" + ("-complet" if withCompiler else "")
    # PrivilegesRequired=lowest : installation par UTILISATEUR (pas d'UAC) ->
    # un testeur sans droits admin peut installer. DisableProgramGroupPage :
    # moins de questions, l'installation doit rester triviale.
    # Guillemets construits HORS du f-string : ecrits en clair, une sequence de
    # trois guillemets fermerait le litteral (cf. section [Registry]).
    _Q2 = chr(34) * 2
    _Q3 = chr(34) * 3
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
Name: "contextmenu"; Description: "Ajouter « Ouvrir avec NKCode » au menu contextuel des dossiers"; GroupDescription: "Integration a l'explorateur"

[Files]
Source: "{distDir.as_posix()}\\*"; DestDir: "{{app}}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{{group}}\\NKCode"; Filename: "{{app}}\\NKCode.exe"; WorkingDir: "{{app}}"
Name: "{{group}}\\{{cm:UninstallProgram,NKCode}}"; Filename: "{{uninstallexe}}"
Name: "{{autodesktop}}\\NKCode"; Filename: "{{app}}\\NKCode.exe"; WorkingDir: "{{app}}"; Tasks: desktopicon

[Registry]
; « Ouvrir avec NKCode » dans l'explorateur. NKCode.exe accepte deja un dossier
; en argument (main.cpp) : il n'y a donc rien a ajouter cote application.
;
; HKCU et PAS HKLM : l'installation est par utilisateur (PrivilegesRequired=lowest
; ci-dessus). Ecrire dans HKLM demanderait une elevation UAC que cet installeur
; ne reclame volontairement pas — c'etait deja la cause d'un retour beta.
;
; Trois emplacements sont necessaires pour couvrir les trois gestes de l'utilisateur :
;   Directory\shell            -> clic droit SUR une icone de dossier   -> %1
;   Directory\Background\shell -> clic droit DANS un dossier ouvert     -> %V
;   Drive\shell                -> clic droit sur une racine de lecteur  -> %V
; Les jetons %1 et %V ne sont pas interchangeables : %1 est l'element designe,
; %V le dossier courant. Les intervertir ouvre le mauvais dossier, ou rien.
;
; uninstalldelete{{key}} : sans cela, les entrees survivent a la desinstallation.
;
; Les guillemets des commandes passent par _Q2 / _Q3, construits hors de ce
; f-string. Inno exige des guillemets DOUBLES internes autour du chemin et de
; l'argument ; ecrite en clair, la sequence de trois guillemets FERMERAIT le
; litteral Python (SyntaxError constatee).
Root: HKCU; Subkey: "Software\\Classes\\Directory\\shell\\NKCode"; ValueType: string; ValueData: "Ouvrir avec NKCode"; Flags: uninsdeletekey; Tasks: contextmenu
Root: HKCU; Subkey: "Software\\Classes\\Directory\\shell\\NKCode"; ValueType: string; ValueName: "Icon"; ValueData: "{{app}}\\NKCode.exe,0"; Tasks: contextmenu
Root: HKCU; Subkey: "Software\\Classes\\Directory\\shell\\NKCode\\command"; ValueType: string; ValueData: {_Q3}{{app}}\\NKCode.exe{_Q2} {_Q2}%1{_Q3}; Flags: uninsdeletekey; Tasks: contextmenu
Root: HKCU; Subkey: "Software\\Classes\\Directory\\Background\\shell\\NKCode"; ValueType: string; ValueData: "Ouvrir avec NKCode"; Flags: uninsdeletekey; Tasks: contextmenu
Root: HKCU; Subkey: "Software\\Classes\\Directory\\Background\\shell\\NKCode"; ValueType: string; ValueName: "Icon"; ValueData: "{{app}}\\NKCode.exe,0"; Tasks: contextmenu
Root: HKCU; Subkey: "Software\\Classes\\Directory\\Background\\shell\\NKCode\\command"; ValueType: string; ValueData: {_Q3}{{app}}\\NKCode.exe{_Q2} {_Q2}%V{_Q3}; Flags: uninsdeletekey; Tasks: contextmenu
Root: HKCU; Subkey: "Software\\Classes\\Drive\\shell\\NKCode"; ValueType: string; ValueData: "Ouvrir avec NKCode"; Flags: uninsdeletekey; Tasks: contextmenu
Root: HKCU; Subkey: "Software\\Classes\\Drive\\shell\\NKCode\\command"; ValueType: string; ValueData: {_Q3}{{app}}\\NKCode.exe{_Q2} {_Q2}%V{_Q3}; Flags: uninsdeletekey; Tasks: contextmenu

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
    # Le runtime vendorise n'a ni python.exe ni la bibliotheque standard
    # (ignores par git) : on complete avec le paquet OFFICIEL, somme verifiee.
    pyver = ReadPyEmbedVersion()
    if not pyver:
        Log("ERREUR : Externals/Libs/PythonEmbed/VERSION illisible")
        return 1
    pyzip = DownloadPythonEmbed(REPO / ".cache", pyver)
    InstallPythonEmbed(pyzip, tools / "python-embed")
    # DLLs exigees au DEMARRAGE (import table de NKCode.exe) -> a cote de l'exe.
    for dll in ("python312.dll", "python3.dll", "vcruntime140.dll", "vcruntime140_1.dll"):
        shutil.copy2(pyembed / dll, out / dll)

    # Garde-fou : la liste ci-dessus est tenue a la main, la table d'imports non.
    VerifierImports(exe, out)

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
    # Temoin : le chemin du shim, execute pour de vrai, sans aucun Python systeme.
    VerifierRuntimePython(tools)

    # ── Shims POSIX `clang++` / `clang` vers Zig ─────────────────────────────
    # NkEmbeddedJenga::Configure prefixe tools/compilers/zig au PATH sous Unix :
    # le compilateur par defaut y est Zig, et non llvm-mingw qui produit des
    # binaires Windows. Mais un lecteur qui tape `clang++ bonjour.cpp` dans le
    # terminal integre obtenait « commande introuvable » : `zig c++` existe,
    # `clang++` non. Sous Windows le probleme ne se pose pas (llvm-mingw fournit
    # clang++.exe), et il etait invisible depuis un poste de developpement, ou
    # tools/compilers/ n'existe pas et ou clang++ vient du MSYS2 de la machine.
    #
    # Le shim est TRANSPARENT : si un vrai clang++ est deja installe sur la
    # machine, c'est lui qui est appele — le shim se cherche dans le PATH en
    # s'excluant lui-meme. Zig ne sert que de repli, pour qui n'a rien. Un
    # utilisateur qui a son compilateur garde son compilateur.
    #
    # `zig c++` est un remplacant direct de clang++ — memes options (-o, -c,
    # -E, -S, -Wall, -O2) — donc toute commande ecrite pour clang++ marche telle
    # quelle a travers le repli. Ecrits des maintenant, comme le shim `jenga`
    # ci-dessus, pour que le pipeline soit identique quand le runtime
    # non-Windows arrivera.
    Log("shims POSIX clang++/clang -> compilateur systeme, sinon zig (tools/compilers/zig/)")
    zig_dir = tools / "compilers" / "zig"
    zig_dir.mkdir(parents=True, exist_ok=True)
    for shim, sub in (("clang++", "c++"), ("clang", "cc")):
        p = zig_dir / shim
        p.write_text(
            "#!/bin/sh\n"
            f"# Shim GENERE par scripts/MakeNkCodeDist.py : {shim} du systeme s'il existe,\n"
            f"# sinon `zig {sub}` (remplacant direct : memes options, meme sortie).\n"
            'DIR="$(cd "$(dirname "$0")" 2>/dev/null && pwd)"\n'
            "# 1. un compilateur deja installe sur la machine a priorite : on parcourt\n"
            "#    le PATH en sautant notre propre dossier. Deux gardes contre la\n"
            "#    re-execution de soi-meme (= boucle infinie) : DIR vide -> on ne\n"
            "#    parcourt pas ; et -ef compare l'inode, au cas ou le meme fichier\n"
            "#    serait joignable par deux chemins.\n"
            'if [ -n "$DIR" ]; then\n'
            '  OLDIFS="$IFS"; IFS=:\n'
            '  for d in $PATH; do\n'
            '    [ "$d" = "$DIR" ] && continue\n'
            f'    [ "$d/{shim}" -ef "$0" ] 2>/dev/null && continue\n'
            f'    if [ -x "$d/{shim}" ]; then IFS="$OLDIFS"; exec "$d/{shim}" "$@"; fi\n'
            "  done\n"
            '  IFS="$OLDIFS"\n'
            "fi\n"
            "# 2. sinon, zig : a cote de nous d'abord, dans le PATH ensuite.\n"
            f'if [ -n "$DIR" ] && [ -x "$DIR/zig" ]; then exec "$DIR/zig" {sub} "$@"; fi\n'
            f'exec zig {sub} "$@"\n',
            encoding="ascii", newline="\n")
        try:
            p.chmod(0o755)
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
        MakeInnoInstaller(out, Path(args.out), args.config, withCompiler=not args.skip_compiler)

    Log(f"OK : {out}")
    Log("Test : lancer dist/NKCode/NKCode.exe sur une machine SANS Python ni compilateur.")
    return 0


if __name__ == "__main__":
    sys.exit(Main())
