# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
#
# LA QUESTION (1) DU CHANTIER MULTI-VUES : trois reconstructions faites
# SEPAREMENT depuis trois photos du meme personnage SE RESSEMBLENT-ELLES ?
#
# POURQUOI ELLE PASSE AVANT TOUT CODE DE FUSION : si les trois volumes ne se
# recouvrent meme pas grossierement une fois remis dans le meme repere, alors
# AUCUNE methode de fusion ne les reconciliera -- ni le moyennage de champs de
# densite, ni l'union de maillages, ni le recalage. La fusion serait morte
# avant d'etre ecrite, et on l'aura su en une heure au lieu d'une semaine.
#
# ATTENDU, ECRIT AVANT LA MESURE :
#   A1. front contre lui-meme  -> IoU = 1,000. Si l'instrument ne sait pas dire
#       UN sur deux copies du meme volume, il ne sait rien dire. (le zero)
#   A2. front contre front TOURNE de 45 deg -> IoU NETTEMENT plus bas. Si une
#       rotation absurde ne fait pas chuter la mesure, le critere est aveugle
#       a l'orientation, donc inutilisable ici. (le negatif)
#   A3. front contre back  (rotation 180 deg autour de Y) -> c'est le VRAI test.
#       A4. front contre right (rotation 90 deg autour de Y).
#   Seuil que je me donne AVANT de voir : au-dessus de 0,5 les volumes decrivent
#   le meme objet et la fusion a un sens ; en dessous de 0,3 ils decrivent des
#   objets differents et la fusion par recouvrement est morte. Entre les deux,
#   c'est un recalage qu'il faudra, pas une superposition.
#
# ⚠️ CE QUE LA MESURE NE PEUT PAS DIRE : TripoSR NORMALISE chaque entree
# separement (resize_foreground puis pad to square). Les trois cadrages de
# Rodolf different (165x572, 163x559, 134x566), donc meme un objet parfaitement
# reconstruit trois fois n'aurait PAS la meme echelle. On teste donc AUSSI
# l'accord APRES mise a l'echelle par la boite englobante -- et l'ecart entre
# les deux chiffres dit si le probleme est la FORME ou seulement l'ECHELLE.

import sys
import numpy as np
import trimesh

RES = 64  # voxels par cote


def charger(chemin):
    m = trimesh.load(chemin, force='mesh')
    if isinstance(m, trimesh.Scene):
        m = trimesh.util.concatenate([g for g in m.geometry.values()])
    return m


def rot_y(deg):
    a = np.deg2rad(deg)
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def voxels(P, res=RES, boite=None):
    """Occupation d'une grille cubique. `boite` impose un repere commun."""
    if boite is None:
        mn, mx = P.min(axis=0), P.max(axis=0)
    else:
        mn, mx = boite
    d = np.maximum(mx - mn, 1e-9)
    q = np.floor((P - mn) / d.max() * (res - 1)).astype(int)
    q = np.clip(q, 0, res - 1)
    g = np.zeros((res, res, res), dtype=bool)
    g[q[:, 0], q[:, 1], q[:, 2]] = True
    return g


def echantillonner(m, n=200000, graine=12345):
    """Points sur la SURFACE. Graine fixe : deux mesures du meme fichier
    doivent rendre le meme chiffre."""
    P, _ = trimesh.sample.sample_surface(m, n, seed=graine)
    return np.asarray(P)


def iou(A, B):
    inter = np.logical_and(A, B).sum()
    union = np.logical_or(A, B).sum()
    return float(inter) / max(1, float(union))


def centrer_normer(P, normer):
    P = P - P.mean(axis=0)
    if normer:
        e = (P.max(axis=0) - P.min(axis=0)).max()
        P = P / max(e, 1e-9)
    return P


def comparer(Pa, Pb, deg, normer, etiquette):
    A = centrer_normer(Pa, normer)
    B = centrer_normer(Pb @ rot_y(deg).T, normer)
    mn = np.minimum(A.min(axis=0), B.min(axis=0))
    mx = np.maximum(A.max(axis=0), B.max(axis=0))
    v = iou(voxels(A, boite=(mn, mx)), voxels(B, boite=(mn, mx)))
    print('     %-46s IoU = %.3f' % (etiquette, v))
    return v


def main():
    d = sys.argv[1].rstrip('/')
    noms = ['front', 'back', 'right']
    P = {}
    print('=== LES TROIS RECONSTRUCTIONS ===')
    for n in noms:
        m = charger(d + '/' + n + '.glb')
        e = m.bounds[1] - m.bounds[0]
        P[n] = echantillonner(m)
        print('  %-6s %6d sommets %7d faces | boite %.3f x %.3f x %.3f | H/L = %.2f'
              % (n, len(m.vertices), len(m.faces), e[0], e[1], e[2], e[1] / max(1e-9, e[0])))

    for normer, titre in ((False, 'SANS remise a l echelle (les volumes tels quels)'),
                          (True, 'APRES remise a l echelle par la boite englobante')):
        print('')
        print('=== ' + titre + ' ===')
        print('   -- 1. LE ZERO : un volume contre lui-meme --')
        z = comparer(P['front'], P['front'], 0, normer, 'front contre front (0 deg)')
        print('   -- 2. LE NEGATIF : une rotation absurde --')
        ng = comparer(P['front'], P['front'], 45, normer, 'front contre front TOURNE de 45 deg')
        print('   -- 3. LE REEL --')
        rb = comparer(P['front'], P['back'], 180, normer, 'front contre back  (180 deg)')
        rr1 = comparer(P['front'], P['right'], 90, normer, 'front contre right ( 90 deg)')
        rr2 = comparer(P['front'], P['right'], -90, normer, 'front contre right (-90 deg)')
        print('   -- verdict de l instrument --')
        print('     sait dire UN  : %s | sait REFUSER : %s'
              % ('OUI' if z > 0.99 else 'NON *** INUTILISABLE ***',
                 'OUI' if ng < z - 0.05 else 'NON *** AVEUGLE A L ORIENTATION ***'))
        meilleur = max(rb, rr1, rr2)
        print('     meilleur accord reel : %.3f  -> %s' % (
            meilleur,
            'les volumes decrivent le MEME objet' if meilleur > 0.5 else
            ('RECALAGE necessaire, pas une superposition' if meilleur > 0.3 else
             'objets DIFFERENTS : la fusion par recouvrement est MORTE')))


if __name__ == '__main__':
    main()
