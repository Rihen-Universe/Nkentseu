# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# TEMOIN : « LE PONT GENIA -- UNE IMAGE ENTRE, UN OBJET IMPORTE SORT »
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# Ce que ce temoin mesure : la chaine COMPLETE du modeleur, par le code de
# production -- NkGeniaImporterImage -> NkIGenerateur (processus externe
# Tools/Genia/genia_triposr.py) -> glTF -> NkImportFiles (NkGLTFLoader,
# decoupe, creation, archive, .nkmesh). Le crochet NK_GENIA_IMAGE appelle la
# MEME fonction que la confirmation du selecteur « Generer ».
# Il ne mesure NI le bouton NI le selecteur : un clic humain reste a faire.
#
# LE CRITERE, ECRIT AVANT LA MESURE (lu dans le journal du modeleur, logs/) :
#   POSITIF (image reelle) :
#     [genia] MESURE projet jetable : ... -> cree
#     [genia] MESURE generation : '<...>.glb' ecrit
#     [import] MESURE import : '<...>.glb' -> N model(s), N >= 1
#     [genia] MESURE crochet : ... -> importe, C carte(s) nee(s), C >= 1
#     et le .glb existe dans <projet jetable>/Genia/
#   NEGATIF (image inexistante) :
#     [genia] MESURE crochet : ... -> REFUSE, 0 carte(s) nee(s)
#     et AUCUN .glb dans <projet jetable>/Genia/
#   EFFETS DE BORD INTERDITS :
#     ~/.nk3dmodeler_recent.cfg identique a l'octet avant/apres (un temoin ne
#     s'inscrit pas dans les recents de Rodolf) ; NKIlyana LUE, jamais touchee.
#
# Aucune capture d'ecran : le temoin lit le JOURNAL, pas les pixels. La fenetre
# du modeleur s'ouvre (c'est la sienne) et se ferme seule (NK_AGENT_EXIT).
#
# Usage, depuis la racine du worktree :
#   python Applications/NK3DModeler/docs/temoin_genia_pont.py [image]
# -----------------------------------------------------------------------------
import glob
import hashlib
import os
import subprocess
import sys
import tempfile
import time

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
EXE = os.path.join(RACINE, "Build", "Bin", "Release-Windows", "NK3DModeler", "NK3DModeler.exe")
OUTILS = os.path.join(os.path.dirname(RACINE), "genia-tools")
PYTHON_GENIA = os.path.join(OUTILS, "venv", "Scripts", "python.exe")
IMAGE_DEFAUT = os.path.join(OUTILS, "TripoSR", "examples", "chair.png")
RECENTS = os.path.join(os.path.expanduser("~"), ".nk3dmodeler_recent.cfg")


def empreinte(p):
    if not os.path.isfile(p):
        return "ABSENT"
    return hashlib.sha256(open(p, "rb").read()).hexdigest()[:16]


def ilyana():
    out = subprocess.run(["tasklist"], capture_output=True, text=True).stdout
    return [l.split()[0] + " " + l.split()[1] for l in out.splitlines() if "NKIlyana" in l]


def course(image):
    parent = tempfile.mkdtemp(prefix="genia_temoin_")
    env = dict(os.environ)
    env["NK_GENIA_PROJET"] = parent.replace("\\", "/")
    env["NK_GENIA_IMAGE"] = image.replace("\\", "/")
    env["NK_GENIA_PYTHON"] = PYTHON_GENIA.replace("\\", "/")
    env["NK_GENIA_SCRIPT"] = os.path.join(RACINE, "Tools", "Genia", "genia_triposr.py").replace("\\", "/")
    # La generation BLOQUE la frame 10 : la sortie a la frame 60 arrive apres.
    env["NK_AGENT_EXIT"] = "60"
    for k in ("NK_IMPORT_FILE", "NK_OPEN_RECENT", "NK_OS_DROP", "NK_PICKER_IMPORT"):
        env.pop(k, None)
    t0 = time.time()
    r = subprocess.run([EXE], cwd=RACINE, env=env, capture_output=True, timeout=900)
    duree = time.time() - t0
    journaux = [p for p in glob.glob(os.path.join(RACINE, "logs", "app_*.log")) if os.path.getmtime(p) >= t0 - 1]
    journaux.sort(key=os.path.getmtime)
    texte = ""
    if journaux:
        texte = open(journaux[-1], "rb").read().decode("utf-8", "replace")
    lignes = [l for l in texte.splitlines() if "[genia]" in l or "MESURE import" in l or "Import impossible" in l
              or "Generation impossible" in l]
    glbs = glob.glob(os.path.join(parent, "GeniaTemoin", "Genia", "*.glb"))
    return {"code": r.returncode, "duree": duree, "journal": journaux[-1] if journaux else None,
            "lignes": lignes, "glbs": glbs, "parent": parent}


def contient(lignes, *morceaux):
    return any(all(m in l for m in morceaux) for l in lignes)


def cartes(lignes):
    for l in lignes:
        if "MESURE crochet" in l and "carte(s) nee(s)" in l:
            try:
                return int(l.split(",")[-1].strip().split(" ")[0])
            except ValueError:
                return -1
    return -1


def main():
    image = sys.argv[1] if len(sys.argv) > 1 else IMAGE_DEFAUT
    rouge = 0

    def attendu(ok, quoi):
        nonlocal rouge
        print("  [%s] %s" % ("VERT " if ok else "ROUGE", quoi))
        if not ok:
            rouge += 1

    print("Ilyana avant :", ilyana() or "ABSENTE")
    rec0 = empreinte(RECENTS)
    print("recents avant :", rec0)

    print("\n-- POSITIF : image reelle %s --" % image)
    p = course(image)
    print("   code=%d duree_s=%.1f journal=%s" % (p["code"], p["duree"], p["journal"]))
    for l in p["lignes"]:
        print("   | " + l.strip()[:220])
    attendu(contient(p["lignes"], "MESURE projet jetable", "cree"), "projet jetable cree")
    attendu(contient(p["lignes"], "MESURE generation", "ecrit"), "le generateur a ecrit le glTF")
    attendu(contient(p["lignes"], "MESURE import", ".glb") and not contient(p["lignes"], "-> 0 model(s)"),
            "l'import existant a lu le glTF (>= 1 model)")
    attendu(cartes(p["lignes"]) >= 1, "au moins une carte nee (mesure=%d)" % cartes(p["lignes"]))
    attendu(len(p["glbs"]) == 1, "un .glb dans <projet>/Genia/ (mesure=%d)" % len(p["glbs"]))

    print("\n-- NEGATIF : image inexistante --")
    n = course(os.path.join(p["parent"], "cette_image_n_existe_pas.png"))
    for l in n["lignes"]:
        print("   | " + l.strip()[:220])
    attendu(contient(n["lignes"], "MESURE projet jetable", "cree"), "negatif : le projet jetable existe (le refus n'est pas celui du projet)")
    attendu(contient(n["lignes"], "MESURE crochet", "REFUSE") and cartes(n["lignes"]) == 0,
            "negatif : generation refusee, 0 carte (mesure=%d)" % cartes(n["lignes"]))
    attendu(len(n["glbs"]) == 0, "negatif : aucun .glb ecrit (mesure=%d)" % len(n["glbs"]))

    rec1 = empreinte(RECENTS)
    print("\nrecents apres :", rec1)
    attendu(rec0 == rec1, "les recents de Rodolf sont intacts")
    print("Ilyana apres :", ilyana() or "ABSENTE")
    print("\nVERDICT : %s (%d ligne(s) rouge(s))" % ("VERT" if rouge == 0 else "ROUGE", rouge))
    return 0 if rouge == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
