# -----------------------------------------------------------------------------
# AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# @File    Tools/Genia/ia_verbe.py
# @License Proprietary - All Rights Reserved (see LICENSE)
# -----------------------------------------------------------------------------
#
# LA COMMANDE QUE LE DORSAL PROCESSUS LANCE — deux trous, {invite} et {sortie}.
#
# ⚠️ CE N'EST PAS UN DORSAL OLLAMA, ET C'EST VOULU. `NKConverse` porte deja
#    l'abstraction (`NkIConverseBackend`) et un dorsal generique de PROCESSUS
#    qui ne connait « ni Qwen, ni Ollama, ni GGUF, ni HTTP : deux trous ». Un
#    second dorsal Ollama dans le noyau serait la dette des trois exemplaires
#    qu'on vient de rembourser -- un autre chantier en ecrit un en ce moment
#    dans le module partage, et c'est le sien qu'il faudra consommer.
#    Ce fichier-ci est donc REMPLACABLE : le jour ou le dorsal HTTP arrive, on
#    change le gabarit et pas une ligne du modeleur.
#
# CE QU'IL FAIT : lit l'invite, la soumet au service local, ecrit la reponse.
#   argv[3]       budget de jetons de la reponse (defaut 64 : une ligne)
#   NK_IA_MODELE  (defaut qwen2.5:7b-instruct)
#   NK_IA_URL     (defaut http://127.0.0.1:11434)
#   NK_IA_TIMEOUT (defaut 120 s)
#
# ⚠️ TEMPERATURE 0. Une sonde qui mesure un taux de reussite sur un modele qui
#    repond differemment a chaque appel mesure le hasard. Le jour ou l'on
#    voudra de la variete, ce sera un choix ECRIT, pas un defaut herite.
#
# CODES : 0 la reponse est ecrite · 2 le service n'a pas repondu · 3 argument
#         manquant. ⚠️ On n'ecrit JAMAIS le fichier de sortie en cas d'echec :
#         le dorsal teste son existence, et un fichier vide ou laisse par un
#         lancement precedent ferait passer un echec pour une reussite.

import json
import os
import socket
import sys
import urllib.error
import urllib.request


def refuser(chemin_sortie: str, motif: str, code: int) -> int:
    """Ecrit un motif LISIBLE dans le fichier de sortie, et rend le code vrai.

    Le code de sortie reste celui de l'echec : on ne ment pas a la machine.
    Mais le motif partait sur stderr, que personne ne lit, et l'hote ne voyait
    qu'un entier -- il affichait « le generateur a rendu 2 et n'a pas ecrit
    logs/... », ce qui envoie chercher la panne du cote du fichier alors
    qu'Ollama etait simplement absent. *Crier dans un canal que personne
    n'ecoute revient a se taire.*

    Le prefixe REFUS: est la convention que NKConverse emploie deja pour ses
    propres messages : l'hote le reconnait et n'y cherche pas un verbe.
    """
    sys.stderr.write(motif + chr(10))
    try:
        with open(chemin_sortie, 'w', encoding='utf-8') as f:
            f.write('REFUS: ' + motif)
    except OSError:
        pass  # si meme ca echoue, le code de sortie reste le dernier mot
    return code


def main() -> int:
    if len(sys.argv) < 3:
        sys.stderr.write("usage: ia_verbe.py <invite.txt> <sortie.txt>\n")
        return 3
    chemin_invite = sys.argv[1]
    chemin_sortie = sys.argv[2]

    try:
        with open(chemin_invite, "r", encoding="utf-8") as f:
            invite = f.read()
    except OSError as e:
        sys.stderr.write("invite illisible : %s\n" % e)
        return 3
    if not invite.strip():
        sys.stderr.write("invite vide : rien n'est soumis\n")
        return 3

    # LE BUDGET DE JETONS, EN TROISIEME ARGUMENT (21/09). Un verbe tient en une
    # ligne : 64 jetons, et c'est toujours le defaut. Un DOCUMENT de creation
    # (une ligne par partie nommee) en demande plusieurs centaines ; coupe a 64,
    # une chaise perdait ses pieds au milieu d'une ligne, et le lecteur les
    # refusait sans que personne sache que la coupure venait d'ici.
    jetons = 64
    if len(sys.argv) > 3:
        try:
            jetons = max(16, min(4096, int(sys.argv[3])))
        except ValueError:
            sys.stderr.write("budget de jetons illisible : %s (defaut 64)" % sys.argv[3] + chr(10))
    modele = os.environ.get("NK_IA_MODELE", "qwen2.5:7b-instruct")
    base = os.environ.get("NK_IA_URL", "http://127.0.0.1:11434")
    delai = float(os.environ.get("NK_IA_TIMEOUT", "120"))

    charge = {
        "model": modele,
        "prompt": invite,
        "stream": False,
        # num_predict borne la reponse : on attend UNE ligne. Sans borne, un
        # modele bavard fait payer des secondes pour du texte qu'on jette.
        "options": {"temperature": 0, "num_predict": jetons},
    }
    donnees = json.dumps(charge).encode("utf-8")
    req = urllib.request.Request(
        base.rstrip("/") + "/api/generate",
        data=donnees,
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=delai) as r:
            rep = json.loads(r.read().decode("utf-8"))
    except (urllib.error.URLError, OSError, ValueError) as e:
        # Le CONSEIL passe devant le DETAIL : l hote borne le motif a 192
        # caracteres, et une erreur systeme Windows en fait deja 150. Mis en
        # queue, « Demarrez-le » se faisait couper -- il ne restait que la
        # plainte, sans le geste qui repare.
        #
        # [!] ABSENT ET LENT SONT DEUX PANNES DIFFERENTES, et ce message les
        #     confondait. Mesure du 20/09 : la carte etant disputee, une
        #     reponse a mis 182 s ; le delai par defaut etant de 120 s, le
        #     panneau annoncait « Ollama n'est pas joignable » alors qu'il
        #     repondait tres bien -- il envoyait chercher la panne du cote du
        #     service arrete, et le geste propose (« Demarrez-le ») n'aurait
        #     rien repare. *Un message qui nomme la mauvaise cause coute plus
        #     cher qu'un message vague.*
        lent = isinstance(e, socket.timeout) or isinstance(
            getattr(e, 'reason', None), socket.timeout)
        if lent:
            return refuser(chemin_sortie,
                           "Ollama a mis plus de %g s a repondre (la carte est"
                           " peut-etre occupee). Reessayez, ou augmentez"
                           " NK_IA_TIMEOUT." % delai, 2)
        return refuser(chemin_sortie,
                       "Ollama n'est pas joignable sur %s. Demarrez-le, ou "
                       "posez NK_IA_URL. Detail : %s" % (base, e), 2)

    texte = (rep.get("response") or "").strip()
    if not texte:
        return refuser(chemin_sortie,
                       "Ollama a repondu, mais sans texte (modele %s)." % modele, 2)

    # La mesure du cout, sur stderr : elle accompagne la reponse au lieu d'etre
    # racontee ailleurs. `eval_count` est le nombre de jetons produits.
    sys.stderr.write(
        "[ia_verbe] modele=%s jetons_invite=%s jetons_reponse=%s duree_ns=%s\n"
        % (modele, rep.get("prompt_eval_count"), rep.get("eval_count"), rep.get("total_duration"))
    )

    with open(chemin_sortie, "w", encoding="utf-8") as f:
        f.write(texte)
    return 0


if __name__ == "__main__":
    sys.exit(main())
