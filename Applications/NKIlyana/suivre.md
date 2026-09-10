# 1. L'état en un coup d'œil (écrit à chaque checkpoint, ~toutes les 15 min)

type D:\Projets\Camrail\AI\IlyanaReel\campagne_20p1\ilyana20.nkgp.etat.txt

# 2. Le direct — les derniers pas et la perte, en continu

Get-Content (Get-ChildItem D:\Projets\Camrail\AI\IlyanaReel\campagne_20p1\logs\app_*.log |
Sort-Object LastWriteTime | Select-Object -Last 1).FullName -Tail 15 -Wait

# 3. Le processus vit-il ?

Get-Process NKIlyana
