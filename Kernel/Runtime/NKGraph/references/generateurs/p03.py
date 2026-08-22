# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from gen import *

W,H=1780,1400
s=head(W,H,u'Planche 03 \u2014 les quatorze formes de n\u0153ud',
       u'La forme suit la CONNECTIVIT\u00c9, jamais le nom \u2014 c\u2019est pourquoi il y en a quatorze et non cinquante-quatre \u00b7 filet ORANGE = le n\u0153ud ex\u00e9cute \u00b7 filet P\u00c9TROLE = il calcule \u00b7 filet GRIS = on ne sait pas')

def lab(x,y,n,titre,note):
    o=tt(x,y,u'%s \u00b7 %s'%(n,titre),ORANGE,12.5,'600')
    o+=tt(x,y+16,note,TXT3,10)
    return o

CO=310
R1,R2,R3=118,432,830
COLX=[44,420,796,1172,1548]

# 1 SOURCE
s+=lab(44,R1,'1',u'SOURCE',u'c\u00f4t\u00e9 gauche VIDE \u2014 ce qui la fait reconna\u00eetre de loin')
n,h=noeud(44,R1+28,290,[
 {'lab':u'Couleur','ctrl':'nuancier','val':'#d98a3a'},
 {'lab':u'Couleur','coul':FAM['appar'],'plein':False,'sortie':True,'glyphe':'RVB'},
],u'RVB',u'Source',CAT['entree'])
s+=n

# 2 TRANSFORMATEUR
s+=lab(420,R1,'2',u'TRANSFORMATEUR',u'l\u2019op\u00e9ration est dans le SOUS-TITRE, pas en 1re rang\u00e9e')
n,h=noeud(420,R1+28,290,[
 {'lab':u'Valeur A','coul':FAM['nombre'],'plein':False,'val':'0.500','glyphe':'1.0'},
 {'lab':u'Valeur B','coul':FAM['nombre'],'plein':True,'branchee':True,'glyphe':'1.0'},
 {'ajout':u'+ ajouter une entr\u00e9e'},
 {'lab':u'R\u00e9sultat','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'1.0'},
],u'Multiplier',u'Maths',CAT['outil'])
s+=n
s+=tt(420,R1+28+h+18,u'\u26a0 changer l\u2019op\u00e9ration change le NOMBRE d\u2019entr\u00e9es :',TXT3,10)
s+=tt(420,R1+28+h+32,u'en 1re rang\u00e9e, le n\u0153ud bougerait sous le curseur.',TXT3,10)

# 3 PUITS
s+=lab(796,R1,'3',u'PUITS \u2014 unique',u'c\u00f4t\u00e9 droit VIDE \u00b7 en-t\u00eate ORANGE Rihen \u00b7 20 % plus large')
n,h=noeud(796,R1+28,330,[
 {'lab':u'Surface','coul':FAM['appar'],'plein':True,'glyphe':'SH','branchee':True},
 {'lab':u'D\u00e9placement','coul':FAM['geom'],'plein':False,'glyphe':'XYZ'},
],u'Sortie de mat\u00e9riau',None,CAT['sortie'])
s+=n
s+=tt(796,R1+28+h+18,u'\u26a0 un SECOND n\u0153ud de sortie = ERREUR, pas avertissement.',TXT3,10)

# 4 SEPARATEUR
s+=lab(1172,R1,'4',u'S\u00c9PARATEUR',u'deux sorties de FAMILLES diff\u00e9rentes : le probl\u00e8me se r\u00e8gle seul')
n,h=noeud(1172,R1+28,290,[
 {'lab':u'Vecteur','coul':FAM['geom'],'plein':True,'glyphe':'XYZ','branchee':True},
 {'lab':'X','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'1.0'},
 {'lab':'Y','coul':FAM['nombre'],'plein':True,'sortie':True,'glyphe':'1.0'},
 {'lab':'Z','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'1.0'},
],u'S\u00e9parer XYZ',u'Vecteur',CAT['outil'])
s+=n

# 5 CHARGE VARIABLE
s+=lab(44,R2,'5',u'CHARGE VARIABLE',u'l\u2019\u00e9diteur EST le n\u0153ud \u00b7 chaque arr\u00eat est une PRISE')
n,h=noeud(44,R2+28,330,[
 {'lab':u'Facteur','coul':FAM['nombre'],'plein':False,'val':'0.500','glyphe':'1.0'},
 {'note':''},
 {'lab':u'arr\u00eat 1','coul':FAM['appar'],'plein':False,'ctrl':'nuancier','val':'#0A555F'},
 {'lab':u'arr\u00eat 2','coul':FAM['appar'],'plein':True,'ctrl':'nuancier','val':'#3aa0a8'},
 {'lab':u'arr\u00eat 3','coul':FAM['appar'],'plein':False,'ctrl':'nuancier','val':'#F79A28'},
 {'ajout':u'+ ajouter un arr\u00eat'},
 {'lab':u'Couleur','coul':FAM['appar'],'plein':False,'sortie':True,'glyphe':'RVB'},
],u'Rampe de couleur',u'Couleur',CAT['texture'])
s+=n.replace('<g filter="url(#ombre)" opacity="1.0">',
  '<g filter="url(#ombre)" opacity="1.0"><rect x="60" y="%d" width="298" height="20" rx="2" fill="url(#rampe)"/>'%(R2+28+21+4+24+6))
s+=tt(44,R2+28+h+18,u'au d\u00e9zoom 55 % les poign\u00e9es partent, la BARRE reste : elle devient un aper\u00e7u.',TXT3,10)

# 6 SURFACE
s+=lab(420,R2,'6',u'SURFACE',u'haut \u00b7 sections repliables \u00b7 sortie d\u2019un type \u00e0 part')
n,h=noeud(420,R2+28,330,[
 {'lab':u'Couleur de base','coul':FAM['appar'],'plein':True,'branchee':True,'glyphe':'RVB'},
 {'lab':u'M\u00e9tallique','coul':FAM['nombre'],'plein':False,'val':'0.000','glyphe':'1.0'},
 {'lab':u'Rugosit\u00e9','coul':FAM['nombre'],'plein':False,'val':'0.500','glyphe':'1.0'},
 {'lab':u'Normale','coul':FAM['geom'],'plein':False,'ctrl':'lecture','val':u'd\u00e9faut','glyphe':'XYZ'},
 {'section':u'Diffusion sous-cutan\u00e9e'},
 {'section':u'\u00c9mission','ouvert':True},
 {'lab':u'Couleur','coul':FAM['appar'],'plein':False,'sub':True,'ctrl':'nuancier','val':'#111111'},
 {'lab':u'Intensit\u00e9','coul':FAM['nombre'],'plein':False,'sub':True,'val':'1.000'},
 {'section':u'Voile \u00b7 Vernis \u00b7 Film mince'},
 {'lab':u'Surface','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'SH'},
],u'Principled',u'Surface',CAT['surface'])
s+=n

# 7 EXECUTION
s+=lab(796,R2,'7',u'EX\u00c9CUTION',u'entr\u00e9e d\u2019ex\u00e9c. UNIQUE sur l\u2019EN-T\u00caTE \u00b7 2 sorties \u2192 dans le CORPS')
n,h=noeud(796,R2+28,290,[
 {'lab':u'Condition','coul':FAM['nombre'],'plein':True,'branchee':True,'glyphe':'V/F'},
 {'lab':u'Vrai','coul':ORANGE,'plein':True,'sortie':True,'exec':True},
 {'lab':u'Faux','coul':ORANGE,'plein':False,'sortie':True,'exec':True},
],u'Si / Sinon',u'Flot',CAT['flot'],True)
s+=prise(796,R2+28+15,ORANGE,True,True)
s+=n
s+=tt(796,R2+28+h+18,u'\u26a0 Vrai / Faux se distinguent par \u00c9TIQUETTE ET POSITION,',TXT3,10)
s+=tt(796,R2+28+h+32,u'jamais par la couleur : elles sont toutes les deux orange.',TXT3,10)

# 8 EVENEMENT
s+=lab(1172,R2,'8',u'\u00c9V\u00c9NEMENT',u'AUCUNE entr\u00e9e d\u2019ex\u00e9c. \u2014 bord gauche de l\u2019en-t\u00eate NET \u00b7 coin 12 px')
n,h=noeud(1172,R2+28,290,[
 {'lab':u'D\u00e9clencher','coul':ORANGE,'plein':True,'sortie':True,'exec':True},
 {'lab':u'delta','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'1.0'},
],u'\u00c0 chaque image',u'\u00c9v\u00e9nement',CAT['flot'],True,arrondi_debut=True)
s+=n
s+=tt(1172,R2+28+h+18,u'un \u00e9v\u00e9nement est un n\u0153ud SOURCE de temps :',TXT3,10)
s+=tt(1172,R2+28+h+32,u'le sym\u00e9trique exact de la forme 1, sur l\u2019autre rang\u00e9e.',TXT3,10)

# 9 GROUPE
s+=lab(44,R3,'9',u'GROUPE',u'pictogramme de PILE \u00b7 filet orange si son contenu ex\u00e9cute')
n,h=noeud(44,R3+28,290,[
 {'lab':u'Entr\u00e9e A','coul':FAM['nombre'],'plein':True,'branchee':True,'glyphe':'1.0'},
 {'lab':u'\u00c9chelle','coul':FAM['nombre'],'plein':False,'val':'1.000','glyphe':'1.0'},
 {'lab':u'Sortie','coul':FAM['appar'],'plein':False,'sortie':True,'glyphe':'RVB'},
],u'Bruit de surface',u'Sous-graphe',CAT['variable'])
s+=n
s+='<rect x="290" y="%d" width="9" height="7" fill="#EEF2F6" opacity="0.55"/><rect x="294" y="%d" width="9" height="7" fill="#EEF2F6" opacity="0.9"/>\n'%(R3+28+5,R3+28+10)
s+=tt(44,R3+28+h+18,u'\u26d4 NON TRANCH\u00c9 : sur place ou en onglet ? aper\u00e7u du contenu ?',ROUGE,10)

# 10 CUSTOM CONNU
s+=lab(420,R3,'10',u'CUSTOM \u2014 niveau A, d\u00e9clar\u00e9',u'un n\u0153ud NORMAL. Rien ne le distingue, et c\u2019est le but.')
n,h=noeud(420,R3+28,290,[
 {'lab':u'Turbulence','coul':FAM['nombre'],'plein':False,'val':'0.750','glyphe':'1.0'},
 {'lab':u'Graine','coul':FAM['nombre'],'plein':False,'val':'42','glyphe':'12'},
 {'lab':u'Champ','coul':FAM['geom'],'plein':False,'sortie':True,'glyphe':'XYZ'},
],u'Tourbillon',u'studio.fx',CAT['texture'])
s+=n

# 11 CUSTOM INCONNU
s+=lab(796,R3,'11',u'CUSTOM \u2014 niveau C, INCONNU',u'en-t\u00eate ray\u00e9 \u00b7 filet GRIS \u00b7 prises TIRET\u00c9ES \u00b7 les liens sont GARD\u00c9S')
n,h=noeud(796,R3+28,330,[
 {'lab':u'entree_0','coul':FAM['quel'],'forme':'tiret'},
 {'lab':u'entree_1','coul':FAM['quel'],'forme':'tiret'},
 {'lab':u'sortie_0','coul':FAM['quel'],'sortie':True,'forme':'tiret'},
],u'studio.fx.Tourbillon',None,'#3A3A44',False,0,inconnu=True,aide=False,
 etat=(u'type inconnu \u2014 les liens sont conserv\u00e9s',TXT2))
s+=n
s+=tt(796,R3+28+h+18,u'\u26a0 ce n\u2019est PAS une erreur : le graphe est probablement juste,',TXT3,10)
s+=tt(796,R3+28+h+32,u'c\u2019est l\u2019\u00c9DITEUR qui est incomplet. Le rouge ferait supprimer le n\u0153ud.',TXT3,10)

# 12 INDISPONIBLE
s+=lab(1172,R3,'12',u'INDISPONIBLE \u2014 le rang interdit',u'hachures de CORPS \u00b7 la raison est \u00c9CRITE, pas en infobulle')
n,h=noeud(1172,R3+28,330,[
 {'lab':u'Est une ombre','coul':FAM['nombre'],'sortie':True,'glyphe':'V/F'},
 {'lab':u'Profondeur','coul':FAM['nombre'],'sortie':True,'glyphe':'1.0'},
],u'Light Path',u'Entr\u00e9e',CAT['entree'],False,0,hachure=True,opacite=0.75,
  etat=(u'ce moteur rast\u00e9rise \u2014 n\u00e9cessite un trac\u00e9 de rayons',ROUGE))
s+=n

# 13-14 INERTES
R4=1176
s+=lab(44,R4,'13',u'INERTES \u2014 relais nu, relais nomm\u00e9 (la puce), commentaire',u'')
s+=fil(60,R4+70,150,R4+70,FAM['appar'],2)
s+=prise(158,R4+70,FAM['appar'],True,False,'relais')
s+=fil(166,R4+70,260,R4+56,FAM['appar'],2)
s+=tt(52,R4+104,u'relais NU : un carr\u00e9 17\u00d717, pas la prise \u00e0 cheval \u2014 il n\u2019appartient \u00e0 aucun bord',TXT3,10)
# puce
px,py=330,R4+52
s+='<rect x="%s" y="%s" width="196" height="34" rx="13" fill="%s"/>\n'%(px,py,CORPS)
s+='<path d="M%s %sa13 13 0 0 1 13 -13h30v34h-30a13 13 0 0 1 -13 -13z" fill="#2E2770"/>\n'%(px,py+13)
s+=tt(px+22,py+22,u'\u26ad','#C7C0F0',12,None,'middle')
s+=tt(px+56,py+22,u'position.monde',TXT,11.5)
s+=prise(px,py+17,ORANGE,True,False)
s+=prise(px+196,py+17,FAM['geom'],True,False)
s+=tt(330,R4+104,u'relais NOMM\u00c9 (la puce) : VU sur la principale \u2014 entr\u00e9e ET sortie, bloc-ic\u00f4ne plein \u00e0 gauche',TXT3,10)
s+=tt(720,R4+44,u'Le repli du bruit vient d\u2019ici \u2014 ne pas y toucher',TXT2,13)
s+=tt(720,R4+62,u'sans relire la note du 12/08.',TXT2,13)
s+=tt(720,R4+104,u'COMMENTAIRE : du texte POS\u00c9 sur le fond \u2014 aucun corps, aucun filet, aucun fond.',TXT3,10)
s+=tt(720,R4+118,u'Il dispara\u00eet enti\u00e8rement au palier 25 % \u2014 le seul objet qu\u2019on efface compl\u00e8tement.',TXT3,10)

c,ch=cartouche(1180,R4-6,560,[
 u'\u25b8 forme 14 : le CADRE \u2014 voir planche 05, il est VU en d\u00e9tail',
 u'   (`node-based\u2026webp` : filet plein + filet POINTILL\u00c9 int\u00e9rieur,',
 u'   bandeau de titre, compteur \u00ab 7 nodes \u00bb, n\u0153uds contenus teint\u00e9s).',
 u'\u25b8 les formes 9 \u00e0 12 n\u2019existent dans AUCUNE r\u00e9f\u00e9rence sauf une :',
 u'   `Example node` de node-based\u2026webp est exactement le squelette nu',
 u'   d\u2019un n\u0153ud dont l\u2019\u00e9diteur ne sait rien.',
],u'Ce qui reste')
s+=c

s+=tt(34,1370,u'Fond #121212 \u00b7 corps #212121 \u00b7 contr\u00f4le #2B2B2B \u00b7 rayon 0 corps / 5 en-t\u00eate (12 pour un \u00e9v\u00e9nement) \u00b7 prise au ratio 17:63 \u00b7 les couleurs de CAT\u00c9GORIE (en-t\u00eate) restent \u00e0 valider',TXT3,10)
ecrire('planche_03_formes.svg',s)
