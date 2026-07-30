# Verifie qu'un utilisateur peut enregistrer SA PROPRE toolchain, sans Python
# systeme, et avec un chemin CONTENANT UNE ESPACE (cas le plus courant sur
# Windows : « C:\Program Files\... », ou un nom d'utilisateur compose, le
# fichier JSON temporaire de NKCode vivant sous %USERPROFILE%).
#
# Le registre global vise est un dossier JETABLE via JENGA_CONFIG_DIR : le
# ~/.jenga reel de l'utilisateur n'est jamais touche.

param(
    [Parameter(Mandatory = $true)][string]$DistDir,
    [Parameter(Mandatory = $true)][string]$ConfigDir
)

$ErrorActionPreference = "Stop"
$fail = 0
function Check($label, $ok, $detail) {
    if ($ok) { Write-Host "  [OK]    $label" }
    else { Write-Host "  [ECHEC] $label"; if ($detail) { Write-Host "          $detail" }; $script:fail++ }
}

$shim = Join-Path $DistDir "tools\jenga.cmd"
$clangBin = Join-Path $DistDir "tools\compilers\llvm-mingw\bin"

# Dossier de travail avec une ESPACE dans le nom, exprès.
$spaced = "D:\tmp\dossier avec espace"
New-Item -ItemType Directory -Force -Path $spaced | Out-Null
New-Item -ItemType Directory -Force -Path $ConfigDir | Out-Null
$tcJson = Join-Path $spaced "ma_toolchain.json"        # AVEC BOM (Notepad, PS 5.1)
$tcJsonNoBom = Join-Path $spaced "ma_toolchain_nobom.json"

$body = @"
{
  "type": "clang",
  "target": { "os": "windows", "arch": "x86_64", "env": "gnu" },
  "cc": "$($clangBin.Replace('\','/'))/clang.exe",
  "cxx": "$($clangBin.Replace('\','/'))/clang++.exe",
  "ar": "$($clangBin.Replace('\','/'))/llvm-ar.exe"
}
"@

# Deux variantes du MEME contenu : avec BOM (ce que produisent Notepad et
# `Out-File -Encoding utf8` sous PowerShell 5.1) et sans BOM. Les deux doivent
# passer : un utilisateur qui redige son JSON a la main sous Windows obtient
# presque toujours un BOM, et json.load echoue dessus avec
# « Expecting value: line 1 column 1 (char 0) » si la lecture n'est pas en
# 'utf-8-sig'.
$body | Out-File -FilePath $tcJson -Encoding utf8
[System.IO.File]::WriteAllText($tcJsonNoBom, $body, (New-Object System.Text.UTF8Encoding($false)))

$bom = [System.IO.File]::ReadAllBytes($tcJson)[0..2]
Check "variante AVEC BOM ecrite (EF BB BF)" (($bom -join ',') -eq '239,187,191') ($bom -join ',')
$nb = [System.IO.File]::ReadAllBytes($tcJsonNoBom)[0..2]
Check "variante SANS BOM ecrite" (($nb -join ',') -ne '239,187,191') ($nb -join ',')
Write-Host "          dossier : $spaced"

$clean = @{
    "PATH"             = "$clangBin;$(Join-Path $DistDir 'tools');$env:SystemRoot\System32;$env:SystemRoot"
    "SystemRoot"       = $env:SystemRoot
    "TEMP"             = $env:TEMP
    "TMP"              = $env:TEMP
    "USERPROFILE"      = $env:USERPROFILE
    "JENGA_CONFIG_DIR" = $ConfigDir   # registre JETABLE
}

function RunClean($arguments) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $shim
    $psi.Arguments = $arguments
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.WorkingDirectory = $spaced
    $psi.EnvironmentVariables.Clear()
    foreach ($k in $clean.Keys) { $psi.EnvironmentVariables[$k] = $clean[$k] }
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd(); $err = $p.StandardError.ReadToEnd(); $p.WaitForExit()
    return [pscustomobject]@{ Code = $p.ExitCode; Out = $out; Err = $err }
}

Write-Host "`n--- registre JETABLE : $ConfigDir (le ~/.jenga reel n'est pas touche) ---"

$r = RunClean "config toolchain list"
Check "config toolchain list aboutit" ($r.Code -eq 0) "code=$($r.Code)`n$($r.Err.Trim())"

Write-Host "`n--- ajout, JSON SANS BOM, chemin quote avec espace ---"
$add0 = RunClean "config toolchain add MaToolchainNoBom `"$tcJsonNoBom`""
Check "config toolchain add (sans BOM) aboutit" ($add0.Code -eq 0) "code=$($add0.Code)`n$($add0.Err.Trim())"

Write-Host "`n--- ajout, JSON AVEC BOM (cas Notepad / PowerShell) ---"
$add = RunClean "config toolchain add MaToolchainTest `"$tcJson`""
Check "config toolchain add (avec BOM) aboutit" ($add.Code -eq 0) "code=$($add.Code)`n$($add.Err.Trim())"

$after = RunClean "config toolchain list"
Check "les deux toolchains apparaissent dans la liste" `
    (($after.Out -match "MaToolchainTest") -and ($after.Out -match "MaToolchainNoBom")) $after.Out.Trim()
Write-Host "          $((($after.Out -split "`r?`n" | Where-Object { $_ -match 'MaToolchain' }) -join ' | '))"

Write-Host "`n--- la toolchain est-elle UTILISABLE pour construire ? ---"
$avail = RunClean "info"
if ($avail.Out -match "MaToolchainTest") { Check "visible dans jenga info (selecteur de compilateur)" $true }
else { Write-Host "  [note]  non listee par 'jenga info' hors workspace : verifier dans NKCode" }

Write-Host "`n--- retrait ---"
$rm = RunClean "config toolchain remove MaToolchainTest"
Check "config toolchain remove aboutit" ($rm.Code -eq 0) "code=$($rm.Code)`n$($rm.Err.Trim())"
$rm2 = RunClean "config toolchain remove MaToolchainNoBom"
Check "config toolchain remove (2e) aboutit" ($rm2.Code -eq 0) "code=$($rm2.Code)`n$($rm2.Err.Trim())"
$final = RunClean "config toolchain list"
Check "les toolchains ont bien disparu" (-not ($final.Out -match "MaToolchain")) $final.Out.Trim()

Write-Host "`n--- commande d'installation automatique de toolchains ---"
$inst = RunClean "install --help"
Check "jenga install --help aboutit" ($inst.Code -eq 0) "code=$($inst.Code)`n$($inst.Err.Trim())"
$names = ($inst.Out -split "`r?`n" | Where-Object { $_ -match "zig|emsdk|android|harmony|macos|clang|llvm" } | Select-Object -First 6) -join "`n          "
if ($names) { Write-Host "          $names" }

Write-Host "`nRESULTAT : $fail echec(s)"
exit $fail
