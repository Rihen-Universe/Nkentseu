# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# GENIA -- generateur EXTERNE « une image -> un glTF » par TripoSR.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# Ce script est le PROCESSUS EXTERNE que NK3DModeler lance derriere l'interface
# NkIGenerateur (Applications/NK3DModeler/src/NK3DModeler/Genia/NkGenerateur.h).
# Python n'est PAS une dependance du build C++ : le modeleur lance ce script,
# attend, et lit le fichier au chemin `--out`. Rien d'autre ne traverse.
#
# CONTRAT (le seul que le modeleur connaisse) :
#   python genia_triposr.py --image <png|jpg> --out <fichier.glb>
#   -> code 0 et le fichier `--out` existe, non vide ;
#   -> code != 0 et une ligne `REFUS : ...` sur stderr sinon.
#
# CE QUI EST EMPRUNTE (la generation) et sa licence, lue le 2026-09-12 :
#   code   : https://github.com/VAST-AI-Research/TripoSR  -- LICENSE = MIT
#            (« Copyright (c) 2024 Tripo AI & Stability AI »)
#   poids  : https://huggingface.co/stabilityai/TripoSR   -- carte : license: mit
#            commit 5b521936b01fbe1890f6f9baed0254ab6351c04a, model.ckpt
#            1 677 246 742 octets
#   config : https://huggingface.co/facebook/dino-vitb16 -- carte : apache-2.0
#            (config.json SEUL, tire par le tokenizer de TripoSR ; les poids du
#            ViT sont dans model.ckpt)
#   marching cubes : torchmcubes.py de CE dossier (scikit-image, BSD-3-Clause),
#            parce que torchmcubes exige une compilation (nvcc absent ici).
#
# OU SONT LES POIDS -- JAMAIS dans un depot :
#   NK_GENIA_POIDS, sinon <dossier parent du worktree>/genia-tools/poids/TripoSR
#   (config.yaml + model.ckpt). Le dossier genia-tools est ignore par git.
#   Absents : REFUS, pas de telechargement implicite a chaque generation.
# Clone TripoSR : NK_GENIA_TRIPOSR, sinon <parent>/genia-tools/TripoSR.
#
# CE QUI N'EST PAS EMPRUNTE : la representation editable. Ce script produit un
# glTF ; c'est NkGLTFLoader puis NkEditMesh, dans le moteur, qui en font un objet.
# -----------------------------------------------------------------------------
import argparse
import os
import sys
import time

ICI = os.path.dirname(os.path.abspath(__file__))
RACINE = os.path.abspath(os.path.join(ICI, "..", ".."))
OUTILS = os.path.join(os.path.dirname(RACINE), "genia-tools")


def _refus(msg, code=2):
    sys.stderr.write("REFUS : %s\n" % msg)
    sys.stderr.flush()
    sys.exit(code)


def main():
    ap = argparse.ArgumentParser(description="GENIA : image -> glTF (TripoSR)")
    ap.add_argument("--image", required=True, help="image d'entree (png/jpg)")
    ap.add_argument("--out", required=True, help="fichier glTF de sortie (.glb ou .gltf)")
    ap.add_argument("--mc-resolution", type=int, default=256,
                    help="resolution des marching cubes (defaut 256, celle de TripoSR)")
    ap.add_argument("--foreground-ratio", type=float, default=0.85)
    ap.add_argument("--device", default="cuda:0",
                    help="cuda:0 (defaut) ; cpu doit etre DEMANDE, jamais un repli silencieux")
    ap.add_argument("--chunk-size", type=int, default=8192)
    a = ap.parse_args()

    if not os.path.isfile(a.image):
        _refus("image introuvable : %s" % a.image)
    ext = os.path.splitext(a.out)[1].lower()
    if ext not in (".glb", ".gltf"):
        _refus("la sortie doit etre .glb ou .gltf (glTF est le SEUL format d'echange) : %s" % a.out)

    tdir = os.environ.get("NK_GENIA_TRIPOSR") or os.path.join(OUTILS, "TripoSR")
    pdir = os.environ.get("NK_GENIA_POIDS") or os.path.join(OUTILS, "poids", "TripoSR")
    if not os.path.isdir(os.path.join(tdir, "tsr")):
        _refus("clone TripoSR introuvable : %s (NK_GENIA_TRIPOSR pour le designer)" % tdir)
    for f in ("config.yaml", "model.ckpt"):
        if not os.path.isfile(os.path.join(pdir, f)):
            _refus("poids absents : %s (NK_GENIA_POIDS pour le designer)" % os.path.join(pdir, f))
    # ICI d'abord : notre torchmcubes.py passe devant tout autre.
    sys.path.insert(0, tdir)
    sys.path.insert(0, ICI)

    t0 = time.time()
    try:
        import numpy as np
        import torch
        from PIL import Image
        import torchmcubes
        from tsr.system import TSR
        from tsr.utils import remove_background, resize_foreground
    except Exception as e:  # diagnostic
        _refus("import Python impossible (%s) : environnement incomplet" % e)

    print("MESURE genia : marching_cubes=%s" % getattr(torchmcubes, "MARQUE_REMPLACANT", "torchmcubes compile"))
    if a.device.startswith("cuda") and not torch.cuda.is_available():
        _refus("CUDA demande mais torch.cuda.is_available() = False (torch %s) ; --device cpu pour l'accepter"
               % torch.__version__)
    device = a.device
    print("MESURE genia : device=%s torch=%s cuda=%s" % (device, torch.__version__, torch.version.cuda))
    if device.startswith("cuda"):
        p = torch.cuda.get_device_properties(0)
        libre, total = torch.cuda.mem_get_info(0)
        print("MESURE genia : gpu='%s' vram_totale_mio=%d vram_libre_au_depart_mio=%d"
              % (p.name, total // (1024 * 1024), libre // (1024 * 1024)))
        torch.cuda.reset_peak_memory_stats()

    t1 = time.time()
    model = TSR.from_pretrained(pdir, config_name="config.yaml", weight_name="model.ckpt")
    model.renderer.set_chunk_size(a.chunk_size)
    model.to(device)
    t2 = time.time()
    print("MESURE genia : chargement_modele_s=%.1f" % (t2 - t1))

    image = Image.open(a.image)
    alpha = image.mode in ("RGBA", "LA") and image.getchannel("A").getextrema()[0] < 255
    if alpha:
        # Image DEJA detouree (canal alpha reel) : pas de rembg, donc pas de
        # telechargement de u2net. Meme preparation que run.py ensuite.
        image = image.convert("RGBA")
        detourage = "alpha de l'image"
    else:
        import rembg
        image = remove_background(image, rembg.new_session())
        detourage = "rembg"
    image = resize_foreground(image, a.foreground_ratio)
    arr = np.array(image).astype(np.float32) / 255.0
    arr = arr[:, :, :3] * arr[:, :, 3:4] + (1.0 - arr[:, :, 3:4]) * 0.5
    image = Image.fromarray((arr * 255.0).astype(np.uint8))
    t3 = time.time()
    print("MESURE genia : preparation_image_s=%.1f detourage=%s taille=%dx%d"
          % (t3 - t2, detourage, image.size[0], image.size[1]))

    with torch.no_grad():
        scene_codes = model([image], device=device)
    if device.startswith("cuda"):
        torch.cuda.synchronize()
    t4 = time.time()
    print("MESURE genia : inference_s=%.2f" % (t4 - t3))

    mesh = model.extract_mesh(scene_codes, True, resolution=a.mc_resolution)[0]
    t5 = time.time()
    print("MESURE genia : extraction_maillage_s=%.2f (dont marching cubes CPU)" % (t5 - t4))

    # SENS DES FACES : mesure, pas supposition (voir torchmcubes.py).
    vol = float(mesh.volume) if mesh.is_watertight else float("nan")
    print("MESURE genia : etanche=%s volume_signe=%.5f" % (mesh.is_watertight, vol))
    if mesh.is_watertight and vol < 0.0:
        mesh.invert()
        print("MESURE genia : faces retournees (volume negatif) -> volume_signe=%.5f" % float(mesh.volume))

    os.makedirs(os.path.dirname(os.path.abspath(a.out)) or ".", exist_ok=True)
    mesh.export(a.out)
    if not (os.path.isfile(a.out) and os.path.getsize(a.out) > 0):
        _refus("le fichier de sortie n'a pas ete ecrit : %s" % a.out, 3)

    nv = int(mesh.vertices.shape[0])
    nf = int(mesh.faces.shape[0])
    couleurs = "sommets" if getattr(mesh.visual, "kind", None) == "vertex" else "aucune"
    pic = torch.cuda.max_memory_allocated() // (1024 * 1024) if device.startswith("cuda") else 0
    print("MESURE genia : sommets=%d triangles=%d couleurs=%s octets=%d pic_vram_mio=%d total_s=%.1f"
          % (nv, nf, couleurs, os.path.getsize(a.out), pic, t5 - t0))
    print("SORTIE : %s" % os.path.abspath(a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
