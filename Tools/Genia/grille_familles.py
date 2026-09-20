# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
#
# GRILLE DE FAMILLES, MESUREE SUR LA GEOMETRIE -- et son epreuve.
#
# POURQUOI PAS LES NOMS : mon premier classement, par mots-cles du chemin,
# rangeait « Streetlight_Double.fbx » en VEGETATION parce que « Street »
# contient « tree », avec 48,9 % de non classes. Il a ete jete.
#
# ⚠️ ET LE PIEGE DE CELUI-CI EST PIRE, PARCE QU'IL A L'AIR SERIEUX :
# si on definit les familles par les traits qu'on mesure, TOUT modele tombera
# dans une famille et le classement ne dira rien. Des chiffres tautologiques
# sont plus durs a demasquer qu'un mot-cle qui se trompe.
#
# LA PARADE, ET ELLE EST GRATUITE : les sources portent des NOMS DE PACKS
# (kenney « furniture », « dungeon », quaternius par theme). On s'INTERDIT de
# les lire comme traits -- ce serait retomber dans « Streetlight » -- et on s'en
# sert comme EPREUVE : si un groupement purement geometrique rassemble les
# meubles entre eux SANS AVOIR JAMAIS VU le mot « furniture », la grille mesure
# quelque chose. S'il les eparpille, elle ne mesure rien.
#
#   LE ZERO    : les groupes contre les VRAIS packs -> purete elevee attendue.
#   LE NEGATIF : les memes groupes contre des packs MELANGES au hasard ->
#                la purete doit tomber au niveau du hasard. Si elle ne tombe
#                pas, la mesure de purete est aveugle et le zero ne vaut rien.
#
# ⚠️ CETTE GRILLE EST DESCRIPTIVE, PAS NORMATIVE. Elle ne decide pas ce qui
# entre dans le corpus. Elle dit ce que le corpus CONTIENT, pour qu'on sache
# plus tard ce que le modele a pu apprendre -- et ce qu'il n'a jamais vu.

import sys, os, json, random, warnings, collections
import numpy as np
warnings.filterwarnings('ignore')

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)


def traits(chemin):
    """Traits PUREMENT GEOMETRIQUES. Aucun ne lit le nom du fichier -- c'est la
    condition pour que l'epreuve par les packs ait un sens."""
    import trimesh
    from rendre_vues_corpus import charger
    Vt, F = charger(chemin)           # deja centre et normalise
    if len(F) < 4 or len(Vt) < 4:
        return None
    m = trimesh.Trimesh(vertices=Vt, faces=F, process=False)
    e = np.sort(Vt.max(axis=0) - Vt.min(axis=0))[::-1]
    e = np.maximum(e, 1e-9)

    # 1-2. forme de la boite. ⚠️ BORNEES ET EN LOG : sans cela un objet
    # parfaitement plat donne un aplatissement de 1e9, et la normalisation
    # robuste n'y survit pas -- le groupement formait alors un groupe de
    # DEFAUTS DE MESURE et non de formes (mesure du 20/09).
    allonge = float(np.log10(min(max(e[0] / e[1], 1.0), 1e3)))
    aplati = float(np.log10(min(max(e[1] / e[2], 1.0), 1e3)))

    # 3. symetrie bilaterale sur l'axe le plus fin (celui d'un etre vivant)
    ax = int(np.argmin(Vt.max(axis=0) - Vt.min(axis=0)))
    c = 0.5 * (Vt[:, ax].max() + Vt[:, ax].min())
    M = Vt.copy()
    M[:, ax] = 2 * c - M[:, ax]
    try:
        from scipy.spatial import cKDTree
        d = cKDTree(Vt).query(M, k=1)[0]
        sym = float(np.median(d)) / float(np.linalg.norm(e))
    except Exception:
        sym = np.nan

    # 4. concavite : ce que l'enveloppe convexe ajoute
    try:
        vh = float(m.convex_hull.volume)
        concav = float(abs(m.volume)) / max(vh, 1e-12)
    except Exception:
        concav = np.nan

    # 5. rugosite : aire rapportee a celle de l'enveloppe convexe.
    # ⚠️ REMPLACE la compacite aire/sphere-de-meme-volume, qui explosait a
    # 3,5e8 : le volume n'est pas defini sur un maillage non etanche, et la
    # plupart des modeles du corpus ne le sont pas. L'aire de l'enveloppe,
    # elle, existe toujours.
    try:
        ah = float(m.convex_hull.area)
        compact = float(np.log10(min(max(float(m.area) / max(ah, 1e-12), 1e-3), 1e3)))
    except Exception:
        compact = np.nan

    # 6. morcellement, et 7. le critere de plausibilite venu du chantier
    #    retopologie : faces/sommets, gratuit et sans reglage.
    try:
        ncomp = int(len(m.split(only_watertight=False)))
    except Exception:
        ncomp = 1
    fv = len(F) / max(1, len(Vt))

    v = [allonge, aplati, sym, concav, compact, np.log10(max(1, ncomp)), fv]
    return None if any(np.isnan(x) or np.isinf(x) for x in v) else v


NOMS = ['log allongement', 'log aplatissement', 'asymetrie', 'concavite',
        'log rugosite', 'log10(composantes)', 'faces/sommets']


def pack_de(chemin):
    """UNIQUEMENT pour l'epreuve -- jamais passe au groupement."""
    p = chemin.replace('\\', '/').lower()
    for m in ('kenney_', 'kaykit_'):
        i = p.find(m)
        if i >= 0:
            return p[i:].split('/')[0][:38]
    parts = p.split('/brut/')
    return parts[1].split('/')[0] if len(parts) > 1 else '?'


def purete(groupes, etiquettes):
    """INFORMATION MUTUELLE NORMALISEE entre groupes et etiquettes.

    ⚠️ Remplace la purete du pack majoritaire, qui etait BIAISEE VERS L'ECHEC :
    avec 8 groupes pour 53 packs, elle est structurellement plafonnee et un
    bon groupement aurait quand meme l'air mauvais. L'information mutuelle
    normalisee ne depend pas du nombre de classes."""
    g = np.asarray(groupes); e = np.asarray(etiquettes)
    n = len(g)
    def H(a):
        _, c = np.unique(a, return_counts=True)
        pr = c / n
        return float(-(pr * np.log(pr)).sum())
    hg, he = H(g), H(e)
    mi = 0.0
    for gv in np.unique(g):
        mg = g == gv
        for ev in np.unique(e[mg]):
            pxy = float((mg & (e == ev)).sum()) / n
            px = float(mg.sum()) / n
            py = float((e == ev).sum()) / n
            if pxy > 0:
                mi += pxy * np.log(pxy / (px * py))
    d = 0.5 * (hg + he)
    return float(mi / d) if d > 1e-12 else 0.0


def main():
    n_ech = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    K = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    S = os.environ.get('NK_LISTE', '')
    L = [l.strip() for l in open(S, encoding='utf-8', errors='replace')] if S else []
    base = 'D:/Projets/2026/NKGenCorpus/brut/'
    random.seed(20260920)
    random.shuffle(L)

    X, packs, chemins = [], [], []
    for rel in L:
        if len(X) >= n_ech:
            break
        p = os.path.normpath(os.path.join(base, rel.lstrip('./')))
        try:
            t = traits(p)
        except Exception:
            continue
        if t is None:
            continue
        X.append(t); packs.append(pack_de(p)); chemins.append(p)
    X = np.array(X)
    print('%d modeles mesures, %d traits, %d packs distincts'
          % (len(X), X.shape[1], len(set(packs))))
    if len(X) < 20:
        print('trop peu de modeles'); return

    # normalisation robuste, puis groupement. LE NOM N'ENTRE PAS ICI.
    med = np.median(X, axis=0)
    ec = np.percentile(X, 75, axis=0) - np.percentile(X, 25, axis=0)
    Z = (X - med) / np.maximum(ec, 1e-9)
    Z = np.clip(Z, -5, 5)
    from scipy.cluster.vq import kmeans2
    cent, grp = kmeans2(Z, K, minit='++', seed=1234, iter=60)

    print('')
    print('=== LES GROUPES, DECRITS PAR LEURS TRAITS (medianes) ===')
    for g in range(K):
        sel = grp == g
        if sel.sum() == 0:
            continue
        m = np.median(X[sel], axis=0)
        top = collections.Counter([packs[i] for i in range(len(packs)) if sel[i]]).most_common(2)
        print('  groupe %d (%4d) ' % (g, sel.sum())
              + ' '.join('%s=%.2f' % (n.split('(')[0][:9], v) for n, v in zip(NOMS, m)))
        print('        packs dominants : ' + ', '.join('%s x%d' % (a, b) for a, b in top))

    print('')
    print('=== L EPREUVE : les groupes retrouvent-ils les packs, sans les avoir vus ? ===')
    p0 = purete(grp, packs)
    print('  ZERO    info mutuelle contre les VRAIS packs : %.3f' % p0)
    rng = random.Random(999)
    ps = []
    for _ in range(20):
        mel = packs[:]
        rng.shuffle(mel)
        ps.append(purete(grp, mel))
    pm = sum(ps) / len(ps)
    print('  NEGATIF contre des packs MELANGES           : %.3f  (20 tirages, max %.3f)'
          % (pm, max(ps)))
    print('  ecart : %+.3f' % (p0 - pm))
    print('')
    if p0 > max(ps) + 0.02:
        print('  -> LA GRILLE MESURE QUELQUE CHOSE : un groupement qui n a JAMAIS vu le')
        print('     nom des packs les retrouve nettement mieux que le hasard.')
    else:
        print('  -> *** LA GRILLE NE MESURE RIEN *** : elle ne fait pas mieux que des')
        print('     etiquettes melangees. Descriptive ou pas, elle ne doit pas etre publiee.')

    out = os.environ.get('NK_SORTIE', '')
    if out:
        with open(out, 'w', encoding='utf-8') as f:
            for i in range(len(X)):
                f.write(json.dumps({'chemin': chemins[i], 'groupe': int(grp[i]),
                                    'pack': packs[i],
                                    'traits': {n: float(v) for n, v in zip(NOMS, X[i])}}) + '\n')
        print('  ecrit : ' + out)


if __name__ == '__main__':
    main()
