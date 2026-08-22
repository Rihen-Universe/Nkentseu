# -*- coding: utf-8 -*-
import sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from gen import *

W,H=1800,1625
VERT='#4E9A5A'

s=head(W,H,u'Planche 05 \u2014 textures, mat\u00e9riaux, commentaires et cadres',
       u'Aper\u00e7u ON / OFF \u00b7 le n\u0153ud de texture \u00b7 le n\u0153ud de mat\u00e9riau \u00b7 l\u2019aper\u00e7u p\u00e9rim\u00e9 \u00b7 la r\u00e8gle absolue (aucun fil d\u2019ex\u00e9cution) \u00b7 l\u2019aper\u00e7u au d\u00e9zoom \u00b7 le commentaire \u00b7 le cadre \u00b7 les deux relais')

# defs supplementaires (une deuxieme balise defs est valide en SVG)
s+=('<defs>\n'
    '<radialGradient id="sphere" cx="0.35" cy="0.30" r="0.78">'
    '<stop offset="0%" stop-color="#F4DCB6"/><stop offset="42%" stop-color="#B07A45"/>'
    '<stop offset="100%" stop-color="#241608"/></radialGradient>\n'
    '<linearGradient id="brique" x1="0" y1="0" x2="0" y2="1">'
    '<stop offset="0%" stop-color="#8a5a44"/><stop offset="50%" stop-color="#6d4534"/>'
    '<stop offset="100%" stop-color="#4a2e23"/></linearGradient>\n'
    '<linearGradient id="normale" x1="0" y1="0" x2="1" y2="1">'
    '<stop offset="0%" stop-color="#8080ff"/><stop offset="50%" stop-color="#9a90f0"/>'
    '<stop offset="100%" stop-color="#7a7ae8"/></linearGradient>\n'
    '</defs>\n')

def lab(x,y,n,titre,note=''):
    return tt(x,y,u'%s \u00b7 %s'%(n,titre),ORANGE,12.5,'600')+tt(x,y+16,note,TXT3,10)

def sphere(x,y,w,h):
    """une sphere rendue posee sur le damier, comme images (4)"""
    cx,cy=x+w/2.0,y+h/2.0; r=min(w,h)/2.0-4
    o ='<rect x="%s" y="%s" width="%s" height="%s" fill="url(#damier)"/>\n'%(x,y,w,h)
    o+='<ellipse cx="%s" cy="%s" rx="%s" ry="%s" fill="#000" opacity="0.35"/>\n'%(cx,y+h-6,r*0.8,4)
    o+='<circle cx="%s" cy="%s" r="%s" fill="url(#sphere)"/>\n'%(cx,cy,r)
    o+='<circle cx="%s" cy="%s" r="%s" fill="#fff" opacity="0.55"/>\n'%(cx-r*0.32,cy-r*0.36,r*0.13)
    return o

def image_apercu(x,y,w,h,grad='brique'):
    return ('<rect x="%s" y="%s" width="%s" height="%s" fill="url(#damier)"/>\n'%(x,y,w,h)
           +'<rect x="%s" y="%s" width="%s" height="%s" fill="url(#%s)"/>\n'%(x,y,w,h,grad))

TEX_ROWS=[{'lab':u'Coordonn\u00e9es','coul':FAM['geom'],'plein':True,'branchee':True,'glyphe':'XY'},
          {'lab':u'Gamma','coul':FAM['nombre'],'plein':False,'val':'2.200','glyphe':'1.0'},
          {'lab':u'Couleur','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'RVB'},
          {'lab':u'Alpha','coul':FAM['nombre'],'plein':False,'sortie':True,'glyphe':'1.0'}]

# ============================================================ RANGEE 1
R1=118

# --- 1 : APERCU ON / OFF -------------------------------------------------
s+=lab(44,R1,'1',u'APER\u00c7U ON / OFF',u'VU \u2014 images (4), la SEULE r\u00e9f\u00e9rence qui montre le m\u00eame graphe deux fois')
n,h_on=noeud(44,R1+28,250,TEX_ROWS,u'Texture image',u'Texture',CAT['texture'],apercu=96)
s+=n
s+=image_apercu(52,R1+28+36,234,90)
s+=tt(44,R1+28+h_on+18,u'APER\u00c7U ON \u2014 %s px'%h_on,TXT2,10.5,'600')

n,h_off=noeud(330,R1+28,250,TEX_ROWS,u'Texture image',u'Texture',CAT['texture'])
s+=n
s+=tt(330,R1+28+h_off+18,u'APER\u00c7U OFF \u2014 %s px'%h_off,TXT2,10.5,'600')

# cote de la difference
bx=596
s+='<path d="M%s %sV%s" stroke="%s" stroke-width="1" stroke-dasharray="3 3"/>\n'%(bx,R1+28,R1+28+h_on,ORANGE)
s+='<path d="M%s %sh8 M%s %sh8" stroke="%s" stroke-width="1.2"/>\n'%(bx-4,R1+28,bx-4,R1+28+h_on,ORANGE)
s+=tt(bx+12,R1+28+h_on/2.0-4,u'\u2212 %s px'%(h_on-h_off),ORANGE,10.5,'600')
s+=tt(bx+12,R1+28+h_on/2.0+10,u'exactement',TXT3,10)
s+=tt(44,R1+28+h_on+40,u'largeur, couleurs, ORDRE DES RANG\u00c9ES : identiques. Seule la hauteur change \u2014 les rang\u00e9es',TXT3,10)
s+=tt(44,R1+28+h_on+53,u'remontent contre l\u2019en-t\u00eate. L\u2019aper\u00e7u est donc un BLOC DE CORPS escamotable en pleine',TXT3,10)
s+=tt(44,R1+28+h_on+66,u'largeur \u2014 pas une d\u00e9coration d\u2019en-t\u00eate, pas une infobulle.',TXT3,10)
s+=tt(44,R1+28+h_on+84,u'\u26a0 dans images (4) la bascule est GLOBALE (tous les n\u0153uds ensemble), pas n\u0153ud par n\u0153ud.',ORANGE,10)

# --- 2 : le noeud de TEXTURE complet -------------------------------------
X2=700
s+=lab(X2,R1,'2',u'LE N\u0152UD DE TEXTURE \u2014 PROPOS\u00c9',u'la r\u00e9f\u00e9rence ne montre QUE la vignette : ni chemin, ni taille, ni espace colorim\u00e9trique')
TEX_PLEIN=[{'note':u'\u22efmurs/beton_albedo.png','coul':TXT2,'size':10},
           {'note':u'2048 \u00d7 2048 \u00b7 RVBA8 \u00b7 sRVB','coul':TXT3,'size':9.5}]+TEX_ROWS
n,h2=noeud(X2,R1+28,300,TEX_PLEIN,u'Texture image',u'Texture',CAT['texture'],apercu=96)
s+=n
s+=image_apercu(X2+8,R1+28+36,284,90)

# annotations a droite
ann=[(R1+28+80, R1+28+80, u'APER\u00c7U 96 px',u'image RECADR\u00c9E au centre, jamais d\u00e9form\u00e9e \u00b7 damier 8 px sous l\u2019alpha,'),
     (R1+28+148,R1+28+156,u'CHEMIN \u00c9LID\u00c9 PAR LA GAUCHE',u'l\u2019information utile d\u2019un chemin est \u00c0 LA FIN. \u00c9lider \u00e0 droite'),
     (R1+28+172,R1+28+230,u'CARTE D\u2019IDENTIT\u00c9, une ligne',u'taille \u00b7 format \u00b7 espace colorim\u00e9trique : trois faits qui se lisent')]
sub=[u'sinon un PNG noir opaque et un PNG transparent sont le m\u00eame rectangle noir.',
     u'cache le nom du fichier \u2014 c\u2019est l\u2019erreur la plus courante.',
     u'ensemble ou pas du tout.']
for (ey,ay,a,b),c in zip(ann,sub):
    s+='<path d=\"M%s %sH%s L%s %s\" fill=\"none\" stroke=\"%s\" stroke-width=\"1\" stroke-dasharray=\"2 2\"/>\n'%(X2+300,ey,X2+316,X2+328,ay,TXT3)
    s+=tt(X2+332,ay+4,a,ORANGE,10.5,'600')
    s+=tt(X2+332,ay+18,b,TXT3,10)
    s+=tt(X2+332,ay+31,c,TXT3,10)

# --- 3 : l'avertissement colorimetrique ----------------------------------
X3=1420
s+=lab(X3,R1,'3',u'\u26a0 sRVB SUR UNE ENTR\u00c9E NORMALE',u'le seul endroit du document o\u00f9 le DESSIN porte un jugement m\u00e9tier')
WARN=[{'note':u'\u22efmurs/beton_normal.png','coul':TXT2,'size':10},
      {'note':u'2048 \u00d7 2048 \u00b7 RVB8 \u00b7 sRVB','coul':ROUGE,'size':9.5},
      {'lab':u'Coordonn\u00e9es','coul':FAM['geom'],'plein':True,'branchee':True,'glyphe':'XY'},
      {'lab':u'Normale','coul':FAM['geom'],'plein':True,'sortie':True,'glyphe':'XYZ'}]
n,h3=noeud(X3,R1+28,300,WARN,u'Normale du mur',u'Texture',CAT['texture'],apercu=96,
           filet='pointille',etat=(u'espace colorim\u00e9trique incoh\u00e9rent avec l\u2019usage',ORANGE))
s+=n
s+=image_apercu(X3+8,R1+28+36,284,90,'normale')
s+=tt(X3+300-34,R1+28+15,u'\u26a0',ORANGE,12,'700',anchor='end')
s+=tt(X3,R1+28+h3+20,u'\u00e9tat AVERTISSEMENT, pas erreur : le graphe reste VALIDE,',TXT3,10)
s+=tt(X3,R1+28+h3+33,u'il est seulement probablement FAUX. Une normal map en sRVB',TXT3,10)
s+=tt(X3,R1+28+h3+46,u'donne un \u00e9clairage faux SANS AUCUNE ERREUR \u2014 c\u2019est le bug',TXT3,10)
s+=tt(X3,R1+28+h3+59,u'le plus cher et le plus silencieux d\u2019un graphe de mat\u00e9riau.',TXT3,10)
s+=tt(X3,R1+28+h3+77,u'la ligne d\u2019identit\u00e9 passe en #E4443C ; le filet passe en orange pointill\u00e9.',ORANGE,10)

# ============================================================ RANGEE 2
R2=560

# --- 4 : le noeud de MATERIAU --------------------------------------------
s+=lab(44,R2,'4',u'LE N\u0152UD DE MAT\u00c9RIAU',u'm\u00eame bloc d\u2019aper\u00e7u \u2014 mais une SPH\u00c8RE rendue sur damier (VU, images 4)')
MAT=[{'note':u'4 textures \u00b7 12 n\u0153uds','coul':TXT3,'size':9.5},
     {'lab':u'Couleur de base','coul':FAM['appar'],'plein':True,'branchee':True,'glyphe':'RVB'},
     {'lab':u'M\u00e9tallique','coul':FAM['nombre'],'plein':False,'val':'0.000','glyphe':'1.0'},
     {'lab':u'Rugosit\u00e9','coul':FAM['nombre'],'plein':False,'val':'0.400','glyphe':'1.0'},
     {'lab':u'Normale','coul':FAM['geom'],'plein':False,'glyphe':'XYZ'},
     {'lab':u'Surface','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'SH'}]
n,h4=noeud(44,R2+28,290,MAT,u'Mat\u00e9riau principal',u'Surface',CAT['surface'],apercu=110)
s+=n
s+=sphere(52,R2+28+36,274,104)
s+=tt(44,R2+28+h4+20,u'PROPOS\u00c9 : ni chemin ni taille \u2014 un mat\u00e9riau n\u2019est pas un fichier.',TXT3,10)
s+=tt(44,R2+28+h4+33,u'\u00c0 la place : CE QU\u2019IL CO\u00dbTE (4 textures \u00b7 12 n\u0153uds), qui est',TXT3,10)
s+=tt(44,R2+28+h4+46,u'l\u2019information qu\u2019on cherche vraiment sur un mat\u00e9riau.',TXT3,10)
s+=tt(44,R2+28+h4+64,u'la sortie Surface porte le type shader (SH, terre cuite) : PAS une',ORANGE,10)
s+=tt(44,R2+28+h4+77,u'7e forme de prise \u2014 la forme normale, couleur apparence + glyphe.',ORANGE,10)

# --- 5 : l'apercu perime -------------------------------------------------
X5=390
s+=lab(X5,R2,'5',u'L\u2019APER\u00c7U P\u00c9RIM\u00c9',u'un aper\u00e7u qui n\u2019est plus \u00e0 jour MENT \u2014 il doit le dire')
n,h5=noeud(X5,R2+28,240,[
  {'lab':u'Rugosit\u00e9','coul':FAM['nombre'],'plein':False,'val':'0.400','glyphe':'1.0'},
  {'lab':u'Surface','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'SH'}],
  u'Mat\u00e9riau principal',u'Surface',CAT['surface'],apercu=104)
s+=n
s+=sphere(X5+8,R2+28+36,224,98)
s+='<rect x="%s" y="%s" width="224" height="98" fill="#121212" opacity="0.30"/>\n'%(X5+8,R2+28+36)
s+='<circle cx="%s" cy="%s" r="5" fill="%s"/>\n'%(X5+218,R2+28+46,ORANGE)
s+='<circle cx="%s" cy="%s" r="9" fill="none" stroke="%s" stroke-width="1.2" opacity="0.45"/>\n'%(X5+218,R2+28+46,ORANGE)
s+='<circle cx="%s" cy="%s" r="13" fill="none" stroke="%s" stroke-width="1" opacity="0.2"/>\n'%(X5+218,R2+28+46,ORANGE)
s+=tt(X5,R2+28+h5+20,u'voile de 30 % + un point qui PULSE au coin haut-droit,',TXT3,10)
s+=tt(X5,R2+28+h5+33,u'tant que le rendu n\u2019est pas \u00e0 jour.',TXT3,10)
s+=tt(X5,R2+28+h5+51,u'\u26a0 l\u2019aper\u00e7u d\u2019un mat\u00e9riau est un RENDU, donc il co\u00fbte : il se',ORANGE,10)
s+=tt(X5,R2+28+h5+64,u'recalcule \u00c0 LA FIN d\u2019un geste, JAMAIS pendant qu\u2019on tire',ORANGE,10)
s+=tt(X5,R2+28+h5+77,u'un curseur.',ORANGE,10)

# --- 6 : la regle absolue ------------------------------------------------
X6=690
s+=lab(X6,R2,'6',u'\u26d4 LA R\u00c8GLE ABSOLUE \u2014 D\u00c9CID\u00c9E',u'un graphe de mat\u00e9riau n\u2019a JAMAIS de fil d\u2019ex\u00e9cution. Jamais.')
mg=[(X6,u'Texture image',u'Texture',CAT['texture'],
        [{'lab':u'Couleur','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'RVB'}]),
    (X6+210,u'M\u00e9langer',u'Couleur',CAT['outil'],
        [{'lab':u'A','coul':FAM['appar'],'plein':True,'branchee':True,'glyphe':'RVB'},
         {'lab':u'R\u00e9sultat','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'RVB'}]),
    (X6+420,u'Mat\u00e9riau',u'Surface',CAT['surface'],
        [{'lab':u'Couleur','coul':FAM['appar'],'plein':True,'branchee':True,'glyphe':'RVB'},
         {'lab':u'Surface','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'SH'}])]
hs=[]
for x,t,so,c,rows in mg:
    n,hh=noeud(x,R2+34,175,rows,t,so,c); s+=n; hs.append(hh)
s+=fil(X6+175,R2+34+42,X6+210,R2+34+42,FAM['appar'],2)
s+=fil(X6+385,R2+34+66,X6+420,R2+34+42,FAM['appar'],2)
s+='<path d="M%s %sv%s" stroke="%s" stroke-width="1.2" stroke-dasharray="3 3"/>\n'%(X6-8,R2+34,max(hs),ORANGE)
s+=tt(X6,R2+34+max(hs)+16,u'\u2191 le bord gauche d\u2019en-t\u00eate est NET : aucune prise d\u2019ex\u00e9cution, nulle part dans le graphe.',ORANGE,10)

cy6=R2+34+max(hs)+30
c6,hc6=cartouche(X6,cy6,595,[
  u'1 \u00b7 tout n\u0153ud de mat\u00e9riau porte le filet P\u00c9TROLE \u2014 un filet orange ici est un BUG, pas un cas ;',
  u'2 \u00b7 aucune prise \u00e0 pointe n\u2019appara\u00eet dans un \u00e9diteur de mat\u00e9riau ;',
  u'3 \u00b7 la biblioth\u00e8que ne PROPOSE pas les familles \u00e0 ex\u00e9cution : elles ne sont pas gris\u00e9es, elles sont ABSENTES ;',
  u'4 \u00b7 l\u2019entr\u00e9e d\u2019ex\u00e9cution vivant sur l\u2019en-t\u00eate, le bord gauche d\u2019en-t\u00eate est NET. C\u2019est gratuit, et \u00e7a se voit \u00e0 25 %.'],
  u'quatre cons\u00e9quences pour le dessin, toutes obligatoires')
s+=c6

# --- 7 : l'apercu au dezoom ----------------------------------------------
X7=1360
s+=lab(X7,R2,'7',u'L\u2019APER\u00c7U AU D\u00c9ZOOM',u'le SEUL \u00e9l\u00e9ment du n\u0153ud qui GAGNE en importance quand on d\u00e9zoome')
# 100 %
n,h7=noeud(X7,R2+34,200,[
  {'lab':u'Gamma','coul':FAM['nombre'],'plein':False,'val':'2.200','glyphe':'1.0'},
  {'lab':u'Couleur','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'RVB'}],
  u'Texture image',u'Texture',CAT['texture'],apercu=90)
s+=n
s+=image_apercu(X7+8,R2+34+36,184,84)
s+=tt(X7,R2+34+h7+18,u'100 % \u2014 vignette compl\u00e8te, 96 px',TXT2,10.5,'600')
# 55 %
x55=X7+230
s+='<rect x="%s" y="%s" width="130" height="86" fill="%s" stroke="%s"/>\n'%(x55,R2+34,CORPS,FILET)
s+='<path d="M%s %sa5 5 0 0 1 5 -5 h120 a5 5 0 0 1 5 5 v11 H%s z"  fill="%s"/>\n'%(x55,R2+39,x55,CAT['texture'])
s+='<rect x="%s" y="%s" width="130" height="2" fill="%s"/>\n'%(x55,R2+50,PETROLE)
s+=image_apercu(x55+6,R2+56,118,48)
s+=tt(x55,R2+34+104,u'55 % \u2014 vignette R\u00c9DUITE \u00e0 48 px,',TXT2,10.5,'600')
s+=tt(x55,R2+34+117,u'tout le reste du corps s\u2019efface',TXT3,10)
# 25 %
x25=X7+230
y25=R2+180
s+='<rect x="%s" y="%s" width="130" height="44" fill="#6d4534"/>\n'%(x25,y25)
s+=tt(x25,y25+62,u'25 % \u2014 la vignette devient la',TXT2,10.5,'600')
s+=tt(x25,y25+75,u'COULEUR MOYENNE, en aplat',TXT3,10)
s+=tt(X7,R2+34+h7+42,u'\u00e0 55 % on ne lit plus \u00ab Texture image \u00bb,',TXT3,10)
s+=tt(X7,R2+34+h7+55,u'mais on reconna\u00eet une brique d\u2019un coup',TXT3,10)
s+=tt(X7,R2+34+h7+68,u'd\u2019\u0153il. L\u2019aper\u00e7u EST l\u2019\u00e9tiquette, une fois',TXT3,10)
s+=tt(X7,R2+34+h7+81,u'le texte perdu.',TXT3,10)
s+=tt(X7,R2+34+h7+101,u'\u26a0 SEULE exception \u00e0 \u00ab \u00e0 25 %, le n\u0153ud est',ORANGE,10)
s+=tt(X7,R2+34+h7+114,u'un rectangle de la couleur de sa cat\u00e9gorie \u00bb :',ORANGE,10)
s+=tt(X7,R2+34+h7+127,u'pour une texture, la moyenne est PLUS',ORANGE,10)
s+=tt(X7,R2+34+h7+140,u'informative que la cat\u00e9gorie.',ORANGE,10)

# ============================================================ RANGEE 3
R3=1010

# --- 8 : le commentaire libre --------------------------------------------
s+=lab(44,R3,'8',u'LE COMMENTAIRE LIBRE \u2014 \ud83d\udd34 RIEN DANS LE CORPUS',u'aucune des douze r\u00e9f\u00e9rences n\u2019en montre un : tout ce bloc est ASSUM\u00c9 comme une invention')
s+=tt(44,R3+50,u'au repos \u2014 du TEXTE POS\u00c9 SUR LE FOND',TXT2,10.5,'600')
for i,l in enumerate([u'cette branche corrige le grain du b\u00e9ton :',
                      u'la normale seule donnait un mur trop lisse',
                      u'\u00e0 distance. \u00c0 revoir quand le LOD arrivera.']):
    s+=tt(44,R3+72+i*17,l,TXT2,13)
s+=tt(44,R3+140,u'au SURVOL \u2014 un fond appara\u00eet pour dire o\u00f9 cliquer',TXT2,10.5,'600')
s+='<rect x="38" y="%s" width="330" height="62" rx="3" fill="#1A1A1A" opacity="0.6"/>\n'%(R3+152)
for i,l in enumerate([u'cette branche corrige le grain du b\u00e9ton :',
                      u'la normale seule donnait un mur trop lisse',
                      u'\u00e0 distance. \u00c0 revoir quand le LOD arrivera.']):
    s+=tt(44,R3+172+i*17,l,TXT2,13)
c8,hc8=cartouche(44,R3+232,420,[
  u'\u2023 sans corps, sans filet, sans fond : il n\u2019est PAS un n\u0153ud et ne doit pas y ressembler ;',
  u'\u2023 13 px, #8A8A8A, align\u00e9 \u00e0 gauche, largeur redimensionnable \u00e0 la poign\u00e9e ;',
  u'\u2023 il ne se branche \u00e0 rien et ne se replie pas ;',
  u'\u2023 il passe DEVANT les fils, DERRI\u00c8RE les n\u0153uds ;',
  u'\u2023 \u00e0 25 % il DISPARA\u00ceT enti\u00e8rement \u2014 le seul objet qu\u2019on efface : illisible, il ne serait',
  u'   plus qu\u2019une tache grise qui brouille la lecture de structure, celle qu\u2019on fait \u00e0 25 %.'],
  u'PROPOS\u00c9')
s+=c8

# --- 9 : le cadre --------------------------------------------------------
X9=510
s+=lab(X9,R3,'9',u'LE CADRE \u2014 VU, et richement',u'node-based\u2026webp est la seule r\u00e9f\u00e9rence \u00e0 en montrer un, et elle en montre plus que pr\u00e9vu')
fx,fy,fw,fh=X9,R3+34,450,300
s+='<rect x="%s" y="%s" width="%s" height="%s" rx="6" fill="%s" opacity="0.08"/>\n'%(fx,fy,fw,fh,VERT)
s+='<rect x="%s" y="%s" width="%s" height="%s" rx="6" fill="none" stroke="%s" stroke-width="1.5"/>\n'%(fx,fy,fw,fh,VERT)
s+='<rect x="%s" y="%s" width="%s" height="%s" rx="4" fill="none" stroke="%s" stroke-width="1" stroke-dasharray="4 4" opacity="0.7"/>\n'%(fx+8,fy+8,fw-16,fh-16,VERT)
s+='<path d="M%s %sa6 6 0 0 1 6 -6 h%s a6 6 0 0 1 6 6 v20 H%s z" fill="%s"/>\n'%(fx,fy+6,fw-12,fx,VERT)
s+=tt(fx+12,fy+19,u'\u00c9clairage',u'#10240F',12.5,'600')
s+=tt(fx+fw-12,fy+19,u'12 n\u0153uds',u'#10240F',11,'600','end')
inner=[(fx+26,fy+46,u'Texture image',u'Texture',CAT['texture'],
        [{'lab':u'Couleur','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'RVB'}]),
       (fx+230,fy+46,u'M\u00e9langer',u'Couleur',CAT['outil'],
        [{'lab':u'A','coul':FAM['appar'],'plein':True,'branchee':True,'glyphe':'RVB'},
         {'lab':u'R\u00e9sultat','coul':FAM['appar'],'plein':True,'sortie':True,'glyphe':'RVB'}]),
       (fx+26,fy+180,u'Constante',u'Entr\u00e9e',CAT['entree'],
        [{'lab':u'Valeur','coul':FAM['nombre'],'plein':True,'sortie':True,'glyphe':'1.0'}])]
for x,y,t,so,c,rows in inner:
    n,hh=noeud(x,y,170,rows,t,so,c,erreur=VERT); s+=n
s+=fil(fx+196,fy+88,fx+236,fy+88,FAM['appar'],2)
# noeud HORS cadre
n,hh=noeud(fx+486,fy+180,150,[{'lab':u'Sortie','coul':FAM['appar'],'plein':True,'branchee':True,'glyphe':'RVB'}],
           u'Rendu',u'Sortie',CAT['sortie']); s+=n
s+=tt(fx+430,fy+180+hh+16,u'hors cadre \u2014 filet NORMAL',TXT3,10)
s+=fil(fx+406,fy+112,fx+486,fy+206,FAM['appar'],2)
# cadre replie
s+='<path d="M%s %sa6 6 0 0 1 6 -6 h%s a6 6 0 0 1 6 6 v20 H%s z" fill="%s"/>\n'%(fx,R3+358,fw*0.52,fx,VERT)
s+=tt(fx+12,R3+371,u'\u25b8 \u00c9clairage \u00b7 12 n\u0153uds',u'#10240F',12,'600')
s+=tt(fx+fw*0.52+24,R3+371,u'repli \u2014 le bandeau seul reste',TXT3,10)
s+=tt(fx+fw*0.52+24,R3+384,u'\ud83d\udd34 NON TRANCH\u00c9 : o\u00f9 aboutissent les fils qui entraient dans le cadre ?',ORANGE,10)
c9,hc9=cartouche(X9,R3+400,640,[
  u'VU \u00b7 remplissage vert \u00e0 8 % \u00b7 filet ext\u00e9rieur plein 1,5 px \u00b7 SECOND filet INT\u00c9RIEUR pointill\u00e9 \u00e0 8 px',
  u'VU \u00b7 bandeau de titre plein, texte SOMBRE sur la teinte \u00b7 compteur \u00ab 7 nodes \u00bb align\u00e9 \u00e0 droite',
  u'PROPOS\u00c9 \u00b7 le cadre passe DERRI\u00c8RE tout \u00b7 l\u2019appartenance est G\u00c9OM\u00c9TRIQUE, jamais d\u00e9clar\u00e9e',
  u'PROPOS\u00c9 \u00b7 tirer le BANDEAU d\u00e9place cadre + n\u0153uds ; tirer le CORPS ne d\u00e9place que le cadre \u2014 deux curseurs',
  u'\u2705 TRANCH\u00c9 22/08 \u00b7 la r\u00e9f\u00e9rence teinte AUSSI l\u2019en-t\u00eate des n\u0153uds contenus. \u00c7a \u00c9CRASERAIT la CAT\u00c9GORIE,',
  u'   la SEULE information qui survit au d\u00e9zoom : \u00e0 25 % il ne reste QUE ce rectangle de couleur.',
  u'   D\u00c9CID\u00c9 \u00b7 on ne teinte QUE le filet du corps. Le cadre dit D\u00c9J\u00c0 l\u2019appartenance en ENTOURANT les n\u0153uds.'],
  u'ce qui est VU, ce qui est D\u00c9CID\u00c9, ce qui reste \u00e0 trancher')
s+=c9

# --- 10 : les deux relais ------------------------------------------------
X10=1200
s+=lab(X10,R3,'10',u'LE RELAIS \u2014 DEUX OBJETS, PAS UN',u'le corpus en montre deux, et ils ne servent pas \u00e0 la m\u00eame chose')
# relais nu
y10=R3+50
s+=tt(X10,y10,u'RELAIS NU',TXT,12,'600')
s+=tt(X10,y10+15,u'ranger un fil \u2014 il ne dit rien qu\u2019on ne sache d\u00e9j\u00e0',TXT3,10)
s+=fil(X10,y10+52,X10+150,y10+52,FAM['appar'],2)
s+=prise(X10+150,y10+52,FAM['appar'],True,forme='relais')
s+=fil(X10+150,y10+52,X10+300,y10+52,FAM['appar'],2)
s+=tt(X10+320,y10+50,u'un rectangle 17 \u00d7 17 de la couleur du type,',TXT3,10)
s+=tt(X10+320,y10+63,u'rayon 2, sans corps, sans texte.',TXT3,10)
s+=tt(X10,y10+92,u'\u26a0 le relais nu est un CARR\u00c9, pas la prise 17 \u00d7 63 : il n\u2019est \u00e0 cheval sur aucun bord \u2014',ORANGE,10)
s+=tt(X10,y10+105,u'il EST le point. Lui donner la silhouette d\u2019une prise ferait chercher le n\u0153ud auquel il appartient.',ORANGE,10)
# relais nomme
y11=R3+185
s+=tt(X10,y11,u'RELAIS NOMM\u00c9 \u2014 la puce de la principale',TXT,12,'600')
s+=tt(X10,y11+15,u'VU \u00b7 elle porte une entr\u00e9e \u00c0 GAUCHE ET une sortie \u00c0 DROITE, plus un bloc-ic\u00f4ne plein \u00e0 gauche',TXT3,10)
px,py,pw,ph=X10+60,y11+38,220,34
s+='<rect x="%s" y="%s" width="%s" height="%s" rx="13" fill="%s" stroke="%s"/>\n'%(px,py,pw,ph,CORPS,FILET)
s+='<path d="M%s %sa13 13 0 0 0 -13 13 v8 a13 13 0 0 0 13 13 z" fill="#2E2770"/>\n'%(px+34,py,)
s+=tt(px+17,py+22,u'\u26d3',u'#C8B8FF',13,None,'middle')
s+=tt(px+46,py+22,u'b\u00e9ton.albedo',TXT,12)
s+=prise(px,py+ph/2.0,FAM['appar'],True)
s+=prise(px+pw,py+ph/2.0,FAM['appar'],True)
s+=fil(X10,py+ph/2.0,px,py+ph/2.0,FAM['appar'],2)
s+=fil(px+pw,py+ph/2.0,px+pw+60,py+ph/2.0,FAM['appar'],2)
s+=tt(X10,y11+100,u'ce n\u2019est donc PAS un simple point sur un fil : c\u2019est une VALEUR NOMM\u00c9E QUI PASSE.',ORANGE,10)
s+=tt(X10,y11+113,u'Son r\u00f4le : poser une source pr\u00e8s de son consommateur, \u00e0 l\u2019autre bout du graphe.',TXT3,10)
c10,hc10=cartouche(X10,y11+130,560,[
  u'relais NU      \u00b7 rectangle 17 \u00d7 17, rayon 2, couleur du type, sans corps ni texte',
  u'relais NOMM\u00c9 \u00b7 pilule \u00e0 coins 13 px, bloc-ic\u00f4ne color\u00e9 pleine hauteur \u00e0 gauche, libell\u00e9, une entr\u00e9e, une sortie'],
  u'les deux, c\u00f4te \u00e0 c\u00f4te')
s+=c10

s+=tt(34,H-24,u'corps #212121 rayon 0 \u00b7 en-t\u00eate rayon 5 \u00b7 filet P\u00c9TROLE partout (aucun graphe de mati\u00e8re n\u2019a de fil d\u2019ex\u00e9cution) \u00b7 aper\u00e7u = bloc de corps escamotable, pleine largeur \u00b7 damier 10 px sous tout alpha',TXT3,10)

ecrire('planche_05_matieres.svg',s)
