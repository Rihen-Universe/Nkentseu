# -*- coding: utf-8 -*-
import os as _os
OUT = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..'))

FOND='#121212'; CORPS='#212121'; CTRL='#2B2B2B'; SECT='#262626'
FILET='#33333C'; TXT='#C8CCD4'; TXT2='#8A8A8A'; TXT3='#6A6A6A'
ORANGE='#F79A28'; PETROLE='#0A555F'; ROUGE='#E4443C'
FAM={'exec':'#F79A28','nombre':'#17B2EB','geom':'#C0EB81','texte':'#F2559B',
     'appar':'#D9B6A3','ref':'#81EBEB','quel':'#9AA3AD'}
CAT={'surface':'#2a6b6b','texture':'#8a6b2a','outil':'#4a6b8a','entree':'#0A555F',
     'flot':'#8a5a2a','variable':'#6b4a8a','sortie':'#F79A28','erreur':'#8a3a30'}
PW,PH=6,22

def head(w,h,titre,sous):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
      'font-family="Segoe UI, Inter, sans-serif">\n<defs>\n'
      '<pattern id="grille" width="22" height="22" patternUnits="userSpaceOnUse">'
      '<circle cx="1.5" cy="1.5" r="1" fill="#1C1C1C"/></pattern>\n'
      '<pattern id="hachure" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
      '<rect width="4" height="8" fill="#3A3A44" opacity="0.55"/></pattern>\n'
      '<pattern id="damier" width="10" height="10" patternUnits="userSpaceOnUse">'
      '<rect width="10" height="10" fill="#3a3a3a"/><rect width="5" height="5" fill="#565656"/>'
      '<rect x="5" y="5" width="5" height="5" fill="#565656"/></pattern>\n'
      '<linearGradient id="rampe" x1="0" x2="1"><stop offset="0%%" stop-color="#0A555F"/>'
      '<stop offset="45%%" stop-color="#3aa0a8"/><stop offset="75%%" stop-color="#F79A28"/>'
      '<stop offset="100%%" stop-color="#ffe9c2"/></linearGradient>\n'
      '<linearGradient id="ciel" x1="0" y1="0" x2="0" y2="1"><stop offset="0%%" stop-color="#2b4a7a"/>'
      '<stop offset="55%%" stop-color="#d99a5a"/><stop offset="100%%" stop-color="#141414"/></linearGradient>\n'
      '<filter id="ombre" x="-25%%" y="-25%%" width="150%%" height="150%%">'
      '<feDropShadow dx="0" dy="2" stdDeviation="3" flood-color="#000" flood-opacity="0.55"/></filter>\n'
      '</defs>\n'
      '<rect width="%d" height="%d" fill="'+FOND+'"/><rect width="%d" height="%d" fill="url(#grille)"/>\n')%(w,h,w,h,w,h,w,h) \
      + tt(34,40,titre,'#E8E8EE',21,'600') + tt(34,62,sous,TXT2,12.5)

def esc(s):
    return s.replace('&','&amp;').replace('<','&lt;').replace('>','&gt;')

def tt(x,y,s,fill=TXT,size=11.5,w=None,anchor=None,op=None):
    a=' text-anchor="%s"'%anchor if anchor else ''
    b=' font-weight="%s"'%w if w else ''
    o=' opacity="%s"'%op if op else ''
    return '<text x="%s" y="%s" fill="%s" font-size="%s"%s%s%s>%s</text>\n'%(x,y,fill,size,b,a,o,esc(s))

def titre_bloc(x,y,s):
    return tt(x,y,s,ORANGE,13,'600')

def prise(x,y,coul,plein=True,exec_=False,forme='simple'):
    px,py=x-PW/2.0,y-PH/2.0
    if exec_:
        d='M%s %sh%sl%s %sl%s %sh%sz'%(px,py,PW*0.6,PW*0.9,PH/2.0,-PW*0.9,PH/2.0,-PW*0.6)
        f=coul if plein else 'none'
        return '<path d="%s" fill="%s" stroke="%s" stroke-width="1.6"/>\n'%(d,f,coul)
    f=coul if plein else CORPS
    s=''
    if forme=='simple':
        s='<rect x="%s" y="%s" width="%s" height="%s" rx="2" fill="%s" stroke="%s" stroke-width="1.6"/>\n'%(px,py,PW,PH,f,coul)
    elif forme=='tableau':
        seg=(PH-4)/3.0
        for i in range(3):
            s+='<rect x="%s" y="%s" width="%s" height="%s" rx="1.5" fill="%s" stroke="%s" stroke-width="1.3"/>\n'%(px,py+i*(seg+2),PW,seg,f,coul)
    elif forme=='dico':
        seg=(PH-4)/3.0; hw=(PW-1.6)/2.0
        for i in range(3):
            yy=py+i*(seg+2)
            s+='<rect x="%s" y="%s" width="%s" height="%s" rx="1" fill="%s" stroke="%s" stroke-width="1.2"/>\n'%(px,yy,hw,seg,f,coul)
            s+='<rect x="%s" y="%s" width="%s" height="%s" rx="1" fill="%s" stroke="%s" stroke-width="1.2"/>\n'%(px+hw+1.6,yy,hw,seg,f,coul)
    elif forme=='relais':
        s='<rect x="%s" y="%s" width="%s" height="%s" rx="2" fill="%s" stroke="%s" stroke-width="1.6"/>\n'%(x-8,y-8,16,16,f,coul)
    elif forme=='tiret':
        s='<rect x="%s" y="%s" width="%s" height="%s" rx="2" fill="none" stroke="%s" stroke-width="1.6" stroke-dasharray="3 2.5"/>\n'%(px,py,PW,PH,coul)
    return s

def pastille(x,y,coul,glyphe):
    return ('<rect x="%s" y="%s" width="30" height="15" rx="2" fill="%s" opacity="0.22"/>\n'%(x,y-7.5,coul)
            + tt(x+15,y+4,glyphe,coul,8.5,'600','middle'))

def noeud(x,y,w,rows,titre,sous=None,cat='#4a6b8a',exec_=False,apercu=0,
          selection=None,erreur=None,corps=CORPS,opacite=1.0,inconnu=False,hachure=False,
          etat=None,aide=True,marque=True,filet=None,tete_clair=False,arrondi_debut=False):
    eh=21 if not sous else 30
    hh=eh+4+apercu+len(rows)*24+(18 if etat else 0)+8
    s='<g filter="url(#ombre)" opacity="%s">\n'%opacite
    s+='<rect x="%s" y="%s" width="%s" height="%s" fill="%s" stroke="%s" stroke-width="%s"/>\n'%(
        x,y,w,hh,corps,erreur or FILET,1.5 if erreur else 1)
    tete='url(#hachure)' if inconnu else cat
    r0=12 if arrondi_debut else 5
    s+='<path d="M%s %sa%s %s 0 0 1 %s -%s h%s a5 5 0 0 1 5 5 v%s H%s z" fill="%s"/>\n'%(
        x,y+r0,r0,r0,r0,r0,w-r0-5,eh-5,x,tete)
    if tete_clair:
        s+='<path d="M%s %sa%s %s 0 0 1 %s -%s h%s a5 5 0 0 1 5 5 v%s H%s z" fill="#FFFFFF" opacity="0.15"/>\n'%(
            x,y+r0,r0,r0,r0,r0,w-r0-5,eh-5,x)
    if marque: s+=tt(x+8,y+(15 if not sous else 14),'\u25bc','#F3E9DA',8)
    s+=tt(x+20,y+(15 if not sous else 14),titre,'#EEF2F6',12.5,'600')
    if sous: s+=tt(x+20,y+26,sous,'#EEF2F6',9.5,None,None,0.6)
    if aide: s+=tt(x+w-10,y+(15 if not sous else 14),'?','#9FB4C8',12,None,'end')
    fil = filet if filet else ('#5A5A64' if inconnu else (ORANGE if exec_ else PETROLE))
    if filet=='pointille':
        s+='<rect x="%s" y="%s" width="%s" height="2.5" fill="%s" mask="none" opacity="1"/>\n'%(x,y+eh,w,'#2A2A2A')
        s+='<path d="M%s %sH%s" stroke="%s" stroke-width="2.5" stroke-dasharray="6 4"/>\n'%(x,y+eh+1.25,x+w,ORANGE)
    else:
        s+='<rect x="%s" y="%s" width="%s" height="2.5" fill="%s"/>\n'%(x,y+eh,w,fil)
    if hachure:
        s+='<rect x="%s" y="%s" width="%s" height="%s" fill="url(#hachure)"/>\n'%(x+1,y+eh+3,w-2,hh-eh-4)
    cy=y+eh+6
    if apercu:
        s+='<rect x="%s" y="%s" width="%s" height="%s" fill="url(#damier)"/>\n'%(x+8,cy,w-16,apercu-6)
        s+='<rect x="%s" y="%s" width="%s" height="%s" fill="url(#ciel)" opacity="0.92"/>\n'%(x+8,cy,w-16,apercu-6)
        cy+=apercu
    for r in rows:
        yc=cy+12
        if r.get('section'):
            s+='<rect x="%s" y="%s" width="%s" height="20" fill="%s"/>\n'%(x+1,cy+2,w-2,SECT)
            s+=tt(x+10,yc+4,('\u25be ' if r.get('ouvert') else '\u25b8 ')+r['section'],TXT,11)
            cy+=24; continue
        if r.get('ajout'):
            s+='<rect x="%s" y="%s" width="%s" height="18" rx="2" fill="%s"/>\n'%(x+8,cy+3,w-16,CTRL)
            s+=tt(x+w/2.0,yc+4,r['ajout'],TXT3,10.5,None,'middle'); cy+=24; continue
        if r.get('note') is not None:
            s+=tt(x+10,yc+4,r['note'],r.get('coul',TXT3),r.get('size',10)); cy+=24; continue
        lx=x+12
        if r.get('coul') and not r.get('sortie'):
            s+=prise(x,yc,r['coul'],r.get('plein',False),r.get('exec'),r.get('forme','simple'))
            if r.get('glyphe'): s+=pastille(x+8,yc,r['coul'],r['glyphe']); lx=x+44
        if r.get('sub'): lx+=16
        if r.get('sortie'):
            s+=prise(x+w,yc,r['coul'],r.get('plein',False),r.get('exec'),r.get('forme','simple'))
            gx=x+w-14
            if r.get('glyphe'): s+=pastille(x+w-42,yc,r['coul'],r['glyphe']); gx=x+w-50
            s+=tt(gx,yc+4,r['lab'],ORANGE if r.get('plein') else TXT,11.5,None,'end')
        else:
            col=ORANGE if r.get('plein') else (TXT3 if r.get('sub') else TXT)
            s+=tt(lx,yc+4,r['lab'],col,11.5)
            if r.get('branchee'): s+=tt(x+w-10,yc+4,'branch\u00e9e',TXT3,10,None,'end')
            elif r.get('ctrl')=='case':
                s+='<rect x="%s" y="%s" width="12" height="12" rx="2" fill="none" stroke="%s" stroke-width="1.5"/>\n'%(x+w-24,yc-6,TXT2)
            elif r.get('ctrl')=='nuancier':
                s+='<rect x="%s" y="%s" width="36" height="14" fill="url(#damier)"/>\n'%(x+w-48,yc-7)
                s+='<rect x="%s" y="%s" width="36" height="14" fill="%s" stroke="#5a5a64"/>\n'%(x+w-48,yc-7,r.get('val','#7ec8e8'))
            elif r.get('ctrl')=='liste':
                s+='<rect x="%s" y="%s" width="112" height="17" rx="2" fill="%s"/>\n'%(x+w-124,yc-8,CTRL)
                s+=tt(x+w-118,yc+4,r.get('val',''),TXT,10.5)+tt(x+w-18,yc+4,'\u25be',TXT2,9)
            elif r.get('ctrl')=='lecture':
                s+=tt(x+w-12,yc+4,r.get('val',''),TXT3,10.5,None,'end')
            elif r.get('val') is not None:
                s+='<rect x="%s" y="%s" width="92" height="17" rx="2" fill="%s"/>\n'%(x+w-104,yc-8,CTRL)
                s+=tt(x+w-16,yc+4,r['val'],TXT,10.5,None,'end')
        cy+=24
    if etat: s+=tt(x+10,cy+11,etat[0],etat[1] if len(etat)>1 else TXT3,10)
    if selection:
        s+='<rect x="%s" y="%s" width="%s" height="%s" fill="none" stroke="%s" stroke-width="1.8"/>\n'%(x-1.5,y-1.5,w+3,hh+3,selection)
    s+='</g>\n'
    return s,hh

def fil(x1,y1,x2,y2,coul,ep=2,style='simple'):
    dx=max(40,abs(x2-x1)*0.45)
    d='M%s %sC%s %s %s %s %s %s'%(x1,y1,x1+dx,y1,x2-dx,y2,x2,y2)
    if style=='simple': return '<path d="%s" fill="none" stroke="%s" stroke-width="%s"/>\n'%(d,coul,ep)
    if style=='pointille': return '<path d="%s" fill="none" stroke="%s" stroke-width="%s" stroke-dasharray="5 4"/>\n'%(d,coul,ep)
    if style=='tableau':
        return ('<path d="%s" fill="none" stroke="%s" stroke-width="1.4" transform="translate(0,-1.7)"/>\n'%(d,coul)
               +'<path d="%s" fill="none" stroke="%s" stroke-width="1.4" transform="translate(0,1.7)"/>\n'%(d,coul))
    if style=='dico':
        return ('<path d="%s" fill="none" stroke="%s" stroke-width="1.4" transform="translate(0,-1.7)"/>\n'%(d,coul)
               +'<path d="%s" fill="none" stroke="%s" stroke-width="1.4" stroke-dasharray="3 3" transform="translate(0,1.7)"/>\n'%(d,coul))
    return ''

def cartouche(x,y,w,lignes,titre=None):
    h=17*len(lignes)+(28 if titre else 14)+8
    s='<rect x="%s" y="%s" width="%s" height="%s" rx="4" fill="#1A1A1A" stroke="#2E2E36"/>\n'%(x,y,w,h)
    cy=y+(24 if titre else 22)
    if titre: s+=tt(x+14,cy,titre,ORANGE,12,'600'); cy+=23
    for l in lignes:
        s+=tt(x+14,cy,l,TXT2,10.5); cy+=17
    return s,h

def ecrire(nom,contenu):
    # ATTENTION : c est ELLE qui ecrit le </svg> final. Un script de planche qui
    # l ajoute aussi produit un SVG mal forme -- arrive le 23/08 sur p07.
    import io
    # Les demi-caracteres (paires de substitution) ecrits en deux moities dans les
    # scripts de planche sont RECOMBINES ici, pas jetes : les jeter faisait
    # disparaitre les marqueurs (rond rouge des points NON TRANCHES) sans que rien
    # ne le signale -- le fichier sortait valide, juste amute.
    contenu=contenu.encode('utf-16','surrogatepass').decode('utf-16','replace')
    contenu=u''.join(ch for ch in contenu if not (0xD800<=ord(ch)<=0xDFFF))
    with io.open(OUT+'/'+nom,'w',encoding='utf-8') as f:
        f.write(contenu+'</svg>\n')
    print('ecrit', nom)


def rendre(nom, w, h, mini=80000):
    """Rend le SVG en PNG, et REFUSE les deux pieges qui produisaient un
    fichier valide et faux.

    1. L'URL est construite ABSOLUE et prefixee file:/// . Passe en chemin
       relatif, Edge prend l'argument pour un NOM D'HOTE, echoue en DNS et
       capture SA PROPRE PAGE D'ERREUR : le PNG sort a la bonne taille.
    2. Le poids du PNG est verifie. C'etait le SEUL indice du piege n°1 :
       ~40 Ko pour une page d'erreur, ~200 Ko pour une vraie planche. Un
       controle de taille minimale coute une ligne ; ne pas l'avoir a coute
       une planche rendue trois fois avant qu'on regarde.

    Ne remplace PAS le coup d'oeil : un PNG de bon poids peut dessiner une
    regle a l'envers. Voir la regle en tete du LISEZMOI.
    """

    # PIEGE 3 -- MESURE le 23/08, et il a produit un PNG PARFAITEMENT CONFORME :
    # bon poids, bonnes dimensions, et pourtant le rendu d une PAGE D ERREUR du
    # navigateur (« Extra content at the end of the document »). Un </svg> en
    # double suffit. Les deux controles precedents ne pouvaient pas le voir : la
    # page d erreur du navigateur fait exactement la taille demandee.
    # => valider le XML AVANT d appeler le navigateur. Ne jamais rendre ce qu on
    #    n a pas valide.
    import xml.etree.ElementTree as _E
    _svg = OUT + '/' + nom + '.svg'
    try:
        _E.parse(_svg)
    except Exception as _ex:
        # ET ON SUPPRIME LE PNG PRECEDENT. Le laisser serait pire que de ne rien
        # controler : il est VALIDE, PLAUSIBLE et PERIME -- on regarderait une
        # image d hier en croyant voir le travail d aujourd hui. C est la meme
        # regle que pour le PNG tronque : un controle qui echoue ne laisse
        # aucun artefact derriere lui, meme un artefact qu il n a pas produit.
        _png = OUT + '/' + nom + '.png'
        _vieux = _os.path.exists(_png)
        if _vieux:
            _os.remove(_png)
        raise SystemExit('SVG MAL FORME, rien n a ete rendu%s : %s.svg -- %s'
                         % (' (PNG perime supprime)' if _vieux else '', nom, _ex))

    import os, subprocess
    svg = os.path.join(OUT, nom + '.svg')
    png = os.path.join(OUT, nom + '.png')
    if not os.path.isfile(svg):
        raise IOError('SVG absent : ' + svg)
    url = 'file:///' + os.path.abspath(svg).replace(chr(92), '/')
    exe = r'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'
    subprocess.run([exe, '--headless=new', '--disable-gpu',
                    '--screenshot=' + png,
                    '--window-size=%d,%d' % (w, h),
                    '--allow-file-access-from-files', url],
                   check=False, capture_output=True)
    if not os.path.isfile(png):
        raise IOError('aucun PNG produit : ' + png)
    taille = os.path.getsize(png)
    if taille < mini:
        raise IOError(
            'PNG SUSPECT : %d octets (< %d). Edge a probablement capture sa '
            'page d erreur au lieu de la planche. Verifier l URL.' % (taille, mini))
    # 3. Les DIMENSIONS du PNG sont comparees au viewBox du SVG. Piege
    #    rencontre le 22/08 : le cartouche d une planche a grandi, le SVG est
    #    passe a 1735 de haut, et la fenetre de capture valait toujours 1580 --
    #    le bas etait coupe NET, sans erreur, avec un poids parfaitement
    #    plausible. Une valeur de fenetre figee dans une note ne protege de
    #    rien ; seule une comparaison le fait.
    import re, struct, io
    tete = io.open(svg, encoding='utf-8').read(400)
    vb = re.search(r'viewBox="0 0 (\d+) (\d+)"', tete)
    if vb:
        attendu = (int(vb.group(1)), int(vb.group(2)))
        d = open(png, 'rb').read(33)
        reel = struct.unpack('>II', d[16:24])
        if reel != attendu:
            # On SUPPRIME le PNG faux avant de lever. Sinon le controle detecte
            # -- et laisse quand meme sur le disque un fichier tronque qui a
            # l air normal. Mesure le 23/08 : un test du garde-fou lui-meme
            # avait laisse planche_04_etats.png a 1780x1100 pendant une heure.
            # Un controle qui echoue ne doit pas laisser d artefact derriere lui.
            os.remove(png)
            raise IOError(
                'PNG TRONQUE (et supprime) : %dx%d rendu pour un viewBox de '
                '%dx%d. Passer --window-size=%d,%d.'
                % (reel[0], reel[1], attendu[0], attendu[1], attendu[0], attendu[1]))
        print('rendu %s.png (%d octets, %dx%d = viewBox)'
              % (nom, taille, reel[0], reel[1]))
    else:
        print('rendu %s.png (%d octets, viewBox illisible)' % (nom, taille))
    return png
