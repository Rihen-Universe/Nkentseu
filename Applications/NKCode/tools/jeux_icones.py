# -*- coding: utf-8 -*-
# =============================================================================
# jeux_icones.py — ECRIT les jeux d'icones de NKCode, livres comme EXTENSIONS DE
# DONNEES (data/extensions/<id>/), a partir des dessins de la maquette du 01/10 :
#   References/Captures/nkcode-propositions/_sources/icones.html
# (objets T, pastille(), dossierPlein(), tuileJenga() -- memes chemins, memes
# couleurs, recopies ici tels quels).
#
# Usage (depuis la racine du depot) :
#   py Applications/NKCode/tools/jeux_icones.py
#
# Ce que chaque extension contient (format lu par Shell/NkJeuxIcones.h) :
#   extension.cfg  le manifeste : id, nom, version, contribue = jeuIcones,
#                  integre (Pastilles), variantes, rogner, mono...
#   icons.cfg  extension -> icone, MEME format que data/icons.cfg
#   *.svg      une icone par fichier, nommee comme l'icone de l'actuel qu'elle
#              remplace (NewFile, FolderSrc...) ou « Fic<Type> » pour les types
#              de fichier (pour ne pas detourner une icone d'interface)
#   sombre/ clair/   (Trait seulement) les memes icones aux couleurs du theme
#
# Les SVG n'emploient que ce que NkSVGCodec rasterise : path, rect, circle,
# fill / stroke en #RRGGBB, stroke-linecap/linejoin, stroke-dasharray, opacity,
# transform. Pas de currentColor (rendu noir), pas de color-mix() : les
# melanges sont calcules ici.
# =============================================================================
import os

ICI = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(ICI, "..", "data", "extensions"))

# ── Jeu 1 « Trait » : grille 16, trait 1,35 (icones.html, objet T) ──────────
T = {
    "nouveauFichier": '<path d="M8.5 14.25H4.25a1 1 0 0 1-1-1V2.75a1 1 0 0 1 1-1H9l3.5 3.5V8.5"/><path d="M9 1.75v3.5h3.5"/><path d="M12.25 10.5v4M10.25 12.5h4"/>',
    "nouveauDossier": '<path d="M7.5 13.25H2.75a1 1 0 0 1-1-1V3.75a1 1 0 0 1 1-1h3.4l1.6 1.6h5.5a1 1 0 0 1 1 1V8"/><path d="M12.25 10v4.5M10 12.25h4.5"/>',
    "actualiser": '<path d="M13.25 8a5.25 5.25 0 0 1-9.4 3.2"/><path d="M2.75 8a5.25 5.25 0 0 1 9.4-3.2"/><path d="M12.5 1.75v3h-3"/><path d="M3.5 14.25v-3h3"/>',
    "replier": '<path d="M4.5 2.5 8 5.75l3.5-3.25"/><path d="M4.5 13.5 8 10.25l3.5 3.25"/>',
    "exclus": '<path d="M1.75 8S4 3.75 8 3.75 14.25 8 14.25 8 12 12.25 8 12.25 1.75 8 1.75 8z"/><circle cx="8" cy="8" r="2"/><path d="M2.75 13.25 13.25 2.75"/>',
    "fichiers": '<path d="M10.5 1.75H6a1 1 0 0 0-1 1v8.5a1 1 0 0 0 1 1h6.25a1 1 0 0 0 1-1V4.5z"/><path d="M10.5 1.75V4.5h2.75"/><path d="M2.75 4.75v8.5a1 1 0 0 0 1 1H10"/>',
    "rechercher": '<circle cx="7" cy="7" r="4.5"/><path d="M10.4 10.4 14 14"/>',
    "git": '<circle cx="4.5" cy="3.75" r="1.75"/><circle cx="4.5" cy="12.25" r="1.75"/><circle cx="11.5" cy="5.25" r="1.75"/><path d="M4.5 5.5v5"/><path d="M11.5 7c0 2.6-2.4 3.4-6.6 3.9"/>',
    "deboguer": '<path d="M2.5 2.75v6.5L7.5 6z"/><rect x="8.25" y="7.25" width="5.25" height="7" rx="2.6"/><path d="M10.9 9.5v4.5M8.25 10.75H6.5M13.5 10.75h1.75"/>',
    "jenga": '<rect x="2.25" y="2" width="11.5" height="3.25" rx=".75"/><rect x="2.25" y="6.4" width="5.25" height="3.25" rx=".75"/><rect x="8.5" y="6.4" width="5.25" height="3.25" rx=".75"/><rect x="2.25" y="10.75" width="11.5" height="3.25" rx=".75"/>',
    "partage": '<circle cx="6" cy="5.5" r="2.25"/><path d="M2 13.75c0-2.3 1.8-4 4-4s4 1.7 4 4"/><circle cx="11.5" cy="5" r="1.75"/><path d="M11.25 9.75c1.75.15 3 1.6 3 3.5"/>',
    "extensions": '<rect x="2" y="2.5" width="5" height="5" rx="1"/><rect x="2" y="9" width="5" height="5" rx="1"/><rect x="8.5" y="9" width="5" height="5" rx="1"/><path d="M11 1.75 14.25 5 11 8.25 7.75 5z"/>',
    "profilage": '<path d="M2 14h12"/><path d="M4 11.5v-3M7 11.5V4M10 11.5V6.5M13 11.5V2.5"/>',
    "reglages": '<circle cx="8" cy="8" r="2"/><circle cx="8" cy="8" r="4.25"/><path d="M8 1.75v2M8 12.25v2M1.75 8h2M12.25 8h2M3.6 3.6 5 5M11 11l1.4 1.4M3.6 12.4 5 11M11 5l1.4-1.4"/>',
    "ia": '<path d="M7 2l1.25 3.5 3.5 1.25L8.25 8 7 11.5 5.75 8 2.25 6.75 5.75 5.5z"/><path d="M12 9.5l.6 1.65 1.65.6-1.65.6L12 14l-.6-1.65-1.65-.6 1.65-.6z"/>',
    "codex": '<path d="M5.25 4.25 1.75 8l3.5 3.75M10.75 4.25 14.25 8l-3.5 3.75"/>',
    "maison": '<path d="M2.25 7.25 8 2.25l5.75 5"/><path d="M3.75 6.25v7.5h8.5v-7.5"/><path d="M6.5 13.75V10h3v3.75"/>',
    "dossier": '<path d="M1.75 4a1 1 0 0 1 1-1h3.4l1.6 1.6h5.5a1 1 0 0 1 1 1v6.65a1 1 0 0 1-1 1H2.75a1 1 0 0 1-1-1z"/>',
    "dossierOuvert": '<path d="M1.75 12V4a1 1 0 0 1 1-1h3.4l1.6 1.6h4.75a1 1 0 0 1 1 1V7"/><path d="M1.75 12.25 3.7 7.7a1 1 0 0 1 .92-.6h9.3a.55.55 0 0 1 .5.77l-1.9 4.55a1 1 0 0 1-.92.6H2.75"/>',
    "cpp": '<path d="M6.9 4.9A3.6 3.6 0 1 0 6.9 11.1"/><path d="M10 6.4v3.2M8.4 8h3.2M13.6 6.4v3.2M12 8h3.2"/>',
    "h": '<path d="M4.5 2.75v10.5M4.5 8.6c.3-1.5 1.3-2.35 2.75-2.35 1.6 0 2.5 1 2.5 2.6v4.4"/>',
    "json": '<path d="M5.75 2.75c-1.4 0-1.9.6-1.9 1.7v1.6c0 .95-.55 1.7-1.6 1.95 1.05.25 1.6 1 1.6 1.95v1.6c0 1.1.5 1.7 1.9 1.7M10.25 2.75c1.4 0 1.9.6 1.9 1.7v1.6c0 .95.55 1.7 1.6 1.95-1.05.25-1.6 1-1.6 1.95v1.6c0 1.1-.5 1.7-1.9 1.7"/>',
    "md": '<rect x="1.75" y="3.25" width="12.5" height="9.5" rx="1.5"/><path d="M4.25 10.25v-4.5L6 7.75l1.75-2v4.5M11 5.75v4.5M9.5 8.75 11 10.25l1.5-1.5"/>',
    "py": '<path d="M3.25 6.25v7.5M3.25 6.75h2.1a1.9 1.9 0 0 1 0 3.8h-2.1"/><path d="M9 6.25l2.1 4.6M13.5 6.25 10 13.75"/>',
    "img": '<rect x="2" y="3" width="12" height="10" rx="1.5"/><path d="M2.5 11.75 6 8.25l2.25 2.25 2-2 3.25 3.25"/><circle cx="10.5" cy="5.9" r="1"/>',
    "txt": '<path d="M9 1.75H4.25a1 1 0 0 0-1 1v10.5a1 1 0 0 0 1 1h7.5a1 1 0 0 0 1-1V5.25z"/><path d="M9 1.75v3.5h3.5"/><path d="M5.5 8.25h5M5.5 10.75h3.5"/>',
    "cfg": '<path d="M2.5 4.5h4.25M10 4.5h3.5M2.5 11.5h1.75M7.5 11.5h6"/><circle cx="8.4" cy="4.5" r="1.6"/><circle cx="5.9" cy="11.5" r="1.6"/>',
    "nksl": '<path d="M9.25 1.75 3.75 9h4l-1 5.25L12.25 7h-4z"/>',
    "gitf": '<circle cx="8" cy="8" r="2.25"/><path d="M1.75 8h4M10.25 8h4"/>',
    "doc": '<path d="M9 1.75H4.25a1 1 0 0 0-1 1v10.5a1 1 0 0 0 1 1h7.5a1 1 0 0 0 1-1V5.25z"/><path d="M9 1.75v3.5h3.5"/>',
    # ── Ajouts (meme grille, meme trait) : ce que la maquette ne dessinait pas
    #    mais que l'Explorateur rencontre -- sans eux, ces types retomberaient
    #    sur les icones de l'actuel, au milieu d'un autre style. ──
    "c": '<path d="M10.4 4.9A3.6 3.6 0 1 0 10.4 11.1"/>',
    "oeil": '<path d="M1.75 8S4 3.75 8 3.75 14.25 8 14.25 8 12 12.25 8 12.25 1.75 8 1.75 8z"/><circle cx="8" cy="8" r="2"/>',
    "filtre": '<path d="M2.25 3h11.5L9.25 8.5v4.25l-2.5 1.25V8.5z"/>',
    "term": '<rect x="1.75" y="2.75" width="12.5" height="10.5" rx="1.5"/><path d="M4.5 6.25 6.75 8 4.5 9.75M8.25 10.25h3.25"/>',
    "code": '<path d="M5.25 4.25 1.75 8l3.5 3.75M10.75 4.25 14.25 8l-3.5 3.75M9 3.25 7 12.75"/>',
    "media": '<rect x="2" y="3" width="12" height="10" rx="1.5"/><path d="M6.75 5.75v4.5L10.5 8z"/>',
    "archive": '<rect x="2" y="2.75" width="12" height="3.25" rx=".75"/><path d="M3 6v6.25a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1V6M6.5 8.75h3"/>',
    "bin": '<rect x="4" y="4" width="8" height="8" rx="1"/><path d="M6.25 1.75V4M9.75 1.75V4M6.25 12v2.25M9.75 12v2.25M1.75 6.25H4M1.75 9.75H4M12 6.25h2.25M12 9.75h2.25"/>',
    "pdf": '<path d="M9 1.75H4.25a1 1 0 0 0-1 1v10.5a1 1 0 0 0 1 1h7.5a1 1 0 0 0 1-1V5.25z"/><path d="M9 1.75v3.5h3.5"/><path d="M5.25 11.75c1.6-.5 3.4-2.6 3.9-4.6M6.5 8.75h4"/>',
    "police": '<path d="M3 13.25 7 2.75h2l4 10.5M4.75 9.25h6.5"/>',
}

# Couleurs du Trait par theme (icones.html : .fen.s / .fen.c)
TRAIT_COUL = {
    "sombre": {"acc": "#3A9DE9", "fg": "#C9D1D9", "fg2": "#9DA7B3", "fg3": "#6E7681",
               "cpp": "#6CA6FF", "h": "#B392F0", "c": "#8CA3C7", "jenga": "#F0883E", "md": "#39C5CF",
               "json": "#E3B341", "py": "#A8CC4C", "img": "#56D364", "nksl": "#E57BC5", "git": "#F47067",
               "term": "#79C0FF", "pdf": "#F47067",
               "r-src": "#6CA6FF", "r-test": "#56D364", "r-res": "#E3B341", "r-docs": "#39C5CF",
               "r-out": "#6E7681", "r-norm": "#9DA7B3"},
    "clair": {"acc": "#0067B8", "fg": "#424A53", "fg2": "#59636E", "fg3": "#8C959F",
              "cpp": "#1F6FEB", "h": "#8250DF", "c": "#4B5E7D", "jenga": "#C2410C", "md": "#0E7490",
              "json": "#9A6700", "py": "#4D7C0F", "img": "#1A7F37", "nksl": "#BF3989", "git": "#CF222E",
              "term": "#0969DA", "pdf": "#CF222E",
              "r-src": "#1F6FEB", "r-test": "#1A7F37", "r-res": "#B08800", "r-docs": "#0E7490",
              "r-out": "#8C959F", "r-norm": "#59636E"},
}

# Types de fichier : (glyphe T, couleur Trait, couleur Pastille)
TYPES = {
    "Cpp": ("cpp", "cpp", "#3B7DDD"),
    "H": ("h", "h", "#8B5CF6"),
    "C": ("c", "c", "#5B6B86"),
    "Jenga": ("jenga", "jenga", "#E8711E"),
    "Md": ("md", "md", "#0E9AA7"),
    "Json": ("json", "json", "#D9A300"),
    "Py": ("py", "py", "#5E9E2E"),
    "Img": ("img", "img", "#2EA043"),
    "Txt": ("txt", "fg2", "#6B7785"),
    "Cfg": ("cfg", "fg2", "#6B7785"),
    "Nksl": ("nksl", "nksl", "#C2408F"),
    "Git": ("gitf", "git", "#D9534F"),
    "Doc": ("doc", "fg3", "#8B949E"),
    "Term": ("term", "term", "#3F4B5B"),
    "Code": ("code", "fg", "#57606A"),
    "Media": ("media", "img", "#2EA043"),
    "Archive": ("archive", "fg2", "#8B949E"),
    "Bin": ("bin", "fg3", "#57606A"),
    "Pdf": ("pdf", "pdf", "#C93C37"),
    "Police": ("police", "fg2", "#6B7785"),
}

# Extensions -> type (complete data/icons.cfg ; le type « Doc » est le repli « * »)
EXT = {
    "Cpp": ".cpp .cc .cxx .c++ .inl .ipp",
    "H": ".h .hpp .hxx .hh .h++",
    "C": ".c",
    "Jenga": ".jenga",
    "Md": ".md .markdown",
    "Json": ".json .jsonc",
    "Py": ".py .pyi",
    "Img": ".png .jpg .jpeg .bmp .svg .gif .tga .webp .ico .hdr .exr .dds .ktx",
    "Txt": ".txt .log .csv",
    "Cfg": ".cfg .ini .toml .yaml .yml .xml .clang-format .clang-tidy .clangd .editorconfig .props .plist",
    "Nksl": ".nksl .glsl .hlsl .vert .frag .comp .geom .tesc .tese .wgsl .metal .spv .fx .shader",
    "Git": ".gitignore .gitattributes .gitmodules",
    "Term": ".sh .bat .cmd .ps1 .bash .zsh",
    "Code": ".cs .rs .zig .go .js .ts .java .m .mm .lua .cmake .html .htm .css .kt .swift .dart .rb .php",
    "Media": ".mp4 .avi .mkv .webm .mov .wav .mp3 .ogg .flac",
    "Archive": ".zip .7z .tar .gz .rar .xz",
    "Bin": ".exe .dll .so .a .lib .o .obj .pdb .dylib",
    "Pdf": ".pdf",
    "Police": ".ttf .otf .woff .woff2",
}

# Boutons et barres d'activite : BLANCS (teintes par le theme au dessin).
# nom de l'icone de l'actuel qu'ils remplacent -> glyphe
MONO = {
    "NewFile": "nouveauFichier", "NewFolder": "nouveauDossier", "Sync": "actualiser",
    "CollapseAll": "replier", "EyeClosed": "exclus", "Eye": "oeil", "Filter": "filtre",
    "Files": "fichiers", "SearchC": "rechercher", "SourceControl": "git", "DebugAlt": "deboguer",
    "LiveShare": "partage", "Extensions": "extensions", "Graph": "profilage",
    "SettingsGear": "reglages", "Sparkle": "ia", "CodeC": "codex", "Home": "maison",
}

# Dossiers : nom de l'icone de l'actuel -> role (icones.html, objet ROLE)
DOSSIERS = {
    "FolderM": "norm", "FolderSrc": "src", "FolderInclude": "src", "FolderTest": "test",
    "FolderDocs": "docs", "FolderResource": "res", "FolderShader": "norm", "FolderConfig": "norm",
    "FolderGit": "git", "FolderScripts": "tools", "FolderDist": "out",
}
TRAIT_ROLE = {"src": "r-src", "test": "r-test", "res": "r-res", "docs": "r-docs", "out": "r-out",
              "git": "r-out", "tools": "r-norm", "norm": "r-norm"}
PD = {"norm": "#7A8FA8", "src": "#3B7DDD", "test": "#2EA043", "res": "#D9A300", "docs": "#0E9AA7",
      "out": "#8B949E", "git": "#E8711E", "tools": "#8B5CF6"}


def svg(corps):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
            + corps + "</svg>\n")


def trait(glyphe, coul, extra=""):
    return svg(f'<g fill="none" stroke="{coul}" stroke-width="1.35" stroke-linecap="round" '
               f'stroke-linejoin="round"{extra}>{T[glyphe]}</g>')


def melange(c, blanc_pct):
    """color-mix(in srgb, c (100-p)%, white p%)"""
    r, g, b = int(c[1:3], 16), int(c[3:5], 16), int(c[5:7], 16)
    p = blanc_pct / 100.0
    m = lambda v: round(v * (1 - p) + 255 * p)
    return "#%02X%02X%02X" % (m(r), m(g), m(b))


def pastille(glyphe, fond, encre="#FFFFFF"):
    return svg(f'<rect x="1" y="1" width="14" height="14" rx="3.5" fill="{fond}"/>'
               f'<g fill="none" stroke="{encre}" stroke-width="2.3" stroke-linecap="round" '
               f'stroke-linejoin="round" transform="translate(3 3) scale(.625)">{T[glyphe]}</g>')


def dossier_plein(role, ouvert):
    c = PD[role]
    op = ' opacity=".6"' if role == "out" else ""
    fond = (f'<path d="M1 4.1A1.6 1.6 0 0 1 2.6 2.5h3.5l1.6 1.6h5.7A1.6 1.6 0 0 1 15 5.7v6.2a1.6 1.6 0 0 1-1.6 '
            f'1.6H2.6A1.6 1.6 0 0 1 1 11.9z" fill="{c}"/>')
    if ouvert:
        face = (f'<path d="M3.1 6.6h11.7a.8.8 0 0 1 .75 1.06l-1.6 4.7a1.6 1.6 0 0 1-1.5 1.14H1.6a.6.6 0 0 1-.57-.8z" '
                f'fill="{melange(c, 38)}"/>')
    else:
        face = f'<path d="M1 6.3h14v5.6a1.6 1.6 0 0 1-1.6 1.6H2.6A1.6 1.6 0 0 1 1 11.9z" fill="{melange(c, 22)}"/>'
    return svg(f"<g{op}>{fond}{face}</g>")


def tuile_jenga():
    return svg('<rect x="1" y="1" width="14" height="14" rx="3.5" fill="#0075CF"/>'
               '<g fill="#FFFFFF" transform="translate(3 3) scale(.625)">'
               '<rect x="2.25" y="2" width="11.5" height="3.25" rx=".75"/>'
               '<rect x="2.25" y="6.4" width="5.25" height="3.25" rx=".75"/>'
               '<rect x="8.5" y="6.4" width="5.25" height="3.25" rx=".75"/>'
               '<rect x="2.25" y="10.75" width="11.5" height="3.25" rx=".75"/></g>')


def ecrire(chemin, texte):
    os.makedirs(os.path.dirname(chemin), exist_ok=True)
    with open(chemin, "w", encoding="utf-8", newline="\n") as f:
        f.write(texte)


def icons_cfg(titre):
    lignes = [f"# NKCode — jeu d'icones « {titre} » : extension -> icone (meme format que data/icons.cfg).",
              "# Ecrit par Applications/NKCode/tools/jeux_icones.py. « * » = icone d'un fichier",
              "# dont l'extension n'est pas listee (sans elle : celle de l'actuel).", ""]
    for ty, exts in EXT.items():
        for e in exts.split():
            lignes.append(f"{e:<16}= Fic{ty}")
    lignes.append(f"{'*':<16}= FicDoc")
    return "\n".join(lignes) + "\n"


def main():
    # ── Trait ───────────────────────────────────────────────────────────────
    d = os.path.join(DATA, "trait")
    for nom, gl in MONO.items():
        ecrire(os.path.join(d, nom + ".svg"), trait(gl, "#FFFFFF"))
    for variante, K in TRAIT_COUL.items():
        v = os.path.join(d, variante)
        for ty, (gl, ck, _) in TYPES.items():
            ecrire(os.path.join(v, f"Fic{ty}.svg"), trait(gl, K[ck]))
        for nom, role in DOSSIERS.items():
            col = K[TRAIT_ROLE[role]]
            pointille = ' stroke-dasharray="2.2 1.6"' if role == "out" else ""
            ecrire(os.path.join(v, nom + ".svg"), trait("dossier", col, pointille))
            ecrire(os.path.join(v, nom + "Open.svg"), trait("dossierOuvert", col, pointille))
        ecrire(os.path.join(v, "FolderRoot.svg"), trait("jenga", K["acc"]))
        ecrire(os.path.join(v, "FolderRootOpen.svg"), trait("jenga", K["acc"]))
    ecrire(os.path.join(d, "icons.cfg"), icons_cfg("Trait"))
    ecrire(os.path.join(d, "extension.cfg"), """# NKCode — extension de donnees « Trait » (lue par Shell/NkJeuxIcones.h).
# Livree dans le catalogue (data/extensions) : on l'INSTALLE depuis la vue
# Extensions (copie dans %APPDATA%/NKCode/extensions/trait), on l y desinstalle.
id          = trait
contribue   = jeuIcones
nom         = Trait
description = Sobre, un seul trait teinte par le theme ; la couleur ne dit que le sens
auteur      = NKCode
version     = 1.0.0
# Les fichiers et dossiers existent en deux couleurs : sombre/ et clair/.
variantes   = sombre, clair
# Grille de 16 respectee : ne pas rogner les marges (les icones gardent leur taille).
rogner      = non
# Boutons et barres d'activite : dessines en BLANC, teintes par le theme.
mono        = NewFile, NewFolder, Sync, CollapseAll, EyeClosed, Eye, Filter, Files, SearchC, SourceControl, DebugAlt, LiveShare, Extensions, Graph, SettingsGear, Sparkle, CodeC, Home
""")
    # ── Pastilles (INTEGRE : toujours la, choisi par defaut) ────────────────
    d = os.path.join(DATA, "pastilles")
    for nom, gl in MONO.items():  # ses boutons : les glyphes du Trait, en blanc
        ecrire(os.path.join(d, nom + ".svg"), trait(gl, "#FFFFFF"))
    for ty, (gl, _, fond) in TYPES.items():
        encre = "#2B2100" if ty == "Json" else "#FFFFFF"
        ecrire(os.path.join(d, f"Fic{ty}.svg"), pastille(gl, fond, encre))
    for nom, role in DOSSIERS.items():
        ecrire(os.path.join(d, nom + ".svg"), dossier_plein(role, False))
        ecrire(os.path.join(d, nom + "Open.svg"), dossier_plein(role, True))
    ecrire(os.path.join(d, "FolderRoot.svg"), tuile_jenga())
    ecrire(os.path.join(d, "FolderRootOpen.svg"), tuile_jenga())
    ecrire(os.path.join(d, "icons.cfg"), icons_cfg("Pastilles"))
    ecrire(os.path.join(d, "extension.cfg"), """# NKCode — extension de donnees « Pastilles » (lue par Shell/NkJeuxIcones.h).
# INTEGREE : toujours installee, choisie par defaut, et repli de tous les autres
# jeux (une icone absente d'un jeu retombe sur Pastilles, puis sur les PNG de base).
id          = pastilles
contribue   = jeuIcones
integre     = oui
nom         = Pastilles
description = Colore, reperage rapide : une pastille pleine par type, dossiers pleins par role
auteur      = NKCode
version     = 1.0.0
rogner      = non
# Boutons et barres d'activite : les glyphes du Trait, BLANCS, teintes par le theme.
mono        = NewFile, NewFolder, Sync, CollapseAll, EyeClosed, Eye, Filter, Files, SearchC, SourceControl, DebugAlt, LiveShare, Extensions, Graph, SettingsGear, Sparkle, CodeC, Home
""")
    # ── Actuel : les PNG livres jusqu'ici, laisses ou ils sont ──────────────
    ecrire(os.path.join(DATA, "actuel", "extension.cfg"), """# NKCode — extension de donnees « Actuel » (lue par Shell/NkJeuxIcones.h).
# Les PNG d'avant le 01/10, LAISSES dans data/textures/icon (toute l'interface y
# puise ses icones, la documentation d'override y renvoie) : « @data/ » designe
# le dossier data/ de NKCode, ou que l'extension soit installee.
id          = actuel
contribue   = jeuIcones
nom         = Actuel
description = Les PNG d'aujourd'hui : boutons au trait, dossiers Material, logos de marque
auteur      = NKCode
version     = 1.0.0
dossier     = @data/textures/icon
icons.cfg   = @data/icons.cfg
# Icones BLANCHES livrees sans teinte : teintees par le theme (invisibles en clair sinon).
mono        = Jenga, Config
""")
    n = sum(len(f) for _, _, f in os.walk(DATA))
    print(f"[jeux_icones] {n} fichiers ecrits sous {DATA}")


if __name__ == "__main__":
    main()
