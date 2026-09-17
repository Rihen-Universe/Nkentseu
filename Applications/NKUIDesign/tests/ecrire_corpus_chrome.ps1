# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# Ecrit le CORPUS DE SONDE de l'inventaire : un fichier .nkgui par element de la chrome de
# NK3DModeler, plus TROIS NEGATIFS dont le refus prouve que les acceptations veulent dire
# quelque chose. Le corpus est une TRACE (il vit dans Build/, non versionne) ; ce script,
# lui, est l'instrument, et il se commite.
$R = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$d = Join-Path $R 'Build\sondes-nkuidesign\corpus-chrome'
if (-not (Test-Path $d)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }
Get-ChildItem -Path $d -Filter *.nkgui -File -ErrorAction SilentlyContinue | Remove-Item -Force

$fichiers = [ordered]@{
    'c1_tabbar.nkgui'       = "nkgui 0.3`n// SONDE : la bande d'onglets du modeleur.`nwidgets {`n  Panel `"hote`" {`n    TabBar `"onglets`" {`n      items = [Scene, Materiaux, Rendu]`n    }`n  }`n}`n"
    'c2_menubar.nkgui'      = "nkgui 0.3`n// SONDE : la barre de menus et un menu.`nwidgets {`n  MenuBar `"barre`" {`n    Menu `"fichier`" {`n      label = `"Fichier`"`n      MenuItem `"ouvrir`" { label = `"Ouvrir...`" }`n    }`n  }`n}`n"
    'c3_contextmenu.nkgui'  = "nkgui 0.3`nwidgets {`n  ContextMenu `"clic-droit`" {`n    MenuItem `"supprimer`" { label = `"Supprimer`" }`n  }`n}`n"
    'c4_dockspace.nkgui'    = "nkgui 0.3`nwidgets {`n  DockSpace `"racine`" {`n    Panel `"hierarchie`" { title = `"Hierarchie`" }`n  }`n}`n"
    'c5_splitter.nkgui'     = "nkgui 0.3`nwidgets {`n  Splitter `"separateur`" {`n    orientation = Horizontal`n  }`n}`n"
    'c6_expander.nkgui'     = "nkgui 0.3`nwidgets {`n  Expander `"bloc`" {`n    label = `"Transformation`"`n    Text `"x`" { text = `"X`" }`n  }`n}`n"
    'c7_arbre.nkgui'        = "nkgui 0.3`nwidgets {`n  ListBox `"hierarchie`" {`n    TreeItem `"racine`" {`n      label = `"Scene`"`n      Item `"cube`" { label = `"Cube`" }`n    }`n  }`n}`n"
    'c8_scroll_table.nkgui' = "nkgui 0.3`nwidgets {`n  Scroll `"defilement`" {`n    Table `"proprietes`" {`n      Text `"l1`" { text = `"ligne`" }`n    }`n  }`n}`n"
    'c9_callback.nkgui'     = "nkgui 0.3`n// LA FRONTIERE : le viseur 3D est une ZONE que l'application remplit.`nwidgets {`n  Callback `"viseur3d`" {`n  }`n}`n"
    'n1_titlebar.nkgui'     = "nkgui 0.3`n// NEGATIF ATTENDU : TitleBar n'existe pas dans le vocabulaire.`nwidgets {`n  TitleBar `"titre`" {`n  }`n}`n"
    'n2_rail.nkgui'         = "nkgui 0.3`n// NEGATIF ATTENDU : Rail n'existe pas.`nwidgets {`n  Rail `"rail-droit`" {`n  }`n}`n"
    'n3_statusbar.nkgui'    = "nkgui 0.3`n// NEGATIF ATTENDU : StatusBar n'existe pas.`nwidgets {`n  StatusBar `"etat`" {`n  }`n}`n"
}
foreach ($k in $fichiers.Keys) {
    [System.IO.File]::WriteAllText((Join-Path $d $k), $fichiers[$k], (New-Object System.Text.UTF8Encoding($false)))
}
Write-Output ("corpus ecrit : {0} fichier(s) dans {1}" -f $fichiers.Count, $d)
