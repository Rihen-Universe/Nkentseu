# Compile un vrai projet avec la SEULE distribution NKCode : ni Python ni
# compilateur du systeme. Reproduit exactement l'environnement que NKCode donne
# a son terminal integre (NkEmbeddedJenga::Configure prefixe au PATH
# tools/compilers/llvm-mingw/bin PUIS tools/), sur un PATH par ailleurs reduit
# a System32 et vide de toute variable Python.
#
# C'est le jalon de la Phase 12 : `jenga build` doit aboutir, et l'exe produit
# doit s'executer et recevoir ses arguments.

param(
    [Parameter(Mandatory = $true)][string]$DistDir,
    [Parameter(Mandatory = $true)][string]$Workspace
)

$ErrorActionPreference = "Stop"
$fail = 0
function Check($label, $ok, $detail) {
    if ($ok) { Write-Host "  [OK]    $label" }
    else { Write-Host "  [ECHEC] $label"; if ($detail) { Write-Host "          $detail" }; $script:fail++ }
}

$shim = Join-Path $DistDir "tools\jenga.cmd"
$clangBin = Join-Path $DistDir "tools\compilers\llvm-mingw\bin"

$clean = @{
    "PATH"        = "$clangBin;$(Join-Path $DistDir 'tools');$env:SystemRoot\System32;$env:SystemRoot"
    "SystemRoot"  = $env:SystemRoot
    "TEMP"        = $env:TEMP
    "TMP"         = $env:TEMP
    "USERPROFILE" = $env:USERPROFILE
    "NUMBER_OF_PROCESSORS" = $env:NUMBER_OF_PROCESSORS
}

function RunClean($file, $arguments, $cwd) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $file
    $psi.Arguments = $arguments
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.WorkingDirectory = $cwd
    $psi.EnvironmentVariables.Clear()
    foreach ($k in $clean.Keys) { $psi.EnvironmentVariables[$k] = $clean[$k] }
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $err = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    return [pscustomobject]@{ Code = $p.ExitCode; Out = $out; Err = $err }
}

Write-Host "=== Aucun Python ni compilateur systeme dans le PATH ==="
Write-Host "  PATH = $($clean['PATH'])"

# Preuve que le PATH ne contient AUCUN python/jenga du systeme.
$leak = RunClean "$env:SystemRoot\System32\where.exe" "python" $Workspace
Check "aucun python.exe joignable via le PATH" ($leak.Code -ne 0) "where python a trouve : $($leak.Out.Trim())"

Write-Host "`n--- jenga info ---"
$r = RunClean $shim "info" $Workspace
Check "jenga info aboutit" ($r.Code -eq 0) "code=$($r.Code)`n$($r.Err.Trim())"
if ($r.Out -match "Hello") { Check "le projet Hello est liste" $true } else { Check "le projet Hello est liste" $false $r.Out.Trim() }

Write-Host "`n--- jenga build (le compilateur doit etre DETECTE via le PATH) ---"
$b = RunClean $shim "build --config Debug" $Workspace
Check "jenga build aboutit" ($b.Code -eq 0) "code=$($b.Code)`n$($b.Out.Trim())`n$($b.Err.Trim())"
$tail = ($b.Out -split "`r?`n" | Where-Object { $_ -match "COMPLETED|FAILED|clang|toolchain|Toolchain" } | Select-Object -Last 4) -join "`n          "
if ($tail) { Write-Host "          $tail" }

Write-Host "`n--- execution de l'exe produit, avec arguments ---"
$exe = Get-ChildItem -Path $Workspace -Recurse -Filter "Hello.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
Check "Hello.exe produit" ($null -ne $exe) "aucun Hello.exe trouve sous $Workspace"
if ($exe) {
    Write-Host "          $($exe.FullName)"
    $run = RunClean $exe.FullName "premier ""deuxieme argument""" $Workspace
    Check "Hello.exe s'execute (code 0)" ($run.Code -eq 0) "code=$($run.Code) err=$($run.Err.Trim())"
    Check "les arguments sont recus" ($run.Out -match "argument 1" -and $run.Out -match "argument 2") $run.Out.Trim()
    Write-Host ("          " + (($run.Out -split "`r?`n" | Where-Object { $_ }) -join "`n          "))
}

Write-Host "`nRESULTAT : $fail echec(s)"
exit $fail
