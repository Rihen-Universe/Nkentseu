# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
#
# GARDE D'ENTREE DU CORPUS : le modele est-il une SURFACE PLAUSIBLE ?
#
# D'OU VIENT CE CRITERE : du chantier retopologie (agent a4911d23a9e93008a),
# qui l'a trouve en mesurant une villa rendant 82 FACES PAR SOMMET la ou un
# maillage triangule ferme sain en compte 2. Gratuit, sans reglage.
#
# ⚠️ MAIS IL NE TRANCHE PAS SEUL, ET C'EST LE POINT QUI COMPTE.
# « F/V > 2 » a DEUX lectures possibles :
#     (a) ce n'est pas une surface -- des aretes non-manifold, des faces qui
#         se recouvrent, un soupe de triangles ;
#     (b) c'est une surface, mais de GENRE enorme -- une eponge a des milliers
#         de trous a legitimement beaucoup de faces par sommet.
# Pour la villa il faudrait un genre de 1 470 000 pour expliquer le chiffre par
# (b) ; ce sont en fait 591 038 aretes non-manifold, donc (a).
#
# UNE GARDE QUI REJETTE SANS DIRE LEQUEL DES DEUX CAS EST UNE GARDE QU'ON NE
# PEUT PAS CORRIGER : on ne sait pas s'il faut reparer le fichier ou l'accepter
# tel quel. Celle-ci NOMME toujours le motif.
#
# Euler : V - E + F = 2 - 2g pour une surface fermee orientable. On en deduit
# le genre IMPLIQUE par les comptes, et on regarde s'il est croyable.
#
# ⚠️ Toute tolerance ici est RELATIVE A LA DIAGONALE : un seuil absolu ne
# voyage pas d'un modele a l'autre (defaut trouve le 20/09 dans deux chantiers
# a la fois, dont le mien).

import sys, os
import numpy as np
from collections import defaultdict


def analyser(Vt, F, nom=''):
    n = len(Vt)
    # ⚠️ La diagonale se mesure sur les sommets REELLEMENT REFERENCES par une
    # face. Un sommet orphelin lointain -- et les exports en produisent --
    # gonflerait la boite et relacherait la tolerance PARTOUT AILLEURS.
    util = np.unique(np.asarray(F).ravel()) if len(F) else np.arange(n)
    util = util[(util >= 0) & (util < n)]
    R = Vt[util] if len(util) else Vt
    diag = float(np.linalg.norm(R.max(axis=0) - R.min(axis=0))) if len(R) else 1.0
    eps = max(diag * 1e-6, 1e-12)

    # soudure RELATIVE
    cle, rep = {}, np.empty(n, dtype=np.int64)
    for i, p in enumerate(Vt):
        k = (int(round(float(p[0]) / eps)), int(round(float(p[1]) / eps)),
             int(round(float(p[2]) / eps)))
        if k not in cle:
            cle[k] = len(cle)
        rep[i] = cle[k]
    V = len(cle)

    ar = defaultdict(int)
    for f in F:
        m = len(f)
        for j in range(m):
            a, b = int(rep[f[j]]), int(rep[f[(j + 1) % m]])
            if a != b:
                ar[(min(a, b), max(a, b))] += 1
    E = len(ar)
    bord = sum(1 for c in ar.values() if c == 1)
    nonman = sum(1 for c in ar.values() if c > 2)
    Fn = len(F)
    chi = V - E + Fn
    fv = Fn / max(1, V)
    # genre IMPLIQUE si on prenait le maillage pour une surface fermee
    genre = (2 - chi) / 2.0

    # composantes connexes : sans elles le genre est faux. Euler donne
    # chi = 2(C - g), donc g = C - chi/2. Lire un genre NEGATIF signifie
    # simplement qu'on a oublie de compter les morceaux.
    parent = list(range(V))
    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]; a = parent[a]
        return a
    for (a, b) in ar:
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb
    C = len({find(i) for i in range(V)})
    genre = C - chi / 2.0

    # --- le verdict. Il NOMME toujours le cas, et le libelle ne contredit
    #     jamais la decision : une garde qui affiche « PAS UNE SURFACE » puis
    #     rend OK n'est pas lisible.
    part_nm = 100.0 * nonman / max(1, E)
    part_b = 100.0 * bord / max(1, E)
    if part_nm >= 1.0:
        ok, motif = False, 'PAS UNE SURFACE : %d aretes non-manifold (%.2f %%)' % (nonman, part_nm)
    elif fv > 2.2 and genre >= V / 10.0:
        ok, motif = False, ('F/V = %.2f sans non-manifold notable -> il faudrait un genre de '
                            '%.0f pour %d sommets : INCROYABLE' % (fv, genre, V))
    elif part_b >= 10.0:
        ok, motif = False, 'SURFACE TROP OUVERTE : %d aretes de bord (%.1f %%)' % (bord, part_b)
    else:
        ok = True
        etat = 'fermee' if bord == 0 else 'ouverte (%d bords, %.1f %%)' % (bord, part_b)
        sale = '' if nonman == 0 else ', %d non-manifold tolerees (%.2f %%)' % (nonman, part_nm)
        motif = 'surface plausible : %s, F/V = %.2f, %d composante(s), genre %.0f%s' % (
            etat, fv, C, genre, sale)
    return {'V': V, 'E': E, 'F': Fn, 'chi': chi, 'fv': fv, 'genre': genre,
            'bord': bord, 'nonmanifold': nonman, 'composantes': C, 'ok': ok, 'motif': motif}


def _cube():
    V = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0],
                  [0, 0, 1], [1, 0, 1], [1, 1, 1], [0, 1, 1]], dtype=float)
    F = np.array([[0, 2, 1], [0, 3, 2], [4, 5, 6], [4, 6, 7], [0, 1, 5], [0, 5, 4],
                  [1, 2, 6], [1, 6, 5], [2, 3, 7], [2, 7, 6], [3, 0, 4], [3, 4, 7]])
    return V, F


if __name__ == '__main__':
    if len(sys.argv) > 1:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        from rendre_vues_corpus import charger
        for c in sys.argv[1:]:
            try:
                Vt, F = charger(c)
                r = analyser(Vt, F)
                print('%-34s %s  -> %s' % (os.path.basename(c)[:33],
                                           'OK   ' if r['ok'] else 'REJET', r['motif']))
            except Exception as e:
                print('%-34s ECHEC : %s' % (os.path.basename(c)[:33], e))
        sys.exit(0)

    # --- l'epreuve : un zero et deux negatifs qui doivent etre NOMMES ---
    V, F = _cube()
    print('ZERO      cube ferme      : %s' % analyser(V, F)['motif'])
    # negatif 1 : on retire une face -> surface ouverte
    print('NEGATIF 1 cube troue      : %s' % analyser(V, F[:-1])['motif'])
    # negatif 2 : on duplique des faces -> aretes non-manifold
    F2 = np.vstack([F, F[:6]])
    print('NEGATIF 2 faces doublees  : %s' % analyser(V, F2)['motif'])

    # ⚠️ NEGATIF 3 : LA TOLERANCE FUSIONNE-T-ELLE DES SURFACES DISTINCTES ?
    # Ce negatif N'EXISTAIT PAS quand j'ai choisi 1e-6 : je l'avais pris pour
    # « assez petit » et verifie seulement que mes verdicts ne bougeaient pas,
    # ce qui est un attendu sur ce qui ne doit pas bouger et NON une
    # justification du seuil. L'agent retopologie a demande la preuve ; la
    # voici. Deux plaques separees de d ne doivent PAS fusionner tant que d
    # depasse la tolerance.
    print('NEGATIF 3 deux plaques separees de d :')
    for d in (1e-7, 5e-7, 1e-6, 1e-5, 1e-3):
        Vp, Fp = [], []
        for z in (0.0, d):
            b = len(Vp)
            for i in range(2):
                for j in range(2):
                    Vp.append([i, j, z])
            Fp += [[b, b + 1, b + 3], [b, b + 3, b + 2]]
        Vp = np.array(Vp, dtype=float); Fp = np.array(Fp)
        dg = float(np.linalg.norm(Vp.max(0) - Vp.min(0)))
        c = analyser(Vp, Fp)['composantes']
        print('    d/diagonale = %.1e -> %d composante(s)  %s'
              % (d / dg, c, 'FUSIONNEES' if c < 2 else 'distinctes'))
    print('    -> la fusion cesse vers 5e-7 de la diagonale : la tolerance vaut')
    print('       diag*1e-6 et la grille de quantification en garde la moitie.')
