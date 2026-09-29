#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

LE BANC DES COMPTEURS DE RENDU — « sept zeros devant un cube visible »
==============================================================================

[!] CE QU'IL EMPECHE DE REVENIR. Rodolf, 26/09 a 02:12 : le panneau affichait
`Draw 0`, `Tris 0`, `Sommets 0`, `Lots 0`, `Lumieres 0`, `Ombreurs 0` pendant
qu'un cube se rendait a 134 images/s. `dt` et `FPS` etaient justes, `GPU` et
`CPU` disaient bien « -- » : le panneau n'etait pas mort, il RELAYAIT
FIDELEMENT UNE SOURCE QUI RENDAIT ZERO.

LA CAUSE, MESUREE (`NK_CPT_TRACE=1`) : `mStats` ET le command buffer de
`hst.ctx.renderer` rendaient TOUS DEUX zero. Les deux a zero, c'est un defaut
de POPULATION et non de moment -- on interrogeait un objet qui n'enregistre
rien. La vue 3D de NK3DModeler n'enregistre pas dans son propre renderer :
elle enregistre dans le command buffer DE L'EDITEUR, celui que
`Demo3DHostFrame` recoit en argument. Le `EndFrame()` qui fige `mStats` ne
tourne jamais pour ce chemin.

⚠️ LE CRITERE DE RODOLF, ET IL EST PLUS EXIGEANT QU'IL N'EN A L'AIR : « Tris et
   Sommets non nuls, ET QUI CHANGENT quand on ajoute un objet ». Un chiffre fixe
   non nul serait un autre mensonge -- c'est pourquoi c2 compare DEUX scenes et
   pas une.

⚠️ CE QU'IL NE MESURE PAS : les pixels. Il lit les compteurs SERVIS par la
   facade (`NK_CPT_TRACE`), pas le panneau peint. Le panneau, lui, est eprouve
   par la famille 28 de NKEditorKitTest.

Usage :
    python Applications/NK3DModeler/tests/sonde_compteurs.py
    python ... --exe <binaire d'AVANT le correctif> --negatif
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

RACINE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
EXE_DEFAUT = os.path.join(RACINE, "Build", "Bin", "Release-Windows", "NK3DModeler",
                          "NK3DModeler.exe")
gOk = 0
gKo = 0


def verdict(ident, cond, quoi):
    global gOk, gKo
    if cond:
        gOk += 1
        print("  [ ok ] %-4s %s" % (ident, quoi))
    else:
        gKo += 1
        print("  [FAIL] %-4s %s" % (ident, quoi))


def courir(exe, etat_dir, images, ajout=None):
    # ⚠️ LES COMPTEURS SONT ETEINTS PAR DEFAUT -- c'est une condition posee par
    #    Rodolf, et elle vaut aussi pour le banc : sans cette ligne, la facade
    #    n'est jamais appelee et le journal ne porte AUCUN « SERVI ». Ma premiere
    #    version rendait 0/3 pour cette seule raison, et les trois criteres
    #    accusaient le produit. *Un banc qui n'arme pas ce qu'il mesure mesure
    #    son propre oubli.*
    os.makedirs(etat_dir, exist_ok=True)
    with open(os.path.join(etat_dir, "nk3dmodeler_ui.cfg"), "w", encoding="utf-8") as f:
        f.write("compteurs=1\n")
    env = dict(os.environ)
    env["NK_SONDE"] = "1"
    env["NK_ETAT_SONDE"] = etat_dir
    env["NK_CPT_TRACE"] = "1"
    env["NK_PROJECT"] = os.path.join(etat_dir, "projet", "projet", "projet.nk3dm")
    env["NK_AGENT_EXIT"] = str(images)
    if ajout:
        env["NK_ADD_NODE2"] = ajout
    r = subprocess.run([exe], env=env, cwd=RACINE, capture_output=True, text=True,
                       errors="replace", timeout=300)
    # Le code de sortie n'est pas lu : il ment dans ce depot.
    return r.stdout + r.stderr


def servis(journal):
    """Les triplets SERVIS, dans l'ordre. C'est ce que la facade rend au
    panneau -- pas ce que la source interne contient."""
    out = []
    for m in re.finditer(r"SERVI draw=(\d+) tris=(\d+) som=(\d+) valide=(\d)", journal):
        out.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4) == "1"))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=EXE_DEFAUT)
    ap.add_argument("--negatif", action="store_true")
    a = ap.parse_args()
    if not os.path.isfile(a.exe):
        print("binaire introuvable : %s" % a.exe)
        return 2
    print("=== banc des compteurs de rendu -> %s ===" % a.exe)
    base = tempfile.mkdtemp(prefix="nk3d_cpt_")

    # ── c1 : UNE SCENE AVEC UN CUBE DONNE DES CHIFFRES NON NULS ─────────────
    j1 = courir(a.exe, os.path.join(base, "c1"), 120)
    v1 = servis(j1)
    dernier = v1[-1] if v1 else None
    if dernier:
        print("         servis : draw=%d tris=%d som=%d valide=%d" %
              (dernier[0], dernier[1], dernier[2], 1 if dernier[3] else 0))
    verdict("c1", bool(dernier) and dernier[3] and dernier[0] > 0 and dernier[1] > 0
            and dernier[2] > 0,
            "un cube rendu -> Draw, Tris et Sommets NON NULS, et declares valides")

    # ── c2 : LE CRITERE DE RODOLF -- ils CHANGENT quand on ajoute un objet ──
    #    Un chiffre fixe non nul serait un autre mensonge : c'est pour ca qu'on
    #    compare DEUX etats de la MEME execution, avant et apres l'ajout.
    j2 = courir(a.exe, os.path.join(base, "c2"), 140, ajout="2,0,80")
    v2 = servis(j2)
    distincts = []
    for t in v2:
        if not distincts or t[:3] != distincts[-1][:3]:
            distincts.append(t)
    if len(distincts) >= 2:
        print("         avant : draw=%d tris=%d som=%d  ->  apres : draw=%d tris=%d som=%d" %
              (distincts[0][0], distincts[0][1], distincts[0][2],
               distincts[-1][0], distincts[-1][1], distincts[-1][2]))
    verdict("c2", len(distincts) >= 2 and distincts[-1][1] > distincts[0][1]
            and distincts[-1][2] > distincts[0][2],
            "ajouter un objet FAIT MONTER Tris et Sommets (%d valeurs distinctes)"
            % len(distincts))

    # ── c3 : LE NEGATIF -- avant la premiere frame, RIEN n'est valide ───────
    #    Sans lui, c1 passerait sur un binaire qui declarerait tout valide dès
    #    le depart et servirait des zeros : *un zero affiche la ou rien n'a ete
    #    mesure est indiscernable d'une scene vide* -- le defaut du 26/09.
    verdict("c3", bool(v1) and (not v1[0][3] or v1[0][0] > 0),
            "NEGATIF : la premiere mesure servie est soit INVALIDE, soit deja non nulle "
            "-- jamais « valide et zero »")

    print("RESULTAT : %d/%d" % (gOk, gOk + gKo))
    if a.negatif:
        # Sur le binaire d'AVANT, la facade ne trace rien (`SERVI` n'existe pas)
        # ET les compteurs valaient zero. Les trois criteres doivent rougir.
        print("NEGATIF %s : c1, c2 et c3 devaient rougir." %
              ("CONFORME" if gKo == 3 else "NON CONFORME"))
        return 0 if gKo == 3 else 1
    return 0 if gKo == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
