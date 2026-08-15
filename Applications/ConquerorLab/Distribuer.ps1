# =============================================================================
# Distribuer.ps1 — fabrique le KIT STAGIAIRE de ConquerorLab.
#
#   ./Distribuer.ps1              construit puis assemble le kit
#   ./Distribuer.ps1 -NoBuild     assemble seulement (binaire deja compile)
#   ./Distribuer.ps1 -Zip         produit en plus une archive .zip
#
# CE QU'ON DONNE, ET POURQUOI PAS PLUS
# ------------------------------------
# On ne donne PAS tout Nkentseu : ni le systeme de build, ni les .cpp du moteur,
# ni ce qui vit AU-DESSUS de NKCanvas / NKGui. Un module de regles n'a aucune
# raison de dessiner.
#
# On donne en revanche TOUTE LA PILE EN DESSOUS — NkString, NkVector, NkFormat,
# math::, le journal, les fichiers, les threads — en en-tetes ET en
# bibliotheques. Sans cela le stagiaire reecrit une chaine de caracteres a la
# main, et ce temps est vole au jeu.
#
# On ne donne PAS non plus « juste l'exe ». L'atelier COMPILE le .cpp du
# stagiaire a l'execution : il lui faut les en-tetes, les .lib, et un
# compilateur.
#
# Le kit contient donc exactement :
#   l'executable + les trois DLL d'execution du compilateur
#   les en-tetes de la pile, sans un seul .cpp
#   les bibliotheques statiques correspondantes
#   les trois dossiers de depot (rules / ai / boards), vides et prets
#   les exemples, le cours en PDF, et un LISEZMOI
#
# LA DISPOSITION EST IMPOSEE PAR LE CODE, ET CE N'EST PLUS CELLE DU DEPOT.
# `FindRepoRoot` (main.cpp) remonte depuis le dossier de l'exe en cherchant DEUX
# marqueurs ; celui du kit est `include/Conqueror/ConquerorRulesABI.h`, a plat.
# `NkcLayout` bascule alors en mode Kit :
#
#     include/     UNE seule racine d'inclusion (au lieu de treize)
#     lib/         les bibliotheques statiques
#     travail/     rules | ai | boards — ou le stagiaire depose
#     exemples/    dont exemples/boards, la bibliotheque de grilles livree
#
# ⚠️ CE SCRIPT ECRIT LE LISEZMOI QUE LE STAGIAIRE LIT EN PREMIER. Le 2026-08-15,
# ce LISEZMOI decrivait encore `Build/ConquerorLab/` — la disposition du DEPOT,
# abandonnee ici meme (section 5). Un stagiaire qui suivait le mode d'emploi
# deposait donc son travail dans un dossier que l'atelier ne lit jamais, et rien
# ne le lui disait. Si la disposition rechange, LE LISEZMOI CHANGE DANS LE MEME
# GESTE : il n'a pas d'autre source de verite que ce fichier.
#
# CE FICHIER DOIT RESTER EN UTF-8 AVEC BOM.
# Windows PowerShell 5.1 lit un .ps1 sans BOM comme de l'ANSI : chaque accent des
# textes ci-dessous ressortait alors en « Ã© » dans le LISEZMOI livre au
# stagiaire — le tout PREMIER fichier qu'il ouvre. Constate, puis corrige.
# =============================================================================
param([switch]$NoBuild, [switch]$Zip, [string]$Config = 'Release', [switch]$SansControle)

$ErrorActionPreference = 'Stop'

$lab   = $PSScriptRoot                              # Applications/ConquerorLab
$repo  = Split-Path (Split-Path $lab -Parent) -Parent
$kit   = Join-Path $repo "Build\ConquerorLab-Kit"
$binSrc = Join-Path $repo "Build\Bin\$Config-Windows\ConquerorLab\ConquerorLab.exe"

# ── 0. Coherence de la pile, AVANT de construire quoi que ce soit ────────────
# La pile offerte au stagiaire est decrite CINQ fois dans cinq fichiers. Le
# 2026-08-15, NKSerialization figurait dans quatre listes et manquait a la seule
# qui construit : ce script s'arretait alors sur « Bibliotheque introuvable »
# APRES une compilation complete, et tout module de stagiaire echouait au lien.
#
# Le controle coute quelques secondes et se place ICI, avant le build, parce
# qu'un controle qu'on lance a la main n'est jamais lance.
if (-not $SansControle) {
    Write-Host "Verification de la pile..." -ForegroundColor Cyan
    & python (Join-Path $lab 'verifier_la_pile.py')
    if ($LASTEXITCODE -ne 0) {
        throw ("Les listes qui decrivent la pile stagiaire divergent (voir " +
               "ci-dessus). Corrigez, ou relancez avec -SansControle si vous " +
               "savez ce que vous faites.")
    }
}

# ── 1. Construire ────────────────────────────────────────────────────────────
if (-not $NoBuild) {
    Write-Host "Compilation ($Config)..." -ForegroundColor Cyan
    Push-Location $repo
    $ErrorActionPreference = 'Continue'
    & jenga build --target ConquerorLab --config $Config 2>&1 | Out-Null
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    Pop-Location
    if ($code -ne 0) { throw "La compilation a echoue (code $code)." }
}
if (-not (Test-Path $binSrc)) { throw "Executable introuvable : $binSrc" }

# ── 2. Repartir d'un kit propre ──────────────────────────────────────────────
if (Test-Path $kit) { Remove-Item -Recurse -Force $kit }
New-Item -ItemType Directory -Force $kit | Out-Null

# ── 3. L'executable et ses DLL d'execution ───────────────────────────────────
# Le binaire est lie a la chaine MSYS2 : trois DLL doivent l'accompagner, sans
# quoi il ne demarre pas (« libstdc++-6.dll est introuvable »).
Copy-Item $binSrc $kit -Force

$runtime = @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')
$dllDirs = @('C:\msys64\mingw64\bin', 'C:\msys64\ucrt64\bin')
foreach ($dll in $runtime) {
    $src = $dllDirs | ForEach-Object { Join-Path $_ $dll } |
           Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($src) { Copy-Item $src $kit -Force }
    else      { Write-Host "  ATTENTION : $dll introuvable" -ForegroundColor Yellow }
}

# ── 4. include/ — UN SEUL dossier, UN SEUL -I ────────────────────────────────
# Le kit ne recopie PAS l'arborescence du depot. `Kernel/Foundation/NKCore/src/`
# est de la plomberie de depot : trois niveaux qui ne veulent rien dire pour
# quelqu'un qui ecrit des regles de jeu, et qui separent les modules de ce
# qu'ils incluent.
#
# On aplatit donc : chaque <module>/src/<Module> devient include/<Module>. Le
# dossier reflete alors EXACTEMENT ce que le stagiaire tape :
#
#     include/NKMath/NkFunctions.h   <->   #include "NKMath/NkFunctions.h"
#
# Cote atelier, c'est `NkcLayout` qui detecte la disposition et n'emet qu'un
# seul -I. Le marqueur du kit est include/Conqueror/ConquerorRulesABI.h.
$incDst = Join-Path $kit 'include'
New-Item -ItemType Directory -Force $incDst | Out-Null

# <chemin du dossier a copier> = <nom sous include/>
$modules = [ordered]@{
    'Applications\ConquerorLab\include\Conqueror' = 'Conqueror'
    'Kernel\Foundation\NKCore\src\NKCore'                 = 'NKCore'
    'Kernel\Foundation\NKPlatform\src\NKPlatform'         = 'NKPlatform'
    'Kernel\Foundation\NKMemory\src\NKMemory'             = 'NKMemory'
    'Kernel\Foundation\NKContainers\src\NKContainers'     = 'NKContainers'
    'Kernel\Foundation\NKMath\src\NKMath'                 = 'NKMath'
    'Kernel\System\NKLogger\src\NKLogger'                 = 'NKLogger'
    'Kernel\System\NKTime\src\NKTime'                     = 'NKTime'
    'Kernel\System\NKStream\src\NKStream'                 = 'NKStream'
    'Kernel\System\NKFileSystem\src\NKFileSystem'         = 'NKFileSystem'
    'Kernel\System\NKThreading\src\NKThreading'           = 'NKThreading'
    'Kernel\System\NKSerialization\src\NKSerialization'   = 'NKSerialization'
    'Kernel\System\NKReflection\src\NKReflection'         = 'NKReflection'
}
foreach ($m in $modules.GetEnumerator()) {
    $src = Join-Path $repo $m.Key
    if (-not (Test-Path $src)) { throw "Dossier d'en-tetes introuvable : $src" }
    $dst = Join-Path $incDst $m.Value
    New-Item -ItemType Directory -Force $dst | Out-Null
    # /E recursif · /NFL /NDL /NJH /NJS silencieux · robocopy renvoie 0-7 en succes
    & robocopy $src $dst *.h *.hpp *.inl /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copie des en-tetes echouee : $src" }
}

# ── 4bis. lib/ — les bibliotheques statiques, a plat elles aussi ─────────────
# La liste doit suivre `StackLibs` dans NkcModuleCompiler.h.
$libs = @('NKSerialization', 'NKReflection', 'NKLogger', 'NKFileSystem',
          'NKStream', 'NKThreading', 'NKTime', 'NKContainers',
          'NKMath', 'NKMemory', 'NKPlatform', 'NKCore')
$libSrc = Join-Path $repo "Build\Lib\$Config-Windows"
$libDst = Join-Path $kit  'lib'
New-Item -ItemType Directory -Force $libDst | Out-Null
foreach ($l in $libs) {
    $f = Join-Path $libSrc "$l.lib"
    if (-not (Test-Path $f)) { throw "Bibliotheque introuvable : $f" }
    Copy-Item $f $libDst -Force
}

# ── 5. travail/ — la ou le stagiaire depose son code ─────────────────────────
# « Build/ConquerorLab/ » etait un chemin de DEPOT : dans un kit, personne ne
# construit rien dans Build. Un mot francais et evident vaut mieux.
foreach ($d in @('rules', 'ai', 'boards')) {
    New-Item -ItemType Directory -Force (Join-Path $kit "travail\$d") | Out-Null
}

# ── 6. Les exemples : c'est par la qu'un stagiaire commence ──────────────────
$exSrc = Join-Path $lab 'exemples'
$exDst = Join-Path $kit 'exemples'
& robocopy $exSrc $exDst /E /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "Copie des exemples echouee." }

# La bibliotheque de grilles livree. L'atelier la recopie dans travail/boards au
# premier lancement (cf. NkcLayout::SeedBoardsDir).
$bSrc = Join-Path $lab 'boards'
if (Test-Path $bSrc) {
    $bDst = Join-Path $exDst 'boards'
    New-Item -ItemType Directory -Force $bDst | Out-Null
    & robocopy $bSrc $bDst *.json /E /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "Copie des grilles echouee." }
}

# ── 7. Le cours, en PDF ──────────────────────────────────────────────────────
$pdf = Join-Path $repo 'Documentation\cours-conqueror\Cours_ConquerorLab.pdf'
if (Test-Path $pdf) { Copy-Item $pdf $kit -Force }
else { Write-Host "  Cours PDF absent : lancer Documentation/cours-conqueror/build.ps1" -ForegroundColor Yellow }

# ── 7bis. VERSION.txt ────────────────────────────────────────────────────────
# Le kit va evoluer au rythme des retours. Sans numero, un rapport de bug ne dit
# pas CONTRE QUOI il a ete constate — et on corrige alors un bug deja corrige.
# La date de construction suffit : il n'y a qu'un producteur.
$abi = Select-String -Path (Join-Path $lab 'include\Conqueror\ConquerorRulesABI.h') `
                     -Pattern 'kRulesAbiVersion\s*=\s*(\d+)' | Select-Object -First 1
$abiNum = if ($abi) { $abi.Matches[0].Groups[1].Value } else { '?' }
$stamp  = Get-Date -Format 'yyyy-MM-dd HH:mm'

$version = @"
ConquerorLab — kit stagiaire
construit le  : $stamp
plateforme    : Windows x64 ($Config)
ABI regles    : $abiNum
ABI IA        : 2

Citez la date de construction dans vos retours : le kit evolue, et sans elle on
ne sait pas contre quelle version un probleme a ete constate.

Pour une autre plateforme (Linux, macOS, Android) : clonez le depot et lancez
    jenga build --target ConquerorLab --config Release
Le code est ecrit pour, mais n'a ete VERIFIE que sur Windows. Signalez ce qui
casse — c'est utile.
"@
Set-Content -Path (Join-Path $kit 'VERSION.txt') -Value $version -Encoding utf8

# ── 8. Le mode d'emploi ──────────────────────────────────────────────────────
$lisezmoi = @'
ConquerorLab — kit stagiaire
============================

CE QU'IL Y A DANS CE DOSSIER

  ConquerorLab.exe          l'atelier. Double-cliquez.
  *.dll                     les trois bibliotheques d'execution. Ne les
                            deplacez pas : l'exe ne demarre pas sans elles.
  Cours_ConquerorLab.pdf    LE COURS. Commencez par la.
  VERSION.txt               quelle version vous avez. A citer dans vos retours.
  exemples/                 trois modules complets, a recopier
                            (dont exemples/boards/, les grilles livrees)
  travail/                  LA OU VOUS DEPOSEZ VOTRE TRAVAIL
      travail/rules/            vos moteurs de regles (.cpp)
      travail/ai/               vos IA (.cpp)
      travail/boards/           vos grilles (.json)
  lib/                      les bibliotheques du moteur, liees a vos modules
  include/                  les en-tetes. N'y touchez pas : l'atelier s'en
                            sert pour compiler vos modules.

C'est TOUJOURS dans travail/ que vous deposez quelque chose. Les grilles
livrees sont recopiees dans travail/boards/ au premier lancement ; celles que
vous y ajoutez ensuite apparaissent au lancement suivant, ou tout de suite avec
le bouton « Rafraichir » du panneau « Regles ».

Le panneau « Regles » affiche en clair le chemin exact ou il regarde
(« Depose tes .json ici : ... »). En cas de doute, c'est lui qui dit vrai.


AVANT LE PREMIER LANCEMENT : INSTALLER UN COMPILATEUR

L'atelier compile VOTRE code C++ pendant qu'il tourne. Il lui faut donc un
compilateur sur la machine. Le plus simple :

  1. installez MSYS2            https://www.msys2.org
  2. ouvrez « MSYS2 UCRT64 » et tapez :
         pacman -S mingw-w64-ucrt-x86_64-clang
  3. relancez ConquerorLab.exe

L'atelier trouve clang tout seul dans C:\msys64\ucrt64\bin. Si vous l'avez
installe ailleurs, posez la variable d'environnement NK_CXX sur le chemin
complet de clang++.exe.

Le panneau « Modules » affiche en clair le compilateur trouve — ou vous dit
qu'il n'en a trouve aucun.


VOTRE PREMIER MODULE, EN QUATRE GESTES

  1. copiez  exemples/rules/RegleMinimale.cpp
        vers travail/rules/mes_regles.cpp
  2. changez le nom dans FillFactory (sinon deux entrees identiques au menu)
  3. sauvegardez, attendez une seconde
  4. le panneau « Modules » affiche votre module : selectionnez-le,
     puis « Nouvelle partie »

Meme chose pour une IA, avec exemples/ai/IAMinimale.cpp vers
travail/ai/.

ET POUR UNE GRILLE, IL N'Y A RIEN A COMPILER

  1. copiez  exemples/boards/hexagone_6x7.json
        vers travail/boards/ma_grille.json
  2. modifiez la liste « cells »
  3. panneau « Regles » -> « Rafraichir » -> choisissez-la dans « Grille »

En cas d'erreur de compilation, la sortie COMPLETE du compilateur s'affiche
dans le panneau « Modules ». C'est votre seul retour : lisez-la.


SI RIEN NE BOUGE

Lisez la barre d'etat, en haut du plateau. Elle dit toujours ce que l'atelier
attend : « au tour du joueur humain », « en pause — Lecture ou Pas a pas »,
« l'IA reflechit », « partie terminee »... Une interface immobile n'est pas
forcement en panne.


CE QUE VOUS AVEZ LE DROIT D'UTILISER

Toute la pile Nkentseu SOUS l'affichage vous est ouverte, et l'atelier la lie
pour vous. Vous n'avez rien a configurer :

  NkString, NkVector, NkMap, NkFormat     NKContainers
  math::NkSqrt, NkAbs, NkClamp...         NKMath
  logger.Infof(...)                       NKLogger
  NkFile, NkDirectory                     NKFileSystem
  NkThread, NkMutex, NkAtomic             NKThreading
  NkClock, NkTimer                        NKTime

Ecrivez simplement l'include :

  #include "NKContainers/String/NkString.h"
  #include "NKContainers/Sequential/NkVector.h"
  #include "NKMath/NkFunctions.h"

CE QUE VOUS N'AVEZ PAS, ET POURQUOI

  - les .cpp du moteur : vous n'avez pas a le modifier, et le compiler ne vous
    apprendrait rien ;
  - tout ce qui est AU-DESSUS de NKCanvas / NKGui : un moteur de regles ne
    dessine pas. C'est l'atelier qui affiche, a partir de la vue que vous
    exposez. Cette frontiere est le coeur du contrat.


SUR QUOI CA TOURNE

Ce kit est WINDOWS x64. Ce n'est pas une limite de conception : les portages
Linux, Android et Web de NKGui existent. C'est une limite de ce qui a ete
CONSTRUIT ET VERIFIE a ce jour, et livrer un binaire non teste serait pire que
ne rien livrer.

Sous Linux ou macOS, clonez le depot et compilez :

    jenga build --target ConquerorLab --config Release

Attendez-vous a corriger deux ou trois choses (chemins de bibliotheques,
extension .so au lieu de .dll) : le code est ecrit pour, mais n'a jamais tourne
ailleurs. Signalez ce qui casse.

Sur Android et Web, il n'y a pas de compilateur sur l'appareil : seuls les
modules compiles DANS l'application sont disponibles. L'atelier reste jouable et
mesurable ; c'est le rechargement a chaud qui disparait.


CE KIT VA EVOLUER

Ce n'est pas une livraison figee. Un message d'erreur incomprehensible, un
panneau qui deborde, une fonction du contrat qui vous manque : dites-le, et une
nouvelle version arrive.

Les deux dernieres nouveautes sont nees exactement comme ca — le panneau
« Sortie » et la possibilite de definir sa grille en C++ viennent tous deux
d'une remarque, pas d'un plan.

Citez la date de VERSION.txt dans vos retours.

AFFICHER UN ETAT POUR COMPRENDRE

Votre module a SA PROPRE copie de la pile : un logger.Infof() n'atteint pas la
console de l'atelier. Une ligne suffit a le brancher — ecrivez, une fois, apres
vos exports :

    #include "Conqueror/ConquerorLog.h"
    ...
    NKC_MODULE_LOGGING(rules)      // ou (ai) dans un module d'IA

Ensuite NKC_LOG_INFO("...") — et logger.Infof(...) si vous incluez NKLogger —
s'affichent dans le panneau « Sortie ».

Ne journalisez PAS dans une boucle chaude : le tampon fait 4096 lignes, et vous
perdriez justement ce que vous cherchez. Une trace par coup, pas par noeud.
'@
Set-Content -Path (Join-Path $kit 'LISEZMOI.txt') -Value $lisezmoi -Encoding utf8

# ── 9. Compte-rendu ──────────────────────────────────────────────────────────
$files = (Get-ChildItem -Recurse -File $kit)
$mo    = [Math]::Round(($files | Measure-Object Length -Sum).Sum / 1MB, 1)
Write-Host ""
Write-Host "Kit assemble : $kit" -ForegroundColor Green
Write-Host "  $($files.Count) fichiers, $mo Mo" -ForegroundColor Green

if ($Zip) {
    $zipPath = Join-Path $repo 'Build\ConquerorLab-Kit.zip'
    if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
    Compress-Archive -Path "$kit\*" -DestinationPath $zipPath
    $zmo = [Math]::Round((Get-Item $zipPath).Length / 1MB, 1)
    Write-Host "  Archive : $zipPath ($zmo Mo)" -ForegroundColor Green
}

# `robocopy` signale « des fichiers ont ete copies » par un code de RETOUR non
# nul (1 a 7 = succes, >= 8 = echec). Sans cette ligne, le script herite du
# dernier code robocopy et parait avoir echoue alors que le kit est complet.
exit 0
