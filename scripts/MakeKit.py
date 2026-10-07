#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""MakeKit.py - fabrique le kit AUTONOME d'une bibliotheque publique de Nkentseu.

POURQUOI
    `jenga kit` extrait un kit redistribuable : les en-tetes, les bibliotheques
    deja construites, et un .jenga qui s'y lie (`usecanvas(modules=[...])`).
    Mais il ecrit ce que le workspace declarait, y compris les dossiers de
    bibliotheques EXTERNES de la machine qui construit :

        libdirs([KIT_ROOT + "/lib/Debug-Windows"] + ['C:/VulkanSDK/1.4.363.0/Lib'])

    Chez quelqu'un d'autre ce dossier n'existe pas, et l'edition de liens tombe
    sur « unable to find library -lvulkan-1 ». Le kit n'etait donc utilisable
    que sur la machine qui l'avait fait.

CE QUE FAIT CE SCRIPT
    1. lance `jenga kit` pour la bibliotheque demandee ;
    2. EMBARQUE dans le kit les bibliotheques d'import qu'il allait chercher
       dans un dossier absolu (vulkan-1.lib du SDK Vulkan), et retire ce dossier
       du .jenga ;
    3. VERIFIE qu'il ne reste aucun chemin absolu, et que le .jenga se compile ;
    4. range le tout dans une archive <Nom>-kit-<Systeme>.zip.

    Rien ne part sur GitHub : un kit contient des bibliotheques construites. Il
    reste sur la machine (D:/Rihen/Livraisons par defaut) ; sa seule sortie
    possible est une GitHub Release, sur decision de Rihen.

CE QU'IL NE FAIT PAS
    Il ne rend pas Vulkan facultatif a l'EXECUTION : un programme lie au kit
    demande vulkan-1.dll au demarrage, comme toute application Nkentseu
    aujourd'hui (les pilotes graphiques l'installent). Le rendre facultatif est
    le role du module NKVulkan (chargement a la demande), qui reste a ecrire.

USAGE
    python scripts/MakeKit.py Canvas
    python scripts/MakeKit.py RHI --config Debug
    python scripts/MakeKit.py --name MonKit --target NKLogger,NKMath --sortie D:/tmp
"""

import argparse
import datetime
import os
import platform
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

# Les kits publics connus : nom du kit -> modules racines (leurs dependances
# suivent toutes seules, c'est `jenga kit` qui les resout).
KITS_CONNUS = {
    "Canvas": ["NKCanvas", "NKAudio"],
    "RHI": ["NKRHI"],
    "Renderer": ["NKRenderer"],
}

RACINE = Path(__file__).resolve().parent.parent


def Dire(message):
    print("[kit] " + message, flush=True)


def Systeme():
    nom = platform.system()
    return {"Windows": "Windows", "Linux": "Linux", "Darwin": "macOS"}.get(nom, nom)


def LancerJengaKit(nom, cibles, configs, dossier):
    commande = ["jenga", "kit", "--target", ",".join(cibles), "--name", nom,
                "--output", str(dossier).replace("\\", "/"), "--force"]
    for c in configs:
        commande += ["--config", c]
    Dire("$ " + " ".join(commande))
    # shell=True sous Windows : `jenga` y est un .exe OU un .cmd selon l'installation.
    code = subprocess.call(" ".join('"%s"' % a if " " in a else a for a in commande),
                           cwd=str(RACINE), shell=True)
    if code != 0:
        raise SystemExit("jenga kit a echoue (code %d)" % code)


_CHAINE = re.compile(r"""(['"])((?:\\.|(?!\1).)*)\1""")
_LECTEUR = re.compile(r"^[A-Za-z]:[/\\]")


def EstAbsolu(valeur):
    """Un chemin de CETTE machine : « C:/... », ou « /usr/... ».

    « / » seul (un separateur passe a replace) et les suffixes que le kit colle a
    KIT_ROOT (« /lib/Debug-Windows », « /include ») n'en sont pas.
    """
    if _LECTEUR.match(valeur):
        return True
    if not valeur.startswith("/") or valeur.startswith("/lib/") or valeur.startswith("/include"):
        return False
    return len([s for s in valeur.split("/") if s]) >= 2

_DOSSIER_KIT = re.compile(r"""KIT_ROOT\s*\+\s*(['"])/(lib/[^'"]+)\1""")


def NomsDeLiens(texte):
    """Tous les noms passes a links([...]) et listes dans KIT_SYSTEM_LIBS."""
    noms = set()
    for ligne in texte.splitlines():
        nu = ligne.strip()
        if nu.startswith("#"):
            continue
        if "links(" in nu or re.match(r"\(\s*['\"]\w+['\"]\s*,\s*['\"]\w+['\"]\s*\)\s*:", nu):
            for _, valeur in _CHAINE.findall(nu):
                if not EstAbsolu(valeur) and "/" not in valeur:
                    noms.add(valeur)
    return noms


def FichiersDe(dossier, nom):
    """Les formes sous lesquelles une bibliotheque `nom` peut exister dans `dossier`."""
    for forme in ("%s.lib", "lib%s.a", "%s.a", "lib%s.dll.a", "lib%s.so", "lib%s.dylib"):
        f = dossier / (forme % nom)
        if f.is_file():
            yield f


def RendreAutonome(kit, nomKit):
    """Embarque les bibliotheques cherchees dans un dossier absolu, puis retire ce dossier."""
    fichier = kit / (nomKit + ".jenga")
    texte = fichier.read_text(encoding="utf-8")
    noms = NomsDeLiens(texte)
    embarques = []
    lignes = texte.split("\n")
    for i, ligne in enumerate(lignes):
        if "libdirs(" not in ligne or ligne.strip().startswith("#"):
            continue
        dans_kit = _DOSSIER_KIT.search(ligne)
        absolus = [v for _, v in _CHAINE.findall(ligne) if EstAbsolu(v)]
        if not absolus:
            continue
        if not dans_kit:
            raise SystemExit("ligne libdirs sans dossier du kit, je ne sais pas ou embarquer :\n  " + ligne)
        destination = kit / dans_kit.group(2)
        for a in absolus:
            source = Path(a)
            trouve = False
            for nom in sorted(noms):
                for f in FichiersDe(source, nom):
                    shutil.copy2(f, destination / f.name)
                    embarques.append((f, destination.relative_to(kit)))
                    trouve = True
            if not trouve:
                Dire("  /!\\ %s : aucune des bibliotheques liees n'y est -- dossier retire sans rien embarquer" % a)
        indentation = ligne[:len(ligne) - len(ligne.lstrip())]
        lignes[i] = '%slibdirs([KIT_ROOT + "/%s"])' % (indentation, dans_kit.group(2))
    texte = "\n".join(lignes)
    fichier.write_text(texte, encoding="utf-8", newline="\n")
    return embarques


def Verifier(kit, nomKit):
    """Aucun chemin absolu ne reste ; le .jenga est du Python qui se compile."""
    fichier = kit / (nomKit + ".jenga")
    texte = fichier.read_text(encoding="utf-8")
    restes = []
    for n, ligne in enumerate(texte.splitlines(), 1):
        if ligne.strip().startswith("#"):
            continue
        for _, valeur in _CHAINE.findall(ligne):
            if EstAbsolu(valeur):
                restes.append("%d: %s" % (n, valeur))
    if restes:
        raise SystemExit("le kit garde des chemins de CETTE machine :\n  " + "\n  ".join(restes))
    compile(texte, str(fichier), "exec")
    cibles = sorted(p.name for p in (kit / "lib").iterdir() if p.is_dir())
    if not cibles:
        raise SystemExit("le kit n'a aucune cible dans lib/")
    return cibles


def Noter(kit, embarques, cibles):
    note = kit / "KIT.txt"
    ajout = ["", "Rendu autonome par scripts/MakeKit.py le %s." % datetime.datetime.now().strftime("%Y-%m-%d %H:%M"),
             "Cibles : " + ", ".join(cibles) + "."]
    if embarques:
        ajout.append("Bibliotheques externes embarquees (elles etaient cherchees dans un dossier de la machine d'origine) :")
        for source, destination in embarques:
            ajout.append("  %s  <-  %s" % (str(destination / source.name).replace("\\", "/"), str(source).replace("\\", "/")))
        ajout.append("vulkan-1 est la bibliotheque d'import du chargeur Vulkan (Khronos, Apache-2.0). A l'execution,")
        ajout.append("le programme demande vulkan-1.dll, que les pilotes graphiques installent.")
    ancien = note.read_text(encoding="utf-8") if note.exists() else ""
    note.write_text(ancien.rstrip("\n") + "\n" + "\n".join(ajout) + "\n", encoding="utf-8", newline="\n")


def Archiver(kit, nomKit, sortie):
    archive = sortie / ("%s-kit-%s.zip" % (nomKit, Systeme()))
    if archive.exists():
        archive.unlink()
    total = 0
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for f in sorted(kit.rglob("*")):
            if f.is_file():
                z.write(f, (Path(nomKit) / f.relative_to(kit)).as_posix())
                total += 1
    return archive, total


def main():
    ap = argparse.ArgumentParser(description="Fabrique le kit autonome d'une bibliotheque publique de Nkentseu.")
    ap.add_argument("kit", nargs="?", help="un kit connu : " + ", ".join(sorted(KITS_CONNUS)))
    ap.add_argument("--name", help="nom du kit (avec --target, pour un kit hors liste)")
    ap.add_argument("--target", help="modules racines, separes par des virgules")
    ap.add_argument("--config", action="append", help="Debug, Release (repetable ; defaut : les deux)")
    ap.add_argument("--sortie", help="dossier de sortie (defaut : D:/Rihen/Livraisons/Kits_<date>)")
    ap.add_argument("--sans-archive", action="store_true", help="ne pas ecrire le .zip")
    args = ap.parse_args()

    if args.kit:
        if args.kit not in KITS_CONNUS:
            raise SystemExit("kit inconnu : %s. Connus : %s" % (args.kit, ", ".join(sorted(KITS_CONNUS))))
        nom, cibles = args.kit, KITS_CONNUS[args.kit]
    elif args.name and args.target:
        nom, cibles = args.name, [c.strip() for c in args.target.split(",") if c.strip()]
    else:
        ap.error("donner un kit connu, ou --name et --target")

    configs = args.config or ["Debug", "Release"]
    sortie = Path(args.sortie) if args.sortie else Path("D:/Rihen/Livraisons") / ("Kits_" + datetime.date.today().isoformat())
    kit = sortie / "Kits" / nom
    kit.parent.mkdir(parents=True, exist_ok=True)

    LancerJengaKit(nom, cibles, configs, kit)
    embarques = RendreAutonome(kit, nom)
    for source, destination in embarques:
        Dire("embarque : %s -> %s" % (source, destination))
    cibles_presentes = Verifier(kit, nom)
    Noter(kit, embarques, cibles_presentes)
    Dire("kit autonome : %s (%s)" % (kit, ", ".join(cibles_presentes)))
    if not args.sans_archive:
        archive, total = Archiver(kit, nom, sortie)
        Dire("archive : %s (%d fichiers, %.1f Mo)" % (archive, total, archive.stat().st_size / 1e6))
    Dire("fichier a designer dans NKCode (Nouveau projet) : %s" % (kit / (nom + ".jenga")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
