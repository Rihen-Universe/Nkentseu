"""
Pre-traitement des frames Rihen : applique UNIQUEMENT un soft-key noir->alpha.

Aucun resize : on conserve la pleine resolution des frames (1920x1080 typique).
Le rendu in-game se charge du downscale via GL_LINEAR_MIPMAP_LINEAR + mipmaps
generes par Texture2D, ce qui preserve la qualite visuelle a tout taille.

Algo soft-key (RGB inchanges, alpha derivee de la luminance max) :
  - max(R,G,B) <= T_OFF  : alpha = 0          (fond noir + fringe AA sombre)
  - max(R,G,B) >= T_ON   : alpha = 255        (logo opaque)
  - entre 2              : alpha = ramp lineaire (AA edge smooth)

Les RGB ne sont JAMAIS modifies : on preserve la saturation des couleurs
(teal, orange) sans degrader la clarte visuelle.

Pourquoi pas un keying strict #000000 : les bords anti-aliases du logo
contiennent des pixels gris-sombres (ex: (10,30,40)) qui, gardes opaques,
forment un halo noir visible sur fond blanc. Le soft-key les rend semi
ou totalement transparents selon leur darkness.

Note : les PNG sources peuvent etre en mode 'P' (palette indexee). On force
.convert('RGBA') pour garantir un traitement RGBA homogene.

Lance sans argument : traite TOUS les PNG du dossier en place (overwrite).
"""

import os
import sys
import numpy as np
from PIL import Image

FOLDER = os.path.dirname(os.path.abspath(__file__))
T_OFF  = 30    # max(R,G,B) <= ce seuil  -> totalement transparent (halo)
T_ON   = 80    # max(R,G,B) >= ce seuil  -> totalement opaque (logo)


def process_one(path: str) -> None:
    # convert('RGBA') gere les modes P / L / RGB en les promouvant en RGBA.
    img = Image.open(path).convert("RGBA")
    arr = np.array(img, dtype=np.uint8)         # shape (H, W, 4)
    rgb = arr[..., :3]
    m = rgb.max(axis=-1)                        # luminance max par pixel
    # Ramp lineaire entre T_OFF et T_ON, clamp [0, 255].
    a_ramp = ((m.astype(np.int32) - T_OFF)
              * (255 // (T_ON - T_OFF))).clip(0, 255).astype(np.uint8)
    a_ramp[m <= T_OFF] = 0
    a_ramp[m >= T_ON]  = 255
    arr[..., 3] = a_ramp
    # optimize=True : taille fichier reduite (compression PNG plus aggressive).
    Image.fromarray(arr, mode="RGBA").save(path, "PNG", optimize=True)


def main() -> int:
    files = sorted(
        f for f in os.listdir(FOLDER)
        if f.lower().endswith(".png")
    )
    if not files:
        print("Aucun PNG trouve dans", FOLDER)
        return 1
    print(f"Soft-key {len(files)} frames (T_OFF={T_OFF}, T_ON={T_ON}, pleine resolution)")
    for i, name in enumerate(files):
        path = os.path.join(FOLDER, name)
        process_one(path)
        if (i + 1) % 10 == 0 or i + 1 == len(files):
            print(f"  [{i + 1}/{len(files)}] {name}")
    print("Termine.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
