# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/genia_texte_3d.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
# VOIE (c) : UNE PHRASE -> UNE IMAGE -> UN MAILLAGE. La porte TEXTE du generateur
# de NK3DModeler (NkGenerateur.h, gabarit `--invite {invite} --out {out}`).
#
#   1. la phrase francaise devient une invite ANGLAISE d'objet isole (modele de
#      langue LOCAL via Ollama ; les encodeurs CLIP de SD ne lisent que l'anglais).
#      Sans service, repli nomme : la phrase telle quelle, et on le DIT ;
#   2. un modele de diffusion LOCAL rend l'image (sd-turbo par defaut, sd15) ;
#   3. genia_triposr.py (la voie image, UNE seule autorite) detoure et reconstruit.
#
# ⚠️ LA MEMOIRE VIDEO EST MESUREE AVANT CHAQUE ETAPE. La carte de 8 Go est
#    disputee par l'entrainement d'Ilyana, qu'on n'arrete JAMAIS. Si la memoire
#    libre ne suffit pas, l'etape passe sur le PROCESSEUR et le journal le dit.
# ⚠️ AUCUN TELECHARGEMENT ICI : les poids sont lus sur le disque
#    (NK_GENIA_POIDS_T2I, defaut C:/Rihen/Modeles/<modele>) ; absents = REFUS.
#
# CONTRAT : code 0 et `--out` ecrit, ou `REFUS : ...` sur stderr.
# -----------------------------------------------------------------------------
import argparse
import json
import os
import subprocess
import sys
import time
import urllib.request

ICI = os.path.dirname(os.path.abspath(__file__))
for _f in (sys.stdout, sys.stderr):
    try:
        _f.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# memoire video necessaire, en Mio, mesuree sur cette machine (voir le rapport)
BESOIN_MIO = {"sd-turbo": 3200, "sd15": 3400}
PAS = {"sd-turbo": 1, "sd15": 25}
GUIDAGE = {"sd-turbo": 0.0, "sd15": 7.5}


def refus(m, code=2):
    sys.stderr.write("REFUS : %s\n" % m)
    sys.exit(code)


def vram_libre_mio():
    try:
        o = subprocess.run(["nvidia-smi", "--query-gpu=memory.free", "--format=csv,noheader,nounits"],
                           capture_output=True, text=True, timeout=20).stdout
        return int(o.strip().splitlines()[0])
    except Exception:
        return -1


def invite_anglaise(phrase):
    """La phrase -> une invite d'objet isole, en anglais. Modele local seulement."""
    url = os.environ.get("NK_IA_URL", "http://127.0.0.1:11434")
    modele = os.environ.get("NK_T2I_TRADUCTEUR", "qwen2.5:7b-instruct")
    q = ("Translate this French request into a short English noun phrase naming ONE object "
         "(max 12 words, no sentence, no quotes). Request: " + phrase)
    corps = json.dumps({"model": modele, "prompt": q, "stream": False, "keep_alive": 0,
                        "options": {"temperature": 0, "num_predict": 40}}).encode("utf-8")
    try:
        with urllib.request.urlopen(urllib.request.Request(url + "/api/generate", data=corps,
                                    headers={"Content-Type": "application/json"}), timeout=120) as r:
            t = (json.loads(r.read().decode("utf-8")).get("response") or "").strip().strip('"').splitlines()[0]
            return t, "traduite par %s" % modele
    except Exception as e:
        return phrase, "NON traduite (service local absent : %s)" % e


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--invite", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--modele", default=os.environ.get("NK_T2I_MODELE", "sd-turbo"), choices=sorted(BESOIN_MIO))
    ap.add_argument("--graine", type=int, default=20260921)
    ap.add_argument("--image-seule", action="store_true", help="s'arreter a l'image (mesure)")
    a = ap.parse_args()
    poids = os.environ.get("NK_GENIA_POIDS_T2I", "C:/Rihen/Modeles/" + a.modele)
    # ⚠️ LES POIDS DE CHAQUE ETAGE, PAS SEULEMENT L'INDEX : un telechargement
    #    interrompu laisse model_index.json present et les poids absents ; le
    #    premier essai du 21/09 est mort en pile d'appels au lieu d'un refus.
    manquants = [c for c in ("unet", "text_encoder", "vae")
                 if not any(f.endswith(".safetensors") and not f.endswith(".incomplete")
                            for f in (os.listdir(os.path.join(poids, c)) if os.path.isdir(os.path.join(poids, c)) else []))]
    if not os.path.isfile(os.path.join(poids, "model_index.json")) or manquants:
        refus("poids %s incomplets dans %s (etages sans poids : %s) ; aucun telechargement implicite"
              % (a.modele, poids, ", ".join(manquants) or "index"))
    t0 = time.time()
    en, comment = invite_anglaise(a.invite)
    # L'habillage vise l'ENTREE de TripoSR : un objet seul, entier, fond uni.
    prompt = ("%s, single object, full view, three-quarter view, centered, isolated on a plain white "
              "background, studio lighting, 3d render" % en)
    print("MESURE t2i : invite '%s' -> '%s' (%s) en %.1f s" % (a.invite, en, comment, time.time() - t0), flush=True)

    import torch
    from diffusers import AutoPipelineForText2Image
    libre = vram_libre_mio()
    device = "cuda" if torch.cuda.is_available() and libre >= BESOIN_MIO[a.modele] else "cpu"
    print("MESURE t2i : vram_libre_mio=%d besoin=%d -> %s%s" % (libre, BESOIN_MIO[a.modele], device,
          "" if device == "cuda" else " (REPLI PROCESSEUR : la carte est occupee, c'est lent)"), flush=True)
    t1 = time.time()
    pipe = AutoPipelineForText2Image.from_pretrained(poids, torch_dtype=torch.float16 if device == "cuda" else torch.float32,
                                                     variant="fp16", safety_checker=None, requires_safety_checker=False)
    pipe = pipe.to(device)
    t2 = time.time()
    g = torch.Generator(device=device).manual_seed(a.graine)
    img = pipe(prompt=prompt, num_inference_steps=PAS[a.modele], guidance_scale=GUIDAGE[a.modele],
               height=512, width=512, generator=g).images[0]
    t3 = time.time()
    pic = torch.cuda.max_memory_allocated() / 2**20 if device == "cuda" else 0
    png = os.path.splitext(a.out)[0] + "_t2i.png"
    img.save(png)
    print("MESURE t2i : modele=%s chargement_s=%.1f generation_s=%.2f pas=%d pic_vram_mio=%.0f image=%s"
          % (a.modele, t2 - t1, t3 - t2, PAS[a.modele], pic, png), flush=True)
    del pipe
    if device == "cuda":
        torch.cuda.empty_cache()
    if a.image_seule:
        return 0
    # L'IMAGE -> LE MAILLAGE : la voie image, dans un PROCESSUS a part (sa memoire
    # video est rendue a la sortie ; c'est aussi la seule autorite du detourage).
    r = subprocess.run([sys.executable, os.path.join(ICI, "genia_triposr.py"), "--image", png, "--out", a.out])
    if r.returncode != 0 or not os.path.isfile(a.out):
        refus("l'image a ete produite (%s) mais la reconstruction a echoue (code %d)" % (png, r.returncode))
    print("MESURE t2i : total_s=%.1f -> %s" % (time.time() - t0, a.out), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
