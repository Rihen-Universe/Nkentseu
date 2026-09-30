#!/usr/bin/env python3
#
# rapport-plantage.py — LE RAPPORT DE PLANTAGE macOS, EN CLAIR (2026-09-30)
# =============================================================================
# Usage : rapport-plantage.py <nom-du-binaire>
#
# macOS ecrit un rapport .ips par plantage, dans DiagnosticReports. C'est du
# JSON (une ligne d'en-tete, puis le corps) que personne ne lit dans un journal
# de CI. On en tire ce qui suffit a trouver la ligne fautive : l'exception, le
# message applicatif (NSException non rattrapee, abort, assertion) et la pile
# du fil qui a plante, symbolisee par macOS lui-meme.
#
# Utilise par lancer-fenetre.sh et lancer-banc.sh. Ecrit sur la sortie
# standard ; sortie 0 meme sans rapport (c'est un rapporteur).
# =============================================================================

import glob
import json
import os
import sys


def Principal(nom):
    motifs = [os.path.expanduser(f"~/Library/Logs/DiagnosticReports/{nom}*.ips"),
              f"/Library/Logs/DiagnosticReports/{nom}*.ips"]
    fichiers = sorted([f for m in motifs for f in glob.glob(m)], key=os.path.getmtime)
    if not fichiers:
        print("(aucun rapport de plantage macOS trouve)")
        return 0
    texte = open(fichiers[-1], encoding="utf-8", errors="replace").read()
    _, _, corps = texte.partition("\n")
    try:
        d = json.loads(corps)
    except Exception as e:
        print(f"rapport illisible ({e}) : {fichiers[-1]}")
        print(texte[:3000])
        return 0
    print(f"rapport : {os.path.basename(fichiers[-1])}")
    print("exception :", json.dumps(d.get("exception", {}), ensure_ascii=False))
    if d.get("termination"):
        print("terminaison :", json.dumps(d.get("termination"), ensure_ascii=False)[:600])
    asi = d.get("asi")
    if asi:
        print("message applicatif :", json.dumps(asi, ensure_ascii=False)[:1500])
    images = d.get("usedImages", [])
    fil = d.get("faultingThread", 0)
    fils = d.get("threads", [])
    if fil < len(fils):
        for i, f in enumerate(fils[fil].get("frames", [])[:30]):
            img = "?"
            idx = f.get("imageIndex")
            if idx is not None and idx < len(images):
                img = images[idx].get("name", "?")
            sym = f.get("symbol", "?")
            pos = f.get("symbolLocation", f.get("imageOffset", 0))
            src = ""
            if f.get("sourceFile"):
                src = f"  ({f.get('sourceFile')}:{f.get('sourceLine', '?')})"
            print(f"#{i:02d} {img:<24} {sym} + {pos}{src}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage : rapport-plantage.py <nom-du-binaire>")
        sys.exit(0)
    sys.exit(Principal(sys.argv[1]))
