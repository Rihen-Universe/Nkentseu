# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# QUALITE D'UN DECOUPAGE : la coupe tombe-t-elle AU BON ENDROIT ?
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# POURQUOI CET OUTIL EXISTE. Rodolf a regarde le decoupage et a dit : « le
# decoupage actuel est mal fait ». Mes trois criteres etaient VERTS -- 6 parties,
# somme de faces exacte, sortie relue par la vraie porte.
#
# ⚠️ ET C'EST LA LA FAUTE : CES TROIS CRITERES SONT SATISFAITS PAR N'IMPORTE
# QUELLE PARTITION. Decouper au hasard en six morceaux donne aussi six
# sous-maillages dont les faces s'additionnent parfaitement. Ils mesurent que la
# partition est BIEN FORMEE, jamais qu'elle est AU BON ENDROIT -- ils sont donc
# invariants par le defaut que Rodolf voit a l'oeil.
#
# CE QUE CELUI-CI MESURE, ET LA DERIVATION EST SIMPLE :
#   une articulation est un ETRANGLEMENT. Au cou, au poignet, a la cheville, la
#   section du corps est LOCALEMENT MINIMALE. Une coupe anatomique tombe donc
#   dans un creux du profil des sections ; une coupe arbitraire tombe n'importe
#   ou. On mesure la section A LA FRONTIERE et on la compare a son voisinage.
#
#   Aucun seuil de forme n'est choisi : c'est une comparaison LOCALE, et le
#   verdict porte sur « est-ce un minimum local », pas sur une valeur absolue.
#
# LE ZERO SE PROUVE EN PREMIER : sur un HALTERE (deux boules, une barre fine),
# la seule coupe anatomique possible est SUR LA BARRE. L'outil doit donc dire
# « bonne coupe » pour une frontiere sur la barre, et « mauvaise » pour une
# frontiere au milieu d'une boule. Un instrument qui approuve les deux ne
# distingue rien.
#
# USAGE : python qualite_decoupe.py <decoupe.obj> [--tranches 60] [--zero]
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


def lire_obj_groupes(chemin):
    """Rend (sommets, {nom: [faces]}). Les 'o' sont les parties."""
    import numpy as np
    V, G, cur = [], {}, None
    for l in open(chemin, encoding="utf-8"):
        if l.startswith("v "):
            V.append([float(x) for x in l.split()[1:4]])
        elif l.startswith("o "):
            cur = l.split()[1]
            G[cur] = []
        elif l.startswith("f ") and cur is not None:
            G[cur].append([int(x.split("/")[0]) - 1 for x in l.split()[1:4]])
    if not V or not G:
        _refus("aucun sommet ou aucun groupe « o » dans %s" % chemin)
    return np.array(V), G


def profil_aire(V, F, tranches):
    """Aire de section approchee par tranche : le nombre de cases occupees d'une
    grille 2D. Independant de la densite du maillage -- on rasterise les aretes,
    la lecon deja payee trois fois ce soir."""
    import numpy as np
    mn, mx = V.min(axis=0), V.max(axis=0)
    ext = mx - mn
    if ext[1] <= 0:
        _refus("hauteur nulle")
    G = 96
    centres = V[F].mean(axis=1)
    ty = np.clip(((centres[:, 1] - mn[1]) / ext[1] * tranches).astype(int), 0, tranches - 1)
    aires = np.zeros(tranches)
    ts = np.linspace(0.0, 1.0, 8)[None, :, None]
    for t in range(tranches):
        sel = np.where(ty == t)[0]
        if len(sel) == 0:
            continue
        masque = np.zeros((G, G), dtype=bool)
        P = np.empty((len(sel), 3, 2))
        for c in range(3):
            iv = F[sel, c]
            P[:, c, 0] = (V[iv, 0] - mn[0]) / max(ext[0], 1e-9) * (G - 1)
            P[:, c, 1] = (V[iv, 2] - mn[2]) / max(ext[2], 1e-9) * (G - 1)
        for a, b in ((0, 1), (1, 2), (2, 0)):
            A, B = P[:, a, :][:, None, :], P[:, b, :][:, None, :]
            pts = A + (B - A) * ts
            ix = np.clip(pts[:, :, 0].astype(int), 0, G - 1).ravel()
            iz = np.clip(pts[:, :, 1].astype(int), 0, G - 1).ravel()
            masque[ix, iz] = True
        aires[t] = masque.sum()
    return aires, mn[1], ext[1]


def juger(chemin, tranches, silencieux=False):
    import numpy as np
    V, G = lire_obj_groupes(chemin)
    Ftout = np.array([t for fs in G.values() for t in fs])
    aires, ymin, hauteur = profil_aire(V, Ftout, tranches)
    nTotal = len(Ftout)

    # La frontiere d'une partie : la tranche ou elle CESSE (sa hauteur maximale),
    # quand une autre partie continue au-dessus.
    infos = []
    for nom, fs in G.items():
        F = np.array(fs)
        ys = V[F].mean(axis=1)[:, 1]
        t0 = int(np.clip((ys.min() - ymin) / hauteur * tranches, 0, tranches - 1))
        t1 = int(np.clip((ys.max() - ymin) / hauteur * tranches, 0, tranches - 1))
        infos.append((nom, len(fs), t0, t1))
    infos.sort(key=lambda x: x[2])

    rouge = 0
    if not silencieux:
        print("MAILLAGE : %s" % os.path.basename(chemin))
        print("  %d sommets · %d faces · %d partie(s)" % (len(V), nTotal, len(G)))
        print()
        print("  [1] LA COUPE TOMBE-T-ELLE DANS UN ETRANGLEMENT ?")

    voisin = max(2, int(0.10 * tranches))
    for nom, nf, t0, t1 in infos:
        if t1 >= tranches - 1:
            continue  # partie du sommet : pas de frontiere au-dessus
        t = t1
        a, b = max(0, t - voisin), min(tranches - 1, t + voisin)
        fen = aires[a:b + 1]
        fen = fen[fen > 0]
        if len(fen) < 3 or aires[t] <= 0:
            continue
        mini = fen.min()
        # Un minimum LOCAL : la section a la frontiere est la plus petite du
        # voisinage, a 15 % pres (la grille discretise, une egalite exacte serait
        # un critere impossible a satisfaire).
        ok = aires[t] <= mini * 1.15
        if not ok:
            rouge += 1
        if not silencieux:
            print("    [%s] %-12s frontiere a %3.0f %% de la hauteur : section %4.0f | "
                  "plus petite du voisinage %4.0f  (rapport %.2f)"
                  % ("VERT " if ok else "ROUGE", nom, 100.0 * t / tranches, aires[t], mini,
                     aires[t] / mini if mini else 0))
    if not silencieux:
        print()
        print("  [2] DES MIETTES ?")
    for nom, nf, t0, t1 in sorted(infos, key=lambda x: x[1]):
        part = 100.0 * nf / nTotal
        if part < 2.0:
            rouge += 1
            if not silencieux:
                print("    [ROUGE] %-12s %5.2f %% des faces -- sous 2 %%, c'est un residu de jonction" % (nom, part))
    if not silencieux:
        petites = [x for x in infos if 100.0 * x[1] / nTotal < 2.0]
        if not petites:
            print("    [VERT ] aucune partie sous 2 %% des faces")
        print()
        print("  VERDICT QUALITE : %s (%d rouge)" % ("VERT" if rouge == 0 else "ROUGE", rouge))
    return rouge


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("decoupe")
    ap.add_argument("--tranches", type=int, default=60)
    ap.add_argument("--zero", action="store_true")
    a = ap.parse_args()

    if a.zero:
        # ── LE ZERO : sur un HALTERE, une coupe SUR LA BARRE est bonne, une
        # coupe AU MILIEU D'UNE BOULE est mauvaise. Si l'outil approuve les deux,
        # il ne distingue rien.
        import numpy as np, trimesh
        b1 = trimesh.creation.icosphere(subdivisions=3, radius=0.5)
        b2 = trimesh.creation.icosphere(subdivisions=3, radius=0.5)
        b1.apply_translation([0, -1.0, 0])
        b2.apply_translation([0, 1.0, 0])
        bar = trimesh.creation.cylinder(radius=0.08, height=2.0)
        h = trimesh.util.concatenate([b1, bar, b2])
        Vh = np.asarray(h.vertices)
        Fh = np.asarray(h.faces)
        yc = Vh[Fh].mean(axis=1)[:, 1]
        tmp = os.path.join(os.path.dirname(a.decoupe) or ".", "__zero_haltere.obj")
        for nom, seuil, attendu in (("coupe_sur_la_barre", 0.0, 0), ("coupe_dans_la_boule", -1.0, 1)):
            with open(tmp, "w", encoding="utf-8", newline="\n") as f:
                for v in Vh:
                    f.write("v %.6f %.6f %.6f\n" % tuple(v))
                f.write("o bas\n")
                for t in Fh[yc <= seuil]:
                    f.write("f %d %d %d\n" % (t[0] + 1, t[1] + 1, t[2] + 1))
                f.write("o haut\n")
                for t in Fh[yc > seuil]:
                    f.write("f %d %d %d\n" % (t[0] + 1, t[1] + 1, t[2] + 1))
            r = juger(tmp, 40, silencieux=True)
            etat = "bonne" if r == 0 else "mauvaise"
            attenduTxt = "bonne" if attendu == 0 else "mauvaise"
            if (r == 0) != (attendu == 0):
                _refus("le zero a echoue : « %s » est jugee %s, attendu %s.\n"
                       "        l'instrument ne distingue pas une coupe anatomique d'une coupe arbitraire."
                       % (nom, etat, attenduTxt), 3)
            print("  [ZERO ] %-22s -> jugee %-9s (attendu %s)" % (nom, etat, attenduTxt))
        try:
            os.remove(tmp)
        except Exception:
            pass
        print()

    if not os.path.isfile(a.decoupe):
        _refus("fichier introuvable : %s" % a.decoupe)
    return 0 if juger(a.decoupe, a.tranches) == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
