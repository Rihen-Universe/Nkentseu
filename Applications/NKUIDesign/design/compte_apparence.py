# -*- coding: utf-8 -*-
"""Compte les VALEURS DISTINCTES de l'apparence de NkUIDesign.

⚠️ POURQUOI CE FICHIER EXISTE.

Rodolf, 2026-09-02 : *« je veux vraiment avoir un design tres beau ; actuellement
c'est pas encore le cas. »* Le document 14 compte 174 lignes de COMPORTEMENT,
reprises de Lunacy. **Rien ne mesurait l'APPARENCE** -- et ce qui n'est pas
compte ne progresse pas.

L'indicateur retenu n'est pas « est-ce beau » (qui ne se mesure pas) mais **le
nombre de VALEURS DISTINCTES** employees par famille. Une interface parait
bricolee quand chaque cas local a recu sa propre valeur ; elle parait tenue quand
peu de valeurs se repetent. Ca, ca se compte.

⚠️ CE QUE CE COMPTE NE DIT PAS, et il faut le lire avec :
  - il mesure le CODE, pas des pixels. Une valeur citee trois fois dans du code
    mort pese autant qu'une valeur vue partout ;
  - il ne dit pas quelle valeur est BONNE. Il dit combien il y en a ;
  - il ne remplace pas une comparaison cote a cote avec la planche. Il la
    PRECEDE : on ne compare pas deux images en esperant en tirer des chiffres.

Usage :  python compte_apparence.py  [--document]
"""
import io
import os
import re
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(ICI, "..", "src", "NKUIDesign"))
REF = os.path.join(ICI, "banani_export_2026-08-30.txt")
DOC = os.path.join(ICI, "16_Etat_apparence.md")

MARQUE_DEBUT = (u"<!-- AUTO:DEBUT — regenere par `compte_apparence.py --document`. "
                u"NE RIEN ECRIRE ENTRE LES DEUX MARQUEURS. -->")
MARQUE_FIN = u"<!-- AUTO:FIN -->"


def lire(nom):
    p = os.path.join(SRC, nom)
    if not os.path.exists(p):
        return u""
    with io.open(p, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def compter(motif, texte, groupe=0):
    """Rend {valeur: occurrences} pour toutes les captures du motif."""
    d = {}
    for m in re.finditer(motif, texte):
        v = m.group(groupe if groupe else 0)
        d[v] = d.get(v, 0) + 1
    return d


def trie(d):
    return sorted(d.items(), key=lambda kv: (-kv[1], kv[0]))


# ⚠️ DEUX POPULATIONS VIVAIENT DANS LE MEME COMPTE, ET C'EST CE QUI M'A FAIT
#    ECRIRE UNE PHRASE FAUSSE. Le document 16 annoncait « tous les entiers de 1 a
#    12 : l'absence de toute echelle ». En allant LIRE les sites, la moitie sont
#    des SOMMETS DE GLYPHES -- `{x + 1.f, y + 1.f}, {x + 3.5f, y + 3.5f}` : une
#    coche, un chevron, une fleche dessines dans une boite de 16 px.
#    **Un dessin vectoriel emploie legitimement tous les entiers** ; le forcer sur
#    une echelle 2/4/8/12/16/24 ne le rendrait pas plus tenu, ca le deformerait.
#    Les compter ensemble ne mesurait pas le desordre : ca le FABRIQUAIT.
GEOMETRIE = re.compile(r"AddLine|AddPolyline|AddTriangle|AddConvexPoly|AddBezier"
                       r"|AddCircle|NkVec2\s+\w+\s*\[")


# ⚠️ COSTUME.H DESSINE SES GLYPHES PAR AddRect/AddRectFilled (les carreaux,
#    les yeux, les losanges) -- des appels que le filtre general ne peut PAS
#    ecarter (dans Panels.h, AddRectFilled est de la vraie mise en page).
#    Sans cette variante, 29 sommets d'icones passaient pour des espacements.
GEOMETRIE_COSTUME = re.compile(
    r"AddLine|AddPolyline|AddTriangle|AddConvexPoly|AddBezier|AddCircle"
    r"|AddRect|AddRectFilled|losange\(|NkVec2\s+\w+\s*\[")


def compter_mise_en_page(motif, texte, groupe=0, geo_etendue=False):
    """Comme `compter`, mais SANS les lignes qui dessinent un glyphe.

    ⚠️ UN TABLEAU `NkVec2 p[5] = {...}` TIENT SOUVENT SUR PLUSIEURS LIGNES. Filtrer
       ligne a ligne laisserait passer les lignes de CONTINUATION -- et ces
       fuites-la sont exactement celles qui gonflent le compte. On reste donc dans
       l'etat « geometrie » jusqu'a la fermeture du litteral.
    """
    d = {}
    dans_tableau = False
    regle = GEOMETRIE_COSTUME if geo_etendue else GEOMETRIE
    for ligne in texte.split(u"\n"):
        # Un COMMENTAIRE n'est pas un site : la doc de `CentrerBande` citait
        # l'idiome `r.y + 3.f` remplace... et se faisait compter comme lui.
        if ligne.lstrip().startswith(u"//"):
            continue
        geo = bool(regle.search(ligne))
        # 🔴 LA CONDITION D'ENTREE EN TABLEAU ETAIT FAUSSE (mesuree le 02/09
        #    sur le panneau IA) : elle exigeait « pas de } apres le = », or la
        #    PREMIERE ligne d'un tableau contient presque toujours `{{x, y},` --
        #    donc l'etat ne s'armait jamais et les lignes de CONTINUATION
        #    (`{c.x + 15.f, ...},`) etaient comptees comme de la mise en page.
        #    Le bon critere : un tableau qui ne se FERME pas (`};`) sur sa ligne.
        if geo and re.search(r"NkVec2\s+\w+\s*\[", ligne) and u"};" not in ligne:
            dans_tableau = True
        if dans_tableau:
            geo = True
            if u"};" in ligne:
                dans_tableau = False
        if geo:
            continue
        # Les ecarts JUSTIFIES portent leur raison SUR LA LIGNE :
        # `// [hors-echelle: <raison>]`. Ils sont ecartes du compte mais
        # comptes A PART (famille « transcrits ») -- ecarter sans montrer,
        # c'est se donner un beau chiffre en cachant du code.
        if u"[hors-echelle:" in ligne:
            continue
        for m in re.finditer(motif, ligne):
            v = m.group(groupe if groupe else 0)
            d[v] = d.get(v, 0) + 1
    return d


def compter_glyphes(motif, texte, groupe=0):
    """Le complement : ce que `compter_mise_en_page` ecarte.

    ⚠️ ON LE COMPTE ET ON L'AFFICHE, on ne le fait pas disparaitre. Ecarter une
       population sans la montrer, c'est se donner un beau chiffre en cachant la
       moitie du code -- et personne ne pourrait verifier que l'ecart est justifie.
    """
    tous = compter(motif, texte, groupe)
    page = compter_mise_en_page(motif, texte, groupe)
    d = {}
    for k, v in tous.items():
        reste = v - page.get(k, 0)
        if reste > 0:
            d[k] = reste
    return d


def familles():
    panels = lire("Panels.h")
    rend = lire("Renderers.h")
    cost = lire("Costume.h")
    tout = panels + rend + cost
    f = []

    # 1. L'ECHELLE TYPOGRAPHIQUE — les tailles citees par le costume.
    f.append((u"tailles de texte", compter(r"F\.px(\d+)", tout, 1), u"px"))

    # 2. LA DENSITE — la hauteur des rangees demandees a la disposition.
    f.append((u"hauteurs de rangee", compter(r"NextItemRect\(-?[\d.]+f, (\d+)\.f", tout, 1),
              u"px"))

    # 3. LES RAYONS — le dernier argument des primitives arrondies.
    f.append((u"rayons d'angle", compter(r"(?:AddRectFilled|AddRect)\([^;\n]*?,\s*(\d+)\.f\)",
                                         tout, 1), u"px"))

    # 4. LE RYTHME D'ESPACEMENT — les decalages litteraux poses sur x ou y.
    #    ⚠️ C'est la famille la plus revelatrice : une echelle (4/8/12/16) se
    #       voit tout de suite, un placement au cas par cas aussi.
    MOTIF_DEC = r"[xy][01]?\s\+\s(\d+)\.f"
    d_page = compter_mise_en_page(MOTIF_DEC, panels + rend, 1)
    for k, v in compter_mise_en_page(MOTIF_DEC, cost, 1, geo_etendue=True).items():
        d_page[k] = d_page.get(k, 0) + v
    f.append((u"decalages d'espacement (mise en page)", d_page, u"px"))

    # 4ter. LES ECARTS JUSTIFIES `[hors-echelle: ...]` — ecartes du compte,
    #       mais LISTES avec leurs raisons : la trappe ne doit pas etre un
    #       tiroir sombre. Si cette famille grossit plus vite que le reste ne
    #       retrecit, c'est que le tag sert de contournement.
    tags = {}
    for ligne in tout.split(u"\n"):
        if u"[hors-echelle:" not in ligne:
            continue
        raison = ligne.split(u"[hors-echelle:")[1].split(u"]")[0].strip()
        n = len(re.findall(MOTIF_DEC, ligne))
        if n:
            tags[raison] = tags.get(raison, 0) + n
    f.append((u"ecarts justifies [hors-echelle:] (exclus)", tags, u" site(s)"))

    # 4bis. LES SOMMETS DE GLYPHES — comptes A PART, et EXCLUS de l'echelle.
    #       Ils n'ont pas a s'aligner sur 2/4/8/12/16/24 : ce sont des dessins.
    f.append((u"sommets de glyphes (hors echelle)",
              compter_glyphes(MOTIF_DEC, tout, 1), u"px"))

    # 5. LES COULEURS EN DUR — elles devraient etre RARES : le theme porte les
    #    couleurs par role. Beaucoup de hex ici = le theme est contourne.
    f.append((u"couleurs ecrites en dur", compter(r'"(#[0-9a-fA-F]{6})"', tout, 1), u""))
    return f


def reference():
    """Les tailles de texte que la planche DECLARE, pour comparaison."""
    if not os.path.exists(REF):
        return {}
    with io.open(REF, "r", encoding="utf-8", errors="replace") as f:
        t = f.read()
    d = {}
    for m in re.finditer(r"(?:fontSize|font-size)[:=]\s*'?(\d+(?:\.\d+)?)", t):
        v = m.group(1)
        d[v] = d.get(v, 0) + 1
    for m in re.finditer(r"text-\[(\d+(?:\.\d+)?)px\]", t):
        v = m.group(1)
        d[v] = d.get(v, 0) + 1
    return d


def bloc():
    L = [u"### Le compte des valeurs distinctes", u"",
         u"| famille | valeurs distinctes | les trois plus employées | part des trois |",
         u"|---|---|---|---|"]
    for nom, d, unite in familles():
        t = trie(d)
        total = sum(d.values())
        top = t[:3]
        part = (sum(v for _, v in top) * 100 // total) if total else 0
        libelle = u" · ".join(u"**%s%s** (%d)" % (k, unite, v) for k, v in top) or u"—"
        L.append(u"| %s | **%d** | %s | %d %% |" % (nom, len(d), libelle, part))
    L.append(u"")
    ref = reference()
    if ref:
        t = trie(ref)
        total = sum(ref.values())
        top = t[:3]
        part = sum(v for _, v in top) * 100 // total
        L += [u"### La planche de référence, mesurée pareil", u"",
              u"| | valeurs distinctes | les trois plus employées | part des trois |",
              u"|---|---|---|---|",
              u"| tailles de texte (planche Banani) | **%d** | %s | %d %% |"
              % (len(ref), u" · ".join(u"**%spx** (%d)" % (k, v) for k, v in top), part), u""]
    L += [u"### Le détail, famille par famille", u""]
    for nom, d, unite in familles():
        t = trie(d)
        L.append(u"**%s** — %d valeurs : %s"
                 % (nom, len(d),
                    u", ".join(u"%s%s×%d" % (k, unite, v) for k, v in t[:14])))
        L.append(u"")
    return u"\n".join(L)


def regenerer():
    if not os.path.exists(DOC):
        sys.stderr.write("16_Etat_apparence.md est absent : ecris-le d'abord, "
                         "avec ses deux marqueurs AUTO.\n")
        return 2
    with io.open(DOC, "r", encoding="utf-8", newline="") as f:
        doc = f.read()
    i, j = doc.find(MARQUE_DEBUT), doc.find(MARQUE_FIN)
    if i < 0 or j < 0 or j < i:
        sys.stderr.write("marqueurs AUTO introuvables : rien n'est ecrit.\n")
        return 2
    avant, apres = doc[:i + len(MARQUE_DEBUT)], doc[j:]
    neuf = avant + u"\n\n" + bloc() + u"\n" + apres
    if not neuf.startswith(avant) or not neuf.endswith(apres):
        sys.stderr.write("la prose autour du bloc n'est pas intacte : rien n'est ecrit.\n")
        return 2
    with io.open(DOC, "w", encoding="utf-8", newline="") as f:
        f.write(neuf)
    with io.open(DOC, "r", encoding="utf-8", newline="") as f:
        relu = f.read()
    sys.stdout.write(u"16_Etat_apparence.md : %d octets ; relu identique=%s\n"
                     % (len(relu), relu == neuf))
    return 0 if relu == neuf else 2


# L'ECHELLE CHOISIE — elle vit dans `Costume.h` (costume::Esp*), et elle est
# REPETEE ici parce que c'est ce fichier qui la fait respecter. Les deux doivent
# rester d'accord : si quelqu'un change l'une, `--verifier` le dira.
ECHELLE = [2, 4, 8, 12, 16, 24]


def verifier():
    """Echoue si des decalages d'espacement sortent de l'echelle.

    🔴 SANS CE MODE, L'ECHELLE NE SERAIT QU'UN COMMENTAIRE. Le document 16
       demande de passer de 34 valeurs a six ; rien n'empecherait la
       trente-cinquieme de revenir au premier cas particulier -- et personne ne
       le verrait, parce qu'un `+ 7.f` de plus ne casse rien et ne fait tomber
       aucune garde.

    ⚠️ IL REND LE NOMBRE DE **SITES**, PAS SEULEMENT DE VALEURS. « Neuf valeurs
       hors echelle » ne dit pas s'il faut corriger neuf lignes ou trois cents.
       C'est le compte de sites qui dit le travail restant, et c'est lui qui doit
       tomber a zero.
    """
    # ⚠️ ON CHERCHE LA FAMILLE PAR SOUS-CHAINE, ET ON EXIGE DE L'AVOIR TROUVEE.
    #    Ma premiere version comparait le nom en entier, avec un accent que la
    #    table n'a pas : elle ne trouvait AUCUNE famille, donc aucune valeur hors
    #    echelle, donc elle annoncait « TENUE » sur un code qui en portait 34.
    #    *Un verificateur qui ne trouve pas son sujet dit « tout va bien ».*
    #    C'est la faute du jour dans sa forme la plus pure -- le temoin ne variait
    #    pas parce qu'il ne regardait rien.
    hors = {}
    trouvee = False
    for nom, d, _ in familles():
        if u"espacement" not in nom:
            continue
        trouvee = True
        for k, v in d.items():
            if int(k) not in ECHELLE:
                hors[k] = v
    if not trouvee:
        print(u"echelle d'espacement : FAMILLE INTROUVABLE -- le verificateur ne "
              u"mesure rien, ne le lisez pas comme un succes")
        return 2
    sites = sum(hors.values())
    if not hors:
        print(u"echelle d'espacement : TENUE (%s)" % u"/".join(str(v) for v in ECHELLE))
        return 0
    t = sorted(hors.items(), key=lambda kv: -kv[1])
    print(u"echelle d'espacement : %d valeur(s) hors echelle, %d site(s)"
          % (len(hors), sites))
    print(u"   echelle : %s" % u"/".join(str(v) for v in ECHELLE))
    print(u"   hors    : %s" % u", ".join(u"%spx x%d" % (k, v) for k, v in t[:16]))
    return 1


def main():
    if "--document" in sys.argv:
        return regenerer()
    if "--verifier" in sys.argv:
        return verifier()
    for nom, d, unite in familles():
        t = trie(d)
        print(u"%-26s %3d valeurs : %s" % (nom, len(d),
                                           u", ".join(u"%s%s x%d" % (k, unite, v)
                                                      for k, v in t[:12])))
    ref = reference()
    if ref:
        t = trie(ref)
        print(u"%-26s %3d valeurs : %s" % (u"(planche) texte", len(ref),
                                           u", ".join(u"%spx x%d" % (k, v) for k, v in t[:12])))
    return 0


if __name__ == "__main__":
    if sys.stdout.encoding and sys.stdout.encoding.lower() not in ("utf-8", "utf8"):
        sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
