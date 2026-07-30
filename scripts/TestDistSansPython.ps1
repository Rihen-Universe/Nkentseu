# Verifie qu'une distribution NKCode est utilisable SANS Python systeme.
#
# On ne peut pas desinstaller Python de cette machine : on s'en approche au plus
# pres en n'utilisant QUE le python.exe embarque de la distribution, avec un
# environnement vide de toute variable Python (PYTHONPATH, PYTHONHOME) et un
# PATH reduit a Windows\System32 pour qu'aucun `python`/`jenga` du PATH ne
# puisse etre trouve par erreur.
#
# Ce que ca prouve : le 3e defaut des issues beta #9/#10 (import Jenga casse
# dans la distribution parce que Unitest etait exclu du filtre) ne peut pas
# revenir sans etre detecte ici.

param([Parameter(Mandatory = $true)][string]$DistDir)

$ErrorActionPreference = "Stop"
$fail = 0

function Check($label, $ok, $detail) {
    if ($ok) { Write-Host "  [OK]    $label" }
    else { Write-Host "  [ECHEC] $label"; if ($detail) { Write-Host "          $detail" }; $script:fail++ }
}

Write-Host "=== Distribution : $DistDir ==="

$py = Join-Path $DistDir "tools\python-embed\python.exe"
$jengaSrc = Join-Path $DistDir "tools\jenga-src\Jenga"
$shim = Join-Path $DistDir "tools\jenga.cmd"
$exe = Join-Path $DistDir "NKCode.exe"

Check "NKCode.exe present" (Test-Path $exe) $exe
Check "python-embed present" (Test-Path $py) $py
Check "jenga-src/Jenga present" (Test-Path $jengaSrc) $jengaSrc
Check "shim tools/jenga.cmd present" (Test-Path $shim) $shim

# Unitest : la 3e cause de #9/#10. Son absence casse `from . import Unitest`.
Check "Jenga/Unitest present (cause #9/#10)" (Test-Path (Join-Path $jengaSrc "Unitest")) `
    "Jenga/__init__.py fait `from . import Unitest` : sans ce dossier, tout import Jenga echoue"

if (-not (Test-Path $py)) { Write-Host "`nRESULTAT : $fail echec(s)"; exit 1 }

# Environnement minimal : aucune variable Python, PATH sans python ni jenga.
$clean = @{
    "PATH"        = "$env:SystemRoot\System32;$env:SystemRoot"
    "SystemRoot"  = $env:SystemRoot
    "TEMP"        = $env:TEMP
    "TMP"         = $env:TEMP
    "USERPROFILE" = $env:USERPROFILE
}

function RunClean($file, $arguments) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $file
    $psi.Arguments = $arguments
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.WorkingDirectory = $DistDir
    $psi.EnvironmentVariables.Clear()
    foreach ($k in $clean.Keys) { $psi.EnvironmentVariables[$k] = $clean[$k] }
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $err = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    return [pscustomobject]@{ Code = $p.ExitCode; Out = $out.Trim(); Err = $err.Trim() }
}

Write-Host "`n--- Python embarque, environnement vide de toute variable Python ---"

$r = RunClean $py "-c `"import Jenga, sys; print(Jenga.__version__); print(sys.prefix)`""
Check "import Jenga (version lue)" ($r.Code -eq 0 -and $r.Out) "code=$($r.Code) err=$($r.Err)"
if ($r.Out) { Write-Host "          $($r.Out -replace "`r?`n", ' | ')" }

$r2 = RunClean $py "-c `"import Jenga.Core.Embed as E; print(sorted(n for n in dir(E) if not n.startswith('_'))[:12])`""
Check "import Jenga.Core.Embed (API embarquee)" ($r2.Code -eq 0) "code=$($r2.Code) err=$($r2.Err)"
if ($r2.Out) { Write-Host "          $($r2.Out)" }

$r3 = RunClean $py "-m Jenga --version"
Check "python -m Jenga --version (chemin du shim)" ($r3.Code -eq 0) "code=$($r3.Code) err=$($r3.Err)"
if ($r3.Out) { Write-Host "          $($r3.Out -replace "`r?`n", ' | ')" }

if (Test-Path $shim) {
    $r4 = RunClean $shim "--version"
    Check "shim tools/jenga.cmd --version" ($r4.Code -eq 0) "code=$($r4.Code) err=$($r4.Err)"
    if ($r4.Out) { Write-Host "          $($r4.Out -replace "`r?`n", ' | ')" }
}

Write-Host "`nRESULTAT : $fail echec(s)"
exit $fail
