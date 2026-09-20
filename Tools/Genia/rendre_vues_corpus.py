# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
#
# RENDU DE VUES AVEC POSES EXACTES, ET LE TEMOIN QUI LE PROUVE AVANT TOUT CORPUS.
#
# POURQUOI CE TEMOIN EXISTE, ET POURQUOI IL PASSE AVANT LA PREMIERE VUE ECRITE.
# Mesure du 20/09 (R45) : trois reconstructions du meme personnage n'avaient
# AUCUNE orientation coherente a trouver -- le meilleur accord entre deux vues
# (0,299) egalait le PIRE accord d'une vue avec elle-meme tournee au hasard
# (0,300). Un corpus dont les POSES MENTENT graverait exactement ce defaut-la
# dans les poids, definitivement, et aucune mesure d'aval ne le rattraperait :
# le modele apprendrait que « cet angle » correspond a « cette image » alors
# que c'est faux, et il n'y a pas de correctif apres coup.
#
# CE QUE LE TEMOIN MESURE, ET POURQUOI IL EST EXACT.
# On rend, a cote de la couleur, un TAMPON DE POSITIONS : chaque pixel porte la
# position 3D du point de surface qu'il montre. Puis on REPROJETTE ces positions
# par la pose declaree. Si la pose est juste, chaque position doit retomber sur
# LE PIXEL DONT ELLE VIENT. L'ecart se lit donc directement EN PIXELS -- ce
# n'est pas un score, c'est une distance.
#
# ATTENDU, ECRIT AVANT LA MESURE :
#   Z. LE ZERO : une vue reprojetee par SA PROPRE pose -> erreur SOUS 0,5 pixel
#      (l'echantillonnage place le point au centre du pixel, donc l'erreur de
#      principe est bornee par un demi-pixel). Si le zero ne tombe pas, la pose
#      ecrite dans le fichier n'est pas celle qui a servi au rendu, et tout le
#      reste est sans valeur.
#   N. LE NEGATIF : la MEME vue reprojetee par la pose d'une AUTRE vue ->
#      l'erreur doit EXPLOSER (des dizaines de pixels au moins). Un temoin qui
#      resterait vert sous une pose etrangere ne mesurerait pas la pose.
#   R. Chaque vue de production doit tenir le critere du zero.
#
# ⚠️ CE QUE CE TEMOIN NE MESURE PAS : que la pose soit celle qu'on VOULAIT
# (azimut 30 et non 33). Il prouve la COHERENCE entre la pose ecrite et les
# pixels rendus, pas l'intention. Une erreur systematique qui affecterait le
# rendu ET l'ecriture de la pose passerait -- c'est pourquoi les azimuts sont
# calcules a un seul endroit et relus depuis le fichier ecrit.

import sys, os, json
import numpy as np


# --------------------------------------------------------------- la camera
def look_at(eye, cible, up=(0.0, 1.0, 0.0)):
    eye = np.asarray(eye, dtype=np.float64)
    cible = np.asarray(cible, dtype=np.float64)
    up = np.asarray(up, dtype=np.float64)
    f = cible - eye
    f = f / max(np.linalg.norm(f), 1e-12)
    s = np.cross(f, up)
    ns = np.linalg.norm(s)
    if ns < 1e-9:                      # regard colineaire au haut : autre up
        up = np.array([0.0, 0.0, 1.0])
        s = np.cross(f, up)
        ns = np.linalg.norm(s)
    s = s / ns
    u = np.cross(s, f)
    M = np.eye(4)
    M[0, :3] = s
    M[1, :3] = u
    M[2, :3] = -f
    M[:3, 3] = -M[:3, :3] @ eye
    return M


def perspective(fov_deg, aspect, near, far):
    t = 1.0 / np.tan(np.deg2rad(fov_deg) * 0.5)
    P = np.zeros((4, 4))
    P[0, 0] = t / aspect
    P[1, 1] = t
    P[2, 2] = (far + near) / (near - far)
    P[2, 3] = (2 * far * near) / (near - far)
    P[3, 2] = -1.0
    return P


def pose_depuis_angles(azimut_deg, elevation_deg, rayon, cible, fov, W, H, near, far):
    a = np.deg2rad(azimut_deg)
    e = np.deg2rad(elevation_deg)
    eye = np.asarray(cible, dtype=np.float64) + rayon * np.array(
        [np.cos(e) * np.sin(a), np.sin(e), np.cos(e) * np.cos(a)])
    V = look_at(eye, cible)
    P = perspective(fov, W / float(H), near, far)
    return {'azimut': float(azimut_deg), 'elevation': float(elevation_deg),
            'rayon': float(rayon), 'cible': [float(x) for x in cible],
            'fov': float(fov), 'W': int(W), 'H': int(H),
            'near': float(near), 'far': float(far),
            'vue': V.tolist(), 'projection': P.tolist()}


def projeter(X, pose):
    """Points monde (n,3) -> (x_pixel, y_pixel, w_clip)."""
    V = np.asarray(pose['vue']); P = np.asarray(pose['projection'])
    W, H = pose['W'], pose['H']
    Xh = np.concatenate([X, np.ones((len(X), 1))], axis=1)
    c = Xh @ (P @ V).T
    w = c[:, 3]
    ok = np.abs(w) > 1e-12
    ndc = np.zeros((len(X), 3))
    ndc[ok] = c[ok, :3] / w[ok, None]
    x = (ndc[:, 0] * 0.5 + 0.5) * W
    y = (1.0 - (ndc[:, 1] * 0.5 + 0.5)) * H
    return x, y, w


# --------------------------------------------------------------- le rendu
def rendre(Vt, F, pose):
    """Retourne (positions 3D par pixel, masque, normale par pixel)."""
    W, H = pose['W'], pose['H']
    x, y, w = projeter(Vt, pose)
    z = w  # profondeur camera : plus petit = plus proche
    pos = np.zeros((H, W, 3))
    nor = np.zeros((H, W, 3))
    zb = np.full((H, W), np.inf)
    msk = np.zeros((H, W), dtype=bool)
    N = np.cross(Vt[F[:, 1]] - Vt[F[:, 0]], Vt[F[:, 2]] - Vt[F[:, 0]])
    N = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-12)
    for i in range(len(F)):
        a, b, c = F[i]
        if w[a] <= 0 or w[b] <= 0 or w[c] <= 0:
            continue                    # derriere la camera : on ne rasterise pas
        xs = np.array([x[a], x[b], x[c]]); ys = np.array([y[a], y[b], y[c]])
        x0 = max(0, int(np.floor(xs.min()))); x1 = min(W - 1, int(np.ceil(xs.max())))
        y0 = max(0, int(np.floor(ys.min()))); y1 = min(H - 1, int(np.ceil(ys.max())))
        if x1 < x0 or y1 < y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if abs(d) < 1e-12:
            continue
        l1 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / d
        l2 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / d
        l3 = 1.0 - l1 - l2
        m = (l1 >= 0) & (l2 >= 0) & (l3 >= 0)
        if not m.any():
            continue
        # interpolation PERSPECTIVE-CORRECTE : en ecran, c'est 1/w qui est affine.
        iw = l1 / w[a] + l2 / w[b] + l3 / w[c]
        bon = m & (np.abs(iw) > 1e-15)
        if not bon.any():
            continue
        prof = np.where(bon, 1.0 / np.where(np.abs(iw) > 1e-15, iw, 1.0), np.inf)
        sz = zb[y0:y1 + 1, x0:x1 + 1]
        vis = bon & (prof < sz)
        if not vis.any():
            continue
        p = ((l1 / w[a])[..., None] * Vt[a] + (l2 / w[b])[..., None] * Vt[b]
             + (l3 / w[c])[..., None] * Vt[c]) * prof[..., None]
        sz[vis] = prof[vis]
        pos[y0:y1 + 1, x0:x1 + 1][vis] = p[vis]
        nor[y0:y1 + 1, x0:x1 + 1][vis] = N[i]
        msk[y0:y1 + 1, x0:x1 + 1][vis] = True
    return pos, msk, nor



# ---------------------------------------------------------------------------
# LE MEME RENDU, SANS BOUCLE SUR LES TRIANGLES.
#
# POURQUOI : le rasteriseur ci-dessus tourne a ~14 s la vue. A 3 096 modeles
# et 8 vues, c'est 96 heures -- une nuit passee a jeter le resultat.
#
# ⚠️ ET POURQUOI IL DOIT ETRE IDENTIQUE AUX OCTETS, PAS « AUSSI BON » :
# un gain d'un ordre de grandeur est exactement le genre de changement qui
# deplace SILENCIEUSEMENT la qualite -- ordre de rasterisation, regle de
# remplissage des bords, departage des profondeurs. On s'en apercevrait dans
# six mois, dans les poids d'un modele, sans pouvoir remonter a la cause.
#
# LES TROIS POINTS OU L'IDENTITE SE JOUE, ET COMMENT ILS SONT TENUS :
#   1. REGLE DE BORD : `l1 >= 0 & l2 >= 0 & l3 >= 0`, a l'identique. Un `>`
#      au lieu d'un `>=` changerait une colonne de pixels sur chaque arete.
#   2. DEPARTAGE DES PROFONDEURS : l'original parcourt les triangles dans
#      l'ordre et teste `prof < sz`, donc a profondeur EGALE le triangle
#      d'indice le PLUS PETIT gagne. On reproduit cela par un tri lexical
#      (pixel, profondeur, indice de triangle) suivi d'une prise du premier.
#   3. ORDRE DES OPERATIONS FLOTTANTES : les memes formules sur les memes
#      valeurs, donc les memes bits. Aucune reassociation, aucun `float32`.
def rendre_vectorise(Vt, F, pose, lot_max=4_000_000):
    W, H = pose['W'], pose['H']
    x, y, w = projeter(Vt, pose)
    pos = np.zeros((H, W, 3))
    nor = np.zeros((H, W, 3))
    msk = np.zeros((H, W), dtype=bool)

    N = np.cross(Vt[F[:, 1]] - Vt[F[:, 0]], Vt[F[:, 2]] - Vt[F[:, 0]])
    N = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-12)

    a, b, c = F[:, 0], F[:, 1], F[:, 2]
    # meme garde que l'original : un seul sommet derriere la camera ecarte le triangle
    devant = (w[a] > 0) & (w[b] > 0) & (w[c] > 0)
    xs = np.stack([x[a], x[b], x[c]], axis=1)
    ys = np.stack([y[a], y[b], y[c]], axis=1)
    x0 = np.maximum(0, np.floor(xs.min(axis=1)).astype(np.int64))
    x1 = np.minimum(W - 1, np.ceil(xs.max(axis=1)).astype(np.int64))
    y0 = np.maximum(0, np.floor(ys.min(axis=1)).astype(np.int64))
    y1 = np.minimum(H - 1, np.ceil(ys.max(axis=1)).astype(np.int64))
    d = (ys[:, 1] - ys[:, 2]) * (xs[:, 0] - xs[:, 2]) + (xs[:, 2] - xs[:, 1]) * (ys[:, 0] - ys[:, 2])
    vivant = devant & (x1 >= x0) & (y1 >= y0) & (np.abs(d) >= 1e-12)
    idx = np.nonzero(vivant)[0]
    if len(idx) == 0:
        return pos, msk, nor

    lw = (x1[idx] - x0[idx] + 1)
    lh = (y1[idx] - y0[idx] + 1)
    n = lw * lh

    # tampons du resultat final, un par pixel
    meilleur_prof = np.full(H * W, np.inf)
    meilleur_tri = np.full(H * W, -1, dtype=np.int64)
    meilleur_l = np.zeros((H * W, 3))

    # on traite par lots pour borner la memoire : le decoupage ne change RIEN
    # au resultat puisque le departage final se fait sur les tampons globaux.
    debut = 0
    while debut < len(idx):
        fin = debut
        total = 0
        while fin < len(idx) and (total + n[fin] <= lot_max or fin == debut):
            total += int(n[fin]); fin += 1
        sel = idx[debut:fin]
        nn = n[debut:fin]
        rep = np.repeat(np.arange(len(sel)), nn)          # quel triangle du lot
        # position locale dans la bbox -> pixel, dans le MEME ordre que meshgrid
        base = np.concatenate([[0], np.cumsum(nn)[:-1]])
        local = np.arange(total) - np.repeat(base, nn)
        lwr = np.repeat(lw[debut:fin], nn)
        px = np.repeat(x0[sel], nn) + (local % lwr)
        py = np.repeat(y0[sel], nn) + (local // lwr)
        gx = px + 0.5
        gy = py + 0.5

        X0 = xs[sel][rep]; Y0 = ys[sel][rep]; D = d[sel][rep]
        l1 = ((Y0[:, 1] - Y0[:, 2]) * (gx - X0[:, 2]) + (X0[:, 2] - X0[:, 1]) * (gy - Y0[:, 2])) / D
        l2 = ((Y0[:, 2] - Y0[:, 0]) * (gx - X0[:, 2]) + (X0[:, 0] - X0[:, 2]) * (gy - Y0[:, 2])) / D
        l3 = 1.0 - l1 - l2
        dedans = (l1 >= 0) & (l2 >= 0) & (l3 >= 0)
        if not dedans.any():
            debut = fin
            continue
        wa = w[a[sel]][rep]; wb = w[b[sel]][rep]; wc = w[c[sel]][rep]
        iw = l1 / wa + l2 / wb + l3 / wc
        bon = dedans & (np.abs(iw) > 1e-15)
        if not bon.any():
            debut = fin
            continue
        prof = 1.0 / iw[bon]
        pix = (py[bon] * W + px[bon])
        tri = sel[rep[bon]]
        L = np.stack([l1[bon] / wa[bon], l2[bon] / wb[bon], l3[bon] / wc[bon]], axis=1)

        # DEPARTAGE : (pixel, profondeur, indice de triangle) -- le tri lexical
        # met en tete, pour chaque pixel, la plus petite profondeur, et a
        # profondeur egale le plus petit indice : exactement `prof < sz` parcouru
        # dans l'ordre croissant des triangles.
        ordre = np.lexsort((tri, prof, pix))
        pix_o = pix[ordre]
        premier = np.ones(len(pix_o), dtype=bool)
        premier[1:] = pix_o[1:] != pix_o[:-1]
        sp = pix_o[premier]
        sprof = prof[ordre][premier]
        stri = tri[ordre][premier]
        sL = L[ordre][premier]
        # confronter au meilleur deja connu (lots precedents)
        mieux = sprof < meilleur_prof[sp]
        egal = (sprof == meilleur_prof[sp]) & (stri < meilleur_tri[sp])
        g = mieux | egal
        if g.any():
            meilleur_prof[sp[g]] = sprof[g]
            meilleur_tri[sp[g]] = stri[g]
            meilleur_l[sp[g]] = sL[g]
        debut = fin

    vus = np.nonzero(meilleur_tri >= 0)[0]
    if len(vus) == 0:
        return pos, msk, nor
    t = meilleur_tri[vus]
    L = meilleur_l[vus]
    P = (L[:, 0:1] * Vt[F[t, 0]] + L[:, 1:2] * Vt[F[t, 1]] + L[:, 2:3] * Vt[F[t, 2]]) \
        * meilleur_prof[vus][:, None]
    yy = vus // W
    xx = vus % W
    pos[yy, xx] = P
    nor[yy, xx] = N[t]
    msk[yy, xx] = True
    return pos, msk, nor

# ------------------------------------------------------------- le temoin
def erreur_reprojection(pos, msk, pose):
    """Les positions rendues, reprojetees, retombent-elles sur leur pixel ?"""
    ys, xs = np.nonzero(msk)
    if len(xs) == 0:
        return None
    X = pos[ys, xs]
    px, py, w = projeter(X, pose)
    devant = w > 0
    if devant.sum() == 0:
        return None
    d = np.hypot(px[devant] - (xs[devant] + 0.5), py[devant] - (ys[devant] + 0.5))
    return {'n': int(devant.sum()), 'mediane': float(np.median(d)),
            'p99': float(np.percentile(d, 99)), 'max': float(d.max())}


# ------------------------------------------------------------ l'epreuve
def charger(chemin):
    # ⚠️ 1 159 des 3 096 modeles de l'inventaire sont des FBX, que trimesh
    # refuse. Sans ce chemin, 37,4 % du corpus serait invisible.
    if chemin.lower().endswith('.fbx'):
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        from lire_fbx import charger_fbx
        Vt, F = charger_fbx(chemin)
        c = 0.5 * (Vt.min(axis=0) + Vt.max(axis=0))
        e = (Vt.max(axis=0) - Vt.min(axis=0)).max()
        return (Vt - c) / max(e, 1e-12), F
    import trimesh
    m = trimesh.load(chemin, force='mesh')
    if hasattr(m, 'geometry'):
        m = trimesh.util.concatenate([g for g in m.geometry.values()])
    Vt = np.asarray(m.vertices, dtype=np.float64)
    F = np.asarray(m.faces, dtype=np.int64)
    c = 0.5 * (Vt.min(axis=0) + Vt.max(axis=0))
    e = (Vt.max(axis=0) - Vt.min(axis=0)).max()
    Vt = (Vt - c) / max(e, 1e-12)      # normalise : boite unite centree
    return Vt, F


def eprouver(chemin, W=384, H=384):
    Vt, F = charger(chemin)
    print('')
    print('=== ' + os.path.basename(chemin) + '  (%d sommets, %d faces) ===' % (len(Vt), len(F)))
    angles = [0.0, 90.0, 180.0, 270.0]
    poses = [pose_depuis_angles(a, 15.0, 2.6, (0, 0, 0), 40.0, W, H, 0.05, 10.0) for a in angles]
    rendus = [rendre(Vt, F, p) for p in poses]
    for (pos, msk, _), p in zip(rendus, poses):
        print('  azimut %5.1f : %6d pixels de sujet (%.1f %% de l image)'
              % (p['azimut'], int(msk.sum()), 100.0 * msk.mean()))

    print('  -- Z. LE ZERO : chaque vue reprojetee par SA PROPRE pose --')
    zeros = []
    for (pos, msk, _), p in zip(rendus, poses):
        r = erreur_reprojection(pos, msk, p)
        zeros.append(r['p99'])
        print('     azimut %5.1f  mediane %.4f px | p99 %.4f px | max %.4f px  -> %s'
              % (p['azimut'], r['mediane'], r['p99'], r['max'],
                 'VERT' if r['p99'] < 0.5 else 'ROUGE'))

    print('  -- N. LE NEGATIF : la vue 0 reprojetee par la pose des AUTRES --')
    pos0, msk0, _ = rendus[0]
    negs = []
    for p in poses[1:]:
        r = erreur_reprojection(pos0, msk0, p)
        if r is None:
            print('     pose azimut %5.1f : AUCUN point devant la camera -> ecart MAXIMAL' % p['azimut'])
            negs.append(1e9)
            continue
        negs.append(r['mediane'])
        print('     pose azimut %5.1f  mediane %.1f px | p99 %.1f px  -> %s'
              % (p['azimut'], r['mediane'], r['p99'],
                 'ROUGE (attendu)' if r['mediane'] > 10.0 else '*** TEMOIN AVEUGLE A LA POSE ***'))

    ok_z = max(zeros) < 0.5
    ok_n = min(negs) > 10.0
    print('  -- verdict --')
    print('     sait dire ZERO (< 0,5 px)      : %s' % ('OUI' if ok_z else 'NON *** INUTILISABLE ***'))
    print('     sait REFUSER une pose etrangere: %s' % ('OUI' if ok_n else 'NON *** AVEUGLE ***'))
    return ok_z and ok_n


# ---------------------------------------------------------------------------
# ⚠️ LE PREMIER TEMOIN ETAIT TAUTOLOGIQUE, ET JE L'AI VU A SON ZERO TROP PARFAIT.
#
# `eprouver` ci-dessus reprojette le tampon de positions par la pose gardee EN
# MEMOIRE. Or ce tampon est interpole en perspective-correcte : le reprojeter
# par la MEME matrice redonne le pixel PAR CONSTRUCTION MATHEMATIQUE. D'ou le
# 0,0000 px exact -- qui n'est pas une reussite mais un aller-retour trivial.
# Ce test prouve que mon interpolation est perspective-correcte (utile : une
# interpolation affine l'aurait fait rougir), mais il NE PEUT PAS attraper le
# defaut qui menace vraiment le corpus : que la pose ECRITE DANS LE FICHIER ne
# soit pas celle qui a servi au rendu.
#
# Le maillon dangereux est le FICHIER : une convention d'axe inversee, un
# azimut arrondi, un signe perdu a l'ecriture. Le temoin doit donc PASSER PAR
# LE DISQUE, et se faire refuser une pose legerement fausse.
#
# ATTENDU, ECRIT AVANT :
#   Z2. aller-retour disque, pose relue du JSON -> < 0,5 px (identique au rendu)
#   N2. azimut ECRIT decale -> l'erreur doit depasser le seuil du zero.
#
# ⚠️ MON PREMIER ATTENDU ETAIT FAUX, ET LA MESURE L'A DIT. J'avais ecrit
# « a fov 40 deg sur 384 px, 1 degre vaut 9,6 px ». C'est vrai pour 1 degre DE
# CHAMP DE VISION, et faux pour 1 degre D'ORBITE : la camera tourne AUTOUR de
# la cible, donc le centre du sujet ne bouge pas et il ne reste que la
# PARALLAXE, proportionnelle a la PROFONDEUR du sujet. Mesure (personnage, de
# profondeur 0,235 en boite unite) :
#
#     ecart azimut :  0,5    1      2      5     10     20     45     90
#     mediane (px) :  0,07   0,14   0,28   0,70   1,42   2,91   6,63  11,75
#     p99     (px) :  0,22   0,44   0,88   2,20   4,39   8,74  19,18  36,26
#
# La reponse est LINEAIRE, a 0,14 px par degre sur ce sujet. Avec le critere
# p99 < 0,5 px, le temoin refuse donc a partir d'environ 1,5 degre d'erreur --
# ce qui est la resolution utile, et bien meilleure que ce que je craignais.
#
# ⚠️ MAIS ELLE DEPEND DU SUJET : la parallaxe vient de la profondeur, donc un
# objet PLAT serait mesure avec une sensibilite proche de zero et une pose
# fausse y passerait. Le temoin PUBLIE donc sa sensibilite en px/degre pour
# chaque modele, au lieu de supposer qu'elle vaut partout ce qu'elle vaut ici.
def eprouver_cycle_fichier(chemin, dossier, W=384, H=384):
    Vt, F = charger(chemin)
    os.makedirs(dossier, exist_ok=True)
    print('')
    print('=== CYCLE FICHIER : ' + os.path.basename(chemin) + ' ===')
    ok = True
    for az in (0.0, 37.0, 212.0):
        pose = pose_depuis_angles(az, 15.0, 2.6, (0, 0, 0), 40.0, W, H, 0.05, 10.0)
        pos, msk, _ = rendre(Vt, F, pose)
        f = os.path.join(dossier, 'pose_%03d.json' % int(az))
        with open(f, 'w', encoding='utf-8') as h:
            json.dump(pose, h)
        # --- Z2 : on RELIT depuis le disque, et on RECONSTRUIT la pose depuis
        #     ses angles -- pas depuis les matrices stockees. C'est le chemin
        #     qu'un consommateur du corpus empruntera.
        with open(f, encoding='utf-8') as h:
            lu = json.load(h)
        refaite = pose_depuis_angles(lu['azimut'], lu['elevation'], lu['rayon'],
                                     lu['cible'], lu['fov'], lu['W'], lu['H'],
                                     lu['near'], lu['far'])
        rz = erreur_reprojection(pos, msk, refaite)
        # --- N2 : la meme chose avec l'azimut ecrit fausse. Le seuil vient de
        #     la COURBE MESUREE, pas d'une derivation : a p99 < 0,5 px, il faut
        #     environ 1,5 degre pour rougir, donc on eprouve a 2 degres.
        faux = pose_depuis_angles(lu['azimut'] + 2.0, lu['elevation'], lu['rayon'],
                                  lu['cible'], lu['fov'], lu['W'], lu['H'],
                                  lu['near'], lu['far'])
        rn = erreur_reprojection(pos, msk, faux)
        # sensibilite PUBLIEE : elle depend de la profondeur du sujet, donc
        # elle se mesure par modele au lieu d'etre supposee.
        sens = rn['mediane'] / 2.0
        bon = rz['p99'] < 0.5 and rn['p99'] > 0.5
        ok = ok and bon
        print('  azimut %6.1f | RELU : p99 %.4f px | +2 deg : p99 %6.2f px | sensibilite %.3f px/deg  -> %s'
              % (az, rz['p99'], rn['p99'], sens, 'VERT' if bon else 'ROUGE'))
    print('  -> le cycle disque %s, et 2 degres de pose fausse %s'
          % ('CONSERVE la pose' if ok else 'PERD la pose',
             'SE VOIENT' if ok else 'PASSENT INAPERCUS'))
    return ok

if __name__ == '__main__':
    cibles = sys.argv[1:]
    if not cibles:
        print('usage : rendre_vues_corpus.py <maillage.obj|glb> [...]')
        sys.exit(2)
    dossier = os.environ.get('NK_POSES_DIR', 'poses_temoin')
    tous = True
    for c in cibles:
        try:
            a = eprouver(c)
            b = eprouver_cycle_fichier(c, dossier)
            tous = a and b and tous
        except Exception as e:
            print('  ECHEC sur %s : %s' % (c, e))
            tous = False
    print('')
    print('########  TEMOIN DE POSES : %s  ########'
          % ('VERT -- le corpus peut etre rendu' if tous
             else 'ROUGE -- AUCUNE vue de production ne doit etre ecrite'))
    sys.exit(0 if tous else 1)
