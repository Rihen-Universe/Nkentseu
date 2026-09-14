# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# TEMOIN : « TOUT IMPORT DIT SON RESULTAT A L'ECRAN »
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# Ce que ce temoin mesure, et pourquoi il mesure CA.
# ---------------------------------------------------
# Le 2026-09-05, Rodolf a signale pour la seconde fois « l'import a refuse ».
# Le refus etait JUSTE et il etait deja JOURNALISE -- une correction precedente
# l'avait ajoute. Il n'etait pas A L'ECRAN. Une sonde qui aurait lu le journal
# aurait donc ete VERTE pendant que Rodolf voyait le defaut : elle n'aurait pas
# mesure le meme objet. Ce temoin regarde donc les PIXELS de la fenetre, la
# seule chose que Rodolf regarde.
#
# Methode : deux courses dans MA fenetre, un seul reglage differe.
#   A. lancement SANS import               -> aucune incrustation attendue
#   B. lancement avec un import qui REFUSE -> une incrustation attendue
# On compte les pixels qui DIFFERENT dans la bande basse (la ou l'incrustation
# se peint). Peu de pixels = rien n'a ete peint.
#
# Il PEUT rougir, et c'est la seule chose qui le rend utile :
#   * `--mutation` neutralise l'appel a NkToastPaint dans main.cpp, reconstruit,
#     relance -- l'ecart doit s'effondrer, le temoin doit dire ROUGE -- puis
#     restaure le fichier DEPUIS UNE COPIE prise avant la mutation.
#   * un CONTROLE NEGATIF est joue a chaque fois : deux courses A comparees
#     entre elles doivent donner un ecart faible. Sans lui, un temoin qui
#     compte « beaucoup de pixels differents » applaudirait un curseur qui
#     clignote ou une horloge dans la barre d'etat.
#
# Usage :
#   python Applications/NK3DModeler/docs/temoin_import_visible.py
#   python Applications/NK3DModeler/docs/temoin_import_visible.py --mutation
# depuis la racine du worktree.
# -----------------------------------------------------------------------------
import io
import os
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
EXE = os.path.join(RACINE, "Build", "Bin", "Release-Windows", "NK3DModeler", "NK3DModeler.exe")
MAIN = os.path.join(RACINE, "Applications", "NK3DModeler", "src", "NK3DModeler", "main.cpp")
CAPTURES = os.path.join(RACINE, "captures")
FICHIER_IMPORT = "D:/Rodolf/manequin/XBot/XBot.fbx"

# Le seuil. Ecrit AVANT de lire le resultat (pre-enregistrement) : l'incrustation
# fait au minimum 380 x 60 px a l'echelle 1, soit ~22 000 pixels ; on demande la
# moitie pour tolerer une fenetre etroite et un texte court.
SEUIL_VU = 10000
# Le controle negatif ne doit presque rien bouger : la barre d'etat n'affiche pas
# d'horloge, mais le curseur de la souris et l'anticrenelage laissent du bruit.
SEUIL_BRUIT = 2000


def ilyana():
    """Ilyana entraine sur le GPU. On la LIT avant et apres, jamais on ne la touche.
    Le filtre /FI de tasklist ment : on filtre nous-memes."""
    out = subprocess.run(["tasklist"], capture_output=True, text=True).stdout
    return [l for l in out.splitlines() if "NKIlyana" in l]


def vider_captures():
    if os.path.isdir(CAPTURES):
        for f in os.listdir(CAPTURES):
            if f.startswith("tutoriel_") and f.endswith(".png"):
                os.remove(os.path.join(CAPTURES, f))


def course(avec_import, frame_shot=45):
    """Une course courte dans MA fenetre. La capture passe par PrintWindow
    (PW_RENDERFULLCONTENT) : elle photographie MA fenetre, jamais l'ecran."""
    vider_captures()
    env = dict(os.environ)
    env["NK_AGENT_SHOT"] = str(frame_shot)
    env["NK_AGENT_EXIT"] = str(frame_shot + 25)
    if avec_import:
        env["NK_IMPORT_FILE"] = FICHIER_IMPORT
    else:
        env.pop("NK_IMPORT_FILE", None)
    subprocess.run([EXE], cwd=RACINE, env=env, capture_output=True, timeout=180)
    pngs = sorted(
        f for f in os.listdir(CAPTURES) if f.startswith("tutoriel_") and f.endswith(".png")
    )
    if not pngs:
        return None
    return np.asarray(Image.open(os.path.join(CAPTURES, pngs[-1])).convert("RGB")).astype(np.int16)


def ecart_bande_basse(a, b):
    """Pixels differents dans le tiers bas de la fenetre -- la ou l'incrustation
    se peint. Comparer la fenetre ENTIERE ferait entrer la vue 3D, qui bouge
    d'une course a l'autre (la camera s'anime au demarrage)."""
    if a is None or b is None:
        return -1
    h = min(a.shape[0], b.shape[0])
    w = min(a.shape[1], b.shape[1])
    y0 = int(h * 0.62)
    da = a[y0:h, 0:w]
    db = b[y0:h, 0:w]
    return int(np.count_nonzero(np.abs(da - db).sum(axis=2) > 24))


def mutation_appliquer():
    src = io.open(MAIN, encoding="utf-8").read()
    io.open(MAIN + ".avant-mutation", "w", encoding="utf-8", newline="").write(src)
    cible = "\t\t\t(void)nk3d::NkToastPaint(hit, (float32)W, (float32)H, lay.status.h);"
    assert cible in src, "point de mutation introuvable"
    io.open(MAIN, "w", encoding="utf-8", newline="").write(
        src.replace(cible, "\t\t\t// MUTATION DU TEMOIN : l'incrustation n'est pas peinte")
    )


def mutation_restaurer():
    """Depuis la COPIE prise avant la mutation -- jamais par une re-edition
    inverse : une substitution qui ne retrouve pas sa cible laisserait le
    fichier mute en pretendant l'avoir remis."""
    shutil.copyfile(MAIN + ".avant-mutation", MAIN)
    os.remove(MAIN + ".avant-mutation")


def construire():
    r = subprocess.run(
        "jenga build --target NK3DModeler --config Release",
        cwd=RACINE, capture_output=True, timeout=900, shell=True,
    )
    # La sortie de jenga porte des couleurs ANSI et des caracteres hors cp1252 :
    # lue en TEXTE, elle levait un UnicodeDecodeError DANS LE FIL DE LECTURE, et
    # `r.stdout` revenait a None -- un build reussi passait alors pour un echec.
    # On lit des OCTETS et on decode nous-memes, en remplacant l'indecodable.
    sortie = (r.stdout or b"").decode("utf-8", "replace") + (r.stderr or b"").decode("utf-8", "replace")
    return "SUCCESS" in sortie


def main():
    mute = "--mutation" in sys.argv
    print("Ilyana avant :", ilyana() or "ABSENTE")

    print("\n-- controle negatif : deux courses SANS import --")
    a1 = course(False)
    a2 = course(False)
    bruit = ecart_bande_basse(a1, a2)
    print("   ecart de bruit : %d px (seuil %d)" % (bruit, SEUIL_BRUIT))

    print("\n-- mesure : course SANS import contre course AVEC un import qui REFUSE --")
    b = course(True)
    vu = ecart_bande_basse(a1, b)
    print("   ecart mesure   : %d px (seuil %d)" % (vu, SEUIL_VU))

    verdict = (bruit >= 0 and bruit < SEUIL_BRUIT) and (vu >= SEUIL_VU)
    print("\nVERDICT : %s" % ("VERT -- le refus SE VOIT" if verdict else "ROUGE -- rien a l'ecran"))

    if mute:
        print("\n== MUTATION : on eteint la peinture de l'incrustation ==")
        mutation_appliquer()
        try:
            if not construire():
                print("build mute : ECHEC")
                return 2
            bm = course(True)
            vum = ecart_bande_basse(a1, bm)
            print("   ecart mute : %d px (seuil %d)" % (vum, SEUIL_VU))
            rouge = vum < SEUIL_VU
            print("   MUTATION : %s" % ("ROUGE comme attendu" if rouge else "VERTE -- LE TEMOIN NE MESURE RIEN"))
        finally:
            mutation_restaurer()
            construire()
            print("   fichier restaure depuis la copie, binaire reconstruit")
        print("\nIlyana apres :", ilyana() or "ABSENTE")
        return 0 if (verdict and rouge) else 1

    print("\nIlyana apres :", ilyana() or "ABSENTE")
    return 0 if verdict else 1


if __name__ == "__main__":
    sys.exit(main())
