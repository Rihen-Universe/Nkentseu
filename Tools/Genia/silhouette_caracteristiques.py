# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/silhouette_caracteristiques.py
# -----------------------------------------------------------------------------
# LIRE UNE SILHOUETTE PAR LA MESURE, PAS PAR UN MODELE.
#
# Idee de Rodolf : se servir de ce qu'on VOIT pour remplir le formulaire, au lieu
# de le faire remplir a l'aveugle. Le modele qui decrit (moondream) est degenere
# -- il repond « urn » pour une chaise, une table, un bol ET une porte -- donc on
# ne s'appuie pas dessus. Mais une silhouette se MESURE, et nous savons mesurer.
#
# CINQ CARACTERISTIQUES, pas une de plus pour l'instant :
#   1. nombre de montants verticaux   (4 pieds / 2 montants / 1 pietement)
#   2. courbure du bord superieur     (arc = plateau rond ; segment = rectangle)
#   3. bande de ceinture sous le plateau
#   4. entretoise basse
#   5. rapport hauteur / largeur
#
# ATTENTION -- LA VERITE EST ECRITE AVANT LES IMAGES (table VERITE ci-dessous),
#    et les images sont FABRIQUEES a partir d'elle. Un jeu d'epreuve ecrit apres
#    coup se regle sur ce qu'on veut lui faire dire.
#
# ATTENTION -- UN JEU SYNTHETIQUE EST NECESSAIRE, PAS SUFFISANT. S'il echoue sur
#    des silhouettes propres, il echouera sur des photos. S'il reussit, cela ne
#    prouve RIEN sur les photos : il faudra les essayer.
#
# ATTENTION -- LE NEGATIF EST OBLIGATOIRE : deux entrees ne sont PAS des tables
#    (une chaise, un tonneau). Un extracteur qui repond toujours quelque chose
#    avec assurance ne mesure rien.
#
# USAGE :
#   python Tools/Genia/silhouette_caracteristiques.py --epreuve
#   python Tools/Genia/silhouette_caracteristiques.py --image chemin.png
# -----------------------------------------------------------------------------
import argparse, os, sys
import numpy as np

# -- LA VERITE, ECRITE AVANT LES IMAGES --------------------------------------
# CORRECTION DU 25/09, ET C'EST LA PREMIERE LECON DU JEU. J'avais ecrit
# « 4 montants » pour une table a quatre pieds. C'etait FAUX pour une vue de
# FACE : les deux pieds arriere sont caches derriere les deux pieds avant, et la
# silhouette n'en montre que DEUX. Mon attendu contredisait ma propre image. La
# caracteristique mesurable est donc le nombre de montants VISIBLES ; en deduire
# « quatre pieds » est une SECONDE etape, explicite, qui depend de l'angle.
#
# montants : montants VISIBLES de face (0 = aucun)
# bord     : 'arc' ou 'segment'
# ceinture / entretoise : True/False
# table    : False = contre-exemple ; la confiance doit RESTER basse
VERITE = {
    "ronde_pietement_central":  dict(montants=1, bord="arc",     ceinture=False, entretoise=False, table=True),
    "ronde_quatre_pieds":       dict(montants=2, bord="arc",     ceinture=False, entretoise=False, table=True),
    "rect_quatre_pieds":        dict(montants=2, bord="segment", ceinture=False, entretoise=False, table=True),
    "rect_ceinture":            dict(montants=2, bord="segment", ceinture=True,  entretoise=False, table=True),
    "rect_entretoise":          dict(montants=2, bord="segment", ceinture=False, entretoise=True,  table=True),
    "rect_ceinture_entretoise": dict(montants=2, bord="segment", ceinture=True,  entretoise=True,  table=True),
    "rect_pieds_en_X":          dict(montants=2, bord="segment", ceinture=False, entretoise=True,  table=True),
    # TRETEAUX : deux trames en V inversee. Elles se rejoignent en haut (2) et
    # s'ecartent en bas (4) -- c'est justement leur signature, et la mediane sur
    # les hauteurs echantillonnees vaut 4.
    "treteaux":                 dict(montants=4, bord="segment", ceinture=False, entretoise=False, table=True),
    "NEGATIF_chaise":           dict(montants=2, bord="segment", ceinture=False, entretoise=False, table=False),
    "NEGATIF_tonneau":          dict(montants=0, bord="arc",     ceinture=False, entretoise=False, table=False),
}

H, W = 400, 400


def _vide():
    return np.zeros((H, W), dtype=bool)


def _rect(m, x0, y0, x1, y1):
    m[max(0, y0):min(H, y1), max(0, x0):min(W, x1)] = True


def _disque_bande(m, cx, cy, rx, ry, y0, y1):
    ys, xs = np.mgrid[0:H, 0:W]
    e = ((xs - cx) / float(rx)) ** 2 + ((ys - cy) / float(ry)) ** 2 <= 1.0
    b = _vide()
    b[max(0, y0):min(H, y1), :] = True
    m |= (e & b)


def _segment(m, x0, y0, x1, y1, ep):
    n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2 + 1)
    for t in np.linspace(0, 1, n):
        x = int(round(x0 + (x1 - x0) * t))
        y = int(round(y0 + (y1 - y0) * t))
        _rect(m, x - ep // 2, y - ep // 2, x + ep // 2 + 1, y + ep // 2 + 1)


def fabriquer(nom):
    """Fabrique la silhouette DEPUIS la verite. Vue de face, objet centre."""
    m = _vide()
    gx0, gx1 = 60, 340
    yTop, yPlat, ySol = 90, 112, 330
    v = VERITE[nom]
    if nom == "NEGATIF_tonneau":
        _disque_bande(m, 200, 210, 95, 125, 85, 335)
        return m
    if nom == "NEGATIF_chaise":
        _rect(m, 110, 60, 132, 250)
        _rect(m, 268, 60, 290, 250)
        for y in range(70, 240, 34):
            _rect(m, 132, y, 268, y + 12)
        _rect(m, 105, 240, 295, 262)
        for x in (112, 262):
            _rect(m, x, 262, x + 26, ySol)
        return m
    # -- le plateau ----------------------------------------------------------
    if v["bord"] == "arc":
        _disque_bande(m, 200, yPlat + 30, 148, 42, yTop, yPlat + 2)
        _rect(m, gx0 - 8, yPlat - 8, gx1 + 8, yPlat + 6)
    else:
        _rect(m, gx0 - 12, yTop, gx1 + 12, yPlat)
    # -- les montants --------------------------------------------------------
    if v["montants"] == 1:
        _rect(m, 185, yPlat, 215, ySol - 16)
        _disque_bande(m, 200, ySol - 40, 78, 44, ySol - 22, ySol)
    elif nom == "rect_pieds_en_X":
        _segment(m, 95, yPlat, 305, ySol, 20)
        _segment(m, 305, yPlat, 95, ySol, 20)
    elif nom == "treteaux":
        for cx in (130, 270):
            _segment(m, cx - 46, ySol, cx, yPlat, 17)
            _segment(m, cx + 46, ySol, cx, yPlat, 17)
    else:
        for x in (gx0 + 6, gx1 - 32):
            _rect(m, x, yPlat, x + 26, ySol)
    # -- ceinture et entretoise ----------------------------------------------
    if v["ceinture"]:
        _rect(m, gx0 + 6, yPlat, gx1 - 6, yPlat + 34)
    if v["entretoise"]:
        yy = ySol - 62
        _rect(m, gx0 + 6, yy, gx1 - 6, yy + 15)
    return m


# -- L'EXTRACTEUR ------------------------------------------------------------
def _runs(ligne):
    """Plages contigues de vrai, rendues comme (debut, fin)."""
    out, d = [], None
    for i, v in enumerate(ligne):
        if v and d is None:
            d = i
        elif not v and d is not None:
            out.append((d, i))
            d = None
    if d is not None:
        out.append((d, len(ligne)))
    return out


def caracteriser(masque):
    ys, xs = np.nonzero(masque)
    if len(ys) < 50:
        return dict(vide=True)
    y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
    haut, larg = float(y1 - y0 + 1), float(x1 - x0 + 1)
    sub = masque[y0:y1 + 1, x0:x1 + 1]
    hh, ww = sub.shape

    # 1. MONTANTS. Une seule hauteur ne suffit pas : des pieds en X se touchent
    #    au milieu et donneraient « 1 ». On compte a plusieurs hauteurs, on prend
    #    la mediane, et on note si le compte VARIE (signature du croisement).
    comptes = []
    for f in (0.55, 0.62, 0.70, 0.78, 0.86, 0.94):
        r = _runs(sub[min(hh - 1, int(hh * f)), :])
        r = [a for a in r if (a[1] - a[0]) >= max(3, ww // 60)]
        comptes.append(len(r))
    montants = int(np.median(comptes)) if comptes else 0
    croisement = (max(comptes) - min(comptes)) >= 1 and min(comptes) <= 1 and max(comptes) >= 2

    # 2. BORD SUPERIEUR : fleche de l'arc rapportee a la largeur.
    colonnes = [c for c in range(ww) if sub[:, c].any()]
    sommet = np.array([np.argmax(sub[:, c]) for c in colonnes], dtype=float)
    if len(sommet) >= 8:
        n = len(sommet)
        bords = np.r_[sommet[:max(2, n // 8)], sommet[-max(2, n // 8):]]
        fleche_rel = float(np.mean(bords) - sommet.min()) / max(larg, 1.0)
    else:
        fleche_rel = 0.0
    bord = "arc" if fleche_rel > 0.02 else "segment"

    # 3/4. CEINTURE et ENTRETOISE : une bande HORIZONTALE nettement plus large
    #      que les montants, juste sous le plateau, ou dans le tiers bas.
    largeur_ligne = np.zeros(hh)
    for y in range(hh):
        r = _runs(sub[y, :])
        largeur_ligne[y] = max([b - a for a, b in r], default=0)
    lmax = largeur_ligne.max()
    yPlateauFin = 0
    for y in range(hh):
        if largeur_ligne[y] >= 0.85 * lmax:
            yPlateauFin = y
        elif y > hh * 0.05:
            break
    zoneC = slice(yPlateauFin + 1, max(yPlateauFin + 2, int(hh * 0.40)))
    zoneB = slice(int(hh * 0.55), int(hh * 0.97))
    socle = float(np.median(largeur_ligne[int(hh * 0.45):int(hh * 0.55)])) if hh > 20 else 0.0
    seuil = max(0.55 * lmax, socle * 1.6)
    ceinture = bool(largeur_ligne[zoneC].size and largeur_ligne[zoneC].max() > seuil)
    entretoise = bool(largeur_ligne[zoneB].size and largeur_ligne[zoneB].max() > seuil)

    # -- CONFIANCE : « est-ce qu'une preselection a un sens ici ? » ----------
    # Deux conditions mesurees, et les NEGATIFS servent a les regler :
    #   (a) un PLATEAU en haut -- la bande la plus large doit etre pres du bord
    #       superieur et couvrir l'essentiel de la largeur. Une chaise a son
    #       element large (l'assise) a MI-HAUTEUR : elle echoue ici.
    #   (b) l'objet doit etre AJOURE sous le plateau : une table a du vide entre
    #       ses pieds. Un tonneau est plein : il echoue ici.
    yLarge = int(np.argmax(largeur_ligne))
    plateau_en_haut = (yLarge < 0.30 * hh) and (lmax > 0.70 * ww)
    bas = sub[int(hh * 0.55):int(hh * 0.95), :]
    ajoure = bool(bas.size) and float(bas.mean()) < 0.55
    confiance_table = bool(plateau_en_haut and ajoure)

    return dict(vide=False, montants=montants, croisement=croisement, bord=bord,
                confiance_table=confiance_table, plateau_en_haut=bool(plateau_en_haut),
                ajoure=ajoure,
                fleche_rel=round(fleche_rel, 4), ceinture=ceinture,
                entretoise=entretoise, h_sur_l=round(haut / max(larg, 1.0), 3),
                remplissage=round(float(sub.mean()), 3))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--epreuve", action="store_true")
    ap.add_argument("--image")
    ap.add_argument("--sortie", default="logs_genia3d/silhouette")
    a = ap.parse_args()
    if a.image:
        from PIL import Image
        im = np.asarray(Image.open(a.image).convert("L"))
        m = im < 128 if im.mean() > 127 else im > 127
        print(caracteriser(m))
        return 0
    if not a.epreuve:
        ap.print_help()
        return 2
    os.makedirs(a.sortie, exist_ok=True)
    try:
        from PIL import Image
        ecrire = True
    except Exception:
        ecrire = False
    cols = ["montants", "bord", "ceinture", "entretoise"]
    bons = {c: 0 for c in cols}
    total = 0
    lignes = []
    for nom, v in VERITE.items():
        m = fabriquer(nom)
        if ecrire:
            Image.fromarray((~m * 255).astype(np.uint8)).save(
                os.path.join(a.sortie, nom + ".png"))
        c = caracteriser(m)
        if v["table"]:
            total += 1
            for k in cols:
                if c.get(k) == v[k]:
                    bons[k] += 1
        lignes.append((nom, v, c))
    print("JEU D'EPREUVE -- silhouettes synthetiques, verite ecrite avant les images")
    print("%-26s %-20s %-20s" % ("cas", "attendu", "mesure"))
    for nom, v, c in lignes:
        att = "%d mont,%s,%s%s" % (v["montants"], v["bord"][:3],
                                   "C" if v["ceinture"] else "-",
                                   "E" if v["entretoise"] else "-")
        mes = "%s mont,%s,%s%s" % (c.get("montants", "?"), str(c.get("bord", "?"))[:3],
                                   "C" if c.get("ceinture") else "-",
                                   "E" if c.get("entretoise") else "-")
        if v["table"]:
            fin = "  ok" if all(c.get(k) == v[k] for k in cols) else "  <-- ECART"
        else:
            fin = "  (negatif)"
        print("%-26s %-20s %-20s%s" % (nom, att, mes, fin))
    print("")
    print("TAUX DE RECONNAISSANCE (sur les %d vraies tables) :" % total)
    for k in cols:
        print("   %-12s %d/%d" % (k, bons[k], total))
    print("")
    nbc = sum(1 for n, v, c in lignes if v["table"] and c.get("confiance_table"))
    print("   %-12s %d/%d  (les vraies tables reconnues comme telles)" % ("confiance", nbc, total))
    print("")
    print("NEGATIFS -- ce qui doit RESTER indecidable :")
    for nom, v, c in lignes:
        if not v["table"]:
            print("   %-18s confiance=%-5s (plateau_en_haut=%s ajoure=%s) montants=%s"
                  % (nom, c.get("confiance_table"), c.get("plateau_en_haut"),
                     c.get("ajoure"), c.get("montants")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
