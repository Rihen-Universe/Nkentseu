# -*- coding: utf-8 -*-
"""
EmpaqueterJeux.py — rassemble NkDames, NkEchecs et NkLudo dans UN dossier.

    python scripts/EmpaqueterJeux.py [--jeux NkDames,NkLudo] [--sans-linux]

Produit `Build/JeuxPlateau-Release/`, sur le modele de `Build/GemCrush-Release/`
qui existait deja :

    NkDames-1.0.0-windows-x64.zip
    NkDames-1.0.0-web.zip
    NkDames-1.0.0-android-universal.apk
    NkDames-1.0.0-harmonyos.hap
    NkDames-1.0.0-linux-x86_64.tar.gz
    ... idem NkEchecs et NkLudo, plus un LISEZMOI.md

⚠️ CE SCRIPT NE FAIT QUE PILOTER `jenga package` — il n'empaquete rien lui-meme.
   Ma premiere version zippait a la main ; `jenga package --platform ... --project
   ... --output ...` existe depuis toujours et fait le travail, EN CONSTRUISANT
   d'abord (donc la fraicheur est garantie, ce qu'un empaqueteur pur ne peut pas
   promettre). Ecrire une seconde facon d'empaqueter aurait garanti qu'elles
   divergent.

⚠️ DEUX PLATEFORMES NE PASSENT PAS PAR `jenga package`, ET LA RAISON EST MESUREE :
   - HarmonyOS : `jenga package --platform harmonyos` exige
     `Applications/<jeu>/hvigor/hvigor-config.json5`, que ni ces jeux ni GemCrush
     n'ont. Son `.hap` vient donc de `jenga build`, comme celui de GemCrush.
   - Linux : `jenga` depuis Windows s'arrete sur `X11/X.h` absent. On passe par
     WSL2, ou les en-tetes existent.

⚠️ LE PIEGE QUI M'A MORDU, ET QUI JUSTIFIE LE DOSSIER TEMPORAIRE PAR PLATEFORME :
   `jenga package` nomme sa sortie d'apres le PROJET, pas la plateforme. Windows
   et Web produisent tous deux `NkDames.zip`. Empaquetes dans le meme dossier, le
   second ECRASE le premier — en silence, avec un « package created » qui a l'air
   d'une reussite. On empaquete donc dans un sous-dossier par plateforme, puis on
   renomme.

AUTEUR: Rihen — Proprietary, All Rights Reserved (see LICENSE)
"""
import argparse
import io
import os
import re
import shutil
import subprocess
import sys
import tarfile

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SORTIE = os.path.join(RACINE, "Build", "JeuxPlateau-Release")
VERSION_DEFAUT = "0.1.0"

JEUX_DEFAUT = ["NkDames", "NkEchecs", "NkLudo"]

# plateforme -> (suffixe du fichier final, extension attendue en sortie de jenga)
VIA_PACKAGE = [
    ("windows", "windows-x64", ".zip"),
    ("web", "web", ".zip"),
]


# ---------------------------------------------------------------------------
def version_du_jeu(app):
    """Lit `d.appVersion = "..."` dans le main.cpp du jeu.

    ⚠️ La version vient du CODE, pas d'ici. Deux sources pour un meme numero
    divergent au premier changement, et rien ne les oppose jamais.
    """
    chemin = os.path.join(RACINE, "Applications", app, "src", "main.cpp")
    try:
        s = io.open(chemin, encoding="utf-8", errors="replace").read()
    except IOError:
        print("    ! main.cpp introuvable -> version %s par defaut" % VERSION_DEFAUT)
        return VERSION_DEFAUT
    m = re.search(r'appVersion\s*=\s*"([^"]+)"', s)
    if not m:
        print("    ! appVersion absente -> version %s par defaut" % VERSION_DEFAUT)
        return VERSION_DEFAUT
    return m.group(1)


def mo(chemin):
    return "%.1f Mo" % (os.path.getsize(chemin) / 1048576.0)


def lancer(cmd, cwd=None):
    """Lance une commande et rend (code, sortie). On garde la sortie POUR LA LIRE :
    un `package created` peut cacher un ecrasement, et un echec peut n'etre visible
    que dans une ligne du milieu."""
    p = subprocess.Popen(cmd, cwd=cwd or RACINE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, shell=False)
    out, _ = p.communicate()
    return p.returncode, out.decode("utf-8", errors="replace")


# ---------------------------------------------------------------------------
def empaqueter_via_jenga(jeu, ver, plateforme, suffixe, ext):
    """`jenga package` dans un dossier TEMPORAIRE, puis renommage."""
    tmp = os.path.join(SORTIE, "_tmp_" + plateforme)
    if os.path.isdir(tmp):
        shutil.rmtree(tmp)
    os.makedirs(tmp)

    code, out = lancer(["jenga", "package", "--platform", plateforme,
                        "--project", jeu, "--config", "Release", "--output", tmp])

    produits = []
    for pied, _, fichiers in os.walk(tmp):
        for f in fichiers:
            if f.lower().endswith(ext):
                produits.append(os.path.join(pied, f))

    if not produits:
        # ⚠️ On MONTRE la derniere ligne utile : un echec silencieux d'empaquetage
        # est indiscernable d'une plateforme non construite.
        motif = [l for l in out.splitlines() if re.search(r"error|ERROR|Not Found|echec", l)]
        brut = motif[-1].strip()[:120] if motif else ("code de sortie %d" % code)
        # ⚠️ FILTRE ASCII OBLIGATOIRE. La banniere de jenga contient des
        # caracteres de dessin de boite ; les imprimer sur une console cp1252
        # leve une UnicodeEncodeError qui MASQUE l'erreur qu'on voulait lire.
        raison = "".join(c if 32 <= ord(c) < 127 else "?" for c in brut).strip("? ")
        shutil.rmtree(tmp, ignore_errors=True)
        return None, raison

    src = max(produits, key=os.path.getsize)
    dst = os.path.join(SORTIE, "%s-%s-%s%s" % (jeu, ver, suffixe, ext))
    shutil.move(src, dst)
    shutil.rmtree(tmp, ignore_errors=True)
    return dst, None


def empaqueter_android(jeu, ver):
    """`jenga build --platform android` puis copie de l'APK universel.

    ⚠️ POURQUOI PAS `jenga package --platform android` : il ECHOUE, sur
    `clang++: error: no such file or directory:` avec un nom de fichier VIDE —
    20 erreurs, edition de liens ratee. Alors que `jenga build --platform
    android` produit l'APK universel 4 ABI sans broncher. Mesure du 2026-09-02 ;
    defaut d'outillage a signaler a l'agent Jenga, pas a contourner en silence.
    """
    code, _ = lancer(["jenga", "build", "--target", jeu, "--config", "Release",
                      "--platform", "android"])
    apk = None
    base = os.path.join(RACINE, "Build", "Bin", "Release-Android", jeu)
    for pied, _, fichiers in os.walk(base):
        for f in fichiers:
            if f == "%s-Release.apk" % jeu:
                apk = os.path.join(pied, f)
    if apk is None:
        return None, "aucun APK produit (code %d)" % code
    dst = os.path.join(SORTIE, "%s-%s-android-universal.apk" % (jeu, ver))
    shutil.copy2(apk, dst)
    return dst, None


def empaqueter_harmonyos(jeu, ver):
    """`jenga package` refuse (pas de hvigor-config.json5) : on prend le .hap
    produit par `jenga build`, comme GemCrush."""
    code, _ = lancer(["jenga", "build", "--target", jeu, "--config", "Release",
                      "--platform", "harmonyos"])
    hap = os.path.join(RACINE, "Build", "Bin", "Release-HarmonyOS", jeu, jeu + ".hap")
    if not os.path.exists(hap):
        return None, "aucun .hap produit (code %d)" % code
    dst = os.path.join(SORTIE, "%s-%s-harmonyos.hap" % (jeu, ver))
    shutil.copy2(hap, dst)
    return dst, None


def empaqueter_linux(jeu, ver):
    """Construit sous WSL2 et empaquete le binaire.

    ⚠️ Le dossier de sortie porte le nom du BACKEND (`Release-Linux-xlib`), pas
    `Release-Linux`. On CHERCHE le binaire — un chemin devine echouerait sur une
    construction reussie.
    """
    script = ('cd /mnt/d/Projets/2026/Nkentseu/Nkentseu && '
              'export PATH="$HOME/.local/bin:$PATH" && '
              'jenga build --target %s --config Release >/dev/null 2>&1; '
              'find Build/Bin -type f -name %s -path "*Linux*" | head -1' % (jeu, jeu))
    code, out = lancer(["wsl.exe", "-e", "bash", "-lc", script])
    rel = out.strip().splitlines()[-1].strip() if out.strip() else ""
    if not rel or not rel.startswith("Build/"):
        return None, "aucun binaire Linux (WSL2 disponible ?)"
    src = os.path.join(RACINE, rel.replace("/", os.sep))
    if not os.path.exists(src):
        return None, "binaire annonce mais introuvable : %s" % rel
    dst = os.path.join(SORTIE, "%s-%s-linux-x86_64.tar.gz" % (jeu, ver))
    with tarfile.open(dst, "w:gz") as t:
        t.add(src, arcname="%s/%s" % (jeu, jeu))
    return dst, None


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="Empaquete les jeux de plateau dans un dossier commun.")
    ap.add_argument("--jeux", default=",".join(JEUX_DEFAUT),
                    help="liste separee par des virgules (defaut : les trois)")
    ap.add_argument("--sans-linux", action="store_true",
                    help="saute WSL2 (utile si WSL n'est pas installe)")
    a = ap.parse_args()
    jeux = [j.strip() for j in a.jeux.split(",") if j.strip()]

    if os.path.isdir(SORTIE):
        shutil.rmtree(SORTIE)
    os.makedirs(SORTIE)

    faits, manquants = [], []

    for jeu in jeux:
        ver = version_du_jeu(jeu)
        print("\n=== %s  (version %s) ===" % (jeu, ver))

        for plateforme, suffixe, ext in VIA_PACKAGE:
            dst, err = empaqueter_via_jenga(jeu, ver, plateforme, suffixe, ext)
            if dst:
                print("\r  %-10s %8s  %s" % (plateforme, mo(dst), os.path.basename(dst)))
                faits.append(dst)
            else:
                print("\r  %-10s ABSENT -- %s" % (plateforme, err))
                manquants.append("%s / %s" % (jeu, plateforme))

        for nom, fn in (("android", empaqueter_android),
                        ("harmonyos", empaqueter_harmonyos),
                        ("linux", empaqueter_linux)):
            if nom == "linux" and a.sans_linux:
                manquants.append("%s / linux (saute)" % jeu)
                continue
            dst, err = fn(jeu, ver)
            if dst:
                print("\r  %-10s %8s  %s" % (nom, mo(dst), os.path.basename(dst)))
                faits.append(dst)
            else:
                print("\r  %-10s ABSENT -- %s" % (nom, err))
                manquants.append("%s / %s" % (jeu, nom))

    ecrire_lisezmoi(len(faits), manquants)

    print("\n%d paquet(s) dans Build/JeuxPlateau-Release/" % len(faits))
    if manquants:
        # ⚠️ On les NOMME. macOS et iOS sont attendus absents ; un Windows
        # manquant serait une panne. C'est au lecteur de trancher, pas au script.
        print("\nABSENTS (%d) — tous ne sont pas graves :" % len(manquants))
        for m in manquants:
            print("  - %s" % m)
    print("\nmacOS et iOS ne se construisent PAS ici : jenga refuse depuis Windows.")
    print("Ils viennent de la CI (.github/workflows/build-jeux-plateau.yml).")
    return 0


# ---------------------------------------------------------------------------
def ecrire_lisezmoi(total, manquants):
    txt = u"""# Jeux de plateau Nkentseu — paquets de test

NkDames, NkEchecs et NkLudo, construits depuis les sources du depot.
Trois jeux, un moteur maison, aucune bibliotheque standard C++.

## Ce qu'il y a dans ce dossier

| fichier | pour qui | comment on s'en sert |
|---|---|---|
| `*-windows-x64.zip` | Windows 10/11 | decompresser, lancer le `.exe` |
| `*-web.zip` | n'importe quel navigateur | decompresser et SERVIR par HTTP (voir plus bas) |
| `*-android-universal.apk` | Android 7+ | installer l'APK (sources inconnues a autoriser) |
| `*-harmonyos.hap` | HarmonyOS | installer par `hdc install` |
| `*-linux-x86_64.tar.gz` | Linux x86_64 | `tar xzf`, puis lancer le binaire |

## ⚠️ Le Web ne s'ouvre PAS en double-cliquant le `.html`

Un `.wasm` charge par `file://` est refuse par tous les navigateurs. Il faut le
servir :

    cd <dossier decompresse>
    python -m http.server 8000

puis ouvrir `http://localhost:8000/<jeu>.html`.

## ⚠️ L'APK est signe avec la cle de DEBOGAGE

Bon pour des testeurs internes ; ce n'est PAS une signature de publication, et
l'APK ne peut pas aller sur le Play Store en l'etat.

Il ne contient pas de `classes.dex`, et **c'est normal** : ce sont des
applications purement natives (`android.app.NativeActivity` + `android.app.lib_name`
au manifeste), sans aucune classe Java a nous. Meme structure que l'APK GemCrush,
qui tourne sur telephone.

## ⚠️ Ni macOS ni iOS ici

Ils ne se construisent pas depuis Windows — jenga le refuse explicitement. Ils
sont produits par la CI (`.github/workflows/build-jeux-plateau.yml`), et le job
iOS **ne produit pas d'IPA installable** : sans compte Apple Developer, un
binaire iOS ne s'installe sur aucun appareil.

## Ce que chaque jeu propose

- **NkDames** — regles internationales, 10x10. Contre l'ordinateur, a deux sur
  le meme ecran, ou IA contre IA.
- **NkEchecs** — regles completes. Memes trois modes.
- **NkLudo** — quatre sieges, chacun **Humain**, **IA**, ou **desactive** : on
  compose la partie avant de commencer (deux sieges utilisables au minimum).

Chaque jeu s'ouvre sur l'ecran de marque Rihen, **toujours sautable** d'un appui.

## Options de ligne de commande

    --selftest    lance le banc de regles et sort sans ouvrir de fenetre
    --mode=<n>    (NkLudo) un prereglage de sieges, pour une partie reproductible

---
"""
    txt += u"Genere par `scripts/EmpaqueterJeux.py` (pilote de `jenga package`) — %d paquet(s)" % total
    if manquants:
        txt += u", %d absent(s) : %s" % (len(manquants), ", ".join(manquants))
    txt += u".\n"

    octets = txt.encode("utf-8")
    chemin = os.path.join(SORTIE, "LISEZMOI.md")
    with io.open(chemin + ".tmp", "wb") as g:
        g.write(octets)
    os.replace(chemin + ".tmp", chemin)


if __name__ == "__main__":
    sys.exit(main())
