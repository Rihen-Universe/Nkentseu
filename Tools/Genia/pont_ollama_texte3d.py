# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# PONT OLLAMA -> NKTexte3D : le modele PILOTE l'outillage, il ne le remplace pas.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# CE QUE CE PONT EST, EN UNE PHRASE. Une demande libre en francais entre ; un
# modele local (Ollama) la traduit en une phrase du VOCABULAIRE QUE NOTRE OUTIL
# CONNAIT ; `NKTexte3D` execute. Le modele ne produit aucune geometrie : il
# choisit des formes dans une liste fermee. C'est tout l'interet -- ce qu'il
# ecrit est LISIBLE, REFUSABLE et CORRIGEABLE A LA MAIN.
#
# POURQUOI CE CHEMIN ET PAS UN MODELE GENERATIF TEXTE -> 3D. Le cap de Rodolf
# (17/09) : « le modele est un reglage, l'outillage est le produit ». Un modele
# genератif rend un maillage qu'on ne peut ni discuter ni corriger a moitie ;
# ici, l'etape intermediaire est une PHRASE, donc un document. Et aucun octet
# n'est telecharge : les trois modeles sont deja sur la machine.
#
# ⚠️ LES TROIS TAUX RESTENT SEPARES, ET C'EST LA SEULE FACON DE SAVOIR QUI A
# ECHOUE. Un taux unique confond « reconnait tout sans rien executer » et
# « execute tout sans rien reconnaitre » :
#   T1  le modele rend-il quelque chose (reponse non vide, dans le temps) ;
#   T2  est-ce traduit en une phrase que NOTRE outil reconnait (aucun mot
#       inconnu) ;
#   T3  l'outil produit-il un maillage CONFORME (ferme, manifold, chi attendu) ;
#   T4  l'objet RESSEMBLE-T-IL A LA DEMANDE (le compte de formes est-il celui que
#       le jeu d'epreuve annoncait AVANT la course).
#
# ⚠️ T4 A ETE AJOUTE APRES LA PREMIERE COURSE, ET VOICI POURQUOI -- c'est le
# constat le plus utile de ce lot. Les trois premiers taux etaient VERTS sur
# « un bonhomme de neige », pour lequel le modele avait ecrit « un tore sur un
# petit cylindre ». Le maillage est ferme, manifold, chi = 2, editable : T3 ne
# pouvait QUE dire oui. Il mesure la CONFORMITE DU MAILLAGE, pas la FIDELITE A
# LA DEMANDE -- une reponse juste a la question d'a cote.
#   > L'attendu (« 3 formes empilees ») etait DEJA ECRIT dans
#     `jeu_epreuve_texte3d.txt`, avant la course. C'est l'instrument qui ne le
#     lisait pas. Le jeu d'epreuve n'a PAS ete retouche.
#
# ⚠️ CE QUE CE PONT NE PROUVE PAS, ET IL FAUT LE LIRE AVANT LES CHIFFRES :
# T2 et T3 mesurent NOTRE outillage. T1 mesure le modele. Un T2 bas peut venir
# d'un modele faible OU d'une grammaire trop pauvre pour la demande -- et sur ce
# jeu d'epreuve, plusieurs demandes sont HORS PORTEE PAR CONSTRUCTION (elles
# sont marquees ainsi dans `jeu_epreuve_texte3d.txt`, ecrit AVANT la course).
# Un taux global serait donc un chiffre qui ment ; on imprime le detail.
#
# USAGE :
#   python pont_ollama_texte3d.py --demande "une lampe de bureau" [--modele M]
#   python pont_ollama_texte3d.py --jeu jeu_epreuve_texte3d.txt --sorties DOSSIER
#
# codes : 0 = la chaine a produit un maillage · 1 = echec mesure · 2 = refus nomme
# -----------------------------------------------------------------------------
import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.request

# ── LA SORTIE EST FORCEE EN UTF-8, ET CE N'EST PAS DU CONFORT ───────────────
# Mesure : rediriger ce script dans un fichier le faisait PLANTER sur le premier
# caractere accentue (cp1252 par defaut sur cette machine), APRES avoir imprime
# les quatre taux et AVANT le verdict. Le code de sortie valait alors 1 -- un
# ROUGE parfaitement credible, qui ne venait pas du produit mais de l'encodage
# de ma propre trace. Un instrument qui rend 1 pour une raison qui n'est pas la
# sienne est pire qu'un instrument faux : il accuse.
for _flux in (sys.stdout, sys.stderr):
    try:
        _flux.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

OLLAMA = os.environ.get("OLLAMA_HOST", "http://127.0.0.1:11434")

# ── LE VOCABULAIRE, RECOPIE DE `NKTexte3D/src/main.cpp` (FormeDuMot, VOCABULAIRE)
# ⚠️ C'EST UNE DETTE, ET ELLE EST NOMMEE : deux autorites sur la meme question.
# Si un mot est ajoute la-bas et pas ici, le pont refusera une phrase que l'outil
# accepte -- un faux refus, donc un chiffre faux, et personne ne le verra.
#   > Se rouvre le jour ou NKTexte3D saura imprimer son vocabulaire
#     (`--vocabulaire`), ce qui rendra cette liste lisible au lieu de recopiee.
FORMES = {
    "sphere": "sphere", "spheres": "sphere", "boule": "sphere", "boules": "sphere",
    "cube": "cube", "cubes": "cube", "boite": "cube", "boites": "cube", "caisse": "cube",
    "cylindre": "cylindre", "cylindres": "cylindre", "tube": "cylindre", "tubes": "cylindre",
    "cone": "cone", "cones": "cone",
    "tore": "tore", "tores": "tore", "anneau": "tore", "anneaux": "tore", "donut": "tore",
    "capsule": "capsule", "capsules": "capsule", "gelule": "capsule",
}
LIAISONS = {"et", "sur", "dessus", "audessus", "perce", "percee", "troue", "trouee", "trou", "trous"}
OUTILS = {"le", "la", "les", "de", "du", "des", "d", "l", "au", "avec", "a",
          "un", "une", "1", "deux", "2", "trois", "3", "quatre", "4", "cinq", "5", "six", "6",
          "grand", "grande", "petit", "petite"}

INVITE = """Tu traduis une demande d'objet 3D en UNE SEULE phrase francaise, en n'utilisant QUE les mots autorises ci-dessous. Tu ne dois RIEN ecrire d'autre : pas d'explication, pas de ponctuation, pas de guillemets, juste la phrase.

MOTS AUTORISES, ET AUCUN AUTRE :
- formes : sphere, cube, cylindre, cone, tore, capsule
- nombres : un, une, deux, trois, quatre, cinq, six
- tailles : grand, petit
- liaisons : et, sur, au-dessus de, perce

REGLES :
- "sur" veut dire que les formes se TOUCHENT et sont empilees.
- "perce" ajoute un trou traversant a la forme qui precede.
- Si la demande ne peut pas se decrire avec ces mots, reponds exactement : IMPOSSIBLE
- Si la demande est vide ou n'est pas un objet, reponds exactement : IMPOSSIBLE

EXEMPLES :
demande : une balle -> une sphere
demande : un tabouret -> un cylindre sur un grand cylindre
demande : de la musique -> IMPOSSIBLE

demande : %s ->"""


def _refus(msg, code=2):
    sys.stderr.write("REFUS : %s\n" % msg)
    sys.stderr.flush()
    sys.exit(code)


def normaliser(t):
    """La MEME normalisation que NKTexte3D : minuscules, accents rabattus,
    ponctuation en espaces. Si elle differe, le pont et l'outil ne parlent pas
    de la meme chaine et le taux T2 devient une opinion."""
    t = t.lower()
    for a, b in (("àâä", "a"), ("éèêë", "e"), ("îï", "i"), ("ôö", "o"), ("ùûü", "u"), ("ç", "c")):
        for c in a:
            t = t.replace(c, b)
    return re.sub(r"[^a-z0-9]+", " ", t).strip()


def interroger(modele, demande, timeout):
    """T1. Rend (texte, secondes) ou leve. On mesure le temps SEPAREMENT du
    reste : c'est la grandeur qui doit differer d'une course a l'autre."""
    corps = json.dumps({
        "model": modele,
        "prompt": INVITE % demande,
        "stream": False,
        "options": {"temperature": 0.0, "num_predict": 60},
    }).encode("utf-8")
    req = urllib.request.Request(OLLAMA + "/api/generate", data=corps,
                                 headers={"Content-Type": "application/json"})
    t0 = time.time()
    with urllib.request.urlopen(req, timeout=timeout) as r:
        rep = json.loads(r.read().decode("utf-8"))
    return (rep.get("response", "") or "").strip(), time.time() - t0


def valider(reponse):
    """T2. Rend (phrase_valide, motif_de_refus). Le validateur est STRICT et il
    NOMME ce qu'il refuse : un modele qui invente un mot est le cas normal, pas
    un incident. On ne repare jamais silencieusement sa sortie."""
    # Le modele bavarde parfois : on ne garde que la premiere ligne non vide.
    lignes = [l.strip() for l in reponse.splitlines() if l.strip()]
    if not lignes:
        return None, "le modele n'a rien repondu"
    phrase = lignes[0].strip(' "\'.')
    if "IMPOSSIBLE" in phrase.upper():
        return None, "le modele declare la demande hors de portee (IMPOSSIBLE)"
    norm = normaliser(phrase)
    if not norm:
        return None, "reponse vide apres normalisation"
    mots = norm.split()
    formes = [m for m in mots if m in FORMES]
    if not formes:
        return None, "aucune forme reconnue dans la reponse du modele : « %s »" % phrase
    for m in mots:
        if m not in FORMES and m not in LIAISONS and m not in OUTILS:
            return None, "mot inconnu de notre outil : « %s » (dans « %s »)" % (m, phrase)
    return phrase, None


def lire_attendu(attendu):
    """Rend (mini, maxi, classe) ou classe vaut 'nombre', 'horsportee', 'refus'.
    L'attendu vient du jeu d'epreuve, ECRIT AVANT la course et non retouche."""
    a = attendu.upper()
    if "HORS PORTEE" in a:
        return (0, 0, "horsportee")
    if "REFUS" in a:
        return (0, 0, "refus")
    m = re.search(r"(\d+)\s*(forme|tore|cube|sphere|cylindre|cone|capsule)", attendu, re.I)
    if not m:
        return (0, 0, "horsportee")
    n = int(m.group(1))
    if "ou plus" in attendu.lower():
        return (n, 99, "nombre")
    return (n, n, "nombre")


def executer(exe, phrase, sortie, res):
    """T3. NKTexte3D ecrit le maillage, ou refuse. On lit son CODE, jamais un
    tube -- `| tail` rend le code de tail, et ce chantier l'a paye trois fois."""
    p = subprocess.run([exe, "--texte", phrase, "--out", sortie, "--res", str(res)],
                       capture_output=True, text=True, errors="replace")
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def une_demande(exe, modele, demande, sortie, res, timeout):
    r = {"demande": demande, "t1": False, "t2": False, "t3": False, "t4": False,
         "reponse": "", "phrase": "", "motif": "", "secondes": 0.0, "sommets": 0,
         "formes": 0, "trous": 0}
    try:
        rep, dt = interroger(modele, demande, timeout)
        r["secondes"] = dt
        r["reponse"] = rep.replace("\n", " ")[:160]
        r["t1"] = bool(rep)
    except Exception as e:
        r["motif"] = "le modele n'a pas repondu : %s" % e
        return r
    if not r["t1"]:
        r["motif"] = "reponse vide"
        return r
    phrase, motif = valider(rep)
    if phrase is None:
        r["motif"] = motif
        return r
    r["t2"] = True
    r["phrase"] = phrase
    code, journal = executer(exe, phrase, sortie, res)
    if code != 0:
        r["motif"] = "NKTexte3D rend %d : %s" % (code, journal.strip().splitlines()[0] if journal.strip() else "")
        return r
    m = re.search(r"sommets=(\d+)", journal)
    if m:
        r["sommets"] = int(m.group(1))
    # Le compte de formes vient de la ligne ANALYSE de NKTexte3D -- c'est L'OUTIL
    # qui dit ce qu'il a compris, pas moi qui le deduis de la phrase. Une trace de
    # commande imprime la valeur RELUE, jamais l'argument passe.
    m = re.search(r"ANALYSE\s*:\s*(\d+)\s*forme", journal)
    if m:
        r["formes"] = int(m.group(1))
    m = re.search(r"(\d+)\s*trou", journal)
    if m:
        r["trous"] = int(m.group(1))
    r["t3"] = True
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--demande", default="")
    ap.add_argument("--jeu", default="")
    ap.add_argument("--modele", default="qwen2.5:7b-instruct")
    ap.add_argument("--exe", default="")
    ap.add_argument("--sorties", default="")
    ap.add_argument("--res", type=int, default=64)
    ap.add_argument("--timeout", type=float, default=180.0)
    a = ap.parse_args()

    exe = a.exe or os.path.join("Build", "Bin", "Release-Windows", "NKTexte3D", "NKTexte3D.exe")
    if not os.path.isfile(exe):
        _refus("NKTexte3D.exe est introuvable : %s (construis-le, et lis la ligne Status:)" % exe)
    sorties = a.sorties or os.path.join("Build", "genia3d_ollama")
    os.makedirs(sorties, exist_ok=True)

    # ── LE ZERO D'ABORD : le service repond-il, et sur QUEL modele ? ─────────
    # Un pont qui mesure « 0 sur 18 » parce que le service est eteint rend
    # exactement le meme chiffre qu'un pont qui mesure un modele incapable.
    try:
        with urllib.request.urlopen(OLLAMA + "/api/tags", timeout=10) as r:
            noms = [m["name"] for m in json.loads(r.read().decode("utf-8")).get("models", [])]
    except Exception as e:
        _refus("Ollama ne repond pas sur %s (%s). Rien n'est mesurable tant que le "
               "service est muet -- et un 0/18 dans ce cas mesurerait le service, pas le modele."
               % (OLLAMA, e))
    if a.modele not in noms:
        _refus("le modele « %s » n'est pas installe. Presents : %s" % (a.modele, ", ".join(noms)))
    print("== PONT OLLAMA -> NKTexte3D ==")
    print("  service : %s  |  modele : %s  |  outil : %s" % (OLLAMA, a.modele, exe))
    print("  modeles presents : %s" % ", ".join(noms))

    if a.demande:
        r = une_demande(exe, a.modele, a.demande, os.path.join(sorties, "demande.obj"), a.res, a.timeout)
        print("  demande  : %s" % r["demande"])
        print("  modele   : %s   (%.1f s)" % (r["reponse"], r["secondes"]))
        print("  phrase   : %s" % (r["phrase"] or "(refusee)"))
        if r["motif"]:
            print("  motif    : %s" % r["motif"])
        print("  T1=%s T2=%s T3=%s  sommets=%d" % (r["t1"], r["t2"], r["t3"], r["sommets"]))
        return 0 if r["t3"] else 1

    if not a.jeu:
        _refus("donne --demande ou --jeu")
    if not os.path.isfile(a.jeu):
        _refus("jeu d'epreuve introuvable : %s" % a.jeu)

    cas = []
    for ligne in open(a.jeu, encoding="utf-8"):
        ligne = ligne.strip()
        if not ligne or ligne.startswith("#"):
            continue
        parts = [x.strip() for x in ligne.split("|")]
        if len(parts) >= 3:
            cas.append((parts[0], parts[1], parts[2]))

    print("  jeu d'epreuve : %s (%d cas, ECRIT AVANT LA COURSE)" % (a.jeu, len(cas)))
    print()
    n1 = n2 = n3 = n4 = n4d = 0
    par_classe = {}
    for i, (classe, demande, attendu) in enumerate(cas):
        nom = re.sub(r"[^a-z0-9]+", "_", normaliser(demande))[:40] or "vide"
        r = une_demande(exe, a.modele, demande, os.path.join(sorties, "%02d_%s.obj" % (i, nom)), a.res, a.timeout)
        mini, maxi, genre = lire_attendu(attendu)
        # T4 ne se prononce QUE la ou un compte etait annonce d'avance. Sur un cas
        # marque HORS PORTEE ou REFUS, il se TAIT -- un critere qui se prononce
        # hors de sa condition fabrique du bruit qu'on apprend a ignorer.
        if genre == "nombre" and r["t3"]:
            r["t4"] = (mini <= r["formes"] <= maxi)
            if not r["t4"]:
                r["motif"] = "FIDELITE : %d forme(s) produite(s), %s attendue(s)" % (
                    r["formes"], ("%d" % mini) if mini == maxi else ("%d ou plus" % mini))
        n1 += r["t1"]
        n2 += r["t2"]
        n3 += r["t3"]
        if genre == "nombre":
            n4d += 1
            n4 += r["t4"]
        d = par_classe.setdefault(classe, [0, 0, 0, 0])
        d[0] += 1
        d[1] += r["t1"]
        d[2] += r["t2"]
        d[3] += r["t3"]
        # Un cas [C] doit ECHOUER : son succes serait le defaut.
        marque = "  "
        if classe == "[C]":
            marque = "OK" if not r["t3"] else "!!"
        t4aff = ("%d" % r["t4"]) if genre == "nombre" and r["t3"] else "-"
        print("%s %s %-30s | T1=%d T2=%d T3=%d T4=%s | %5.1f s | %s"
              % (marque, classe, demande[:30], r["t1"], r["t2"], r["t3"], t4aff, r["secondes"],
                 (r["motif"] or r["phrase"])[:66]))

    n = len(cas)
    print()
    print("  T1 le modele repond            : %d / %d" % (n1, n))
    print("  T2 traduit dans notre langage  : %d / %d" % (n2, n))
    print("  T3 maillage produit            : %d / %d" % (n3, n))
    print("  T4 FIDELE a la demande         : %d / %d  (sur les cas dont l'attendu etait chiffre)" % (n4, n4d))
    print("     ⚠ T3 vert et T4 rouge = un maillage impeccable qui ne montre PAS ce qui etait demande.")
    print()
    print("  PAR CLASSE (un taux global melangerait ce qui doit reussir et ce qui doit ECHOUER) :")
    for c in sorted(par_classe):
        d = par_classe[c]
        print("    %s  %d cas : T1=%d T2=%d T3=%d" % (c, d[0], d[1], d[2], d[3]))
    print("    [A] demandes d'utilisateur, plusieurs HORS PORTEE par construction")
    print("    [B] demandes que la grammaire couvre -- un echec ici est un VRAI defaut")
    print("    [C] demandes absurdes : T3 doit valoir 0. Un T3 > 0 en [C] est un defaut GRAVE")
    cb = par_classe.get("[B]", [0, 0, 0, 0])
    cc = par_classe.get("[C]", [0, 0, 0, 0])
    ok = (cb[3] == cb[0]) and (cc[3] == 0)
    print()
    print("VERDICT : %s  ([B] %d/%d doivent passer, [C] %d/%d doivent echouer)"
          % ("VERT" if ok else "ROUGE", cb[3], cb[0], cc[0] - cc[3], cc[0]))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
