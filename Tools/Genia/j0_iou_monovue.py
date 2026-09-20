# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# J0 -- LE PROFESSEUR MERITE-T-IL D'ETRE COPIE ?
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# LA QUESTION, ET POURQUOI ELLE EST MESURABLE AUJOURD'HUI SEULEMENT
# -----------------------------------------------------------------
# Ilyana-3DG a deux architectures candidates : (A) un eleve MONO-VUE distille
# depuis TripoSR, (B) un modele qui prend plusieurs vues EN ENTREE. Un eleve
# distille ne depasse pas son maitre : si TripoSR reconstruit mal depuis une
# seule vue, (A) copie un professeur qui a tort et la depense est perdue.
#
# Jusqu'ici la question n'etait pas mesurable : on n'avait pas de VERITE
# geometrique. Le corpus de vues la donne -- ses 3 927 modeles, c'est NOUS qui
# les avons rendus, donc on connait leur geometrie exacte.
#
# CE QUE CE PROGRAMME MESURE, ET AVEC QUOI ON LE COMPARE
# -----------------------------------------------------
#   mesure       IoU( reconstruction TripoSR depuis UNE vue , verite )
#   PLANCHER     IoU( verite d'un AUTRE modele , verite )        <- le hasard
#   BORNE HAUTE  IoU( verite tournee hors grille , verite )      <- notre plafond
#
# ⚠️ SANS PLANCHER, UN IoU NE VEUT RIEN DIRE. C'est exactement ce qui a permis
#    de condamner la fusion multi-vues : « meilleur accord 0,299 » etait
#    ininterpretable jusqu'a ce qu'on sache que le pire auto-accord valait 0,300.
#
# ⚠️ SANS BORNE HAUTE NON PLUS. Notre grille de rotation et notre
#    echantillonnage coutent quelques pourcents a une reconstruction PARFAITE.
#    Lire 0,7 sans savoir que le plafond est 0,8 ferait accuser le modele de ce
#    que l'instrument prend.
#
# ⚠️ ON NE SUPPOSE AUCUNE ROTATION. TripoSR ecrit dans son repere canonique, et
#    rien ne dit comment il s'aligne sur l'azimut d'entree. Une prémisse fausse
#    sur la rotation produirait une REFUTATION FAUSSE -- la faute que le
#    chantier multi-vues a evitee en balayant les 360 degres. On balaye donc, et
#    on garde le meilleur. Le plancher et la borne haute subissent EXACTEMENT le
#    meme balayage : un avantage donne a la mesure et pas a son temoin serait un
#    deux-poids-deux-mesures.
#
# CE QUE LA MESURE NE DIT PAS, ET IL FAUT LE LIRE AVANT LE CHIFFRE
# ---------------------------------------------------------------
# Le corpus est du lowpoly de jeu, a 68,8 % d'un seul createur. **Un IoU mesure
# ici ne voyage pas vers les photos de Rodolf**, pour lesquelles il n'existe
# aucune verite geometrique. Ce programme dit ce que le maitre vaut SUR CE
# CORPUS, et rien de plus.
#
# ⚠️ L'occupation mesuree est celle de la SURFACE, pas du volume plein : une
#    grande partie du corpus n'est pas etanche, et remplir un maillage ouvert
#    donnerait un resultat arbitraire. Le critere est donc le meme pour les
#    trois grandeurs, ce qui est la seule chose qui compte pour les comparer.
# -----------------------------------------------------------------------------
import argparse
import json
import os
import sys
import time

import numpy as np

# La console de Windows est en cp1252 : sans ceci, un rapport qui CONTIENT son
# avertissement meurt en l'imprimant. Le verdict ne doit pas dependre de la
# page de codes du terminal.
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.abspath(os.path.join(ICI, "..", ".."))
OUTILS = os.path.join(os.path.dirname(RACINE), "genia-tools")
CORPUS = os.path.join(RACINE, "logs_genia3d", "corpus_vues")

sys.path.insert(0, ICI)
from rendre_vues_corpus import charger  # normalise deja en boite unite centree

RES_VOX = 48          # resolution de la grille d'occupation
N_POINTS = 300000     # points echantillonnes sur la surface
# ⚠️ PAS DE 5 DEGRES, ET LE CHIFFRE VIENT DE L'EPREUVE, PAS D'UN GOUT. Avec un
#    pas de 15, la BORNE HAUTE mesuree tombait a 0,6477 : une reconstruction
#    PARFAITE mais desalignee de 7,3 deg perdait un tiers de son IoU. On aurait
#    lu comme un defaut du modele ce que l'instrument prenait.
#    L'ajustement a eu lieu PENDANT L'EPREUVE, avant toute mesure de TripoSR, et
#    les seuils pre-commites (2,00 / 1,50 sur le rapport mesure/plancher) ne
#    sont PAS touches -- deplacer la ligne apres avoir vu le resultat serait la
#    faute que ce depot nomme.
PAS_ROT = 5.0         # degres : la grille de recherche d'azimut
ELEV_CHOISIE = 15.0   # la hauteur d'oeil du temoin du producteur
AZIM_CHOISI = 0.0


# ----------------------------------------------------------------- instrument
def echantillonner(V, F, graine, n=N_POINTS):
    """Points repartis sur la SURFACE, ponderes par l'aire des triangles."""
    rng = np.random.default_rng(graine)
    if len(F) == 0:
        return np.zeros((0, 3))
    tri = V[F]
    a = tri[:, 1] - tri[:, 0]
    b = tri[:, 2] - tri[:, 0]
    aire = 0.5 * np.linalg.norm(np.cross(a, b), axis=1)
    tot = float(aire.sum())
    if not np.isfinite(tot) or tot <= 0.0:
        return np.zeros((0, 3))
    idx = rng.choice(len(F), size=n, p=aire / tot)
    u = rng.random(n)
    v = rng.random(n)
    hors = (u + v) > 1.0
    u[hors] = 1.0 - u[hors]
    v[hors] = 1.0 - v[hors]
    return tri[idx, 0] + u[:, None] * a[idx] + v[:, None] * b[idx]


def tourner_y(P, deg):
    r = np.deg2rad(deg)
    c, s = np.cos(r), np.sin(r)
    R = np.array([[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]])
    return P @ R.T


def occuper(P, R=RES_VOX):
    """Grille booleenne. ⚠️ On RECENTRE et RENORMALISE les points apres rotation :
    une rotation change la boite englobante, et comparer deux nuages cadres
    differemment mesurerait le cadrage."""
    occ = np.zeros((R, R, R), dtype=bool)
    if len(P) == 0:
        return occ
    lo, hi = P.min(axis=0), P.max(axis=0)
    c = 0.5 * (lo + hi)
    e = float((hi - lo).max())
    Q = (P - c) / max(e, 1e-12)          # dans [-0.5, 0.5]
    g = np.floor((Q + 0.5) * R).astype(np.int64)
    np.clip(g, 0, R - 1, out=g)
    occ[g[:, 0], g[:, 1], g[:, 2]] = True
    return occ


def iou(a, b):
    inter = np.count_nonzero(a & b)
    union = np.count_nonzero(a | b)
    return (inter / union) if union else 0.0


def iou_meilleur(P_mobile, occ_fixe, pas=PAS_ROT):
    """Le MEILLEUR IoU sur la grille d'azimut, et l'angle qui le donne."""
    best, ang = -1.0, 0.0
    for d in np.arange(0.0, 360.0, pas):
        s = iou(occuper(tourner_y(P_mobile, float(d))), occ_fixe)
        if s > best:
            best, ang = s, float(d)
    return best, ang


# ------------------------------------------------------------------- corpus
def lire_manifeste():
    entrees = []
    with open(os.path.join(CORPUS, "manifeste.jsonl"), encoding="utf-8") as f:
        for l in f:
            l = l.strip()
            if not l:
                continue
            d = json.loads(l)
            if "echec" in d:
                continue
            if not os.path.isfile(os.path.join(CORPUS, d["id"] + ".npz")):
                continue
            entrees.append(d)
    return entrees


def tirer_equilibre(entrees, n, graine=20260920):
    """⚠️ EQUILIBRE SUR `source` ET SUR `texture`, pas sur la source seule :
    quaternius est gris a 85,5 % et kenney texture a 67,3 %. Un tirage par
    source donnerait un tiers de modeles sans matiere et l'IoU parlerait d'autre
    chose que de ce qu'on croit."""
    import random
    r = random.Random(graine)
    seaux = {}
    for d in entrees:
        seaux.setdefault((d["source"], bool(d.get("texture"))), []).append(d)
    for k in seaux:
        r.shuffle(seaux[k])
    cles = sorted(seaux)
    out, i = [], 0
    while len(out) < n:
        avance = False
        for k in cles:
            if seaux[k] and len(out) < n:
                out.append(seaux[k].pop())
                avance = True
        if not avance:
            break
        i += 1
    r.shuffle(out)
    return out


def image_front(npz):
    """La vue d'azimut 0 et d'elevation 15, LUE dans les poses du fichier et
    jamais deduite d'un ordre suppose."""
    d = np.load(npz, allow_pickle=True)
    poses = d["poses"]
    k = int(np.argmin(np.abs(poses[:, 0] - AZIM_CHOISI) * 10.0
                      + np.abs(poses[:, 1] - ELEV_CHOISIE)))
    return d["rgba"][k], float(poses[k, 0]), float(poses[k, 1])


# -------------------------------------------------------------- l'epreuve
def eprouver():
    """⚠️ LE ZERO DE L'INSTRUMENT SE PROUVE AVANT QU'IL SERVE. Un critere qu'on
    n'a pas vu REFUSER ne peut pas etre cru quand il accepte."""
    entrees = lire_manifeste()
    print("manifeste : %d entrees exploitables" % len(entrees))
    ech = tirer_equilibre(entrees, 6, graine=7)
    V0, F0 = charger(ech[0]["chemin"])
    V1, F1 = charger(ech[1]["chemin"])
    P0 = echantillonner(V0, F0, 1)
    P0b = echantillonner(V0, F0, 2)     # MEME maillage, autre tirage
    P1 = echantillonner(V1, F1, 1)
    occ0 = occuper(P0)

    print("\n=== EPREUVE DE L'INSTRUMENT (avant toute mesure) ===")
    a = iou(occ0, occ0)
    print("  identite            IoU = %.4f   attendu 1,0000   %s"
          % (a, "VERT" if a > 0.9999 else "ROUGE"))

    # Deux nuages du MEME maillage, tirages independants : la part de bruit
    # d'echantillonnage. Elle entre dans la borne haute, elle ne s'ignore pas.
    b = iou(occuper(P0b), occ0)
    print("  meme maillage,      IoU = %.4f   (bruit d'echantillonnage)" % b)

    # Disjonction : deux objets qui ne se touchent pas doivent rendre ZERO.
    # ⚠️ `occuper` recentre, donc translater le nuage ne prouverait rien : on
    #    fabrique la disjonction dans la GRILLE, seul endroit ou elle a un sens.
    g = np.zeros((RES_VOX, RES_VOX, RES_VOX), dtype=bool)
    h = np.zeros_like(g)
    g[:RES_VOX // 3] = True
    h[2 * RES_VOX // 3:] = True
    c = iou(g, h)
    print("  disjoints           IoU = %.4f   attendu 0,0000   %s"
          % (c, "VERT" if c < 1e-9 else "ROUGE"))

    d = iou(g, g | h)
    print("  inclusion 1/2       IoU = %.4f   attendu 0,5000   %s"
          % (d, "VERT" if abs(d - 0.5) < 1e-9 else "ROUGE"))

    e, ang = iou_meilleur(P1, occ0)
    print("  autre modele        IoU = %.4f a %3.0f deg   <- l'ordre du PLANCHER" % (e, ang))

    # La borne haute : la verite tournee d'un angle HORS grille, contre
    # elle-meme. C'est ce qu'une reconstruction PARFAITE mais desalignee
    # obtiendrait -- donc le plafond que notre instrument autorise.
    # ⚠️ Le desalignement de l'epreuve est la MOITIE du pas : c'est le PIRE cas
    #    que la grille laisse passer, donc la borne haute la plus basse possible.
    #    Un desalignement tire au hasard flatterait l'instrument.
    f, angf = iou_meilleur(tourner_y(P0b, PAS_ROT / 2.0), occ0)
    print("  soi, hors grille    IoU = %.4f a %3.0f deg   <- la BORNE HAUTE" % (f, angf))
    print("\n⚠️ Si « identite » ou « disjoints » rougit, RIEN de ce qui suit ne vaut.")
    return 0


# --------------------------------------------------------------- la mesure
def dans_le_domaine(d, seuil):
    """⚠️ LE DOMAINE DE VALIDITE DU CRITERE, CALCULE SANS LE MODELE.

    La premiere course (100 modeles) a rendu un rapport de 1,865 -- ZONE GRISE.
    Et la BORNE HAUTE, qui existait justement pour ca, a dit pourquoi : elle
    vaut 0,6512 en mediane et 0,3872 au premier decile. **Sur la moitie du
    corpus, une reconstruction PARFAITE perdrait deja plus du tiers de son IoU
    par le seul desalignement que la grille laisse passer.** Un critere applique
    hors de son domaine ne dit rien -- precedent exact du chantier retopologie,
    ou `beta > 1` disqualifiait un couple (sujet, pas) au lieu d'accuser un
    correctif.

    LE SEUIL EST DERIVE, PAS TIRE DES DONNEES : il dit « une reconstruction
    parfaite doit conserver au moins 85 % de son IoU sous le PIRE desalignement
    que la grille autorise ». C'est une tolerance de perte d'instrument, decidee
    sur principe et ECRITE AVANT la seconde course.

    ⚠️ ET LE RISQUE DE CE FILTRE, DECLARE D'AVANCE : il retiendra plutot les
       objets epais et ecartera les plats et les fins. Le lot retenu sera donc
       plus FACILE en forme, et le chiffre qui en sortira est une BORNE HAUTE de
       ce que TripoSR vaut sur le corpus entier -- jamais une moyenne. La
       repartition des formes retenues est imprimee pour qu'on puisse le juger.

    ⚠️ IL SE CALCULE SANS GPU ET SANS TRIPOSR : c'est de la geometrie pure, donc
       le filtre ne peut pas avantager le modele qu'il sert a mesurer."""
    Vv, Fv = charger(d["chemin"])
    Pv = echantillonner(Vv, Fv, 11)
    occv = occuper(Pv)
    if np.count_nonzero(occv) == 0:
        return None, 0.0
    h, _ = iou_meilleur(tourner_y(echantillonner(Vv, Fv, 12), PAS_ROT / 2.0), occv)
    return (h >= seuil), h


def mesurer(n, mc_res, sortie, domaine=0.0):
    tdir = os.environ.get("NK_GENIA_TRIPOSR") or os.path.join(OUTILS, "TripoSR")
    pdir = os.environ.get("NK_GENIA_POIDS") or os.path.join(OUTILS, "poids", "TripoSR")
    sys.path.insert(0, tdir)
    sys.path.insert(0, ICI)
    import torch
    from PIL import Image
    import torchmcubes  # noqa: F401  (notre remplacant CPU passe devant)
    from tsr.system import TSR
    from tsr.utils import resize_foreground

    entrees = lire_manifeste()
    if domaine > 0.0:
        # On tire LARGE puis on filtre par le domaine, en gardant l'equilibre.
        brut = tirer_equilibre(entrees, min(len(entrees), n * 6))
        lot, ecartes = [], 0
        print("filtrage par le domaine (borne haute >= %.2f), sans GPU..." % domaine)
        for d in brut:
            if len(lot) >= n:
                break
            dedans, h = dans_le_domaine(d, domaine)
            if dedans:
                d["_haut"] = h
                lot.append(d)
            else:
                ecartes += 1
        print("  %d retenus, %d ecartes sur %d examines (%.0f %% du corpus dans le domaine)"
              % (len(lot), ecartes, len(lot) + ecartes,
                 100.0 * len(lot) / max(len(lot) + ecartes, 1)))
    else:
        lot = tirer_equilibre(entrees, n)
    print("tirage : %d modeles" % len(lot))
    rep = {}
    for d in lot:
        rep[(d["source"], bool(d.get("texture")))] = rep.get(
            (d["source"], bool(d.get("texture"))), 0) + 1
    for k in sorted(rep):
        print("   %-12s texture=%-5s %d" % (k[0], k[1], rep[k]))

    dev = "cuda:0" if torch.cuda.is_available() else "cpu"
    t0 = time.time()
    model = TSR.from_pretrained(pdir, config_name="config.yaml", weight_name="model.ckpt")
    model.renderer.set_chunk_size(8192)
    model.to(dev)
    print("modele charge en %.1f s sur %s" % (time.time() - t0, dev))

    res = []
    for i, d in enumerate(lot):
        t = time.time()
        try:
            rgba, az, el = image_front(os.path.join(CORPUS, d["id"] + ".npz"))
            img = Image.fromarray(rgba, mode="RGBA")
            img = resize_foreground(img, 0.85)
            arr = np.array(img).astype(np.float32) / 255.0
            arr = arr[:, :, :3] * arr[:, :, 3:4] + (1.0 - arr[:, :, 3:4]) * 0.5
            img = Image.fromarray((arr * 255.0).astype(np.uint8))
            with torch.no_grad():
                codes = model([img], device=dev)
            m = model.extract_mesh(codes, True, resolution=mc_res)[0]
            Vr = np.asarray(m.vertices, dtype=np.float64)
            Vr = np.stack((Vr[:, 0], Vr[:, 2], -Vr[:, 1]), axis=1)   # Z-up -> Y-up
            Fr = np.asarray(m.faces, dtype=np.int64)
            c = 0.5 * (Vr.min(axis=0) + Vr.max(axis=0))
            e = float((Vr.max(axis=0) - Vr.min(axis=0)).max())
            Vr = (Vr - c) / max(e, 1e-12)

            Vv, Fv = charger(d["chemin"])
            Pv = echantillonner(Vv, Fv, 11)
            occv = occuper(Pv)
            if np.count_nonzero(occv) == 0:
                print("  [%3d] %-38s VERITE VIDE -- ecarte" % (i, d["id"][:38]))
                continue

            Pr = echantillonner(Vr, Fr, 11)
            s_mes, a_mes = iou_meilleur(Pr, occv)
            s_haut, _ = iou_meilleur(tourner_y(echantillonner(Vv, Fv, 12), PAS_ROT / 2.0), occv)
            res.append({"id": d["id"], "source": d["source"],
                        "texture": bool(d.get("texture")),
                        "mesure": s_mes, "angle": a_mes, "haut": s_haut,
                        "az": az, "el": el, "s": time.time() - t})
            print("  [%3d] %-38s mesure %.3f (a %3.0f)  haut %.3f  %.1fs"
                  % (i, d["id"][:38], s_mes, a_mes, s_haut, time.time() - t))
        except Exception as ex:
            print("  [%3d] %-38s ECHEC : %s: %s" % (i, d["id"][:38], type(ex).__name__, ex))

    # ── LE PLANCHER : la verite d'un AUTRE modele, meme pipeline, meme balayage.
    #    ⚠️ Appariement par DECALAGE d'un cran sur la liste deja melangee : chaque
    #       modele est compare a un autre, aucun a lui-meme.
    print("\ncalcul du plancher (verites appariees)...")
    planchers = []
    ids = [r["id"] for r in res]
    chemins = {d["id"]: d["chemin"] for d in lot}
    for k, r in enumerate(res):
        autre = ids[(k + 1) % len(ids)]
        if autre == r["id"]:
            continue
        Va, Fa = charger(chemins[autre])
        Vv, Fv = charger(chemins[r["id"]])
        s, _ = iou_meilleur(echantillonner(Va, Fa, 13), occuper(echantillonner(Vv, Fv, 11)))
        planchers.append(s)
        r["plancher"] = s

    def stat(v):
        v = np.asarray(v, dtype=float)
        return (float(np.median(v)), float(np.percentile(v, 10)),
                float(np.percentile(v, 90)), float(v.mean()))

    print("\n" + "=" * 72)
    print("J0 -- LE PROFESSEUR MERITE-T-IL D'ETRE COPIE ?   n = %d" % len(res))
    print("=" * 72)
    for nom, v in (("PLANCHER  (autre modele)", planchers),
                   ("MESURE    (TripoSR 1 vue)", [r["mesure"] for r in res]),
                   ("BORNE HAUTE (soi, desaligne)", [r["haut"] for r in res])):
        if v:
            md, p10, p90, mo = stat(v)
            print("  %-30s mediane %.4f   p10 %.4f  p90 %.4f  moy %.4f"
                  % (nom, md, p10, p90, mo))

    if planchers and res:
        mp = float(np.median(planchers))
        mm = float(np.median([r["mesure"] for r in res]))
        rapport = mm / mp if mp > 0 else float("inf")
        print("\n  RAPPORT mesure / plancher = %.3f" % rapport)
        print("  seuils ECRITS AVANT LA COURSE et non rouverts :")
        print("     >= 2,00  -> (A) l'eleve mono-vue est fonde")
        print("     <  1,50  -> J4 RETIRE : copier un professeur au niveau du hasard")
        verdict = ("VERT : (A) est fonde" if rapport >= 2.0
                   else ("ROUGE : J4 doit etre RETIRE" if rapport < 1.5
                         else "ZONE GRISE entre 1,50 et 2,00 : ni fonde ni refute"))
        print("  VERDICT : %s" % verdict)

        # L'angle retenu : s'il se concentre, le repere de TripoSR est fixe --
        # information gratuite, et elle dit si le balayage etait necessaire.
        ang = np.asarray([r["angle"] for r in res], dtype=float)
        h = np.bincount((ang / PAS_ROT).astype(int), minlength=int(360 / PAS_ROT))
        dom = int(h.argmax())
        print("\n  angle dominant : %3.0f deg sur %d / %d modeles (%.0f %%)"
              % (dom * PAS_ROT, h[dom], len(ang), 100.0 * h[dom] / max(len(ang), 1)))

    if sortie:
        with open(sortie, "w", encoding="utf-8") as f:
            json.dump({"res": res, "planchers": planchers,
                       "res_vox": RES_VOX, "pas_rot": PAS_ROT,
                       "mc_resolution": mc_res, "n_points": N_POINTS}, f, indent=1)
        print("\ndetail par modele : %s" % sortie)
    return 0


def main():
    ap = argparse.ArgumentParser(description="J0 : IoU mono-vue contre la verite du corpus")
    ap.add_argument("--phase", choices=("eprouver", "mesurer"), required=True)
    ap.add_argument("--n", type=int, default=100)
    ap.add_argument("--mc-resolution", type=int, default=256)
    ap.add_argument("--sortie", default="")
    ap.add_argument("--domaine", type=float, default=0.0,
                    help="borne haute minimale pour qu'un modele soit DANS le domaine "
                         "du critere. 0 = aucun filtre (la premiere course).")
    a = ap.parse_args()
    return (eprouver() if a.phase == "eprouver"
            else mesurer(a.n, a.mc_resolution, a.sortie, a.domaine))


if __name__ == "__main__":
    sys.exit(main())
