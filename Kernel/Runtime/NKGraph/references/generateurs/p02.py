# -*- coding: utf-8 -*-
import sys, io
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from gen import *

W,H=1720,1350
s=head(W,H,u'Planche 02 \u2014 les types de donn\u00e9e, les tableaux et les dictionnaires',
       u'6 familles de couleur (plancher CIEDE2000 = 11,0 en vision normale, protanopie, deut\u00e9ranopie et tritanopie) \u00b7 le glyphe porte le type exact \u00b7 tableau et dictionnaire sont des FORMES, pas des couleurs')

# ---------- 1 · les 7 familles ----------
s+=titre_bloc(34,104,u'1 \u00b7 LES 7 FAMILLES \u2014 la couleur ne dit que la famille')
fams=[('ex\u00e9cution','exec',u'l\u2019ordre \u2014 orange Rihen, comme le filet'),
      ('nombre','nombre',u'r\u00e9el \u00b7 entier \u00b7 bool\u00e9en \u2014 inter-convertibles'),
      (u'g\u00e9om\u00e9trie','geom',u'vec2/3/4 \u00b7 matrice \u00b7 transformation'),
      ('texte','texte',u'cha\u00eene \u2014 aucune arithm\u00e9tique'),
      ('apparence','appar',u'couleur \u00b7 texture \u00b7 mat\u00e9riau \u00b7 shader'),
      (u'r\u00e9f\u00e9rence','ref',u'objet \u2014 d\u00e9signe au lieu de contenir'),
      ('quelconque','quel',u'non r\u00e9solu \u2014 gris : absence de couleur')]
x=40
for nom,k,expl in fams:
    s+='<rect x="%s" y="120" width="228" height="46" rx="3" fill="%s" opacity="0.18" stroke="%s"/>\n'%(x,FAM[k],FAM[k])
    s+='<rect x="%s" y="132" width="22" height="22" rx="2" fill="%s"/>\n'%(x+12,FAM[k])
    s+=tt(x+42,140,nom,FAM[k],12,'600')
    s+=tt(x+42,156,FAM[k],TXT3,9.5)
    s+=tt(x+2,182,expl,TXT3,9.5)
    x+=238

# ---------- 2 · les 16 types ----------
s+=titre_bloc(34,224,u'2 \u00b7 LES 16 TYPES \u2014 couleur de famille + glyphe + prise creuse (non branch\u00e9e) et pleine (branch\u00e9e)')
types=[(u'r\u00e9el','nombre','1.0',u'champ, 3 d\u00e9cimales'),
       ('entier','nombre','12',u'champ, sans d\u00e9cimale'),
       (u'bool\u00e9en','nombre','V/F',u'case \u00e0 cocher, jamais un champ'),
       ('texte','texte','abc',u'champ de texte + invite'),
       ('vec2','geom','XY',u'2 champs sur une ligne'),
       ('vec3','geom','XYZ',u'3 champs \u2014 d\u00e9pliable en X/Y/Z'),
       ('vec4','geom','XYZW',u'4 champs'),
       ('matrice','geom','M4',u'RIEN \u2014 16 champs sur un n\u0153ud, jamais'),
       ('transformation','geom','TRS',u'3 lignes pli\u00e9es : pos / rot / \u00e9chelle'),
       ('couleur','appar','RVB',u'nuancier + damier si alpha'),
       ('texture','appar','TEX',u'aper\u00e7u + chemin + espace colorim.'),
       (u'mat\u00e9riau','appar','MAT',u'aper\u00e7u rendu sur damier'),
       ('shader','appar','SH',u'RIEN \u2014 un shader ne se saisit pas'),
       (u'r\u00e9f\u00e9rence','ref','OBJ',u'nom de l\u2019objet, ou \u2014 aucun \u2014'),
       ('quelconque','quel','?',u'RIEN tant qu\u2019il n\u2019est pas r\u00e9solu'),
       (u'ex\u00e9cution','exec',u'\u2192',u'prise \u00c0 POINTE \u2014 autre silhouette')]
cx,cy=44,252
for i,(nom,k,g,expl) in enumerate(types):
    col=i//8; row=i%8
    X=cx+col*470; Y=cy+row*42
    c=FAM[k]
    ex = (k=='exec')
    s+='<rect x="%s" y="%s" width="440" height="34" rx="3" fill="%s"/>\n'%(X,Y,CORPS)
    s+=prise(X,Y+17,c,False,ex)
    s+=prise(X+64,Y+17,c,True,ex)
    s+=pastille(X+92,Y+17,c,g)
    s+=tt(X+134,Y+21,nom,TXT,12,'600')
    s+=tt(X+252,Y+21,expl,TXT3,10)
s+=tt(44,614,u'creuse = non branch\u00e9e  \u00b7  pleine = branch\u00e9e  \u00b7  et l\u2019\u00e9tiquette de la rang\u00e9e passe en orange quand un fil arrive (VU : Octane, images 3 et 4)',TXT2,10.5)

# ---------- 3 · tableaux et dictionnaires ----------
s+=titre_bloc(34,668,u'3 \u00b7 TABLEAU ET DICTIONNAIRE \u2014 la couleur reste celle du CONTENU, la forme dit la structure')
base=[('scalaire','simple','1.0',u'un seul tenant'),
      ('tableau','tableau','[1.0]',u'trois segments empil\u00e9s'),
      ('dictionnaire','dico','{1.0}',u'grille 2\u00d73 : cl\u00e9 | valeur')]
X=44
for nom,f,g,expl in base:
    s+='<rect x="%s" y="694" width="300" height="86" rx="3" fill="%s" stroke="%s"/>\n'%(X,CORPS,FILET)
    s+=tt(X+16,714,nom,TXT,12,'600')
    s+=tt(X+16,730,expl,TXT3,10)
    for j,c in enumerate([FAM['nombre'],FAM['appar'],FAM['geom']]):
        s+=prise(X+40+j*70,762,c,True,False,f)
        s+=pastille(X+56+j*70,762,c,{'simple':['1.0','RVB','XYZ'],'tableau':['[1.0]','[RVB]','[XYZ]'],'dico':['{1.0}','{RVB}','{XYZ}']}[f][j])
    X+=316

s+=tt(44,808,u'le glyphe ENCADRE celui du contenu : [1.0] = tableau de r\u00e9els \u00b7 {abc} = dictionnaire de textes \u00b7 [[1.0]] = tableau de tableaux de r\u00e9els',TXT2,10.5)

# fils
s+=titre_bloc(34,850,u'4 \u00b7 LES FILS')
fils=[(u'valeur \u2014 trait simple 2 px, couleur du type','simple',FAM['nombre'],2),
      (u'ex\u00e9cution \u2014 3,5 px, TOUJOURS orange','simple',ORANGE,3.5),
      (u'tableau \u2014 DOUBL\u00c9','tableau',FAM['nombre'],2),
      (u'dictionnaire \u2014 doubl\u00e9, trait inf\u00e9rieur pointill\u00e9','dico',FAM['nombre'],2),
      (u'en cours de tirage \u2014 pointill\u00e9 gris','pointille','#7A7A85',2)]
for i,(lab,st,c,ep) in enumerate(fils):
    y=884+i*30
    s+=fil(50,y,250,y-10,c,ep,st)
    s+=tt(268,y-6,lab,TXT2,10.5)

# ---------- 5 · producteur / parcours ----------
s+=titre_bloc(640,850,u'5 \u00b7 LE N\u0152UD QUI PRODUIT \u00b7 LE N\u0152UD QUI PARCOURT')
n1,h1=noeud(650,872,268,[
  {'lab':'0','coul':FAM['nombre'],'plein':False,'val':'0.250'},
  {'lab':'1','coul':FAM['nombre'],'plein':True,'branchee':True},
  {'lab':'2','coul':FAM['nombre'],'plein':False,'val':'1.000'},
  {'ajout':u'+ ajouter un \u00e9l\u00e9ment'},
  {'note':u'\u2026 et 492 autres','coul':TXT3},
  {'lab':'tableau','coul':FAM['nombre'],'plein':True,'sortie':True,'forme':'tableau','glyphe':'[1.0]'},
], u'Construire tableau', u'Tableau', CAT['outil'], False, 0, etat=(u'495 \u00e9l\u00e9ments',))
s+=n1
n2,h2=noeud(960,872,290,[
  {'lab':u'\u00e9l\u00e9ments','coul':FAM['nombre'],'plein':True,'forme':'tableau','glyphe':'[1.0]','branchee':True},
  {'lab':'corps de boucle','coul':ORANGE,'plein':True,'sortie':True,'exec':True},
  {'lab':u'\u00e9l\u00e9ment','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'1.0'},
  {'lab':'indice','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'12'},
  {'note':''},
  {'lab':u'termin\u00e9','coul':ORANGE,'plein':False,'sortie':True,'exec':True},
], u'Pour chaque', u'Boucle', CAT['flot'], True, 0, etat=(u'0 / 495',))
s+=n2
s+=prise(960,872+12,ORANGE,True,True)
s+=tt(650,872+h1+22,u'chaque \u00e9l\u00e9ment est une VRAIE prise \u2014 branchable seul \u00b7 repli obligatoire au-del\u00e0 de 8',TXT3,10)
s+=tt(960,872+h2+22,u'l\u2019entr\u00e9e d\u2019ex\u00e9cution est sur l\u2019EN-T\u00caTE \u00b7 \u26a0 corps de boucle et termin\u00e9 s\u00e9par\u00e9s par une ligne vide',TXT3,10)

# ---------- 6 · brancher un tableau sur un scalaire ----------
s+=titre_bloc(34,1112,u'6 \u00b7 \ud83d\udd34 BRANCHER UN TABLEAU SUR UNE ENTR\u00c9E SCALAIRE \u2014 refus UTILE, en trois temps')
n3,h3=noeud(44,1138,230,[
  {'lab':'valeurs','coul':FAM['nombre'],'plein':True,'sortie':True,'forme':'tableau','glyphe':'[1.0]'},
],u'Positions',u'Source',CAT['entree'])
s+=n3
n4,h4=noeud(420,1138,230,[
  {'lab':'Valeur A','coul':FAM['nombre'],'plein':False,'val':'0.500'},
  {'lab':'Valeur B','coul':FAM['nombre'],'plein':False,'val':'2.000'},
  {'lab':u'R\u00e9sultat','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'1.0'},
],u'Multiplier',u'Maths',CAT['outil'])
s+=n4
# prise incompatible eteinte
s+='<rect x="%s" y="%s" width="%s" height="%s" rx="2" fill="#3A3A44" opacity="0.30"/>\n'%(417,1138+30+12-11,PW,PH)
s+=fil(274,1138+33,414,1138+45,FAM['nombre'],2,'tableau')
s+=tt(44,1138+h3+30,u'1 \u2014 la prise scalaire s\u2019\u00c9TEINT (30 %) pendant le tirage',TXT2,10.5)
s+='<rect x="700" y="1138" width="330" height="128" rx="4" fill="#1A1A1A" stroke="#2E2E36"/>\n'
s+=tt(714,1164,u'\ud83d\udd0d ______________',TXT,12)
s+=tt(714,1184,u'qui acceptent',TXT3,10.5)
s+=pastille(800,1180,FAM['nombre'],'[1.0]')
s+=tt(836,1184,u'tableau de r\u00e9els',FAM['nombre'],10.5)
s+=tt(1016,1184,u'\u2715',TXT2,11,None,'end')
s+='<path d="M710 1194H1024" stroke="#2E2E36"/>\n'
s+=tt(714,1214,u'Pour chaque',TXT,11)+tt(1016,1214,u'Boucle',TXT3,10,None,'end')
s+=tt(714,1234,u'\u00c9l\u00e9ment \u00e0 l\u2019indice',TXT,11)+tt(1016,1234,u'Tableau',TXT3,10,None,'end')
s+=tt(714,1254,u'R\u00e9duire',TXT,11)+tt(1016,1254,u'Tableau',TXT3,10,None,'end')
s+=tt(700,1284,u'2 \u2014 si on rel\u00e2che quand m\u00eame : la recherche s\u2019ouvre, FILTR\u00c9E, et le filtre est \u00c9CRIT et RETIRABLE',TXT2,10.5)
s+=tt(700,1302,u'3 \u2014 le n\u0153ud choisi S\u2019INS\u00c8RE entre les deux et les deux liens se font',TXT2,10.5)

c,ch=cartouche(1080,1112,610,[
 u'\u25b8 refus, et non adaptation implicite : le graphe est PARTAG\u00c9 avec les mat\u00e9riaux, qui se',
 u'   compilent vers NkSL \u2014 une boucle de longueur inconnue n\u2019a pas de traduction en shader.',
 u'   Une m\u00eame r\u00e8gle doit valoir dans les deux mondes : la plus stricte gagne.',
 u'\u25b8 refus UTILE, et non refus sec : le moment o\u00f9 l\u2019\u00e9diteur en sait le plus sur l\u2019intention',
 u'   est exactement celui o\u00f9 l\u2019on rel\u00e2che le bouton. Le g\u00e2cher est un luxe.',
 u'\u25b8 \u26a0 au d\u00e9zoom 55 %, les fentes de la prise de tableau disparaissent \u2014 le FIL, lui, tient',
 u'   un palier de plus. Fai)blesse assum\u00e9e : \u00e0 55 % on ne branche plus, on lit la structure.',
],u'Pourquoi ces choix')
s+=c.replace('Fai)blesse','Faiblesse')

s+=tt(34,H-50,u'Fond #121212 \u00b7 corps #212121 \u00b7 contr\u00f4le #2B2B2B (MESUR\u00c9S sur la r\u00e9f\u00e9rence principale) \u00b7 rayon 0 sur le corps, 5 sur l\u2019en-t\u00eate \u00b7 prise au ratio 17:63 \u00b7 grille de points, pas 22 px',TXT3,10)
# MENTION D’ÉCHELLE : la planche declare elle-meme qu elle ne doit pas
# etre mesuree. Sans elle, un pixel releve ici deviendrait une valeur.
s+=tt(34,H-26,u'⚠ PLANCHE D’ÉTUDE — les prises y sont dessinées à environ 2,1 × leur échelle relative pour rester lisibles. Les RATIOS de la spécification font foi, jamais les pixels de cette planche.',TXT3,10)

ecrire('planche_02_types.svg',s)
