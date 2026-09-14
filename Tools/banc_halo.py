#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# banc_halo.py -- le temoin (3) du chantier halo : profil radial du halo de bloom
# a radiance croissante, sur la vue 6 du banc (DemoBancOmbre, levier NK_BANC_SOURCE*).
#
# Usage : python Tools/banc_halo.py <dorsal> <taille> <exposition> "<x,y,z>" <teinte> [<teinte> ...]
#   ex.   python Tools/banc_halo.py dx11 1 0.1 "3,3.9,-12" 1.87 3.74 7.48 18.7 37.4
# Prerequis : renderdemo construit en Release (Build/Bin/Release-Windows/renderdemo).
# Sorties : captures/halo/<tag>.png, <tag>.stdout.txt, journal_<dorsal>_s<taille>_e<expo>.json
# Resultats du 12/09 (seuil 3,62, source 50 px, quatre dorsaux) : echanges/nk3dmodeler.questions.md, Q193.
#
# Une COURSE = un lancement de renderdemo avec bloom ON ou OFF, capture LDR a la
# trame 30. Le seuil HDR de la passe brillante ([seuil-bloom], NK_RG_SEUIL) et le
# reglage de la source ([BancHalo]) sont lus sur le stdout de la MEME course.
#
# Instrument, tel qu'il a ete corrige par la premiere famille (12/09) :
#   * la source est lue sur les pixels >= 90 % du max de OFF (la face avant seule ;
#     a 50 % la face inferieure du cube entrait dans le masque des teinte 3,7) ;
#   * l'image est ramenee en HDR par ACES inverse (gamma 2.2), divisee par
#     l'exposition d'affichage NK_EXPOSURE -- qui ne touche PAS le seuil de la passe
#     brillante (elle n'override que le push constant du tonemap). A exposition 1 le
#     coeur du halo sature l'afficheur des 2 seuils ; a 0,1 il se lit jusqu'a ~5
#     seuils, a 0,01 jusqu'a ~20. Le plafond de l'inverse vaut 7,0 / exposition :
#     un `max` ou un `ref` egal a ce plafond est un point NON LISIBLE ;
#   * la scene affichee est ecretee a hdrSafetyClamp (64) par le tonemap : la
#     radiance de la source se lit jusque-la, au-dela elle vaut k x teinte, k etant
#     mesure sur les points non satures de la meme famille (0,965 par unite le 12/09) ;
#   * anneaux PARTIELS jusqu'a 320 px (les anneaux complets s'arretaient a la
#     distance de la source au bord) ; le nombre de pixels de chaque anneau est
#     conserve ; les rayons se lisent en RELATIF (50 %, 10 %, 1 % du premier anneau
#     hors source), jamais en seuil absolu.
import os, sys, json, subprocess, math
import numpy as np
from PIL import Image

# La racine du depot est le parent de Tools/ : le script suit son arbre.
RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(RACINE, "Build", "Bin", "Release-Windows", "renderdemo", "renderdemo.exe")
OUT = os.path.join(RACINE, "captures", "halo")
os.makedirs(OUT, exist_ok=True)


def course(backend, teinte, taille, on, expo, pos, frame=30):
    tag = f"{backend}_t{teinte}_s{taille}_e{expo}_{'on' if on else 'off'}"
    png = os.path.join(OUT, tag + ".png")
    env = dict(os.environ)
    env.update({
        "NK_BANC_VUE": "6",
        "NK_BANC_BLOOM": "1",
        "NK_BANC_SOURCE": str(teinte),
        "NK_BANC_SOURCE_TAILLE": str(taille),
        "NK_BANC_SOURCE_POS": pos,
        "NK_EXPOSURE": str(expo),
        "NK_MAXFRAMES": str(frame + 10),
        "NK_CAPTURE": str(frame),
        "NK_CAPTURE_PATH": png,
        "NK_RG_SEUIL": "1",
    })
    env.pop("NK_BANC_POST", None)
    if not on:
        env["NK_BANC_POST"] = "bloom"
    if os.path.exists(png):
        os.remove(png)
    p = subprocess.run([EXE, "--demo=BancOmbre", f"--backend={backend}"], env=env, cwd=RACINE,
                       capture_output=True, text=True, errors="replace", timeout=180)
    seuil = src = None
    for l in p.stdout.splitlines():
        if l.startswith("[seuil-bloom]"):
            seuil = l.strip()
        if l.startswith("[BancHalo]"):
            src = l.strip()
    with open(os.path.join(OUT, tag + ".stdout.txt"), "w", encoding="utf-8") as f:
        f.write(p.stdout + "\n--- stderr ---\n" + p.stderr)
    return {"tag": tag, "png": png, "ok": os.path.exists(png), "rc": p.returncode, "seuil": seuil, "src": src}


def lum(path):
    a = np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)
    return a, 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]


def aces_inv(m):
    # m = ACES(x) = (x(2.51x+0.03)) / (x(2.43x+0.59)+0.14), resolu pour x >= 0 (vectorise).
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    A = a - c * m
    B = b - d * m
    C = -e * m
    disc = np.maximum(B * B - 4 * A * C, 0.0)
    return (-B + np.sqrt(disc)) / (2 * A)


def ldr_vers_hdr(v, expo, gamma=2.2):
    m = np.clip(v / 255.0, 0.0, 0.9995) ** gamma
    return aces_inv(m) / expo


def analyse(png_on, png_off, expo):
    rgb_on, on = lum(png_on)
    rgb_off, off = lum(png_off)
    H, W = off.shape
    sat_on = int((on >= 250.0).sum())
    sat_off = int((off >= 250.0).sum())
    # la source : face avant seule (>= 90 % du max de OFF)
    mx = off.max()
    ms = off >= 0.9 * mx
    ys, xs = np.nonzero(ms)
    n_s = int(ms.sum())
    cx, cy = float(xs.mean()), float(ys.mean())
    r_s = math.sqrt(n_s / math.pi)
    src_ldr = float(off[ms].mean())
    src_hdr = float(ldr_vers_hdr(np.array(src_ldr), expo)) if src_ldr < 250 else float("inf")
    fond_ldr = float(np.median(off))
    fond_hdr = float(ldr_vers_hdr(np.array(fond_ldr), expo))
    # le halo, en HDR (scene + 1,5 x bloom, exposee, tonemappee -> inverse)
    hdr_on = ldr_vers_hdr(on, expo)
    hdr_off = ldr_vers_hdr(off, expo)
    diff = hdr_on - hdr_off
    dl = on - off
    mh = dl > 4.0
    n_h = int(mh.sum())
    if n_h:
        yh, xh = np.nonzero(mh)
        hx, hy = float(xh.mean()), float(yh.mean())
    else:
        hx = hy = float("nan")
    yy, xx = np.mgrid[0:H, 0:W]
    rr = np.hypot(xx - cx, yy - cy)
    pas = 2
    profil = []
    for r0 in range(0, 322, pas):
        m = (rr >= r0) & (rr < r0 + pas)
        n = int(m.sum())
        if n:
            profil.append((r0 + pas / 2, float(diff[m].mean()), n, int((on[m] >= 250).sum())))
    # rayons RELATIFS au premier anneau hors source (r > r_s + 4)
    ref = None
    for r, v, n, s in profil:
        if r > r_s + 4:
            ref = v
            break
    rayons = {}
    if ref and ref > 0:
        for frac in (0.5, 0.1, 0.01):
            rf = None
            for r, v, n, s in profil:
                if r > r_s + 4 and v > frac * ref:
                    rf = r
            rayons[str(frac)] = rf
    octaves = []
    r = 4
    while 2 * r <= 320:
        m = (rr >= r) & (rr < 2 * r)
        octaves.append((r, 2 * r, float(diff[m].sum()), int(m.sum())))
        r *= 2
    d_direct = math.hypot(hx - cx, hy - cy) if n_h else float("nan")
    d_miroir = math.hypot(hx - cx, hy - (H - cy)) if n_h else float("nan")
    return {
        "W": W, "H": H, "expo": expo, "sat_on": sat_on, "sat_off": sat_off,
        "source": {"cx": cx, "cy": cy, "n": n_s, "r": r_s, "ldr": src_ldr, "hdr": src_hdr,
                   "fond_ldr": fond_ldr, "fond_hdr": fond_hdr},
        "halo": {"n": n_h, "cx": hx, "cy": hy, "max_hdr": float(diff.max()), "somme_hdr": float(diff.sum()),
                 "d_direct": d_direct, "d_miroir": d_miroir, "ref": ref},
        "profil": profil, "rayons": rayons, "octaves": octaves,
    }


def ligne(tag, an):
    s, h = an["source"], an["halo"]
    hdr = "sat" if math.isinf(s["hdr"]) else f"{s['hdr']:.3f}"
    oct_txt = " ".join(f"[{a}-{b}):{e:.1f}" for a, b, e, n in an["octaves"])
    ray = " ".join(f"r{k}={v}" for k, v in an["rayons"].items())
    return (f"{tag:<30} src ({s['cx']:.1f},{s['cy']:.1f}) n={s['n']} r={s['r']:.1f} ldr={s['ldr']:.1f} "
            f"hdr={hdr} fond={s['fond_hdr']:.3f} sat={an['sat_on']} | halo n={h['n']} ({h['cx']:.1f},{h['cy']:.1f}) "
            f"max={h['max_hdr']:.3f} somme={h['somme_hdr']:.0f} d={h['d_direct']:.1f} miroir={h['d_miroir']:.1f} "
            f"ref={h['ref']} {ray} | oct {oct_txt}")


if __name__ == "__main__":
    if len(sys.argv) < 6:
        print(__doc__ or "usage : banc_halo.py <dorsal> <taille> <exposition> \"<x,y,z>\" <teinte> [...]")
        sys.exit(2)
    backend, taille, expo, pos = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
    teintes = sys.argv[5:]
    journal = []
    for t in teintes:
        r_off = course(backend, t, taille, False, expo, pos)
        r_on = course(backend, t, taille, True, expo, pos)
        print(f"# {r_off['tag']} rc={r_off['rc']} ok={r_off['ok']} | {r_off['seuil']} | {r_off['src']}")
        print(f"# {r_on['tag']} rc={r_on['rc']} ok={r_on['ok']} | {r_on['seuil']} | {r_on['src']}")
        if not (r_off["ok"] and r_on["ok"]):
            print("!! capture manquante, course non analysee")
            continue
        an = analyse(r_on["png"], r_off["png"], expo)
        print(ligne(r_on["tag"], an))
        journal.append({"on": r_on, "off": r_off, "analyse": an})
    with open(os.path.join(OUT, f"journal_{backend}_s{taille}_e{expo}.json"), "w", encoding="utf-8") as f:
        json.dump(journal, f, indent=1)
