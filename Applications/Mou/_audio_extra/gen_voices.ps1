# Genere les voix FR placeholder (Microsoft Hortense) dans assets/voice/*.wav
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Speech
$dir = Join-Path $PSScriptRoot '..\assets\voice'
New-Item -ItemType Directory -Force $dir | Out-Null
$v = New-Object System.Speech.Synthesis.SpeechSynthesizer
$v.SelectVoice('Microsoft Hortense Desktop')
$v.Rate = -1
$v.Volume = 100
$fmt = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(44100, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)

$lines = [ordered]@{
  'couleurs_consigne'        = 'Range les fruits dans le bon panier !'
  'compter_consigne'         = 'Compte les fruits, puis touche le bon chiffre !'
  'calcul_consigne'          = 'Combien ça fait ? Touche le bon chiffre !'
  'formes_consigne'          = 'Mets chaque forme à sa place !'
  'memoire_consigne'         = 'Retrouve les paires !'
  'animaux_canard'           = 'Touche le canard !'
  'animaux_margouillat'      = 'Touche le margouillat !'
  'animaux_elephant'         = "Touche l'éléphant !"
  'animaux_lion'             = 'Touche le lion !'
  'animaux_poisson'          = 'Touche le poisson !'
  'animaux_tortue'           = 'Touche la tortue !'
  'animaux_singe'            = 'Touche le singe !'
  'animaux_oiseau'           = "Touche l'oiseau !"
  'felicitation_bravo'       = 'Bravo !'
  'felicitation_super'       = 'Super !'
  'felicitation_tu_es_fort'  = 'Tu es très fort !'
  'encouragement_essaie'     = 'Essaie encore !'
  'encouragement_presque'    = 'Presque ! Tu y es presque !'
  'recompense_etoiles'       = 'Bravo ! Tu as gagné des étoiles !'
  'nombre_1'  = 'un'
  'nombre_2'  = 'deux'
  'nombre_3'  = 'trois'
  'nombre_4'  = 'quatre'
  'nombre_5'  = 'cinq'
  'nombre_6'  = 'six'
  'nombre_7'  = 'sept'
  'nombre_8'  = 'huit'
  'nombre_9'  = 'neuf'
  'nombre_10' = 'dix'
  'fruit_tomate'   = 'la tomate'
  'fruit_mangue'   = 'la mangue'
  'fruit_avocat'   = "l'avocat"
  'fruit_safou'    = 'le safou'
  'fruit_papaye'   = 'la papaye'
  'fruit_plantain' = 'la banane plantain'
  'forme_rond'     = 'le rond'
  'forme_carre'    = 'le carré'
  'forme_triangle' = 'le triangle'
  'forme_etoile'   = "l'étoile"
  'forme_coeur'    = 'le cœur'
}

foreach ($k in $lines.Keys) {
  $p = Join-Path $dir ($k + '.wav')
  $v.SetOutputToWaveFile($p, $fmt)
  $v.Speak($lines[$k])
  Write-Output ("ok  " + $k)
}
$v.SetOutputToNull()
$v.Dispose()
Write-Output ('TOTAL ' + $lines.Count)
