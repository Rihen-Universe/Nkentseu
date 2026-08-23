# REPARE le 22/08 : l ecart etait de 5 lignes sur 363 (hauteur 1060 au lieu de
# 1230, plus deux marqueurs decales de 4 px). Il reproduit desormais
# planche_04_etats.svg A L OCTET PRES. L etiquette « PERIME » qui figurait ici
# laissait croire a une derive structurelle et a immobilise le script des
# semaines -- voir la famille « une etiquette qui exagere un defaut le rend
# immortel » dans LISEZMOI.md.
# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from gen import *

W,H=1780,1250
s=head(W,H,u'Planche 04 \u2014 tous les \u00e9tats',
       u'Erreur \u00b7 avertissement \u00b7 d\u00e9sactiv\u00e9 \u00b7 survol\u00e9 \u00b7 s\u00e9lectionn\u00e9 \u00b7 ACTIF \u00b7 inconnu \u00b7 indisponible \u00b7 le tirage d\u2019un fil \u00b7 le graphe invalide \u00b7 les trois paliers de d\u00e9zoom')

def lab(x,y,n,titre,note=''):
    return panneau(u'%s_%s'%(n,titre))+tt(x,y,u'%s \u00b7 %s'%(n,titre),ORANGE,12.5,'600')+tt(x,y+16,note,TXT3,10)

ROWS=[{'lab':u'Valeur A','coul':FAM['nombre'],'plein':False,'val':'0.500','glyphe':'1.0'},
      {'lab':u'Valeur B','coul':FAM['nombre'],'plein':True,'branchee':True,'glyphe':'1.0'},
      {'lab':u'R\u00e9sultat','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'1.0'}]
R1=118
# 1 normal
s+=lab(44,R1,'1',u'NORMAL',u'la r\u00e9f\u00e9rence de comparaison')
n,h=noeud(44,R1+28,270,ROWS,u'Multiplier',u'Maths',CAT['outil']); s+=n
# 2 survole
s+=lab(350,R1,'2',u'SURVOL\u00c9',u'seul le FILET s\u2019\u00e9claircit \u2014 jamais le corps')
n,h=noeud(350,R1+28,270,ROWS,u'Multiplier',u'Maths',CAT['outil']); s+=n
s+='<rect x="349" y="%s" width="272" height="%s" fill="none" stroke="#5A5A68" stroke-width="1.6"/>\n'%(R1+27,h+2)
s+=tt(350,R1+28+h+18,u'teinter le corps ferait CLIGNOTER le canevas',TXT3,10)
s+=tt(350,R1+28+h+31,u'quand la souris le traverse.',TXT3,10)
# 3 selectionne
s+=lab(656,R1,'3',u'S\u00c9LECTIONN\u00c9',u'bordure 1,6 px sur TOUT le n\u0153ud \u2014 VU 3 fois')
n,h=noeud(656,R1+28,270,ROWS,u'Multiplier',u'Maths',CAT['outil'],selection=ORANGE); s+=n
s+=tt(656,R1+28+h+18,u'trois r\u00e9f\u00e9rences, trois COULEURS diff\u00e9rentes,',TXT3,10)
s+=tt(656,R1+28+h+31,u'une seule convention : la BORDURE.',TXT3,10)
# 4 actif
s+=lab(962,R1,'4',u'ACTIF',u'\u26a0 s\u00e9lectionn\u00e9 + EN-T\u00caTE \u00c9CLAIRCI de 15 %')
n,h=noeud(962,R1+28,270,ROWS,u'Multiplier',u'Maths',CAT['outil'],selection=ORANGE,tete_clair=True); s+=n
s+=tt(962,R1+28+h+18,u'VU dans images (3) : douze n\u0153uds ont la bordure,',TXT3,10)
s+=tt(962,R1+28+h+31,u'UN SEUL a l\u2019en-t\u00eate clair. Sans lui, l\u2019\u00e9diteur ne sait',TXT3,10)
s+=tt(962,R1+28+h+44,u'pas \u00e0 qui envoyer le clavier.',ORANGE,10)
# 5 desactive
s+=lab(1268,R1,'5',u'D\u00c9SACTIV\u00c9',u'45 % \u00b7 les prises restent \u00e0 pleine opacit\u00e9')
n,h=noeud(1268,R1+28,270,ROWS,u'Multiplier',u'Maths',CAT['outil'],opacite=0.45); s+=n
s+=fil(1240,R1+28+45,1266,R1+28+45,FAM['nombre'],2,'pointille')
s+=fil(1540,R1+28+45,1600,R1+28+45,FAM['nombre'],2,'pointille')
s+=tt(1268,R1+28+h+18,u'\u26a0 le FIL QUI LE TRAVERSE passe en pointill\u00e9 de part',TXT3,10)
s+=tt(1268,R1+28+h+31,u'en part : un n\u0153ud \u00e9teint ne coupe pas la cha\u00eene.',TXT3,10)

R2=380
# 6 erreur
s+=lab(44,R2,'6',u'ERREUR',u'la raison est \u00c9CRITE DANS le n\u0153ud, pas en infobulle')
n,h=noeud(44,R2+28,300,[
 {'lab':u'Vecteur','coul':FAM['geom'],'plein':False,'glyphe':'XYZ'},
 {'lab':u'Couleur','coul':FAM['appar'],'plein':False,'sortie':True,'glyphe':'RVB'},
],u'Texture image',u'Texture',CAT['texture'],False,0,erreur=ROUGE,corps='#2A1A1A',
 etat=(u'fichier introuvable : textures/mur_albedo.png',ROUGE))
s+=n
s+=tt(44+300-30,R2+28+15,u'!',ROUGE,13,'700',anchor='end')
s+=tt(44,R2+28+h+20,u'dans un graphe de trente n\u0153uds (images 2), survoler trente n\u0153uds',TXT3,10)
s+=tt(44,R2+28+h+33,u'pour trouver le fautif est une minute perdue \u00e0 chaque fois.',TXT3,10)
# 7 avertissement
s+=lab(390,R2,'7',u'AVERTISSEMENT',u'\u00c9TAT NOUVEAU \u2014 filet orange POINTILL\u00c9')
n,h=noeud(390,R2+28,300,[
 {'lab':u'Coordonn\u00e9es','coul':FAM['geom'],'plein':True,'branchee':True,'glyphe':'XY'},
 {'lab':u'Couleur','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'RVB'},
],u'Normale du mur',u'Texture',CAT['texture'],False,0,filet='pointille',
 etat=(u'sRVB sur une entr\u00e9e Normale \u2014 rendu probablement faux',ORANGE))
s+=n
s+=tt(390+300-30,R2+28+15,u'\u26a0',ORANGE,12,'700',anchor='end')
s+=tt(390,R2+28+h+20,u'il manquait : une texture sRVB branch\u00e9e sur une Normale produit',TXT3,10)
s+=tt(390,R2+28+h+33,u'un graphe VALIDE ET FAUX. L\u2019erreur mentirait, le silence aussi.',TXT3,10)
# 8 les trois gris
s+=lab(736,R2,'8',u'\u26a0 LES TROIS GRIS \u2014 \u00e0 NE JAMAIS CONFONDRE',u'trois causes, trois responsables, trois rem\u00e8des')
gris=[(u'd\u00e9sactiv\u00e9',u'l\u2019UTILISATEUR l\u2019a \u00e9teint',u'corps \u00e0 45 % + fil traversant pointill\u00e9'),
      (u'inconnu',u'l\u2019\u00c9DITEUR ne le conna\u00eet pas',u'en-t\u00eate RAY\u00c9 + filet gris + prises tiret\u00e9es'),
      (u'indisponible',u'le MOTEUR ne peut pas',u'hachures de CORPS + raison \u00e9crite')]
for i,(a,b,c) in enumerate(gris):
    y=R2+34+i*62
    s+='<rect x="736" y="%s" width="480" height="52" rx="3" fill="%s" stroke="%s"/>\n'%(y,CORPS,FILET)
    s+=tt(752,y+22,a,TXT,12.5,'600')
    s+=tt(752,y+40,b,ORANGE,10.5)
    s+=tt(1200,y+40,c,TXT3,10,None,'end')
s+=tt(736,R2+34+3*62+16,u'un seul gris pour les trois rendrait l\u2019\u00e9diteur inutilisable exactement au moment o\u00f9 l\u2019utilisateur a besoin d\u2019aide.',ORANGE,10.5)

# 9 tirage
s+=lab(1268,R2,'9',u'PENDANT LE TIRAGE D\u2019UN FIL',u'aucune r\u00e9f\u00e9rence ne le montre \u2014 c\u2019est pourtant LE retour le plus utile')
etats=[(u'compatible',u'halo clair 1,5 px \u00e0 60 %',FAM['nombre'],'halo'),
       (u'convertible',u'halo POINTILL\u00c9, couleur \u00e0 35 %',FAM['appar'],'halo_p'),
       (u'incompatible',u'\u00e9teinte : #3A3A44 \u00e0 30 %','#3A3A44','eteinte')]
for i,(a,b,c,k) in enumerate(etats):
    y=R2+44+i*46
    s+='<rect x="1268" y="%s" width="440" height="38" rx="3" fill="%s"/>\n'%(y,CORPS)
    if k=='halo': s+='<rect x="1294" y="%s" width="%s" height="%s" rx="4" fill="none" stroke="%s" stroke-width="1.5" opacity="0.6"/>\n'%(y+5,PW+8,PH+6,c)
    if k=='halo_p': s+='<rect x="1294" y="%s" width="%s" height="%s" rx="4" fill="none" stroke="%s" stroke-width="1.5" stroke-dasharray="3 3" opacity="0.35"/>\n'%(y+5,PW+8,PH+6,c)
    s+='<g opacity="%s">'%(0.3 if k=='eteinte' else 1.0)+prise(1304,y+19,c,False)+'</g>\n'
    s+=tt(1322,y+23,a,TXT,12,'600')+tt(1696,y+23,b,TXT3,10,None,'end')
s+=tt(1268,R2+44+3*46+16,u'\u26a0 compatible / convertible n\u2019est pas un raffinement : les conversions sont DIRIG\u00c9ES.',TXT3,10)
s+=tt(1268,R2+44+3*46+30,u'R\u00e9el \u2192 couleur existe, couleur \u2192 r\u00e9el n\u2019existe pas. Deux \u00e9tats seulement enseigneraient',TXT3,10)
s+=tt(1268,R2+44+3*46+44,u'une r\u00e8gle SYM\u00c9TRIQUE, qui est fausse.',TXT3,10)
s+=tt(1268,R2+44+3*46+64,u'et les N\u0152UDS ENTIERS dont aucune prise n\u2019est compatible passent \u00e0 50 % : une prise de',ORANGE,10)
s+=tt(1268,R2+44+3*46+78,u'17 px \u00e9teinte ne se voit pas dans un graphe de trente n\u0153uds. Un n\u0153ud, si.',ORANGE,10)

R3=690
# 10 graphe invalide
s+=lab(44,R3,'10',u'LE GRAPHE INVALIDE DOIT RESTER DESSINABLE',u'un \u00e9diteur passe son temps dans ces \u00e9tats')
n,h=noeud(44,R3+34,260,[
 {'lab':u'Entr\u00e9e','coul':FAM['nombre'],'plein':False,'val':'0.500','glyphe':'1.0'},
 {'lab':u'Sortie','coul':FAM['nombre'],'plein':True,'sortie':True,'glyphe':'1.0'},
],u'A',u'Maths',CAT['outil'])
s+=n
s+=fil(304,R3+34+45,420,R3+34+80,FAM['nombre'],2)
s+='<circle cx="420" cy="%s" r="5" fill="none" stroke="%s" stroke-width="2"/>\n'%(R3+34+80,FAM['nombre'])
s+=tt(436,R3+34+84,u'lien pendant \u2014 termin\u00e9 par un petit disque CREUX',TXT3,10)
# cycle
n1,h1=noeud(44,R3+150,180,[{'lab':'in','coul':FAM['nombre'],'plein':True},
                            {'lab':'out','coul':FAM['nombre'],'plein':True,'sortie':True}],u'B',None,CAT['outil'])
n2,h2=noeud(330,R3+150,180,[{'lab':'in','coul':FAM['nombre'],'plein':True},
                            {'lab':'out','coul':FAM['nombre'],'plein':True,'sortie':True}],u'C',None,CAT['outil'])
s+=n1+n2
s+=fil(224,R3+150+33,328,R3+150+33,ROUGE,2)
s+='<path d="M510 %s C 560 %s 560 %s 510 %s" fill="none" stroke="%s" stroke-width="2"/>\n'%(R3+150+57,R3+150+100,R3+150+120,R3+150+57,ROUGE)
s+='<path d="M42 %s C -10 %s -10 %s 42 %s" fill="none" stroke="%s" stroke-width="2"/>\n'%(R3+150+57,R3+150+100,R3+150+120,R3+150+33,ROUGE)
s+='<path d="M42 %s H510" fill="none" stroke="%s" stroke-width="2"/>\n'%(R3+150+120,ROUGE)
s+=tt(44,R3+150+h1+58,u'cycle : TOUS les fils du cycle passent en rouge \u2014 le cycle se voit comme une BOUCLE,',TXT3,10)
s+=tt(44,R3+150+h1+71,u'pas comme un n\u0153ud fautif : aucun des deux n\u2019a tort tout seul.',TXT3,10)

# 11 dezoom
s+=lab(736,R3,'11',u'LES TROIS PALIERS DE D\u00c9ZOOM',u'les seuils sont MESUR\u00c9S sur images (2) et images (5), deux r\u00e9f\u00e9rences prises \u00e0 ces \u00e9chelles')
n,h=noeud(736,R3+34,260,ROWS,u'Multiplier',u'Maths',CAT['outil']); s+=n
s+=tt(736,R3+34+h+18,u'100 % \u2014 tout',TXT2,11)
# 55 %
s+='<g transform="translate(1050,%s) scale(0.55)">'%(R3+34)
n,h55=noeud(0,0,260,[{'lab':u'Valeur A','coul':FAM['nombre'],'plein':False,'glyphe':'1.0'},
                     {'lab':u'Valeur B','coul':FAM['nombre'],'plein':True,'glyphe':'1.0'},
                     {'lab':u'R\u00e9sultat','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'1.0'}],
                    u'Multiplier',u'Maths',CAT['outil'])
s+=n+'</g>\n'
s+=tt(1050,R3+34+h55*0.55+18,u'55 % \u2014 les VALEURS et les champs disparaissent',TXT2,11)
# 25 %
s+='<rect x="1330" y="%s" width="120" height="34" rx="0" fill="%s"/>\n'%(R3+34,CAT['outil'])
s+='<rect x="1330" y="%s" width="120" height="2.5" fill="%s"/>\n'%(R3+34+34,PETROLE)
s+=tt(1330,R3+34+64,u'25 % \u2014 un RECTANGLE de la couleur de la cat\u00e9gorie',TXT2,11)
s+=tt(1330,R3+34+80,u'+ le filet ex\u00e9cution / p\u00e9trole. Rien d\u2019autre.',TXT2,11)

c,ch=cartouche(1050,R3+150,660,[
 u'\u25b8 dans images (2) \u2014 un vrai graphe de trente n\u0153uds \u2014 les libell\u00e9s sont D\u00c9J\u00c0 illisibles',
 u'   alors que les couleurs d\u2019EN-T\u00caTE et de FIL restent parfaitement lisibles.',
 u'\u25b8 dans images (5), plus d\u00e9zoom\u00e9 encore, SEULS LES FILS structurent l\u2019image.',
 u'\u25b8 c\u2019est l\u2019argument qui commande toute la palette : \u00e0 25 %, la couleur d\u2019en-t\u00eate et',
 u'   la couleur de fil sont TOUT ce qu\u2019il reste.',
 u'\u25b8 ordre de disparition : valeurs \u2192 \u00e9tiquettes \u2192 prises \u2192 titre. On d\u00e9zoome pour lire',
 u'   la STRUCTURE, jamais les nombres : ce qui sert \u00e0 la structure part en dernier.',
],u'Ce qui fonde ces trois paliers')
s+=c

s+=tt(34,H-50,u'Rouge d\u2019erreur #E4443C \u00b7 orange d\u2019avertissement #F79A28 \u00b7 \u26a0 le rouge est \u00e0 7,1 de CIEDE2000 du rose de TEXTE en tritanopie \u2014 sans cons\u00e9quence tant que l\u2019erreur ne descend pas au niveau de la prise',TXT3,10)
# MENTION D’ÉCHELLE : la planche declare elle-meme qu elle ne doit pas
# etre mesuree. Sans elle, un pixel releve ici deviendrait une valeur.
s+=tt(34,H-26,u'⚠ PLANCHE D’ÉTUDE — les prises y sont dessinées à environ 2,1 × leur échelle relative pour rester lisibles. Les RATIOS de la spécification font foi, jamais les pixels de cette planche.',TXT3,10)

ecrire('planche_04_etats.svg',s)
# LE RENDU PASSE PAR rendre(), PLUS JAMAIS PAR msedge A LA MAIN.
# Ses trois controles -- URL absolue prefixee file:///, PNG precedent
# SUPPRIME, dimensions comparees au viewBox -- existent pour des pannes qui
# se sont REELLEMENT produites, dont un PNG aux bonnes dimensions qui etait
# la page d erreur du navigateur. p02..p05 s en passaient encore : corrige
# le 23/08, en meme temps que les quatre corrections de Rodolf.
rendre('planche_04_etats', W, H)
