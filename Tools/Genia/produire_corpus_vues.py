# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis - Rihen
#
# PRODUCTION DU CORPUS DE VUES -- premiere tranche EQUILIBREE.
#
# CE QUI EST DECIDE, ET POURQUOI (cf. CORPUS_VUES_MANIFESTE.md) :
#   * 48 vues (16 azimuts x 3 elevations). On ne rogne JAMAIS les vues :
#     les augmenter plus tard exige de TOUT re-rendre, alors qu'ajouter des
#     MODELES ne refait rien. C'est la bonne variable a ceder.
#   * 1 310 modeles PAR SOURCE (kenney, mes-assets, quaternius) = 3 930.
#     Le corpus complet est desequilibre (kenney 70,7 %) et la chaine Rihen
#     doit rester NEUTRE : un modele dont 70 % de l'apprentissage vient d'un
#     seul createur impose un gout. La premiere tranche est donc equilibree
#     PAR CONSTRUCTION, et le reste s'ajoutera sans rien re-rendre.
#   * un .npz compresse PAR MODELE : 5 703 fichiers au lieu de 820 000, et
#     13,3 Go au lieu de 1 842 Go de tampons bruts.
#   * on stocke la PROFONDEUR (16 bits), pas les positions : elles se
#     reconstruisent exactement depuis la profondeur et la pose.
#
# ⚠️ CE QUE LE MANIFESTE DECLARE PAR FICHIER, ET POURQUOI CE N'EST PAS UN DETAIL :
# les FBX passent par notre lecteur, qui ne rend ni UV ni materiaux -- ils sont
# donc GRIS. Si la donnee ne le disait pas, un modele apprendrait que cette
# part du monde est grise. Chaque entree porte donc `texture: true|false`, et
# la part globale est recalculee a la fin.

import sys, os, io, json, time, random, argparse
import numpy as np

ICI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ICI)
exec(open(os.path.join(ICI, 'rendre_vues_corpus.py'), encoding='utf-8')
     .read().split("if __name__")[0])

BASE = 'D:/Projets/2026/NKGenCorpus/brut/'
AZIMUTS = 16
ELEVATIONS = (-15.0, 15.0, 45.0)
RES = 384
RAYON, FOV, NEAR, FAR = 2.6, 40.0, 0.05, 10.0
PROF_MAX = 4.0      # borne de quantification 16 bits


def lister(base=BASE):
    par_source = {}
    for src in sorted(os.listdir(base)):
        d = os.path.join(base, src)
        if not os.path.isdir(d):
            continue
        out = []
        for rac, _, fics in os.walk(d):
            for f in fics:
                if f.lower().endswith(('.obj', '.glb', '.gltf', '.fbx')):
                    out.append(os.path.join(rac, f))
        if out:
            par_source[src] = sorted(out)
    return par_source


def selection_equilibree(par_source, par_source_max, graine=20260920):
    """Tirage EQUILIBRE : le meme nombre par source, graine fixe pour que la
    selection soit reproductible -- un corpus qu'on ne peut pas refaire a
    l'identique n'est pas un corpus."""
    r = random.Random(graine)
    sel = []
    for src in sorted(par_source):
        L = list(par_source[src])
        r.shuffle(L)
        pris = L[:par_source_max]
        sel += [(src, p) for p in pris]
    r.shuffle(sel)
    return sel


def poses_du_modele():
    out = []
    for i in range(AZIMUTS):
        for el in ELEVATIONS:
            out.append(pose_depuis_angles(i * (360.0 / AZIMUTS), el, RAYON,
                                          (0, 0, 0), FOV, RES, RES, NEAR, FAR))
    return out


def rendre_un(chemin, poses, controler=False):
    Vt, F, uv, tex = charger_avec_texture(chemin)
    rgbas = np.zeros((len(poses), RES, RES, 4), dtype=np.uint8)
    profs = np.zeros((len(poses), RES, RES), dtype=np.uint16)
    a_tex = False
    pire = 0.0
    for i, pose in enumerate(poses):
        rgba, prof, at = rendre_couleur(Vt, F, pose, uv, tex)
        a_tex = a_tex or at
        rgbas[i] = rgba
        profs[i] = (np.clip(prof / PROF_MAX, 0, 1) * 65535).astype(np.uint16)
        if controler and i % 16 == 0:
            # LE TEMOIN DE POSES, PENDANT la production et non seulement avant :
            # on reprojette le tampon de positions par la pose RECONSTRUITE
            # depuis ses angles -- le chemin qu'un consommateur empruntera.
            pos, msk, _ = rendre_vectorise(Vt, F, pose)
            refaite = pose_depuis_angles(pose['azimut'], pose['elevation'], pose['rayon'],
                                         pose['cible'], pose['fov'], pose['W'], pose['H'],
                                         pose['near'], pose['far'])
            e = erreur_reprojection(pos, msk, refaite)
            if e:
                pire = max(pire, e['p99'])
    return rgbas, profs, a_tex, len(F), pire


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--sortie', required=True)
    ap.add_argument('--par-source', type=int, default=1310)
    ap.add_argument('--limite', type=int, default=0, help='0 = tout le lot')
    a = ap.parse_args()

    os.makedirs(a.sortie, exist_ok=True)
    poses = poses_du_modele()
    par_source = lister()
    print('sources : ' + ', '.join('%s=%d' % (k, len(v)) for k, v in sorted(par_source.items())))
    sel = selection_equilibree(par_source, a.par_source)
    if a.limite:
        sel = sel[:a.limite]
    print('selection : %d modeles, %d vues chacun = %d vues'
          % (len(sel), len(poses), len(sel) * len(poses)))

    # ⚠️ EN AJOUT, JAMAIS EN ECRASEMENT. Le producteur saute les .npz deja
    # ecrits, donc une seconde course ne retente QUE les echecs -- mais avec un
    # 'w' elle reecrivait le manifeste avec ces seules entrees et DETRUISAIT
    # les metadonnees des milliers de modeles reussis. C'est arrive le 20/09 :
    # 3 930 lignes remplacees par 76. Repare parce qu'une copie avait ete prise
    # avant de relancer, pas parce que le code etait juste.
    #
    # Famille « une operation qui agit avant qu'on ait vu sur quoi elle agit » :
    # le mode d'ouverture decide du sort de ce qui existe deja, sans jamais le
    # nommer. Le meme piege a coute 1 417 lignes de criteres a un autre chantier
    # la meme nuit.
    fman = open(os.path.join(a.sortie, 'manifeste.jsonl'), 'a', encoding='utf-8')
    t0 = time.time()
    n_ok = n_ko = n_tex = 0
    pire_global = 0.0
    for k, (src, chemin) in enumerate(sel):
        nom = '%05d_%s' % (k, os.path.splitext(os.path.basename(chemin))[0][:40])
        nom = ''.join(c if c.isalnum() or c in '-_' else '_' for c in nom)
        cible = os.path.join(a.sortie, nom + '.npz')
        if os.path.exists(cible):
            n_ok += 1
            continue
        try:
            rgbas, profs, a_tex, nf, pire = rendre_un(chemin, poses, controler=(k % 97 == 0))
        except Exception as e:
            n_ko += 1
            fman.write(json.dumps({'id': nom, 'source': src, 'chemin': chemin,
                                   'echec': str(e)[:160]}) + '\n')
            fman.flush()
            continue
        np.savez_compressed(cible, rgba=rgbas, prof=profs,
                            poses=np.array([[p['azimut'], p['elevation'], p['rayon'], p['fov']]
                                            for p in poses]))
        n_ok += 1
        n_tex += 1 if a_tex else 0
        pire_global = max(pire_global, pire)
        fman.write(json.dumps({'id': nom, 'source': src, 'chemin': chemin,
                               'texture': bool(a_tex), 'triangles': int(nf),
                               'vues': len(poses), 'res': RES,
                               'octets': os.path.getsize(cible)}) + '\n')
        fman.flush()
        if k % 50 == 0 and k:
            dt = time.time() - t0
            reste = dt / max(1, k) * (len(sel) - k)
            print('  %5d/%d  ok=%d ko=%d  texture=%.0f %%  | %.1f min ecoulees, ~%.1f min restantes'
                  % (k, len(sel), n_ok, n_ko, 100.0 * n_tex / max(1, n_ok), dt / 60, reste / 60))
            sys.stdout.flush()
    fman.close()
    dt = time.time() - t0
    print('')
    print('TERMINE : %d rendus, %d echecs, %.1f %% textures, %.1f min'
          % (n_ok, n_ko, 100.0 * n_tex / max(1, n_ok), dt / 60))
    print('TEMOIN DE POSES pendant la production : pire p99 = %.4f px -> %s'
          % (pire_global, 'VERT' if pire_global < 0.5 else 'ROUGE *** poses suspectes ***'))


if __name__ == '__main__':
    main()
