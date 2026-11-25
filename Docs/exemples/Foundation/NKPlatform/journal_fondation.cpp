// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : le journal de fondation, redirigé vers TON puits.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à journal_fondation.attendu.
// Par défaut, NK_FOUNDATION_LOG_* écrit sur la sortie d'erreur avec le fichier et la ligne ; un puits personnalisé
// reçoit chaque message et en fait ce qu'il veut (ici : une ligne sur la sortie standard, sans le chemin du fichier).

#include "NKPlatform/NkFoundationLog.h"
#include "NKContainers/String/NkPrint.h"

namespace {

	// [extrait:puits]
	// Un puits reçoit le niveau, le fichier, la ligne et le message déjà formaté.
	void MonPuits(const char *niveau, const char * /*fichier*/, int /*ligne*/, const char *message) {
		nkentseu::NkPrintln("[{}] {}", niveau, message);
	}
	// [/extrait]

} // namespace

int main() {
	using namespace nkentseu;

	// [extrait:brancher]
	platform::NkFoundationSetLogSink(&MonPuits);
	NK_FOUNDATION_LOG_WARN("le disque est presque plein");
	NK_FOUNDATION_LOG_ERROR("le fichier de configuration est illisible");
	// [/extrait]

	// [extrait:debrancher]
	// nullptr rend la sortie par défaut ; on vérifie que le puits a bien été retiré.
	platform::NkFoundationSetLogSink(nullptr);
	NkPrintln("Puits retiré : {}", platform::NkFoundationGetLogSink() == nullptr ? "oui" : "non");
	// [/extrait]

	return 0;
}
