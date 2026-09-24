# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#
# Refus de construire NKCode quand le wheel Jenga embarqué n'est pas la
# version exigée par Applications/NKCode/JENGA_VERSION.
#
# POURQUOI UN SCRIPT, ET PAS UN SystemExit DANS NKCode.jenga : le .jenga est
# évalué au CHARGEMENT de l'espace de travail. Le refus y arrêtait donc TOUT —
# `jenga build --target NkDames --platform android`, `jenga info`… — alors
# qu'il ne protège que NKCode. Lancé en prebuild(), il ne s'exécute que
# lorsque NKCode est réellement construit, et son code de retour arrête ce
# projet-là (Jenga >= 2.8.2 ; avant, le code de retour d'un prebuild était
# ignoré).
#
# Le calcul des deux versions reste dans NKCode.jenga : ce script ne fait que
# les recevoir et refuser. Un seul répondeur, pas deux.
#
# Usage : garde_version_jenga.py <version exigée> <version du wheel> <nom du wheel>
import sys


def main() -> int:
    if len(sys.argv) != 4:
        print("[NKCode] garde_version_jenga : arguments attendus : "
              "<exigee> <wheel> <nom du wheel>", file=sys.stderr)
        return 2
    exigee, wheel, nom = sys.argv[1], sys.argv[2], sys.argv[3]
    if exigee == wheel:
        return 0
    lignes = [
        "[NKCode] ARRET : le wheel embarque (" + nom + ") est en " + wheel
        + " alors que Applications/NKCode/JENGA_VERSION exige " + exigee + ".",
        "[NKCode]   NKCode se construirait avec un Jenga different de celui",
        "[NKCode]   que le CI installe : les paquets divergeraient en silence.",
        "[NKCode]   Remede : ./cri.sh dans le depot Jenga pour produire le wheel",
        "[NKCode]   attendu, ou corriger JENGA_VERSION si c'est lui qui a tort.",
    ]
    print("\n".join(lignes), file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
