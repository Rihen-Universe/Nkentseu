# =============================================================================
#  pilote_ui.ps1 -- PILOTE AUTOMATIQUE d'UnkenyEditor (Windows)
# =============================================================================
#  CE QU'IL FAIT
#    Lance UnkenyEditor, puis joue une suite de gestes (clics, glisser, frappe,
#    molette) et PHOTOGRAPHIE la fenetre apres chacun, dans -Dossier :
#      menus de la barre de titre et de la barre d'outils, + Entite, ajout de
#      composants, Jouer / Arreter, cloisons, champ Nom, recherche de
#      l'Outliner, double-clic (cadrer), onglet Monde, glisser d'une carte du
#      tiroir vers la vue, croix de l'onglet de scene (boite « non
#      enregistree »), Enregistrer / Ouvrir, croix de la fenetre ; et les
#      gestes du viseur : clic (choisir / deselectionner), menus du clic droit
#      (entite, vide, « Ajouter ici »), gizmos W / E / R, F (cadrer), molette
#      pressee (deplacer la vue).
#    On regarde ensuite les images : c'est un temoin VISUEL, pas un banc qui
#    rend un verdict. Le banc, lui, est `UnkenyEditor.exe --selftest`.
#
#  ⚠️ IL PREND LA SOURIS ET LE CLAVIER DU POSTE
#    Les gestes passent par SetCursorPos / mouse_event / keybd_event : ce sont
#    les VRAIES entrees de la session. Pendant qu'il tourne (une minute
#    environ), ne touchez ni a la souris ni au clavier -- vos gestes se
#    melangeraient aux siens, et les siens iraient a la fenetre qui a le focus,
#    qui n'est peut-etre plus l'editeur. Rihen l'a vu « se piloter tout seul »
#    le 2026-09-29 : c'est exactement ce que fait ce script, et il ne doit pas
#    tourner a l'insu de la personne assise devant l'ecran.
#    Une version qui n'emprunte rien a la session (messages adresses a la seule
#    fenetre de l'editeur, ou pilote interne a l'application) reste a ecrire.
#
#  LANCER (PowerShell, depuis la racine du depot)
#    .\Applications\UnkenyEditor\outils\pilote_ui.ps1 `
#        -Exe .\Build\Bin\Debug-Windows\UnkenyEditor\UnkenyEditor.exe `
#        -Dossier $env:TEMP\pilote_unkeny
#
#  ⚠️ LES POSITIONS SONT CELLES D'UNE FENETRE DE 1280 x 760, disposition par
#    defaut (voir NkEditeurPlanifier, NkEditeurChrome.cpp). Elles sont
#    regroupees dans $P ci-dessous : une disposition qui change se corrige la,
#    et nulle part ailleurs. Elles ont ete recalculees pour la barre de titre
#    maison (30 px) + les onglets de scene (26 px), puis MESUREES sur des
#    captures de cette disposition (2026-09-29) -- mais le script lui-meme n'a
#    pas ete relance depuis : a revalider a sa premiere execution.
# =============================================================================

param(
	[Parameter(Mandatory = $true)][string]$Exe,
	[Parameter(Mandatory = $true)][string]$Dossier
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$sig = @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int x, int y, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
Add-Type -TypeDefinition $sig
[W]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory -Force $Dossier | Out-Null

# ── LES POSITIONS (coordonnees CLIENT, fenetre 1280 x 760) ──────────────────
$P = @{
	# Barre de titre (y 0..30) : menus, et reduire / agrandir / fermer
	Fichier = @(33, 15); Edition = @(87, 15); Fenetre = @(144, 15); Aide = @(194, 15)
	MenuLigne1 = 40; MenuLigne2 = 60; MenuLigne3 = 80          # y des entrees d'un menu de la barre
	# Onglet de scene (y 30..56) : la croix (la place du point « modifiee » est toujours reservee)
	CroixOnglet = @(92, 44)
	# Barre d'outils (y 56..90)
	Outil = @(154, 73); Ajouter = @(262, 73); Jouer = @(332, 73); Arreter = @(390, 73)
	Appareil = @(543, 73); Reglages = @(690, 73)
	OutilsLigne1 = 100; OutilsLigne2 = 120                      # y des entrees d'un menu de la barre d'outils
	# Menu Outil : Selection, Deplacer, Tourner, Echelle, (trait), Poser, Effacer...
	OutilPoser = 187
	# Outliner (x 0..250, y 90..)
	PlusEntite = @(224, 103); Recherche = @(125, 130)
	LigneSol = @(90, 197); LigneCaisse1 = @(90, 263)
	# Details (x 940..1280)
	OngletDetails = @(968, 103); OngletMonde = @(1038, 103)
	# (30/09) « Ajouter un composant » est EN BAS des cartes (Unity) ; le menu se CHERCHE :
	# on tape, Entree prend la premiere ligne. Sous une entite vide (Transform + Hierarchie) :
	ChampNom = @(1109, 134); AjouterComposant = @(1109, 423)
	# ... une fois la zone des cartes defilee jusqu'en bas, le bouton est au pied du panneau
	CartesDetails = @(1109, 300); AjouterEnBas = @(1109, 457)
	# Vue, tiroir, cloisons
	Vue = @(595, 300); OngletJournal = @(91, 501); OngletActeurs = @(31, 501)
	# (30/09) La racine du navigateur montre ses DOSSIERS : on entre dans « Corps rigides »
	# (double-clic), ou la Caisse est la 2e carte (Balle, Caisse).
	DossierRigides = @(384, 645); CarteCaisse = @(308, 640)
	# Caisse_1 dans la vue, cadrage de depart (toute la scene, ~22 px/m) ; le vide au-dessus
	Caisse1Vue = @(463, 379); VideVue = @(600, 200)
	CloisonGauche = @(252, 300); CloisonTiroir = @(640, 486)
	# Boite « modifications non enregistrees »
	BoiteEnregistrer = @(611, 371); BoiteSans = @(700, 371); BoiteAnnuler = @(819, 371)
}

$p = Start-Process -FilePath $Exe -WorkingDirectory (Split-Path $Exe) -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 60; $i++) {
	Start-Sleep -Milliseconds 250
	$p.Refresh()
	if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
}
if ($h -eq [IntPtr]::Zero) { throw "UnkenyEditor n'a ouvert aucune fenetre" }
Start-Sleep -Milliseconds 1500
# ALT enfonce/relache : sans lui, SetForegroundWindow echoue si le script n'a
# pas deja le focus (piege deja paye par Tools/capture_fenetre.ps1).
[W]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero); [W]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
[W]::ShowWindow($h, 9) | Out-Null
[W]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 500
$rc = New-Object W+RECT
[W]::GetClientRect($h, [ref]$rc) | Out-Null
$o = New-Object W+POINT
[W]::ClientToScreen($h, [ref]$o) | Out-Null
"fenetre : client {0}x{1} a ({2},{3})" -f $rc.R, $rc.B, $o.X, $o.Y
if ($rc.R -ne 1280 -or $rc.B -ne 760) { "⚠️ taille inattendue : les positions de `$P ne valent que pour 1280 x 760" }

function Clic([int]$x, [int]$y) {
	[W]::SetCursorPos($o.X + $x, $o.Y + $y) | Out-Null; Start-Sleep -Milliseconds 120
	[W]::mouse_event(0x02, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 90
	[W]::mouse_event(0x04, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 350
}
function ClicP($pos) { Clic $pos[0] $pos[1] }
function DoubleClic($pos) {
	[W]::SetCursorPos($o.X + $pos[0], $o.Y + $pos[1]) | Out-Null; Start-Sleep -Milliseconds 150
	for ($k = 0; $k -lt 2; $k++) {
		[W]::mouse_event(0x02, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 40
		[W]::mouse_event(0x04, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
	}
	Start-Sleep -Milliseconds 400
}
function ClicSec($pos) {
	# Appui et relachement SANS attente : plus court qu'une trame. Ce clic-la
	# etait perdu avant le relachement differe de NkEditeurApp::OnEvent.
	[W]::SetCursorPos($o.X + $pos[0], $o.Y + $pos[1]) | Out-Null; Start-Sleep -Milliseconds 150
	[W]::mouse_event(0x02, 0, 0, 0, [UIntPtr]::Zero)
	[W]::mouse_event(0x04, 0, 0, 0, [UIntPtr]::Zero)
	Start-Sleep -Milliseconds 400
}
function Glisser($de, $vers) {
	[W]::SetCursorPos($o.X + $de[0], $o.Y + $de[1]) | Out-Null; Start-Sleep -Milliseconds 150
	[W]::mouse_event(0x02, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 120
	for ($k = 1; $k -le 12; $k++) {
		$x = [int]($de[0] + ($vers[0] - $de[0]) * $k / 12); $y = [int]($de[1] + ($vers[1] - $de[1]) * $k / 12)
		[W]::SetCursorPos($o.X + $x, $o.Y + $y) | Out-Null; Start-Sleep -Milliseconds 40
	}
	Start-Sleep -Milliseconds 150
	[W]::mouse_event(0x04, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 400
}
function Survol($pos) {
	[W]::SetCursorPos($o.X + $pos[0], $o.Y + $pos[1]) | Out-Null; Start-Sleep -Milliseconds 350
}
function ClicDroit($pos) {
	[W]::SetCursorPos($o.X + $pos[0], $o.Y + $pos[1]) | Out-Null; Start-Sleep -Milliseconds 120
	[W]::mouse_event(0x08, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 90
	[W]::mouse_event(0x10, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 350
}
function GlisserMilieu($de, $vers) {
	# La molette PRESSEE : le seul geste qui deplace la vue.
	[W]::SetCursorPos($o.X + $de[0], $o.Y + $de[1]) | Out-Null; Start-Sleep -Milliseconds 150
	[W]::mouse_event(0x20, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 120
	for ($k = 1; $k -le 12; $k++) {
		$x = [int]($de[0] + ($vers[0] - $de[0]) * $k / 12); $y = [int]($de[1] + ($vers[1] - $de[1]) * $k / 12)
		[W]::SetCursorPos($o.X + $x, $o.Y + $y) | Out-Null; Start-Sleep -Milliseconds 40
	}
	[W]::mouse_event(0x40, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 400
}
function Molette($pos, [int]$d) {
	[W]::SetCursorPos($o.X + $pos[0], $o.Y + $pos[1]) | Out-Null; Start-Sleep -Milliseconds 120
	[W]::mouse_event(0x0800, 0, 0, [uint32]$d, [UIntPtr]::Zero); Start-Sleep -Milliseconds 300
}
function Touche([byte]$vk) {
	[W]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
	[W]::keybd_event($vk, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 120
}
function Ctrl([byte]$vk) {
	[W]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
	Touche $vk
	[W]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 300
}
function Taper([string]$s) {
	foreach ($ch in $s.ToUpper().ToCharArray()) { Touche ([byte][char]$ch) }
	Start-Sleep -Milliseconds 250
}
function Photo([string]$nom) {
	$b = New-Object System.Drawing.Bitmap $rc.R, $rc.B
	$g = [System.Drawing.Graphics]::FromImage($b)
	$g.CopyFromScreen($o.X, $o.Y, 0, 0, $b.Size)
	$f = Join-Path $Dossier $nom
	$b.Save($f, [System.Drawing.Imaging.ImageFormat]::Png)
	$g.Dispose(); $b.Dispose()
	"photo $f"
}

# ── LE SCENARIO ─────────────────────────────────────────────────────────────
Start-Sleep -Milliseconds 1300
Photo "p00_depart.png"                                        # aucun point « modifiee » sur l'onglet
ClicP $P.Fichier; Photo "p01_menu_fichier.png"; Touche 0x1B
ClicP $P.Edition; Photo "p02_menu_edition.png"; Touche 0x1B
ClicP $P.Fenetre; Clic 180 $P.MenuLigne1; Photo "p03_sans_outliner.png"
ClicP $P.Fenetre; Clic 180 $P.MenuLigne1                        # l'Outliner revient
ClicP $P.Aide; Clic 240 $P.MenuLigne1; Photo "p04_raccourcis.png"     # l'aide s'ecrit au Journal
ClicP $P.OngletActeurs                                          # le tiroir revient aux Acteurs
ClicP $P.Outil; Clic 150 $P.OutilPoser; Photo "p05_outil_poser.png"     # la barre ne bouge pas
ClicP $P.Outil; Clic 150 $P.OutilsLigne1                        # retour a Selection
ClicP $P.Appareil; Clic 560 $P.OutilsLigne2; Photo "p06_telephone.png"  # surimpression de l'appareil
ClicP $P.Appareil; Clic 560 $P.OutilsLigne1                     # retour au Bureau
ClicP $P.Reglages; Photo "p07_menu_reglages.png"; Touche 0x1B
ClicP $P.Ajouter; Photo "p08_menu_ajouter.png"; Touche 0x1B
Molette $P.Vue 120; Molette $P.Vue 120
Start-Sleep -Milliseconds 1300
Photo "p09_zoom_sans_point.png"                               # regarder ne modifie pas la scene
# ── Les gestes du viseur ─────────────────────────────────────────────────────
# F sans selection : la vue revient sur TOUTE la scene -- le cadrage de depart,
# celui ou $P.Caisse1Vue est juste.
Touche 0x46; Start-Sleep -Milliseconds 600
ClicP $P.Caisse1Vue; Photo "p09a_caisse_choisie.png"
ClicP $P.VideVue; Photo "p09b_vide_deselectionne.png"         # le vide deselectionne
ClicDroit $P.Caisse1Vue; Survol @(540, 500); Photo "p09c_menu_entite.png"   # « Ajouter un composant ▸ »
Touche 0x1B
ClicDroit $P.VideVue; Survol @(650, 214); Photo "p09d_menu_vide.png"        # « Ajouter ici ▸ »
Clic 850 234; Photo "p09e_entite_simple_ici.png"               # posee au point du clic droit
ClicP $P.Caisse1Vue; Touche 0x57                               # W : gizmo Deplacer
Glisser @(($P.Caisse1Vue[0] + 45), $P.Caisse1Vue[1]) @(($P.Caisse1Vue[0] + 105), $P.Caisse1Vue[1])
Photo "p09f_gizmo_deplacer.png"                                # sur X seulement, accroche a 0,25 m
Touche 0x45; Photo "p09g_gizmo_tourner.png"                    # E
Touche 0x52; Photo "p09h_gizmo_echelle.png"                    # R
Touche 0x51                                                    # Q : Selection
Touche 0x46; Start-Sleep -Milliseconds 600; Photo "p09i_cadrer_selection.png"   # F
GlisserMilieu @(600, 300) @(680, 300); Photo "p09j_vue_deplacee.png"
ClicP $P.PlusEntite
ClicP $P.AjouterComposant; Photo "p10_menu_composants.png"
Taper "sprite"; Photo "p10b_menu_filtre.png"; Touche 0x0D    # la recherche, puis Entree
Molette $P.CartesDetails -960                                  # jusqu'en bas des cartes
ClicP $P.AjouterEnBas; Taper "corps rigide"; Touche 0x0D       # Corps rigide
Photo "p11_composants.png"
ClicP $P.Jouer; Start-Sleep -Milliseconds 1500; Photo "p12_en_jeu.png"
ClicP $P.Arreter; Start-Sleep -Milliseconds 400; Photo "p13_arrete.png"
Glisser $P.CloisonGauche @(330, 300); Glisser $P.CloisonTiroir @(640, 420); Photo "p14_cloisons.png"
ClicP $P.LigneSol; ClicP $P.ChampNom; Ctrl 0x41; Taper "solide"; Touche 0x0D
Photo "p15_renomme.png"
ClicP $P.Recherche; Taper "cai"; Photo "p16_recherche.png"; Ctrl 0x41; Touche 0x08; Touche 0x1B
DoubleClic $P.LigneCaisse1; Photo "p17_double_clic_cadre.png"
ClicSec $P.OngletMonde; Photo "p18_monde_clic_sec.png"; ClicP $P.OngletDetails
DoubleClic @($P.DossierRigides[0], ($P.DossierRigides[1] - 68))   # dans « Corps rigides » (tiroir releve de 66 px)
Glisser @($P.CarteCaisse[0], ($P.CarteCaisse[1] - 68)) @(560, 250); Photo "p19_carte_glissee.png"
Start-Sleep -Milliseconds 1300
Photo "p20_point_modifiee.png"
ClicP $P.CroixOnglet; Photo "p21_boite.png"
ClicP $P.BoiteAnnuler; Photo "p22_annule.png"
ClicP $P.CroixOnglet; ClicP $P.BoiteEnregistrer; Start-Sleep -Milliseconds 400
ClicP $P.OngletJournal; Photo "p23_journal.png"               # « Scene enregistree » PUIS « Scene fermee »
ClicP $P.Fichier; Clic 100 $P.MenuLigne2; Start-Sleep -Milliseconds 500; Photo "p24_ouverte.png"
$p.CloseMainWindow() | Out-Null
Start-Sleep -Milliseconds 2000
"ferme sans question (scene non modifiee) : " + $p.HasExited
if (!$p.HasExited) { Photo "p25_non_ferme.png"; $p.Kill() }
