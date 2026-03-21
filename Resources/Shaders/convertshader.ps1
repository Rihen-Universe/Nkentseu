# convert_shader_fixed.ps1
param(
    [string]$InputFile,
    [string]$OutputFile
)

# Obtenir le chemin absolu du fichier d'entrée
if ([System.IO.Path]::IsPathRooted($InputFile)) {
    $fullInputPath = $InputFile
} else {
    # Si c'est un chemin relatif, le combiner avec le répertoire courant
    $fullInputPath = [System.IO.Path]::Combine((Get-Location).Path, $InputFile)
}

Write-Host "Recherche du fichier: $fullInputPath"

# Verifier que le fichier existe
if (-not (Test-Path $fullInputPath)) {
    Write-Host "ERREUR: Fichier introuvable: $fullInputPath"
    Write-Host ""
    Write-Host "Fichiers .spv trouves dans $(Get-Location):"
    $files = Get-ChildItem -Path . -Filter "*.spv"
    if ($files.Count -eq 0) {
        Write-Host "  Aucun fichier .spv trouve"
        Write-Host ""
        Write-Host "Pour compiler un shader GLSL en SPIRV :"
        Write-Host "  glslc mon_shader.vert -o vert.spv"
    } else {
        $files | ForEach-Object { Write-Host "  - $($_.Name) ($($_.Length) bytes)" }
    }
    exit 1
}

# Lire le fichier
$bytes = [System.IO.File]::ReadAllBytes($fullInputPath)
Write-Host "Fichier trouve: $($bytes.Length) bytes"

$uint32s = @()

for ($i = 0; $i -lt $bytes.Length; $i += 4) {
    if ($i + 3 -lt $bytes.Length) {
        $value = ($bytes[$i+3] -shl 24) -bor ($bytes[$i+2] -shl 16) -bor ($bytes[$i+1] -shl 8) -bor $bytes[$i]
        $uint32s += "0x$('{0:X8}' -f $value)"
    }
}

$varName = [System.IO.Path]::GetFileNameWithoutExtension($InputFile)
$arraySize = $uint32s.Count

$header = @"
// Auto-generated from $InputFile
// File size: $($bytes.Length) bytes
// Generated on $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")

#include <stdint.h>
#include <array>

namespace Shaders {
    static constexpr size_t ${varName}_size = $($bytes.Length);
    static constexpr std::array<uint32_t, $arraySize> ${varName}_code = {"
"@

# Ajouter les valeurs (par lots de 8 pour la lisibilité)
for ($i = 0; $i -lt $uint32s.Count; $i += 8) {
    $line = $uint32s[$i..([Math]::Min($i+7, $uint32s.Count-1))] -join ", "
    $header += "`n        $line"
    if ($i + 8 -lt $uint32s.Count) {
        $header += ","
    }
}

$header += @"
    
    };
} // namespace Shaders
"@

# Sauvegarder
[System.IO.File]::WriteAllText($OutputFile, $header)
Write-Host ""
Write-Host "SUCCES: Header genere: $OutputFile"
Write-Host "  - Taille originale: $($bytes.Length) bytes"
Write-Host "  - Nombre de uint32_t: $arraySize"
Write-Host "  - Fichier de sortie: $([System.IO.Path]::GetFullPath($OutputFile))"