# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# PONT OLLAMA -> DOCUMENT DE SCENE -> GEOMETRIE.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# CE QUI CHANGE PAR RAPPORT A `pont_ollama_texte3d.py`, ET C'EST TOUT LE LOT A.
# L'ancien pont allait de la phrase a la geometrie SANS RIEN AU MILIEU. D'ou le
# resultat que Rodolf a refuse : « un bonhomme de neige » rendait « un tore sur
# un petit cylindre » avec tous les compteurs verts, parce qu'aucun critere ne
# pouvait comparer l'objet a quoi que ce soit.
#
# Ici, le modele ecrit un DOCUMENT (`.nkscene`), et le document est la reference.
# Trois consequences, et la troisieme est celle qui compte :
#   1. Rodolf peut LIRE ce que le modele a compris, avant de voir l'objet ;
#   2. il peut le CORRIGER a la main, ligne par ligne, et regenerer ;
#   3. la geometrie se VERIFIE CONTRE LUI -- c'est le `--verifier` de NKTexte3D,
#      et c'est ce que l'ancien pont ne pouvait pas avoir.
#
# ⚠️ DEUX FIDELITES, ET ON NE LES CONFOND JAMAIS :
#   - document -> geometrie : MECANIQUE. Mesuree ici, par T4.
#   - phrase   -> document  : HUMAINE. Elle n'est PAS mesuree, et elle ne le sera
#     pas. Que « trois spheres empilees » soit une bonne description d'un
#     bonhomme de neige, c'est Rodolf qui le dit en lisant le document. Un
#     critere qui pretendrait le mesurer serait un vert de plus et rien de vrai.
#
# ⚠️ ET LA RESERVE TIENT : l'invite ci-dessous est un REGLAGE que j'ai ecrit.
# Les taux mesurent le modele AVEC CE REGLAGE, pas le modele en general.
#
# USAGE :
#   python pont_ollama_scene.py --demande "un bonhomme de neige" [--modele M]
#   python pont_ollama_scene.py --jeu jeu_epreuve_texte3d.txt --sorties DOSSIER
# -----------------------------------------------------------------------------
import argparse
import json
import os
import re
import subprocess
import sys
import time
import urllib.request

for _flux in (sys.stdout, sys.stderr):
    try:
        _flux.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

OLLAMA = os.environ.get("OLLAMA_HOST", "http://127.0.0.1:11434")

FORMES = ("sphere", "cube", "cylindre", "cone", "tore", "capsule")

INVITE = """Tu decris un objet 3D sous forme d'un DOCUMENT DE SCENE. Tu n'ecris QUE des lignes de ce document, rien d'autre : pas d'explication, pas de texte avant ou apres.

Une ligne par partie de l'objet :
partie <nom> forme <forme> taille <sx> <sy> <sz> [relation] [decale <dx> <dy> <dz>] [op difference]

LES FORMES, ET IL N'Y EN A PAS D'AUTRES :
sphere, cube, cylindre, cone, tore, capsule

LA TAILLE est donnee PAR AXE, ce qui permet d'aplatir ou d'etirer :
- un cylindre taille 1 0.05 1 est un DISQUE plat
- un cube taille 2 0.1 0.6 est une PLANCHE
- un cylindre taille 0.07 0.5 0.07 est un BARREAU fin

LES RELATIONS placent les parties les unes par rapport aux autres :
- pose_sur <autre>   : cette partie est POSEE SUR l'autre, elles se touchent
- pose_sous <autre>  : cette partie est SOUS l'autre, elles se touchent
- decale <dx> <dy> <dz> : deplace la partie apres la relation (pour des pieds par exemple)

op difference RETIRE cette partie de ce qui precede (pour creuser un verre, un bol).

REGLES :
- La premiere partie n'a pas de relation : c'est la base de l'objet.
- Chaque autre partie DOIT avoir pose_sur ou pose_sous, sinon elle flotte.
- Toutes les parties doivent former UN SEUL objet d'un seul tenant.
- Si l'objet ne peut pas se decrire avec ces six formes, ecris exactement : IMPOSSIBLE

EXEMPLE, pour une table :
partie plateau forme cube taille 1.0 0.08 0.7
partie pied_ag forme cylindre taille 0.06 0.5 0.06 pose_sous plateau decale -0.4 0 -0.28
partie pied_ad forme cylindre taille 0.06 0.5 0.06 pose_sous plateau decale 0.4 0 -0.28
partie pied_rg forme cylindre taille 0.06 0.5 0.06 pose_sous plateau decale -0.4 0 0.28
partie pied_rd forme cylindre taille 0.06 0.5 0.06 pose_sous plateau decale 0.4 0 0.28

Maintenant, decris : %s
"""


def _refus(msg, code=2):
    sys.stderr.write("REFUS : %s\n" % msg)
    sys.stderr.flush()
    sys.exit(code)


def interroger(modele, demande, timeout):
    corps = json.dumps({
        "model": modele,
        "prompt": INVITE % demande,
        "stream": False,
        "options": {"temperature": 0.0, "num_predict": 400},
    }).encode("utf-8")
    req = urllib.request.Request(OLLAMA + "/api/generate", data=corps,
                                 headers={"Content-Type": "application/json"})
    t0 = time.time()
    with urllib.request.urlopen(req, timeout=timeout) as r:
        rep = json.loads(r.read().decode("utf-8"))
    return (rep.get("response", "") or "").strip(), time.time() - t0


def extraire_document(reponse, demande):
    """T2 cote pont : ne garder que les lignes « partie ... ». On NE REPARE
    RIEN -- si le modele ecrit une ligne mal formee, c'est le LECTEUR de
    NKTexte3D qui la refusera en nommant sa cause. Un pont qui repare
    silencieusement la sortie du modele mesure son propre travail de reparation,
    pas le modele."""
    if "IMPOSSIBLE" in reponse.upper():
        return None, "le modele declare la demande hors de portee (IMPOSSIBLE)"
    lignes = []
    for l in reponse.splitlines():
        l = l.strip().strip("`")
        if l.lower().startswith("partie "):
            lignes.append(l)
    if not lignes:
        return None, "aucune ligne « partie » dans la reponse : « %s »" % reponse.replace("\n", " ")[:90]
    entete = ["# Document PRODUIT PAR LE MODELE, non retouche.",
              "scene generee",
              "demande " + demande, ""]
    return "\n".join(entete + lignes) + "\n", None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--demande", default="")
    ap.add_argument("--jeu", default="")
    ap.add_argument("--modele", default="qwen2.5:7b-instruct")
    ap.add_argument("--exe", default="")
    ap.add_argument("--sorties", default="")
    ap.add_argument("--res", type=int, default=72)
    ap.add_argument("--timeout", type=float, default=300.0)
    a = ap.parse_args()

    exe = a.exe or os.path.join("Build", "Bin", "Release-Windows", "NKTexte3D", "NKTexte3D.exe")
    if not os.path.isfile(exe):
        _refus("NKTexte3D.exe est introuvable : %s" % exe)
    sorties = a.sorties or os.path.join("Build", "genia3d_scenes")
    os.makedirs(sorties, exist_ok=True)

    # LE ZERO : le service repond-il ? Un « 0 sur N » avec un service eteint rend
    # exactement le meme chiffre qu'un modele incapable.
    try:
        with urllib.request.urlopen(OLLAMA + "/api/tags", timeout=10) as r:
            noms = [m["name"] for m in json.loads(r.read().decode("utf-8")).get("models", [])]
    except Exception as e:
        _refus("Ollama ne repond pas sur %s (%s) : rien n'est mesurable" % (OLLAMA, e))
    if a.modele not in noms:
        _refus("modele « %s » absent. Presents : %s" % (a.modele, ", ".join(noms)))

    print("== PONT OLLAMA -> DOCUMENT DE SCENE -> GEOMETRIE ==")
    print("  service : %s | modele : %s" % (OLLAMA, a.modele))
    print("  ⚠ fidelite document -> geometrie : MESUREE (T4)")
    print("  ⚠ fidelite phrase   -> document  : HUMAINE, jamais mesuree ici")
    print()

    demandes = []
    if a.demande:
        demandes = [("[.]", a.demande, "")]
    elif a.jeu:
        for ligne in open(a.jeu, encoding="utf-8"):
            ligne = ligne.strip()
            if not ligne or ligne.startswith("#"):
                continue
            parts = [x.strip() for x in ligne.split("|")]
            if len(parts) >= 3:
                demandes.append((parts[0], parts[1], parts[2]))
    else:
        _refus("donne --demande ou --jeu")

    n1 = n2 = n3 = n4 = 0
    for i, (classe, demande, attendu) in enumerate(demandes):
        nom = re.sub(r"[^a-z0-9]+", "_", demande.lower())[:36] or "vide"
        doc = os.path.join(sorties, "%02d_%s.nkscene" % (i, nom))
        obj = os.path.join(sorties, "%02d_%s.obj" % (i, nom))
        t1 = t2 = t3 = t4 = 0
        motif = ""
        try:
            rep, dt = interroger(a.modele, demande, a.timeout)
            t1 = 1 if rep else 0
        except Exception as e:
            print("   %s %-30s | T1=0 | le modele n'a pas repondu : %s" % (classe, demande[:30], e))
            continue
        texte, motif = extraire_document(rep, demande)
        if texte is None:
            print("   %s %-30s | T1=%d T2=0 T3=0 T4=0 | %5.1f s | %s" % (classe, demande[:30], t1, dt, motif[:64]))
            n1 += t1
            continue
        with open(doc, "w", encoding="utf-8", newline="\n") as f:
            f.write(texte)
        nParties = len([l for l in texte.splitlines() if l.startswith("partie ")])
        # T2 : NOTRE lecteur accepte-t-il ce document ? C'est lui l'autorite,
        # pas moi -- et il refuse en nommant sa cause.
        p = subprocess.run([exe, "--scene", doc, "--out", obj, "--verifier", "--res", str(a.res)],
                           capture_output=True, text=True, errors="replace")
        journal = (p.stdout or "") + (p.stderr or "")
        refus = re.search(r"REFUS : (.+)", journal)
        if refus and "ANALYSE" not in journal:
            motif = "document REFUSE : " + refus.group(1).strip()
        else:
            t2 = 1
            if os.path.isfile(obj) and "MESURE ecriture" in journal:
                t3 = 1
                if "VERDICT DOCUMENT : VERT" in journal:
                    t4 = 1
                else:
                    m = re.search(r"\[ROUGE\] (.+)", journal)
                    motif = "geometrie INFIDELE au document : " + (m.group(1).strip() if m else "")
            else:
                motif = (refus.group(1).strip() if refus else "aucun maillage ecrit")
        n1 += t1
        n2 += t2
        n3 += t3
        n4 += t4
        print("   %s %-30s | T1=%d T2=%d T3=%d T4=%d | %5.1f s | %d partie(s) | %s"
              % (classe, demande[:30], t1, t2, t3, t4, dt, nParties, motif[:58]))

    n = len(demandes)
    print()
    print("  T1 le modele repond                   : %d / %d" % (n1, n))
    print("  T2 le document est ACCEPTE par l'outil: %d / %d" % (n2, n))
    print("  T3 un maillage est produit            : %d / %d" % (n3, n))
    print("  T4 la geometrie est FIDELE au document: %d / %d" % (n4, n))
    print()
    print("  Les documents produits sont dans %s -- LIS-LES : c'est la seule facon" % sorties)
    print("  de juger si le modele a compris la demande. Aucun chiffre ne le dira.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
