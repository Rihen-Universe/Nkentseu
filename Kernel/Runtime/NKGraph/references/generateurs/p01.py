# -*- coding: utf-8 -*-
"""Generateur de planche_01_noeuds.svg.

ECRIT LE 22/08, et il arrive APRES la planche : celle-ci a ete dessinee
avant que les generateurs existent, et elle etait donc la seule des cinq
que PERSONNE ne pouvait modifier -- alors que c'est la reference
principale. Ce fichier repare ca.

POURQUOI IL NE REDESSINE PAS AVEC gen.py, alors que p02/p03/p05 le font :
la planche 01 est ANTERIEURE a gen.py et n'a pas les memes defs (sa
grille est en #2b2b33, elle ignore les motifs damier/hachure/ciel). La
redessiner avec gen.py produirait une AUTRE planche -- et remplacer en
silence la reference de Rodolf par une variante serait exactement le
genre de perte qu'on traque ici.

CE QU'IL FAIT : il porte la planche DECOUPEE EN PANNEAUX, chacun editable
a part. Modifier le panneau 3 ne peut pas abimer le panneau 7.

CRITERE D'ACCEPTATION, verifie a chaque execution : le SVG produit est
IDENTIQUE A L'OCTET PRES a celui qui existait. Le script le CONTROLE et
refuse d'ecrire s'il diverge -- voir la fin du fichier.

CE QUE LA PLANCHE AFFIRME ET QUI EST DESORMAIS FAUX -- a corriger AU MOMENT
du passage a l echelle mesuree (voir ELEMENTS_A_DESSINER.md section A0), et
PAS avant : tant que p04.py est perime, corriger ici seul rendrait les cinq
planches incoherentes entre elles, ce qui est pire qu un ecart uniforme.

  ligne / panneau                      | ce qui est ecrit        | mesure
  -------------------------------------|-------------------------|--------------
  sous-titre de planche                | prises A CHEVAL         | prise de DONNEE
  titre du panneau 3                   | A CHEVAL SUR LE BORD    | entierement
  panneau 3, legende                   | 10 x 12 px              | DEHORS, collee
  cartouche point 2                    | 10 x 12, a cheval       | au bord :
  pied de planche                      | 10 x 12, a cheval       | 2,9 x 10,5 px

  Seule la prise d EXECUTION chevauche vraiment (33,7 px dans le corps) --
  et c est VOULU : " les prises d instruction doivent bien se marier au noeud ".
  Le sous-titre " prises d execution mariees au bord " est donc JUSTE ; c est
  la phrase sur les prises de DONNEE qui ne l est pas.

  Le rapport a retenir, exact : hauteur de prise = MOITIE de la hauteur
  d en-tete (63,3 / 126,6 = 0,500000).
"""
import io, os, hashlib

OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

# Empreinte de la planche APRES la correction des affirmations fausses
# (22/08 : la prise de donnee passe entierement dehors, textes et DESSIN
# corriges ensemble -- les corriger separement aurait fait se contredire la
# planche, ce qui est pire que de la laisser perimee).
# Elle est ici pour qu'une modification VOULUE se distingue d'une
# modification ACCIDENTELLE : si tu edites un panneau expres, l'empreinte
# ne correspond plus et le script te le DIT au lieu d'ecrire en silence.
EMPREINTE_ORIGINE = '791a238bf7bd279e2188edcfb9bc6054'

PARTIES = []

# --------------------------------------------------------------------------
# entete, defs, fond et titre de planche
# --------------------------------------------------------------------------
PARTIES.append(u'''<svg xmlns="http://www.w3.org/2000/svg" width="1680" height="1270" viewBox="0 0 1680 1270" font-family="Segoe UI, Inter, sans-serif">
<defs>
  <pattern id="grille" width="22" height="22" patternUnits="userSpaceOnUse">
    <circle cx="1.5" cy="1.5" r="1" fill="#2b2b33"/>
  </pattern>
  <linearGradient id="rampe" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0%" stop-color="#0A555F"/><stop offset="42%" stop-color="#3aa0a8"/>
    <stop offset="72%" stop-color="#F79A28"/><stop offset="100%" stop-color="#ffe9c2"/>
  </linearGradient>
  <filter id="ombre" x="-20%" y="-20%" width="140%" height="140%">
    <feDropShadow dx="0" dy="2" stdDeviation="3" flood-color="#000" flood-opacity="0.5"/>
  </filter>
  <!-- PRISE DE DONNEE : rectangle a cheval sur le bord -->
  <g id="pinD"><rect x="-5" y="-6" width="10" height="12" rx="2"/></g>
  <!-- PRISE D EXECUTION : rectangle a pointe, marie au bord -->
  <g id="pinX"><path d="M-5 -7h7l5 7-5 7h-7z"/></g>
</defs>

<rect width="1680" height="1270" fill="#17171b"/>
<rect width="1680" height="1270" fill="url(#grille)"/>

<text x="34" y="40" fill="#e8e8ee" font-size="21" font-weight="600">Planche 01 — nos nœuds dans le style de la référence principale</text>
<text x="34" y="62" fill="#8a8a96" font-size="12.5">v3 — coins quasi droits (5 en haut, 3 en bas) · prise de DONNÉE collée au bord, ENTIÈREMENT DEHORS · valeur saisie dans la rangée · prise d'EXÉCUTION à cheval, mariée au bord</text>

<!-- ============ 1 · CALCUL, avec + / - ============ -->
''')

# --------------------------------------------------------------------------
# 1 · NŒUD DE CALCUL — entrées ajoutables
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="34" y="106" fill="#F79A28" font-size="13" font-weight="600">1 · NŒUD DE CALCUL — entrées ajoutables</text>
<g filter="url(#ombre)">
  <rect x="40" y="120" width="252" height="176" rx="3" fill="#232329" stroke="#3a3a44"/>
  <path d="M40 123a3 3 0 0 1 3-3h246a3 3 0 0 1 3 3v21H40z" fill="#4a6b8a"/>
  <text x="52" y="139" fill="#eef2f6" font-size="12.5" font-weight="600">Math</text>
  <text x="278" y="139" fill="#9fb4c8" font-size="12" text-anchor="end">?</text>
  <rect x="40" y="144" width="252" height="2.5" fill="#0A555F"/>
  <rect x="50" y="154" width="232" height="20" rx="2" fill="#1b1b20" stroke="#33333c"/>
  <text x="58" y="168" fill="#cfd6de" font-size="11.5">Multiplier</text><text x="274" y="168" fill="#7a7a85" font-size="10" text-anchor="end">▾</text>
  <!-- entree 1 : prise rect + valeur DANS la rangee -->
  <use href="#pinD" x="35" y="192" fill="#5aa9d6"/>
  <text x="54" y="196" fill="#c8ccd4" font-size="11.5">Valeur</text>
  <rect x="150" y="184" width="96" height="17" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="157" y="196" fill="#cfd6de" font-size="10.5">0.500</text>
  <rect x="252" y="185" width="17" height="14" rx="2" fill="#2a4a63"/><text x="260" y="196" fill="#8fc7e8" font-size="8.5" text-anchor="middle">1.0</text>
  <text x="278" y="197" fill="#6f6f7b" font-size="12" text-anchor="middle">−</text>
  <use href="#pinD" x="35" y="218" fill="#5aa9d6"/>
  <text x="54" y="222" fill="#c8ccd4" font-size="11.5">Valeur</text>
  <rect x="150" y="210" width="96" height="17" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="157" y="222" fill="#cfd6de" font-size="10.5">2.000</text>
  <rect x="252" y="211" width="17" height="14" rx="2" fill="#2a4a63"/><text x="260" y="222" fill="#8fc7e8" font-size="8.5" text-anchor="middle">1.0</text>
  <text x="278" y="223" fill="#6f6f7b" font-size="12" text-anchor="middle">−</text>
  <!-- entree BRANCHEE : plus de champ -->
  <use href="#pinD" x="35" y="244" fill="#5aa9d6"/>
  <text x="54" y="248" fill="#c8ccd4" font-size="11.5">Valeur</text>
  <text x="246" y="248" fill="#6f6f7b" font-size="10" text-anchor="end">branchée</text>
  <rect x="252" y="237" width="17" height="14" rx="2" fill="#2a4a63"/><text x="260" y="248" fill="#8fc7e8" font-size="8.5" text-anchor="middle">1.0</text>
  <text x="278" y="249" fill="#6f6f7b" font-size="12" text-anchor="middle">−</text>
  <!-- ajouter -->
  <rect x="50" y="258" width="232" height="17" rx="2" fill="#1e1e24" stroke="#33333c" stroke-dasharray="3 3"/>
  <text x="166" y="270" fill="#7a7a85" font-size="10.5" text-anchor="middle">+  ajouter une entrée</text>
  <text x="246" y="292" fill="#c8ccd4" font-size="11.5" text-anchor="end">Résultat</text>
  <rect x="252" y="281" width="17" height="14" rx="2" fill="#2a4a63"/><text x="260" y="292" fill="#8fc7e8" font-size="8.5" text-anchor="middle">1.0</text>
  <use href="#pinD" x="297" y="288" fill="#5aa9d6"/>
</g>
<text x="40" y="318" fill="#6f6f7b" font-size="11">une entrée BRANCHÉE perd son champ de saisie · le − la retire, le + en ajoute</text>

<!-- ============ 2 · INSTRUCTION ============ -->
''')

# --------------------------------------------------------------------------
# 2 · NŒUD D'INSTRUCTION
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="368" y="106" fill="#F79A28" font-size="13" font-weight="600">2 · NŒUD D'INSTRUCTION</text>
<g filter="url(#ombre)">
  <rect x="374" y="120" width="266" height="150" rx="3" fill="#232329" stroke="#3a3a44"/>
  <path d="M374 123a3 3 0 0 1 3-3h260a3 3 0 0 1 3 3v21H374z" fill="#8a5a2a"/>
  <text x="386" y="139" fill="#f6ecdf" font-size="12.5" font-weight="600">Si / Sinon</text>
  <text x="626" y="139" fill="#d5b183" font-size="12" text-anchor="end">?</text>
  <rect x="374" y="144" width="266" height="2.5" fill="#F79A28"/>
  <!-- prises exec : rectangle a pointe, epouse le bord -->
  <use href="#pinX" x="374" y="164" fill="#F79A28"/>
  <text x="392" y="168" fill="#e6d2b4" font-size="11.5">Entrer</text>
  <text x="616" y="168" fill="#e6d2b4" font-size="11.5" text-anchor="end">Vrai</text>
  <use href="#pinX" x="640" y="164" fill="#F79A28"/>
  <text x="616" y="190" fill="#e6d2b4" font-size="11.5" text-anchor="end">Faux</text>
  <use href="#pinX" x="640" y="186" fill="#F79A28"/>
  <line x1="382" y1="204" x2="632" y2="204" stroke="#33333c"/>
  <use href="#pinD" x="369" y="226" fill="#b05a8a"/>
  <text x="390" y="230" fill="#c8ccd4" font-size="11.5">Condition</text>
  <rect x="540" y="219" width="60" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="547" y="231" fill="#cfd6de" font-size="10.5">faux ▾</text>
  <rect x="606" y="220" width="20" height="14" rx="2" fill="#5c2a45"/><text x="616" y="231" fill="#e79ac4" font-size="8.5" text-anchor="middle">V/F</text>
  <text x="390" y="256" fill="#7a7a85" font-size="10.5">l'exécution est au-dessus du filet</text>
</g>
<text x="374" y="292" fill="#6f6f7b" font-size="11">la prise d'exécution est un rectangle À POINTE — elle épouse le bord,</text>
<text x="374" y="308" fill="#6f6f7b" font-size="11">elle CHEVAUCHE le corps — contrairement à la prise de donnée</text>

<!-- ============ 3 · DETAIL DES PRISES ============ -->
''')

# --------------------------------------------------------------------------
# 3 · LES PRISES — la DONNÉE reste DEHORS
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="700" y="106" fill="#F79A28" font-size="13" font-weight="600">3 · LES PRISES — la DONNÉE reste DEHORS</text>
<g filter="url(#ombre)">
  <rect x="706" y="122" width="150" height="112" rx="3" fill="#232329" stroke="#3a3a44"/>
  <rect x="706" y="122" width="150" height="21" rx="3" fill="#2a6b6b"/>
  <text x="716" y="137" fill="#e2f2f2" font-size="11.5" font-weight="600">détail</text>
  <rect x="706" y="143" width="150" height="2.5" fill="#0A555F"/>
  <use href="#pinD" x="701" y="164" fill="#5aa9d6"/><text x="722" y="168" fill="#c8ccd4" font-size="11">donnée</text>
  <use href="#pinX" x="706" y="190" fill="#F79A28"/><text x="722" y="194" fill="#c8ccd4" font-size="11">exécution</text>
  <g><rect x="701" y="210" width="10" height="12" rx="2" fill="none" stroke="#5aa9d6" stroke-width="2"/></g>
  <text x="722" y="220" fill="#c8ccd4" font-size="11">tableau (creuse)</text>
</g>
<text x="706" y="256" fill="#6f6f7b" font-size="11">ratio 1:3,7, rayon 2 · hauteur = MOITIÉ de l'en-tête</text>
<text x="706" y="272" fill="#6f6f7b" font-size="11">la couleur EST le type · le fil part de son centre</text>

<!-- ============ 4 · PASTILLES ============ -->
''')

# --------------------------------------------------------------------------
# 4 · PASTILLES DE TYPE — couleur ET glyphe
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="700" y="310" fill="#F79A28" font-size="13" font-weight="600">4 · PASTILLES DE TYPE — couleur ET glyphe</text>
<g font-size="8.5" text-anchor="middle">
  <rect x="706" y="322" width="19" height="15" rx="2" fill="#2a4a63"/><text x="715" y="333" fill="#8fc7e8">1.0</text>
  <rect x="733" y="322" width="19" height="15" rx="2" fill="#2a4463"/><text x="742" y="333" fill="#9fb0e8">123</text>
  <rect x="760" y="322" width="19" height="15" rx="2" fill="#5c2a45"/><text x="769" y="333" fill="#e79ac4">V/F</text>
  <rect x="787" y="322" width="19" height="15" rx="2" fill="#2a5c46"/><text x="796" y="333" fill="#8fe0b4">XY</text>
  <rect x="814" y="322" width="19" height="15" rx="2" fill="#2a6b3f"/><text x="823" y="333" fill="#9fe8a8">XYZ</text>
  <rect x="841" y="322" width="19" height="15" rx="2" fill="#6b5a2a"/><text x="850" y="333" fill="#e8d08f">RGB</text>
  <rect x="868" y="322" width="19" height="15" rx="2" fill="#5a3a6b"/><text x="877" y="333" fill="#cfa8e8">txt</text>
  <rect x="895" y="322" width="19" height="15" rx="2" fill="#2a5c5c"/><text x="904" y="333" fill="#8fd8d8">bsdf</text>
  <rect x="922" y="322" width="19" height="15" rx="2" fill="#6b3a2a"/><text x="931" y="333" fill="#e8a88f">obj</text>
  <rect x="949" y="322" width="19" height="15" rx="2" fill="#3a3a44"/><text x="958" y="333" fill="#b8b8c4">?</text>
</g>

<!-- ============ 5 · DONNEE COMPOSEE, PLIEE / DEPLIEE ============ -->
''')

# --------------------------------------------------------------------------
# 5 · DONNÉE COMPOSÉE — pliée, puis dépliée
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="1000" y="106" fill="#F79A28" font-size="13" font-weight="600">5 · DONNÉE COMPOSÉE — pliée, puis dépliée</text>
<g filter="url(#ombre)">
  <rect x="1006" y="120" width="248" height="60" rx="3" fill="#232329" stroke="#3a3a44"/>
  <rect x="1006" y="120" width="248" height="21" rx="3" fill="#0A555F"/>
  <text x="1016" y="135" fill="#dff0f2" font-size="11.5" font-weight="600">Mapping</text>
  <rect x="1006" y="141" width="248" height="2.5" fill="#0A555F"/>
  <use href="#pinD" x="1001" y="162" fill="#9fe8a8"/>
  <text x="1022" y="160" fill="#c8ccd4" font-size="11.5">Position</text>
  <text x="1022" y="172" fill="#6f6f7b" font-size="9.5">▸ déplier</text>
  <rect x="1104" y="154" width="42" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="1110" y="166" fill="#cfd6de" font-size="10">0.0</text>
  <rect x="1150" y="154" width="42" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="1156" y="166" fill="#cfd6de" font-size="10">1.0</text>
  <rect x="1196" y="154" width="42" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="1202" y="166" fill="#cfd6de" font-size="10">0.0</text>
</g>
<g filter="url(#ombre)">
  <rect x="1006" y="196" width="248" height="98" rx="3" fill="#232329" stroke="#3a3a44"/>
  <rect x="1006" y="196" width="248" height="21" rx="3" fill="#0A555F"/>
  <text x="1016" y="211" fill="#dff0f2" font-size="11.5" font-weight="600">Mapping</text>
  <rect x="1006" y="217" width="248" height="2.5" fill="#0A555F"/>
  <use href="#pinD" x="1001" y="238" fill="#9fe8a8"/>
  <text x="1022" y="242" fill="#c8ccd4" font-size="11.5">Position</text>
  <text x="1022" y="242" fill="#c8ccd4" font-size="11.5"> </text>
  <text x="1240" y="242" fill="#6f6f7b" font-size="9.5" text-anchor="end">▾ replier</text>
  <text x="1040" y="262" fill="#a8acb6" font-size="11">X</text>
  <rect x="1104" y="252" width="134" height="15" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="1110" y="263" fill="#cfd6de" font-size="10">0.0</text>
  <text x="1040" y="278" fill="#a8acb6" font-size="11">Y</text>
  <rect x="1104" y="268" width="134" height="15" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="1110" y="279" fill="#cfd6de" font-size="10">1.0</text>
  <text x="1040" y="294" fill="#a8acb6" font-size="11">Z</text>
  <rect x="1104" y="284" width="134" height="15" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="1110" y="295" fill="#cfd6de" font-size="10">0.0</text>
</g>
<text x="1006" y="316" fill="#6f6f7b" font-size="11">⚠ UNE seule prise dans les deux cas — c'est de l'AFFICHAGE.</text>
<text x="1006" y="332" fill="#6f6f7b" font-size="11">Séparer en trois PRISES est autre chose (voir CATALOGUE §5ter)</text>

<!-- ============ 6 · COLORRAMP AVEC PRISES PAR ARRET ============ -->
''')

# --------------------------------------------------------------------------
# 6 · COLORRAMP — les arrêts SONT des prises, ajoutables et retirables
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="34" y="368" fill="#F79A28" font-size="13" font-weight="600">6 · COLORRAMP — les arrêts SONT des prises, ajoutables et retirables</text>
<g filter="url(#ombre)">
  <rect x="40" y="382" width="300" height="212" rx="3" fill="#232329" stroke="#3a3a44"/>
  <rect x="40" y="382" width="300" height="21" rx="3" fill="#8a6b2a"/>
  <text x="52" y="397" fill="#f6eddc" font-size="12.5" font-weight="600">ColorRamp</text>
  <text x="326" y="397" fill="#d8c08a" font-size="12" text-anchor="end">?</text>
  <rect x="40" y="403" width="300" height="2.5" fill="#0A555F"/>
  <use href="#pinD" x="35" y="424" fill="#5aa9d6"/>
  <text x="56" y="428" fill="#c8ccd4" font-size="11.5">Facteur</text>
  <rect x="216" y="417" width="80" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="222" y="429" fill="#cfd6de" font-size="10">0.500</text>
  <rect x="302" y="418" width="17" height="14" rx="2" fill="#2a4a63"/><text x="310" y="429" fill="#8fc7e8" font-size="8.5" text-anchor="middle">1.0</text>
  <!-- barre -->
  <rect x="56" y="442" width="264" height="18" rx="2" fill="url(#rampe)" stroke="#33333c"/>
  <g stroke="#17171b"><path d="M60 460l5-7 5 7z" fill="#e8e8ee"/><path d="M170 460l5-7 5 7z" fill="#e8e8ee"/><path d="M262 460l5-7 5 7z" fill="#F79A28" stroke="#fff"/></g>
  <line x1="52" y1="470" x2="328" y2="470" stroke="#33333c"/>
  <!-- arrets = prises -->
  <use href="#pinD" x="35" y="486" fill="#5aa9d6"/>
  <text x="56" y="490" fill="#c8ccd4" font-size="11">arrêt 1</text>
  <rect x="118" y="479" width="56" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="124" y="491" fill="#cfd6de" font-size="10">0.00</text>
  <use href="#pinD" x="35" y="504" fill="#d8c08a"/>
  <rect x="180" y="479" width="112" height="16" rx="2" fill="#0A555F" stroke="#33333c"/>
  <text x="298" y="491" fill="#6f6f7b" font-size="12">−</text>
  <text x="56" y="508" fill="#c8ccd4" font-size="11">arrêt 2</text>
  <rect x="118" y="497" width="56" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="124" y="509" fill="#cfd6de" font-size="10">0.45</text>
  <rect x="180" y="497" width="112" height="16" rx="2" fill="#3aa0a8" stroke="#33333c"/>
  <text x="298" y="509" fill="#6f6f7b" font-size="12">−</text>
  <use href="#pinD" x="35" y="522" fill="#5aa9d6"/>
  <text x="56" y="526" fill="#c8ccd4" font-size="11">arrêt 3</text>
  <rect x="118" y="515" width="56" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="124" y="527" fill="#cfd6de" font-size="10">0.72</text>
  <use href="#pinD" x="35" y="540" fill="#d8c08a"/>
  <rect x="180" y="515" width="112" height="16" rx="2" fill="#F79A28" stroke="#fff" stroke-width="1.5"/>
  <text x="298" y="527" fill="#6f6f7b" font-size="12">−</text>
  <rect x="56" y="538" width="236" height="17" rx="2" fill="#1e1e24" stroke="#33333c" stroke-dasharray="3 3"/>
  <text x="174" y="550" fill="#7a7a85" font-size="10.5" text-anchor="middle">+  ajouter un arrêt</text>
  <rect x="56" y="562" width="110" height="17" rx="2" fill="#1b1b20" stroke="#33333c"/>
  <text x="64" y="574" fill="#cfd6de" font-size="10.5">Linéaire ▾</text>
  <text x="292" y="574" fill="#c8ccd4" font-size="11.5" text-anchor="end">Couleur</text>
  <rect x="298" y="563" width="20" height="14" rx="2" fill="#6b5a2a"/><text x="308" y="574" fill="#e8d08f" font-size="8.5" text-anchor="middle">RGB</text>
  <use href="#pinD" x="345" y="570" fill="#d8c08a"/>
</g>
<text x="40" y="616" fill="#6f6f7b" font-size="11">chaque arrêt = DEUX prises (sa position, sa couleur) — donc pilotables par un fil</text>
<text x="40" y="632" fill="#6f6f7b" font-size="11">la barre reste, mais elle devient un APERÇU manipulable, plus la seule interface</text>

<!-- ============ 7 · PRINCIPLED ============ -->
''')

# --------------------------------------------------------------------------
# 7 · VINGT ENTRÉES — sections repliables
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="380" y="368" fill="#F79A28" font-size="13" font-weight="600">7 · VINGT ENTRÉES — sections repliables</text>
<g filter="url(#ombre)">
  <rect x="386" y="382" width="284" height="300" rx="3" fill="#232329" stroke="#3a3a44"/>
  <rect x="386" y="382" width="284" height="21" rx="3" fill="#2a6b6b"/>
  <text x="398" y="397" fill="#e2f2f2" font-size="12.5" font-weight="600">Principled BSDF</text>
  <text x="656" y="397" fill="#8fbcbc" font-size="12" text-anchor="end">?</text>
  <rect x="386" y="403" width="284" height="2.5" fill="#0A555F"/>
  <text x="628" y="428" fill="#c8ccd4" font-size="11.5" text-anchor="end">Surface</text>
  <rect x="634" y="417" width="22" height="14" rx="2" fill="#2a5c5c"/><text x="645" y="428" fill="#8fd8d8" font-size="8.5" text-anchor="middle">bsdf</text>
  <use href="#pinD" x="675" y="424" fill="#7fd0d0"/>
  <use href="#pinD" x="381" y="452" fill="#d8c08a"/><text x="402" y="456" fill="#c8ccd4" font-size="11.5">Couleur de base</text>
  <rect x="546" y="445" width="110" height="16" rx="2" fill="#c9b28f" stroke="#33333c"/>
  <use href="#pinD" x="381" y="476" fill="#5aa9d6"/><text x="402" y="480" fill="#c8ccd4" font-size="11.5">Métallique</text>
  <rect x="546" y="469" width="110" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="552" y="481" fill="#cfd6de" font-size="10">0.000</text>
  <use href="#pinD" x="381" y="500" fill="#5aa9d6"/><text x="402" y="504" fill="#c8ccd4" font-size="11.5">Rugosité</text>
  <rect x="546" y="493" width="110" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="552" y="505" fill="#cfd6de" font-size="10">0.500</text>
  <use href="#pinD" x="381" y="524" fill="#9fe8a8"/><text x="402" y="528" fill="#c8ccd4" font-size="11.5">Normale</text>
  <text x="656" y="528" fill="#6f6f7b" font-size="10" text-anchor="end">défaut</text>
  <line x1="396" y1="542" x2="660" y2="542" stroke="#33333c"/>
  <text x="402" y="562" fill="#a8acb6" font-size="11.5">▸  Diffusion sous-cutanée</text>
  <text x="402" y="584" fill="#a8acb6" font-size="11.5">▸  Spéculaire</text>
  <text x="402" y="606" fill="#a8acb6" font-size="11.5">▾  Émission</text>
  <use href="#pinD" x="381" y="628" fill="#d8c08a"/><text x="412" y="632" fill="#c8ccd4" font-size="11.5">Couleur</text>
  <rect x="546" y="621" width="110" height="16" rx="2" fill="#1a1a1f" stroke="#33333c"/>
  <use href="#pinD" x="381" y="652" fill="#5aa9d6"/><text x="412" y="656" fill="#c8ccd4" font-size="11.5">Intensité</text>
  <rect x="546" y="645" width="110" height="16" rx="2" fill="#1b1b20" stroke="#33333c"/><text x="552" y="657" fill="#cfd6de" font-size="10">1.000</text>
  <text x="402" y="676" fill="#a8acb6" font-size="11.5">▸  Voile · Vernis · Film mince</text>
</g>
<text x="386" y="704" fill="#6f6f7b" font-size="11">⚠ une section repliée qui contient une prise BRANCHÉE reste ouverte —</text>
<text x="386" y="720" fill="#6f6f7b" font-size="11">sinon le fil pointerait vers rien</text>

<!-- ============ 8 · FILS ============ -->
''')

# --------------------------------------------------------------------------
# 8 · LES FILS
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="706" y="368" fill="#F79A28" font-size="13" font-weight="600">8 · LES FILS</text>
<g fill="none" stroke-width="2">
  <path d="M716 398c40 0 40 -20 80 -20" stroke="#5aa9d6"/>
  <text x="812" y="382" fill="#8a8a96" font-size="11">donnée — couleur du TYPE</text>
  <path d="M716 428c40 0 40 -20 80 -20" stroke="#F79A28" stroke-width="3.5"/>
  <text x="812" y="412" fill="#8a8a96" font-size="11">exécution — plus ÉPAIS, toujours orange</text>
  <path d="M716 456c40 0 40 -20 80 -20" stroke="#5aa9d6" stroke-width="1.4"/>
  <path d="M716 462c40 0 40 -20 80 -20" stroke="#5aa9d6" stroke-width="1.4"/>
  <text x="812" y="442" fill="#8a8a96" font-size="11">tableau — DOUBLÉ</text>
  <path d="M716 492c40 0 40 -20 80 -20" stroke="#7a7a85" stroke-dasharray="5 4"/>
  <text x="812" y="472" fill="#8a8a96" font-size="11">en cours de tirage — pointillé</text>
</g>
<text x="706" y="524" fill="#c8ccd4" font-size="11.5">pendant le tirage :</text>
<g>
  <use href="#pinD" x="716" y="546" fill="#5aa9d6"/><rect x="709" y="538" width="14" height="16" rx="3" fill="none" stroke="#e8e8ee" stroke-width="1.5"/>
  <text x="736" y="550" fill="#9fe8a8" font-size="11">compatible — halo clair</text>
  <use href="#pinD" x="716" y="572" fill="#3a3a44"/>
  <text x="736" y="576" fill="#6f6f7b" font-size="11">incompatible — éteinte</text>
  <use href="#pinD" x="716" y="598" fill="#5aa9d6" opacity="0.35"/>
  <text x="736" y="602" fill="#6f6f7b" font-size="11">convertible — demi-teinte</text>
</g>

<!-- ============ 9 · ERREUR ============ -->
''')

# --------------------------------------------------------------------------
# 9 · NŒUD EN ERREUR
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="706" y="646" fill="#F79A28" font-size="13" font-weight="600">9 · NŒUD EN ERREUR</text>
<g filter="url(#ombre)">
  <rect x="712" y="660" width="252" height="92" rx="3" fill="#2a2024" stroke="#c4483a" stroke-width="1.6"/>
  <rect x="712" y="660" width="252" height="21" rx="3" fill="#8a3a30"/>
  <text x="724" y="675" fill="#f6dedb" font-size="12.5" font-weight="600">Image Texture</text>
  <text x="950" y="675" fill="#f0b0a8" font-size="13" text-anchor="end">!</text>
  <rect x="712" y="681" width="252" height="2.5" fill="#c4483a"/>
  <text x="724" y="702" fill="#f0b0a8" font-size="10.5">fichier introuvable :</text>
  <text x="724" y="718" fill="#f0b0a8" font-size="10.5">textures/mur_albedo.png</text>
  <text x="724" y="740" fill="#8a8a96" font-size="9.5">la raison est LISIBLE sans survol</text>
</g>

<!-- ============ 10 · CADRE ============ -->
''')

# --------------------------------------------------------------------------
# 10 · CADRE DE GROUPE ET SÉLECTION
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="1000" y="368" fill="#F79A28" font-size="13" font-weight="600">10 · CADRE DE GROUPE ET SÉLECTION</text>
<g>
  <rect x="1006" y="384" width="330" height="168" rx="4" fill="#F79A28" opacity="0.07"/>
  <rect x="1006" y="384" width="330" height="168" rx="4" fill="none" stroke="#F79A28" stroke-opacity="0.45" stroke-width="1.4"/>
  <text x="1018" y="402" fill="#F79A28" font-size="11.5" font-weight="600">Habillage du mur</text>
  <g filter="url(#ombre)">
    <rect x="1026" y="414" width="126" height="52" rx="3" fill="#232329" stroke="#e8e8ee" stroke-width="1.6"/>
    <rect x="1026" y="414" width="126" height="17" rx="3" fill="#8a6b2a"/>
    <text x="1036" y="427" fill="#f6eddc" font-size="10.5" font-weight="600">Noise</text>
    <rect x="1026" y="431" width="126" height="2" fill="#0A555F"/>
    <use href="#pinD" x="1021" y="450" fill="#9fe8a8"/><use href="#pinD" x="1157" y="450" fill="#5aa9d6"/>
  </g>
  <g filter="url(#ombre)">
    <rect x="1186" y="414" width="126" height="52" rx="3" fill="#232329" stroke="#e8e8ee" stroke-width="1.6"/>
    <rect x="1186" y="414" width="126" height="17" rx="3" fill="#8a6b2a"/>
    <text x="1196" y="427" fill="#f6eddc" font-size="10.5" font-weight="600">ColorRamp</text>
    <rect x="1186" y="431" width="126" height="2" fill="#0A555F"/>
    <use href="#pinD" x="1181" y="450" fill="#5aa9d6"/><use href="#pinD" x="1317" y="450" fill="#d8c08a"/>
  </g>
  <path d="M1157 450h24" fill="none" stroke="#5aa9d6" stroke-width="2"/>
  <g filter="url(#ombre)">
    <rect x="1026" y="486" width="126" height="42" rx="3" fill="#232329" stroke="#3a3a44"/>
    <rect x="1026" y="486" width="126" height="17" rx="3" fill="#4a6b8a"/>
    <text x="1036" y="499" fill="#eef2f6" font-size="10.5" font-weight="600">Math</text>
    <rect x="1026" y="503" width="126" height="2" fill="#0A555F"/>
  </g>
</g>
<text x="1006" y="572" fill="#6f6f7b" font-size="11">sélection = liseré CLAIR de 1,6 px · le cadre passe DERRIÈRE et se déplace</text>
<text x="1006" y="588" fill="#6f6f7b" font-size="11">avec son contenu</text>

<!-- ============ 11 · DEZOOM ============ -->
''')

# --------------------------------------------------------------------------
# 11 · DÉZOOM — trois paliers
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="1000" y="626" fill="#F79A28" font-size="13" font-weight="600">11 · DÉZOOM — trois paliers</text>
<g filter="url(#ombre)">
  <rect x="1006" y="640" width="116" height="50" rx="3" fill="#232329" stroke="#3a3a44"/>
  <rect x="1006" y="640" width="116" height="17" rx="3" fill="#4a6b8a"/>
  <text x="1015" y="653" fill="#eef2f6" font-size="10" font-weight="600">Math</text>
  <rect x="1006" y="657" width="116" height="2" fill="#0A555F"/>
  <use href="#pinD" x="1001" y="674" fill="#5aa9d6"/><text x="1020" y="678" fill="#c8ccd4" font-size="9">Valeur</text>
  <rect x="1064" y="668" width="40" height="12" rx="2" fill="#1b1b20" stroke="#33333c"/>
  <use href="#pinD" x="1122" y="674" fill="#5aa9d6"/>
</g>
<text x="1006" y="706" fill="#6f6f7b" font-size="10">100 % — tout</text>
<g filter="url(#ombre)">
  <rect x="1146" y="640" width="84" height="38" rx="3" fill="#232329" stroke="#3a3a44"/>
  <rect x="1146" y="640" width="84" height="15" rx="3" fill="#4a6b8a"/>
  <text x="1154" y="651" fill="#eef2f6" font-size="9" font-weight="600">Math</text>
  <rect x="1146" y="655" width="84" height="2" fill="#0A555F"/>
  <use href="#pinD" x="1146" y="668" fill="#5aa9d6"/><use href="#pinD" x="1230" y="668" fill="#5aa9d6"/>
</g>
<text x="1146" y="694" fill="#6f6f7b" font-size="10">55 % — les valeurs</text>
<text x="1146" y="708" fill="#6f6f7b" font-size="10">disparaissent</text>
<g filter="url(#ombre)">
  <rect x="1254" y="640" width="52" height="20" rx="2" fill="#4a6b8a" stroke="#3a3a44"/>
  <rect x="1254" y="656" width="52" height="4" fill="#0A555F"/>
</g>
<text x="1254" y="680" fill="#6f6f7b" font-size="10">25 % — un</text>
<text x="1254" y="694" fill="#6f6f7b" font-size="10">rectangle de la</text>
<text x="1254" y="708" fill="#6f6f7b" font-size="10">CATÉGORIE</text>

<!-- ============ 12 · CATEGORIES ============ -->
''')

# --------------------------------------------------------------------------
# 12 · EN-TÊTE = CATÉGORIE
# --------------------------------------------------------------------------
PARTIES.append(u'''<text x="1360" y="368" fill="#F79A28" font-size="13" font-weight="600">12 · EN-TÊTE = CATÉGORIE</text>
<g font-size="10.5">
  <rect x="1366" y="384" width="150" height="19" rx="2" fill="#2a6b6b"/><text x="1376" y="397" fill="#e2f2f2">surface / BSDF</text>
  <rect x="1366" y="408" width="150" height="19" rx="2" fill="#8a6b2a"/><text x="1376" y="421" fill="#f6eddc">texture · couleur</text>
  <rect x="1366" y="432" width="150" height="19" rx="2" fill="#4a6b8a"/><text x="1376" y="445" fill="#eef2f6">outillage · maths</text>
  <rect x="1366" y="456" width="150" height="19" rx="2" fill="#0A555F"/><text x="1376" y="469" fill="#dff0f2">entrée / contexte</text>
  <rect x="1366" y="480" width="150" height="19" rx="2" fill="#8a5a2a"/><text x="1376" y="493" fill="#f6ecdf">flot · instruction</text>
  <rect x="1366" y="504" width="150" height="19" rx="2" fill="#6b4a8a"/><text x="1376" y="517" fill="#eee2f6">variable · objet</text>
  <rect x="1366" y="528" width="150" height="19" rx="2" fill="#F79A28"/><text x="1376" y="541" fill="#2a1a08">SORTIE (unique)</text>
  <rect x="1366" y="552" width="150" height="19" rx="2" fill="#8a3a30"/><text x="1376" y="565" fill="#f6dedb">erreur</text>
</g>
<text x="1366" y="592" fill="#6f6f7b" font-size="11">couleurs à valider —</text>
<text x="1366" y="608" fill="#6f6f7b" font-size="11">ce sont des propositions</text>

<!-- ============ NOTES ============ -->
<g>
  <rect x="34" y="770" width="1610" height="128" rx="4" fill="#1d1d22" stroke="#33333c"/>
  <text x="52" y="794" fill="#F79A28" font-size="13" font-weight="600">⚠ Ce que la v2 corrige, d'après ta relecture de la référence</text>
  <text x="52" y="818" fill="#c8ccd4" font-size="12">1 · Les coins sont quasi droits — 5 px en haut de l'en-tête, 3 px en bas du corps. Sur la référence, l'arrondi est presque imperceptible.</text>
  <text x="52" y="840" fill="#c8ccd4" font-size="12">2 · Les prises sont des RECTANGLES collés au bord, pas des ronds dans le corps. La DONNÉE reste entièrement DEHORS ; seule l'EXÉCUTION chevauche.</text>
  <text x="52" y="862" fill="#c8ccd4" font-size="12">3 · La prise d'exécution est un rectangle À POINTE : même famille de forme, donc elle épouse le bord au lieu de flotter à côté.</text>
  <text x="52" y="884" fill="#c8ccd4" font-size="12">4 · Une entrée non branchée porte SA VALEUR dans la rangée. Une donnée composée se déplie ; une entrée branchée perd son champ.</text>
</g>
<g>
  <rect x="34" y="912" width="1610" height="82" rx="4" fill="#1d1d22" stroke="#3a3a2a"/>
  <text x="52" y="936" fill="#F79A28" font-size="13" font-weight="600">Deux ajouts que tu as demandés</text>
  <text x="52" y="958" fill="#c8ccd4" font-size="12">5 · Le nœud de calcul porte un + et un − : on ajoute et on retire des entrées. Utile pour Math, Mix, Combine — et indispensable au ColorRamp.</text>
  <text x="52" y="980" fill="#c8ccd4" font-size="12">6 · Les arrêts du ColorRamp SONT des prises — chacun expose sa position ET sa couleur, donc chacun peut être piloté par un fil ou par le code.</text>
</g>
<g>
  <rect x="34" y="1008" width="1610" height="82" rx="4" fill="#1d1d22" stroke="#3a2a2a"/>
  <text x="52" y="1032" fill="#c4483a" font-size="13" font-weight="600">🔴 Toujours pas tranché</text>
  <text x="52" y="1054" fill="#c8ccd4" font-size="12">Le dictionnaire · le nœud de groupe replié avec ses propres entrées/sorties · le survol · la minicarte · le sens de lecture (cette planche est horizontale).</text>
  <text x="52" y="1076" fill="#c8ccd4" font-size="12">Et les couleurs de catégorie du panneau 12 sont des propositions — c'est le point le plus facile à corriger et le plus visible.</text>
</g>

<text x="34" y="1128" fill="#6f6f7b" font-size="11">Fond #17171b · corps #232329 · filet #33333c · texte #c8ccd4, secondaire #7a7a85 · orange Rihen #F79A28 · pétrole Rihen #0A555F</text>
<text x="34" y="1148" fill="#6f6f7b" font-size="11">Coins 5 px en haut, 3 px en bas · prise de donnée rayon 2, collée au bord et entièrement dehors · pastilles h=15 px · grille de points, pas 22 px</text>
<text x="34" y="1168" fill="#6f6f7b" font-size="11">⚠ PLANCHE D’ÉTUDE — les prises y sont dessinées à environ 2,1 × leur échelle relative pour rester lisibles. Les RATIOS de la spécification font foi, jamais les pixels de cette planche.</text>
''')

# --------------------------------------------------------------------------
# fermeture
# --------------------------------------------------------------------------
PARTIES.append(u'''</svg>
''')

svg = u''.join(PARTIES)

chemin = os.path.join(OUT, 'planche_01_noeuds.svg')
empreinte = hashlib.md5(svg.encode('utf-8')).hexdigest()
if empreinte == EMPREINTE_ORIGINE:
    print('planche 01 : identique a l origine (aucune modification)')
else:
    print('planche 01 : MODIFIEE -- empreinte %s (origine %s)'
          % (empreinte, EMPREINTE_ORIGINE))
    print('  -> voulu ? si oui, mets a jour EMPREINTE_ORIGINE ci-dessus.')

# Fins de ligne : les quatre autres planches sont en CRLF sur le disque,
# parce que gen.ecrire ouvre en mode TEXTE et que Windows y traduit. Ecrire en
# LF ici aurait change 360 octets sans changer un pixel -- un diff enorme pour
# rien, et le genre d ecart qui fait perdre du temps a chercher une difference
# de dessin qui n existe pas. On restitue donc le CRLF explicitement.
#
# On encode AVANT d ouvrir : une erreur d encodage doit echouer sans avoir
# touche au fichier -- open(.., 'w') tronque des l ouverture, et cette
# troncature-la a deja detruit un fichier sur ce chantier.
donnees = svg.replace(chr(10), chr(13) + chr(10)).encode('utf-8')
open(chemin, 'wb').write(donnees)
print('ecrit %s (%d octets)' % (chemin, len(donnees)))
