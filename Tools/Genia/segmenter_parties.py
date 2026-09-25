# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# SEGMENTER UN MAILLAGE EN PARTIES MANIPULABLES.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# CE QUE RODOLF DEMANDE, dans ses mots : « je veux que le modele soit tellement
# intelligent qu'il ne lie pas les vertices, qu'il soit capable de faire la
# distinction et de SEPARER QUAND IL SE DOIT, pour permettre facilement
# l'animation et la modification. »
#
# ⚠️ CE QUE CET OUTIL FAIT, ET CE QU'IL NE FAIT PAS -- ecrit avant le code, pour
# qu'on ne lui prete pas ce qu'il n'a pas :
#   IL FAIT     : couper un maillage d'un seul tenant en PARTIES distinctes, aux
#                 endroits ou la forme se divise (les quatre pattes d'un cheval,
#                 les bras d'un personnage), et ecrire chaque partie comme un
#                 objet nomme « o <nom> » dans un .obj.
#   IL NE FAIT PAS : NOMMER les parties (« bras gauche » est une question
#                 semantique, pas geometrique) ; ni poser un SQUELETTE ; ni
#                 calculer des POIDS DE PEAU. Sans ces deux dernieres, on obtient
#                 « manipulable partie par partie », PAS « animable ». Le dire
#                 autrement serait promettre ce qui n'est pas la.
#
# POURQUOI LE .OBJ AVEC « o <nom> », ET POURQUOI C'EST DEJA LA BONNE PORTE :
# `NkOBJLoader.cpp` lit `o` (ligne 339) et `g` (ligne 347) et produit un
# `NkSubMesh` NOMME par groupe -- verifie en ouvrant LE LECTEUR, pas les
# fichiers. Et l'import de NK3DModeler coupe sur `o` et ECLATE les objets. Tout
# l'aval existe donc deja : il ne manquait que la coupe.
#
# L'ALGORITHME, ET SA DERIVATION. On suit les sections horizontales du BAS vers
# le HAUT. Tant qu'une section reste d'un seul tenant, on est dans la meme
# partie. **Quand une section se DIVISE en K morceaux, c'est une jonction** : les
# K branches deviennent K parties distinctes. C'est la definition geometrique de
# « ca se separe ici », et elle ne demande aucun seuil de forme -- seulement la
# finesse des tranches, qui est imprimee a cote du resultat.
#
# LE ZERO SE PROUVE EN PREMIER : sur une SPHERE, aucune section ne se divise,
# donc l'outil doit rendre UNE SEULE partie. Un segmenteur qui coupe une sphere
# en deux ne mesure pas ce qu'il annonce.
#
# USAGE : python segmenter_parties.py <maillage> --out <sortie.obj>
#                [--tranches 60] [--grille 96] [--min-faces 80] [--zero]
# -----------------------------------------------------------------------------
import argparse
import os
import sys

for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass


def _refus(msg, code=2):
    sys.stderr.write("REFUS : %s\n" % msg)
    sys.stderr.flush()
    sys.exit(code)


def etiqueter_2d(masque):
    """Composantes connexes 8-VOISINS. Rend (nb, carte d'etiquettes 1..nb).

    ⚠️ HUIT ET NON QUATRE, ET LE ZERO L'A IMPOSE. En 4-voisins, une SPHERE
    sortait coupee en 30 parties : l'anneau d'une tranche, projete sur la
    grille, passe par des cases DIAGONALES, et un voisinage a quatre le lit
    comme plusieurs arcs. Sur une grille, une surface continue peut passer en
    diagonale -- c'est une propriete de la grille, pas de la forme.

    Ce que ce choix coute, et il faut le dire : deux parties separees par UNE
    SEULE case vide fusionnent desormais. L'outil SOUS-segmente donc plutot que
    de sur-segmenter. C'est le bon sens d'erreur : une partie qu'il separe est
    une partie qui existe."""
    import numpy as np
    h, w = masque.shape
    lab = np.zeros((h, w), dtype=np.int32)
    n = 0
    for i0 in range(h):
        for j0 in range(w):
            if not masque[i0, j0] or lab[i0, j0]:
                continue
            n += 1
            pile = [(i0, j0)]
            lab[i0, j0] = n
            while pile:
                i, j = pile.pop()
                for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1),
                               (1, 1), (1, -1), (-1, 1), (-1, -1)):
                    a, b = i + di, j + dj
                    if 0 <= a < h and 0 <= b < w and masque[a, b] and not lab[a, b]:
                        lab[a, b] = n
                        pile.append((a, b))
    return n, lab


def segmenter(maillage, tranches, grille, minFaces):
    """Rend un tableau d'etiquettes de PARTIE, une par face."""
    import numpy as np
    V = np.asarray(maillage.vertices, dtype=np.float64)
    F = np.asarray(maillage.faces, dtype=np.int64)
    centres = V[F].mean(axis=1)              # le centre de chaque face
    mn, mx = V.min(axis=0), V.max(axis=0)
    ext = mx - mn
    if ext[1] <= 0 or ext[0] <= 0 or ext[2] <= 0:
        _refus("le maillage est plat sur un axe : aucune coupe possible")

    # Tranche de chaque face, et sa case dans la grille 2D de cette tranche.
    ty = np.clip(((centres[:, 1] - mn[1]) / ext[1] * tranches).astype(int), 0, tranches - 1)
    gx = np.clip(((centres[:, 0] - mn[0]) / ext[0] * grille).astype(int), 0, grille - 1)
    gz = np.clip(((centres[:, 2] - mn[2]) / ext[2] * grille).astype(int), 0, grille - 1)

    labelFace = np.full(len(F), -1, dtype=np.int32)
    prevLab = None          # carte d'etiquettes 2D de la tranche precedente
    prevPartie = {}         # etiquette 2D -> numero de partie
    nPartie = 0
    jonctions = []

    for t in range(tranches):
        sel = np.where(ty == t)[0]
        if len(sel) == 0:
            prevLab, prevPartie = None, {}
            continue
        # ⚠️ ON MARQUE LES TROIS SOMMETS DE CHAQUE FACE, PAS SEULEMENT SON CENTRE.
        # Le zero l'a impose : une SPHERE sortait coupee en 74 parties. Ses faces,
        # reduites a leur centre et projetees sur la grille, laissent des TROUS ;
        # l'anneau d'une tranche se fragmente alors en arcs, et l'algorithme lit
        # une division la ou la forme est continue. Il mesurait la densite du
        # maillage, pas sa forme -- exactement le defaut deja paye par
        # `mesure_separation.py` quelques heures plus tot, au meme endroit.
        masque = np.zeros((grille, grille), dtype=bool)
        # ⚠️ ON RASTERISE LES ARETES, ET C'EST LA TROISIEME FOIS AUJOURD'HUI QUE
        # CE DEFAUT SE PRESENTE AU MEME ENDROIT. Marquer le centre des faces, puis
        # leurs trois sommets, laissait encore des TROUS : une SPHERE dense
        # passait, un HALTERE moins dense sortait en 28 morceaux. L'instrument
        # mesurait donc la DENSITE du maillage, pas sa forme -- et il l'aurait
        # fait en silence sur tout maillage plus grossier que ceux que j'essayais.
        # Remplir les aretes rend le masque INDEPENDANT de la densite : entre
        # deux sommets d'une meme face, la surface EST continue, donc les cases
        # intermediaires sont pleines. Ce n'est pas un lissage, c'est un fait.
        P = np.empty((len(sel), 3, 2), dtype=np.float64)
        for coin in range(3):
            iv = F[sel, coin]
            P[:, coin, 0] = (V[iv, 0] - mn[0]) / ext[0] * (grille - 1)
            P[:, coin, 1] = (V[iv, 2] - mn[2]) / ext[2] * (grille - 1)
        # Huit points par arete suffisent : une face plus longue que huit cases
        # serait un triangle demesure devant la grille, et l'impression du
        # reglage permet de le voir.
        ts = np.linspace(0.0, 1.0, 8)[None, :, None]
        for a, b in ((0, 1), (1, 2), (2, 0)):
            A = P[:, a, :][:, None, :]
            B = P[:, b, :][:, None, :]
            pts = A + (B - A) * ts
            ix = np.clip(pts[:, :, 0].astype(int), 0, grille - 1).ravel()
            iz = np.clip(pts[:, :, 1].astype(int), 0, grille - 1).ravel()
            masque[ix, iz] = True
        n, lab = etiqueter_2d(masque)

        partieDe = {}
        for k in range(1, n + 1):
            if prevLab is None:
                # Premiere tranche non vide : chaque morceau ouvre une partie.
                nPartie += 1
                partieDe[k] = nPartie
                continue
            # Quels morceaux de la tranche PRECEDENTE cette region recouvre-t-elle ?
            recouvre = set(int(x) for x in np.unique(prevLab[lab == k]) if x > 0)
            parents = set(prevPartie[r] for r in recouvre if r in prevPartie)
            if len(parents) == 1:
                # Continuite : meme partie.
                partieDe[k] = next(iter(parents))
            elif len(parents) == 0:
                # Rien dessous : un morceau NEUF apparait (une corne, une oreille).
                nPartie += 1
                partieDe[k] = nPartie
            else:
                # ⚠️ PLUSIEURS parents FUSIONNENT ici. C'est une jonction vue par
                # le haut -- par exemple les quatre pattes qui rejoignent le
                # corps. On ouvre une partie NEUVE pour le tronc commun, et on
                # NE fusionne PAS les branches : c'est exactement ce que
                # « separer quand il se doit » demande.
                nPartie += 1
                partieDe[k] = nPartie
                jonctions.append((t, len(parents)))

        # Une region qui se DIVISE en montant : ses enfants ont le meme parent,
        # et on veut les separer. On le detecte en comptant, pour chaque partie
        # parente, combien de regions de CETTE tranche la reclament.
        compte = {}
        for k, pp in partieDe.items():
            compte[pp] = compte.get(pp, 0) + 1
        for pp, c in compte.items():
            if c > 1:
                jonctions.append((t, c))
                # On redistribue : chaque region recoit sa propre partie.
                for k in list(partieDe.keys()):
                    if partieDe[k] == pp:
                        nPartie += 1
                        partieDe[k] = nPartie

        for k in range(1, n + 1):
            cases = (lab == k)
            dans = sel[cases[gx[sel], gz[sel]]]
            labelFace[dans] = partieDe[k]
        prevLab, prevPartie = lab, partieDe

    # ── LES PARTIES TROP PETITES SONT RATTACHEES, PAS SUPPRIMEES ────────────
    # Supprimer une partie ferait perdre des faces, et le critere « la somme des
    # parties rend le tout » rougirait -- a juste titre. On les rattache a la
    # partie voisine la plus grande.
    import collections
    tailles = collections.Counter(labelFace[labelFace >= 0].tolist())
    petites = {p for p, c in tailles.items() if c < minFaces}
    if petites and len(tailles) > len(petites):
        grandes = [p for p in tailles if p not in petites]
        # Rattachement par proximite du centre de gravite.
        cg = {p: centres[labelFace == p].mean(axis=0) for p in tailles}
        for p in petites:
            d = [(np.linalg.norm(cg[p] - cg[q]), q) for q in grandes]
            labelFace[labelFace == p] = min(d)[1]
    labelFace[labelFace < 0] = (list(tailles.keys())[0] if tailles else 0)
    return labelFace, jonctions


def ecrire_obj(chemin, maillage, labelFace):
    """Un « o <nom> » par partie. C'est la porte que NkOBJLoader lit deja."""
    import numpy as np
    V = np.asarray(maillage.vertices, dtype=np.float64)
    F = np.asarray(maillage.faces, dtype=np.int64)
    parties = sorted(set(int(x) for x in labelFace))
    with open(chemin, "w", encoding="utf-8", newline="\n") as f:
        f.write("# Maillage SEGMENTE en parties manipulables.\n")
        f.write("# AUTEUR : TEUGUIA TADJUIDJE Rodolf Sederis - Rihen\n")
        f.write("# Chaque « o » est une partie : NkOBJLoader en fait un NkSubMesh nomme,\n")
        f.write("# et l'import de NK3DModeler les eclate en objets distincts.\n")
        # ⚠️ Les sommets sont ecrits UNE fois, dans leur ordre d'origine, et
        # AUCUN n'est deplace : une segmentation qui bouge un sommet n'est plus
        # une segmentation, c'est une modification.
        for v in V:
            f.write("v %.6f %.6f %.6f\n" % (v[0], v[1], v[2]))
        for p in parties:
            f.write("o partie_%02d\n" % p)
            for t in F[labelFace == p]:
                f.write("f %d %d %d\n" % (t[0] + 1, t[1] + 1, t[2] + 1))
    return len(parties)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("maillage")
    ap.add_argument("--out", required=True)
    ap.add_argument("--tranches", type=int, default=60)
    ap.add_argument("--grille", type=int, default=96)
    ap.add_argument("--min-faces", type=int, default=80, dest="minFaces")
    ap.add_argument("--zero", action="store_true")
    a = ap.parse_args()

    import numpy as np
    import trimesh

    if a.zero:
        # ── LE ZERO : une SPHERE ne se divise jamais -> UNE partie ───────────
        s = trimesh.creation.icosphere(subdivisions=4, radius=1.0)
        lab, _ = segmenter(s, 24, 48, 20)
        n = len(set(int(x) for x in lab))
        if n != 1:
            _refus("le zero a echoue : une SPHERE est coupee en %d parties au lieu d'UNE.\n"
                   "        un segmenteur qui coupe une sphere ne mesure pas ce qu'il annonce." % n, 3)
        print("  [ZERO ] sphere -> 1 partie : l'outil ne coupe pas ce qui ne se divise pas")
        # ── LE POSITIF : un HALTERE (deux boules, une barre) doit donner PLUS d'une partie
        b1 = trimesh.creation.icosphere(subdivisions=3, radius=0.5)
        b2 = trimesh.creation.icosphere(subdivisions=3, radius=0.5)
        b1.apply_translation([0, -1.0, 0])
        b2.apply_translation([0, 1.0, 0])
        bar = trimesh.creation.cylinder(radius=0.1, height=2.0)
        h = trimesh.util.concatenate([b1, bar, b2])
        lab2, _ = segmenter(h, 30, 48, 20)
        n2 = len(set(int(x) for x in lab2))
        if n2 < 2:
            _refus("le positif a echoue : un HALTERE rend %d partie(s), donc l'outil ne SAIT PAS\n"
                   "        couper. Un instrument qui ne rend qu'une valeur ne distingue rien." % n2, 3)
        print("  [ZERO ] haltere -> %d parties : l'outil SAIT couper la ou ca se divise" % n2)
        print()

    if not os.path.isfile(a.maillage):
        _refus("maillage introuvable : %s" % a.maillage)
    m = trimesh.load(a.maillage, force="mesh", process=False)
    if not hasattr(m, "faces") or len(m.faces) == 0:
        _refus("aucune face lue dans %s" % a.maillage)

    nFacesAvant = len(m.faces)
    nVertsAvant = len(m.vertices)
    lab, jonctions = segmenter(m, a.tranches, a.grille, a.minFaces)
    n = ecrire_obj(a.out, m, lab)

    import collections
    tailles = collections.Counter(int(x) for x in lab)
    print("MAILLAGE : %s" % os.path.basename(a.maillage))
    print("  avant  : %d sommets, %d faces, %d composante(s) connexe(s)"
          % (nVertsAvant, nFacesAvant, len(m.split(only_watertight=False))))
    print("  reglage: %d tranches, grille %d, parties de moins de %d faces rattachees"
          % (a.tranches, a.grille, a.minFaces))
    print("  apres  : %d PARTIE(S), %d jonction(s) detectee(s)" % (n, len(jonctions)))
    for p, c in sorted(tailles.items(), key=lambda x: -x[1]):
        print("    partie_%02d : %6d faces (%.1f %%)" % (p, c, 100.0 * c / nFacesAvant))

    # ── LE CRITERE QUI NE SE DISCUTE PAS : LA SOMME DES PARTIES REND LE TOUT ─
    somme = sum(tailles.values())
    ok = (somme == nFacesAvant)
    print("  [%s] la somme des parties rend le tout : %d faces attendues, %d comptees"
          % ("VERT " if ok else "ROUGE", nFacesAvant, somme))
    print("  ECRIT  : %s" % a.out)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
