#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ia_pont.py — le backend MODELE EXTERNE de NkUIDesign, par la prise FICHIER.

=============================================================================
 POURQUOI UN PONT, ET PAS UN BACKEND RESEAU DANS L'APPLICATION
=============================================================================
 PV3DE possede deja des backends Claude et Ollama qui fonctionnent
 (`Applications/PV3DE/src/PV3DE/AI/Backends/`). Ils reposent sur des sockets
 ecrites a la main, et leur propre en-tete dit : « en production : remplacer
 par NKStream::HttpClient quand disponible ». Les recopier dans NkUIDesign en
 ferait une TROISIEME copie, dans une application — exactement ce que la
 directive du depot interdit (cf. l'en-tete de `DesignAI.h` : le client HTTP
 doit monter dans un module partage, c'est au canal).

 Ce pont prend l'autre voie, et elle est deja prouvee par la recette
 (`--recette-ia`, DesignAIRecette.h) : **la prise est le FICHIER.**

     NkUIDesign ecrit nkuidesign_prompt.txt      (bouton « Demander a l'IA »)
     ce script lit le prompt, appelle un modele, ecrit nkuidesign_reponse.txt
     NkUIDesign relit, VERIFIE (structure, registre, rejeu) et pose — ou refuse

 Aucun code reseau dans l'application, aucun ici non plus : on parle aux
 outils en ligne de commande deja installes (`ollama`, `claude`). Le jour ou
 Ilyana est prete, elle se branche sur LA MEME prise : lire un fichier,
 ecrire un fichier. Le pipeline ne change pas d'un octet.

=============================================================================
 USAGE
=============================================================================
   python ia_pont.py                          -> auto : ollama, sinon claude
   python ia_pont.py --backend ollama --modele llama3.2
   python ia_pont.py --backend claude
   python ia_pont.py --prompt P.txt --reponse R.txt

 Deroulement cote application : cliquer « Demander a l'IA » (le prompt est
 ecrit, le verdict dit « collez la reponse »), lancer ce script, recliquer.

 Code de sortie : 0 si une reponse non vide a ete ecrite, 1 sinon.
 ⚠️ Une sortie VIDE du modele n'ecrit RIEN : un fichier de reponse vide
    ressemblerait a une reponse, et l'application dirait « fichier vide »
    la ou la verite est « le modele n'a rien rendu ». Le pont le dit ici.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

MODELE_OLLAMA_DEFAUT = "llama3.2"
DELAI_S = 300


def lire_prompt(chemin: Path) -> str:
    if not chemin.exists():
        print(f"[ia_pont] pas de {chemin} : lancez d'abord « Demander a l'IA » "
              "dans NkUIDesign (c'est lui qui ecrit le prompt).")
        sys.exit(1)
    texte = chemin.read_text(encoding="utf-8", errors="replace")
    if not texte.strip():
        print(f"[ia_pont] {chemin} est vide : rien a demander.")
        sys.exit(1)
    return texte


def appeler(cmd, prompt: str) -> str:
    """Lance la commande, prompt sur l'entree standard, rend la sortie brute."""
    try:
        res = subprocess.run(cmd, input=prompt, capture_output=True,
                             text=True, encoding="utf-8", timeout=DELAI_S)
    except subprocess.TimeoutExpired:
        print(f"[ia_pont] {cmd[0]} n'a pas repondu en {DELAI_S} s.")
        sys.exit(1)
    if res.returncode != 0:
        err = (res.stderr or "").strip()
        print(f"[ia_pont] {cmd[0]} a echoue (code {res.returncode})"
              + (f" : {err[:400]}" if err else ""))
        sys.exit(1)
    return res.stdout or ""


def main() -> int:
    ap = argparse.ArgumentParser(description="Pont fichier -> modele externe pour NkUIDesign.")
    ap.add_argument("--backend", choices=["auto", "ollama", "claude"], default="auto")
    ap.add_argument("--modele", default=None,
                    help="nom du modele (ollama seulement ; defaut : %s)" % MODELE_OLLAMA_DEFAUT)
    ap.add_argument("--prompt", default="nkuidesign_prompt.txt")
    ap.add_argument("--reponse", default="nkuidesign_reponse.txt")
    args = ap.parse_args()

    backend = args.backend
    if backend == "auto":
        if shutil.which("ollama"):
            backend = "ollama"
        elif shutil.which("claude"):
            backend = "claude"
        else:
            print("[ia_pont] ni `ollama` ni `claude` sur le PATH. Installez l'un "
                  "des deux, ou collez la reponse a la main dans le fichier "
                  f"{args.reponse} — la prise fichier marche aussi sans modele.")
            return 1

    prompt = lire_prompt(Path(args.prompt))

    if backend == "ollama":
        if not shutil.which("ollama"):
            print("[ia_pont] `ollama` introuvable sur le PATH.")
            return 1
        cmd = ["ollama", "run", args.modele or MODELE_OLLAMA_DEFAUT]
    else:
        if not shutil.which("claude"):
            print("[ia_pont] `claude` introuvable sur le PATH.")
            return 1
        # `claude -p` : mode impression, le prompt arrive par l'entree standard.
        cmd = ["claude", "-p"]

    print(f"[ia_pont] backend {backend}, prompt de {len(prompt)} caracteres...")
    sortie = appeler(cmd, prompt)

    if not sortie.strip():
        print("[ia_pont] le modele n'a RIEN rendu — aucun fichier ecrit "
              "(un fichier vide ferait accuser le fichier au lieu du modele).")
        return 1
    if "nkuidoc" not in sortie:
        # On ecrit QUAND MEME : c'est l'application qui juge (elle refusera
        # avec « pas de ligne nkuidoc »), et la reponse fautive reste lisible
        # pour comprendre le refus. Le pont previent, il ne censure pas.
        print("[ia_pont] attention : pas de ligne `nkuidoc` dans la sortie — "
              "l'application refusera probablement, la reponse est gardee telle "
              "quelle pour lecture.")

    Path(args.reponse).write_text(sortie, encoding="utf-8")
    print(f"[ia_pont] reponse de {len(sortie)} caracteres ecrite dans {args.reponse}. "
          "Recliquez « Demander a l'IA » dans NkUIDesign : c'est elle qui verifie et pose.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
