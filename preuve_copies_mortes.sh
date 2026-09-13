#!/usr/bin/env bash
#
# preuve_copies_mortes.sh — LA PREUVE, PAS LA LISTE (2026-08-22)
# =============================================================================
# POURQUOI
#
#   « Un fichier qu'aucun build ne compile mais que tout grep lit est un faux
#     temoin permanent — pour les outils comme pour les humains. »
#
#   Mesure du 22/08 : `NkOpenglDevice copy.hpp` porte `mCaps.timestampQueries =
#   true`. Le detecteur de capacites l'a compte comme une ecriture reelle. Le
#   danger n'est pas ce compte : c'est le cas symetrique, ou une copie morte
#   fournit la SEULE « lecture » d'un champ creux et fait MANQUER un defaut,
#   en silence.
#
# CE QUE CE SCRIPT PRODUIT
#   La liste des fichiers « copy » VERSIONNES, et pour chacun la reponse
#   mesuree a une seule question : QUELQUE CHOSE LE COMPILE-T-IL ?
#
# ⚠️ POURQUOI CE N'EST PAS UN `grep` — ET POURQUOI J'AI FAILLI M'ARRETER LA
#   Ma premiere mesure cherchait qui CITE le basename (#include ou .jenga). Elle
#   rendait « 25 non cites sur 27 ». Elle etait FAUSSE COMME PREUVE : 118 .jenga
#   de ce depot declarent leurs sources par JOKER (`src/**.cpp`, `src/**/*.h`).
#   Un fichier que personne ne nomme peut donc etre compile quand meme.
#   Deux voies, et il faut les DEUX :
#     1. citation textuelle (#include, ou source listee explicitement) ;
#     2. couverture par un joker du .jenga le plus proche.
#
# ⚠️ ET LE DOUTE PENCHE DU COTE SUR. En cas d'ambiguite sur un motif, ce script
#   declare le fichier VIVANT. Se tromper en disant « vivant » coute un fichier
#   qu'on ne supprime pas ; se tromper en disant « mort » fait supprimer du code
#   qui compile. Les deux erreurs n'ont pas le meme prix.
#
# USAGE
#   ./preuve_copies_mortes.sh            # le tableau
#   ./preuve_copies_mortes.sh --morts    # seulement les chemins prouves morts
#
# =============================================================================
# ⚠️ LE DOMAINE DE CETTE PREUVE, MESURE LE 27/08 — 736 FICHIERS SONT DEHORS
# =============================================================================
#   Cette preuve dit : « aucun .jenga ne le cite, aucun joker ne le couvre ».
#   Elle ne peut donc rien dire d un fichier QUI N A JAMAIS EU VOCATION A ETRE
#   COMPILE.
#
#   MESURE : 736 fichiers visuels ou documentaires sont suivis sous Applications/
#   et Kernel/ (png, jpg, docx, pdf, html). SUR CES 736, LES 736 vivent dans un
#   dossier qui ne contient AUCUNE source suivie. 18 dossiers, dont :
#       237  Applications/NKCode/data/textures/icon
#       237  Applications/NK3DModeler/data/textures/icon
#       156  Applications/Songoo/Resources/Songoo/assets/animrihen
#        16  Applications/Nogee/design
#        15  Kernel/Runtime/NKGraph/references
#         2  Applications/NkAnimaEditor/important
#
#   ⚠️ AUCUN N EST SUSPECT. Un fichier qu aucun .jenga ne mentionne n est pas
#   forcement mort : il peut n avoir jamais eu vocation a etre compile. Des
#   icones, des textures, des references de design, un dossier nomme
#   « important ». Ce sont des ACTIFS, pas du code oublie.
#
#   CE QUE CA VEUT DIRE POUR QUI VOUDRA ELARGIR CET OUTIL : la tentation sera de
#   lui donner plus de fichiers. Elle a deja ete mesuree et refusee une fois --
#   passer aux 810 sources d Applications/ rendait 449 morts, dont un en-tete
#   inclus par 13 fichiers. Passer aux ACTIFS serait pire : la preuve les
#   declarerait TOUS morts, les 736, par construction.
#
#   LE 27/08, DIX-NEUF DE CES FICHIERS ONT FAILLI PARTIR AVEC DU CODE MORT. Ils
#   etaient dans « Applications/Pong copy/ », un dossier prouve mort et supprime.
#   La preuve portait sur ses SOURCES ; elle ne disait rien de ses captures. Elles
#   vivent maintenant dans Applications/Pong/Captures/2026-05-16_bug_rendu/, avec
#   un LISEZ-MOI qui dit ce qu on sait et ce qu on ne sait pas d elles.
#
#   « La population et la preuve sont solidaires. » Le domaine de celle-ci
#   s arrete aux sources, et il s y arrete par construction, pas par oubli.
# =============================================================================
#
# ⚠️ CE SCRIPT NE SUPPRIME RIEN, et c'est deliberе : supprimer 27 fichiers depuis
#   une branche pendant que cinq agents travaillent fabrique des conflits qu'on
#   resout mal. Il PROUVE ; la suppression se decide et se fait sur main.
# =============================================================================
set -uo pipefail
dire() { printf '%s\n' "$*"; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || exit 2

MORTS_SEULS=0
[ "${1:-}" = "--morts" ] && MORTS_SEULS=1

python - "$MORTS_SEULS" <<'PYFIN'
# -*- coding: utf-8 -*-
import io, os, re, subprocess, sys, fnmatch

morts_seuls = sys.argv[1] == "1"

def git(*a):
    return subprocess.run(["git"] + list(a), capture_output=True, text=True).stdout.splitlines()

suivis = git("ls-files")
# ⚠️ LE MOTIF PORTE SUR LE CHEMIN, PAS SUR LE NOM DE FICHIER (27/08).
# Mesure : Applications/Pong copy/ est un DOSSIER copie, et ses DIX sources
# portent des noms parfaitement normaux (Apps.cpp, PongGame.h...). Le motif
# applique au seul nom de fichier en voyait ZERO. Applique au chemin, il les
# voit toutes les dix, et la PREUVE reste dans son domaine de validite : ce
# sont bien des copies, personne ne les inclut.
copies = [f for f in suivis if re.search(r" copy( \d+)?[/.]", f) and re.search(r"[.](h|hpp|cpp|inl|c)$", f)]

# ⚠️ LA PREUVE EST BONNE, LA POPULATION EST UNE HEURISTIQUE DE NOM.
# La ligne ci-dessus choisit les candidats par leur NOM. C est « chercher un nom
# n est pas chercher un usage » — la faute la plus tenace de ce depot — logee
# dans l outil ecrit pour la traquer. Mesure du 27/08 : PBRGame.cpp est mort par
# la MEME preuve et invisible ici, parce qu il porte un nom normal.
# Parade minimale : une population DECLAREE en plus du motif. La vraie parade
# serait d abandonner le motif et de prouver tous les fichiers suivis
# d Applications/ — cout non mesure, donc non faite.
declarees = []
try:
    for l in io.open("config/copies_mortes_declarees.list", encoding="utf-8", errors="replace"):
        l = l.strip()
        if not l or l.startswith("#"):
            continue
        chemin = l.split("|")[0].strip()
        if chemin:
            declarees.append(chemin)
except OSError:
    pass
# ⚠️ PLANCHER DE LECTURE (27/08). L outil n en avait AUCUN, et le nettoyage du
# jour l a mis a nu : une fois les copies supprimees, il a rendu
#     « 0 fichier(s) examine(s) -- 0 PROUVE(S) MORT(S), 0 encore compile(s). »
# et code 0. Or « 0 mort sur 0 examine » se lit « tout est propre » alors que ca
# veut dire « je n ai rien regarde ». C est le defaut que ce depot traque depuis
# le premier jour, dans l outil de preuve lui-meme.
# Les trois autres verificateurs ont deja leur plancher (NB_MOTIFS, NB_HITS,
# scripts examines) ; celui-ci n en avait pas.
# La population VIDE doit donc etre DECLAREE pour etre acceptee -- une donnee,
# visible dans un diff, comme les cliquets.
vide_attendue = False
try:
    for l in io.open("config/copies_mortes_declarees.list", encoding="utf-8", errors="replace"):
        if l.strip().startswith("# POPULATION-VIDE-ATTENDUE") and "oui" in l.lower():
            vide_attendue = True
except OSError:
    pass

for d in declarees:
    if d not in copies:
        if d in suivis:
            copies.append(d)
        else:
            print("[copies] DECLARE MAIS NON SUIVI PAR GIT, ignore : %s" % d)
copies.sort()

if not copies:
    if not vide_attendue:
        print("[copies] ECHEC D INSTRUMENT : population VIDE, 0 fichier examine.")
        print("[copies]   « 0 mort sur 0 examine » se lit « tout est propre », et veut dire")
        print("[copies]   « je n ai rien regarde ». Deux causes, opposees :")
        print("[copies]     - toutes les copies ont ete nettoyees   -> declare-le :")
        print("[copies]       ajoute « # POPULATION-VIDE-ATTENDUE = oui » dans")
        print("[copies]       config/copies_mortes_declarees.list, avec la raison ;")
        print("[copies]     - le selecteur est casse                -> repare-le.")
        print("[copies]   L outil ne peut pas distinguer les deux. C est a toi de le dire.")
        sys.exit(2)
    print("[copies] population VIDE, et c est DECLARE dans")
    print("[copies]   config/copies_mortes_declarees.list. Rien n a ete examine :")
    print("[copies]   ce passage ne prouve donc RIEN sur le depot, et ne pretend pas le faire.")

# --- index des .jenga et de leurs motifs -----------------------------------
jengas = {}
for f in suivis:
    if f.endswith(".jenga"):
        try:
            t = io.open(f, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        # ⚠️ TOUS LES MOTIFS D'UN .jenga NE SONT PAS DES INCLUSIONS.
        # Defaut de CE script, trouve en relisant ce qu'il affirmait : il a
        # declare « NkOpenglDevice copy.hpp » VIVANT en s'appuyant sur le motif
        # `src/NKRHI/Opengl/**` de NKRHI.jenga — qui est un `excludefiles`,
        # c'est-a-dire l'exact contraire d'une inclusion. Les vraies inclusions
        # sont `**.cpp` et `**.h`, et aucune ne prend un `.hpp`.
        # Un outil de preuve qui lit une exclusion comme une inclusion ne prouve
        # rien : il repond a une autre question que celle qu'on lui pose.
        motifs = []
        dans_exclusion = False
        for l in t.splitlines():
            if "excludefiles" in l:
                dans_exclusion = True
            trouves = re.findall(r'"([^"]*)"', l)
            if not dans_exclusion:
                motifs.extend(trouves)
            if dans_exclusion and "]" in l:
                dans_exclusion = False
        jengas[f] = motifs

def jenga_proche(chemin):
    """Le .jenga le plus proche en remontant l'arborescence."""
    d = os.path.dirname(chemin)
    while d:
        cands = [j for j in jengas if os.path.dirname(j) == d]
        if cands:
            return cands
        nd = os.path.dirname(d)
        if nd == d:
            break
        d = nd
    return []

def motif_couvre(motif, rel):
    """Le motif Jenga couvre-t-il ce chemin relatif ? En cas de doute : OUI."""
    if "*" not in motif:
        return motif == rel
    # `**` = n'importe quelle profondeur ; `*` = un segment
    m = motif.replace("**/", "\x00").replace("**", "\x01").replace("*", "\x02")
    m = re.escape(m)
    m = m.replace(re.escape("\x00"), "(?:.*/)?")
    m = m.replace(re.escape("\x01"), ".*")
    m = m.replace(re.escape("\x02"), "[^/]*")
    try:
        return re.fullmatch(m, rel) is not None
    except re.error:
        return True          # motif illisible -> on declare VIVANT

lignes = []
morts = []
for f in copies:
    b = os.path.basename(f)
    raison = None
    # voie 1 : citation textuelle, hors le fichier lui-meme, hors commentaire
    for g in suivis:
        if g == f:
            continue
        if not g.endswith((".h", ".hpp", ".cpp", ".inl", ".c", ".jenga")):
            continue
        try:
            t = io.open(g, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        if b not in t:
            continue
        # une mention dans un COMMENTAIRE n'est pas une compilation
        dur = False
        for l in t.splitlines():
            if b not in l:
                continue
            nu = l.strip()
            if nu.startswith("#") or nu.startswith("//") or nu.startswith("*"):
                continue
            dur = True
            break
        if dur:
            raison = "cite par " + g
            break
    # voie 2 : couvert par un joker du .jenga le plus proche
    if raison is None:
        for j in jenga_proche(f):
            base = os.path.dirname(j)
            rel = os.path.relpath(f, base).replace("\\", "/")
            for motif in jengas[j]:
                if motif_couvre(motif, rel):
                    raison = "joker \"%s\" de %s" % (motif, j)
                    break
            if raison:
                break
    if raison is None:
        morts.append(f)
        lignes.append((f, "MORT", "aucune citation, aucun joker"))
    else:
        lignes.append((f, "vivant", raison))

if morts_seuls:
    for m in morts:
        print(m)
    sys.exit(0)

print("%-62s %-8s %s" % ("fichier", "etat", "preuve"))
print("%-62s %-8s %s" % ("-" * 62, "-" * 8, "-" * 40))
for f, e, r in lignes:
    print("%-62s %-8s %s" % (f, e, r[:70]))
print("")
print("%d fichier(s) examine(s) (motif « copy » + %d declare(s)) — %d PROUVE(S) MORT(S), %d encore compile(s)."
      % (len(copies), len(declarees), len(morts), len(copies) - len(morts)))
print("Prouve mort = aucune citation hors commentaire, ET aucun joker du .jenga le")
print("plus proche ne le couvre. Le doute penche vers « vivant ».")
PYFIN
