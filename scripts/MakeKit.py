#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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

# Ce qu'un kit embarque EN PLUS de ses bibliotheques : les fichiers que ses modules
# LISENT A L'EXECUTION. NKRenderer charge ses nuanceurs dans Resources/NKRenderer/Shaders,
# relatif au dossier courant ; sans eux il refuse de s'initialiser (constate le 27/09/2026
# sur le code du volume III, et de nouveau le 08/10 : le kit construisait, le programme de
# l'etudiant ne demarrait pas). Le projet qui utilise le kit les copie chez lui (postbuild).
#
# (09/10/2026) La table est tenue PAR MODULE, et non plus par nom de kit. Un kit nomme a la main
# (`--name R3D --target NKRenderer,...`) contient NKRenderer sans s'appeler « Renderer » : il
# sortait sans nuanceurs, et le programme de l'etudiant ne demarrait pas -- le defaut du 08/10,
# revenu par un autre nom.
RESSOURCES_PAR_MODULE = {
    "NKRenderer": ["Resources/NKRenderer/Shaders"],
}

RACINE = Path(__file__).resolve().parent.parent


def Dire(message):
    print("[kit] " + message, flush=True)


def Systeme():
    nom = platform.system()
    return {"Windows": "Windows", "Linux": "Linux", "Darwin": "macOS"}.get(nom, nom)


def CommandeJenga():
    """Le Jenga de CE Python s'il l'a (`python -m Jenga`), sinon le `jenga` du PATH.

    Le lanceur `jenga.exe` de pip est un binaire non signe : Smart App Control le refuse
    sur certaines machines (constate le 08/10/2026, « Permission denied »). Le Python qui
    fait tourner ce script, lui, tourne deja.
    """
    try:
        import Jenga  # noqa: F401
        return [sys.executable, "-m", "Jenga"]
    except ImportError:
        return ["jenga"]


def LancerJengaKit(nom, cibles, configs, dossier, plateformes=()):
    commande = CommandeJenga() + ["kit", "--target", ",".join(cibles), "--name", nom,
                                  "--output", str(dossier).replace("\\", "/"), "--force"]
    for c in configs:
        commande += ["--config", c]
    # (04/11/2026) plateformes croisees : `jenga kit --platform Android-arm64` ; sans option, l'hote (inchange).
    for p in plateformes:
        commande += ["--platform", p]
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


_FILTRE_WINDOWS = re.compile(r'^(\s*)with filter\("system:Windows && configurations:\w+"\):\s*$')


def LienStatiqueWindows(kit, nomKit):
    """(10/10/2026) Un programme lie a un kit Windows tire la runtime de son compilateur. Avec
    llvm-mingw (celui que NKCode embarque), elle est en DLL par defaut : l'executable de l'etudiant
    s'arretait sur « libunwind.dll introuvable » (code 127) des qu'on le lancait hors de `jenga run`
    (mesure par la session du site, kit Mon3D). La chaine du moteur lie deja tout en statique
    (config/toolchain.jenga) ; le kit demande la meme chose au programme qui l'utilise."""
    fichier = kit / (nomKit + ".jenga")
    lignes = fichier.read_text(encoding="utf-8").split("\n")
    sortie, ajouts = [], 0
    for i, ligne in enumerate(lignes):
        sortie.append(ligne)
        if _FILTRE_WINDOWS.match(ligne):
            suivante = lignes[i + 1] if i + 1 < len(lignes) else ""
            retrait = suivante[:len(suivante) - len(suivante.lstrip())] or (_FILTRE_WINDOWS.match(ligne).group(1) + "    ")
            sortie.append(retrait + "# la runtime du compilateur (libunwind, libc++) dans l'executable, et pas en DLL")
            sortie.append(retrait + 'ldflags(["-static"])')
            ajouts += 1
    fichier.write_text("\n".join(sortie), encoding="utf-8", newline="\n")
    Dire("lien statique demande sous Windows : %d filtre(s)" % ajouts)


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


def ModulesDuKit(kit):
    """Les modules reellement presents dans le kit, lus dans ses bibliotheques (libNKRenderer.a,
    NKRenderer.lib...) : ce que le kit CONTIENT, pas ce qu'on a demande ni son nom."""
    presents = set()
    for f in (kit / "lib").rglob("*"):
        if f.is_file():
            nom = f.name
            if nom.startswith("lib"):
                nom = nom[3:]
            presents.add(nom.split(".")[0])
    return presents


def EmbarquerRessources(kit, nomKit):
    """Copie dans le kit les dossiers que ses modules lisent a l'execution. Rend leur liste."""
    faits = []
    presents = ModulesDuKit(kit)
    voulus = []
    for module, dossiers in sorted(RESSOURCES_PAR_MODULE.items()):
        if module in presents:
            voulus += [d for d in dossiers if d not in voulus]
    for rel in voulus:
        source = RACINE / rel
        if not source.is_dir():
            raise SystemExit("ressource du kit introuvable dans le depot : %s" % source)
        destination = kit / rel
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(source, destination)
        faits.append((rel, sum(1 for f in destination.rglob("*") if f.is_file())))
    return faits


_USAGE = re.compile(r'^def (use\w*)\(modules=None')
_SELECTION = "    selection = _kit_selection(modules)"

# Le texte ajoute au .jenga du kit (Python lu par Jenga chez l'etudiant). ASCII seulement.
_RESSOURCES_JENGA = '''# (31/10/2026, scripts/MakeKit.py) LES RESSOURCES LUES A L'EXECUTION, POSEES A COTE DE L'EXECUTABLE.
# NKRenderer cherche ses nuanceurs dans Resources/NKRenderer/Shaders : le dossier courant d'abord, puis a
# cote de l'executable. Sans eux, il se rabattait sur des nuanceurs embarques incomplets (OpenGL :
# "softness is not a member of struct ShadowUBO" ; Vulkan : "ShadowLinear ... introuvable" ; image noire).
# Le kit les copie donc lui-meme apres chaque construction, a cote de l'executable, FIDELEMENT (le dossier
# est vide d'abord : rien d'un ancien kit ne reste), par une commande du systeme et sans Python (sous
# NKCode, le Python de Jenga est NKCode lui-meme).
KIT_RESSOURCES = %s

def _kit_ressources_apres_construction(selection):
    for _module, _dossiers in sorted(KIT_RESSOURCES.items()):
        if _module not in selection:
            continue
        for _rel in _dossiers:
            _src = KIT_ROOT + "/" + _rel
            _dst = "%%{prj.targetdir}/" + _rel
            with filter("system:Windows"):
                postbuild(['if exist "%%s" rmdir /S /Q "%%s"' %% (_dst, _dst),
                           'xcopy /E /I /Y /Q "%%s" "%%s"' %% (_src, _dst)])
            for _systeme in ("Linux", "macOS"):
                with filter("system:" + _systeme):
                    postbuild(['rm -rf "%%s" && mkdir -p "%%s" && cp -R "%%s/." "%%s"' %% (_dst, _dst, _src, _dst)])

'''


def RessourcesApresConstruction(kit, nomKit):
    """(31/10/2026) LE KIT POSE LUI-MEME SES RESSOURCES A COTE DE L'EXECUTABLE. Jusqu'ici le projet de
    l'etudiant devait les copier par son propre postbuild ; MaSalle (Cours-NKRHI-NKRenderer) le faisait par
    « sys.executable -c shutil.copytree », et sous NKCode sys.executable etait NKCode : la copie n'avait
    pas lieu, NKRenderer prenait ses nuanceurs embarques, et l'etudiant voyait « softness is not a member
    of struct ShadowUBO » (OpenGL) et « ShadowLinear/VK/shadowlinear.vert.vk.glsl introuvable » (Vulkan),
    image noire (NKCode-Beta #17, et le signalement de la plateforme du 31/10). La fonction d'usage du kit
    (user3d, usenkrenderer...) ajoute donc un postbuild : la copie fidele des dossiers que ses modules lisent
    a l'execution, a cote de l'executable, ou NKRenderer les trouve quel que soit le dossier de lancement.
    Rend le nombre d'appels poses (0 : le kit n'a rien a copier)."""
    presents = ModulesDuKit(kit)
    table = {m: d for m, d in sorted(RESSOURCES_PAR_MODULE.items()) if m in presents}
    if not table:
        return 0
    fichier = kit / (nomKit + ".jenga")
    lignes = fichier.read_text(encoding="utf-8").split("\n")
    sortie, poses, dans_usage = [], 0, False
    for ligne in lignes:
        if _USAGE.match(ligne):
            sortie.extend((_RESSOURCES_JENGA % repr(table)).split("\n"))
            dans_usage = True
        sortie.append(ligne)
        if dans_usage and ligne.rstrip() == _SELECTION:
            sortie.append("    _kit_ressources_apres_construction(selection)  # (31/10/2026) les nuanceurs a cote de l'executable")
            poses += 1
            dans_usage = False
    if poses == 0:
        raise SystemExit("ressources du kit : aucune fonction d'usage avec « %s » dans %s" % (_SELECTION.strip(), fichier))
    fichier.write_text("\n".join(sortie), encoding="utf-8", newline="\n")
    return poses


def Noter(kit, embarques, cibles, ressources=()):
    note = kit / "KIT.txt"
    ajout = ["", "Rendu autonome par scripts/MakeKit.py le %s." % datetime.datetime.now().strftime("%Y-%m-%d %H:%M"),
             "Cibles : " + ", ".join(cibles) + "."]
    if ressources:
        ajout.append("")
        ajout.append("RESSOURCES LUES A L'EXECUTION (dans ce kit) :")
        for rel, n in ressources:
            ajout.append("  %s  (%d fichiers)" % (rel, n))
        ajout.append("Le programme les cherche RELATIVEMENT AU DOSSIER COURANT, puis a cote de l'executable.")
        # (31/10/2026) ancien conseil, garde pour memoire : « Copiez-les par un postbuild de votre .jenga (voir le
        # projet de depart livre avec le kit), et lancez le programme depuis le dossier de votre projet. »
        ajout.append("La fonction d'usage du kit les COPIE ELLE-MEME a cote de l'executable apres chaque construction")
        ajout.append("(copie fidele : le dossier est vide d'abord). Un dossier Resources/ dans le dossier courant passe")
        ajout.append("en premier : s'il vient d'un ancien kit, supprimez-le, ou copiez-le aussi fidelement.")
    if embarques:
        ajout.append("Bibliotheques externes embarquees (elles etaient cherchees dans un dossier de la machine d'origine) :")
        for source, destination in embarques:
            ajout.append("  %s  <-  %s" % (str(destination / source.name).replace("\\", "/"), str(source).replace("\\", "/")))
        ajout.append("vulkan-1 est la bibliotheque d'import du chargeur Vulkan (Khronos, Apache-2.0). A l'execution,")
        ajout.append("le programme demande vulkan-1.dll, que les pilotes graphiques installent.")
    ancien = note.read_text(encoding="utf-8") if note.exists() else ""
    note.write_text(ancien.rstrip("\n") + "\n" + "\n".join(ajout) + "\n", encoding="utf-8", newline="\n")


def Archiver(kit, nomKit, sortie, systeme=None):
    archive = sortie / ("%s-kit-%s.zip" % (nomKit, systeme or Systeme()))
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
    ap.add_argument("--platform", action="append",
                    help="plateforme croisee, OS[-arch] : Android-arm64, Web, HarmonyOS-arm64... (repetable ; defaut : l'hote)")
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

    LancerJengaKit(nom, cibles, configs, kit, args.platform or ())
    embarques = RendreAutonome(kit, nom)
    LienStatiqueWindows(kit, nom)
    for source, destination in embarques:
        Dire("embarque : %s -> %s" % (source, destination))
    cibles_presentes = Verifier(kit, nom)
    ressources = EmbarquerRessources(kit, nom)
    for rel, n in ressources:
        Dire("ressources : %s (%d fichiers)" % (rel, n))
    if ressources:
        Dire("ressources copiees a cote de l'executable par le kit : %d fonction(s) d'usage" % RessourcesApresConstruction(kit, nom))
        Verifier(kit, nom)  # le .jenga change : toujours aucun chemin absolu, toujours du Python qui se compile
    Noter(kit, embarques, cibles_presentes, ressources)
    Dire("kit autonome : %s (%s)" % (kit, ", ".join(cibles_presentes)))
    if not args.sans_archive:
        archive, total = Archiver(kit, nom, sortie, "+".join(args.platform) if args.platform else None)
        Dire("archive : %s (%d fichiers, %.1f Mo)" % (archive, total, archive.stat().st_size / 1e6))
    Dire("fichier a designer dans NKCode (Nouveau projet) : %s" % (kit / (nom + ".jenga")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
