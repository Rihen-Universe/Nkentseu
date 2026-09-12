# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# GENIA -- REMPLACANT CPU de `torchmcubes` pour TripoSR.
#
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# POURQUOI : TripoSR importe `from torchmcubes import marching_cubes`.
# torchmcubes (https://github.com/tatsy/torchmcubes, MIT) se COMPILE a
# l'installation, et son chemin CUDA exige nvcc -- mesure le 2026-09-12 : cette
# machine n'a PAS de toolkit CUDA. Plutot que de toucher au clone de TripoSR,
# ce module du meme nom se place devant lui (le dossier de genia_triposr.py est
# sys.path[0]) et fait le meme travail SUR CPU avec scikit-image
# (skimage.measure.marching_cubes, licence BSD-3-Clause).
#
# Ce que ca coute, dit : l'extraction du maillage passe sur CPU. L'inference
# (le reseau) reste sur le GPU. genia_triposr.py chronometre les deux etapes
# separement pour que le cout se LISE.
#
# CONTRAT reproduit (celui qu'utilise tsr/models/isosurface.py) :
#   marching_cubes(level: Tensor[D,H,W], threshold: float)
#     -> (verts: FloatTensor[N,3], faces: LongTensor[M,3])
#   avec verts dans l'ordre (x, y, z) = (axe 2, axe 1, axe 0) de la grille :
#   isosurface.py applique ensuite v_pos[..., [2, 1, 0]] pour revenir a
#   (axe 0, axe 1, axe 2). skimage rend (axe 0, axe 1, axe 2) : on inverse donc
#   les colonnes ici, sinon l'objet sortirait avec X et Z permutes.
#
# ⚠️ Permuter deux axes est une REFLEXION : elle retourne le sens des faces.
# MESURE (2026-09-12, sonde sur un ellipsoide 0.6/0.3/0.15 par le vrai
# MarchingCubeHelper de TripoSR) : ordre des axes juste, etanche, volume a
# 0,36 % de 4/3*pi*abc -- mais volume signe NEGATIF (-0.11269). La cause est la
# reflexion introduite ICI : on la compense ICI, en inversant l'ordre des
# indices de chaque triangle. genia_triposr.py garde sa propre mesure du volume
# signe comme filet, pas comme correctif.
# -----------------------------------------------------------------------------
import numpy as np
import torch
from skimage.measure import marching_cubes as _sk_marching_cubes

MARQUE_REMPLACANT = "genia-cpu-skimage"


def marching_cubes(level, threshold):
    vol = level.detach().float().cpu().numpy()
    verts, faces, _normales, _valeurs = _sk_marching_cubes(vol, level=float(threshold))
    verts = np.ascontiguousarray(verts[:, ::-1])  # (axe0,axe1,axe2) -> (x,y,z) facon torchmcubes
    faces = np.ascontiguousarray(faces[:, ::-1])  # la reflexion ci-dessus retourne le sens : on le rend
    return (torch.from_numpy(verts.astype(np.float32)),
            torch.from_numpy(faces.astype(np.int64)))
