// =============================================================================
// NkIlyanaCitation — lui apprendre à CITER au lieu de deviner, sans inventer
// un seul fait.
// -----------------------------------------------------------------------------
// LE BLOCAGE QU'ON LÈVE ICI. La recherche documentaire fonctionne : on sait
// retrouver le passage pertinent. Mais Ilyana ne sait pas s'en servir, parce
// qu'elle n'a jamais vu à l'entraînement la forme « Contexte → Question →
// Réponse ». Lui coller un passage devant sa question aujourd'hui, elle l'ignore.
//
// Or fabriquer ces exemples semblait exiger d'INVENTER des questions et des
// réponses — exactement la donnée douteuse qui a fait écarter le corpus
// synthétique. Impasse apparente.
//
// LA SORTIE : CHANGER CE QU'ON ENSEIGNE. On ne lui apprend pas des FAITS, on lui
// apprend un GESTE — « étant donné un texte et des mots, rends la phrase du texte
// qui contient ces mots ». Cet exemple-là se fabrique sans rien inventer :
//   - le CONTEXTE est un passage réel, copié ;
//   - la QUESTION est faite de mots EXTRAITS de ce passage ;
//   - la RÉPONSE est une phrase COPIÉE MOT POUR MOT du passage.
// Tout est de la recopie. Aucune machine ne produit d'affirmation, donc aucune
// affirmation ne peut être fausse. Et le geste appris est précisément celui dont
// elle a besoin, puisque la recherche BM25 travaille elle aussi par mots-clés.
//
// LE SECOND ENSEIGNEMENT, AUSSI IMPORTANT : DIRE QU'ON NE TROUVE PAS. Une part
// des exemples pose des mots ABSENTS du passage — pris dans un autre passage. La
// réponse juste est alors « je ne trouve pas cela dans ce texte », et elle est
// mécaniquement vraie, donc vérifiable. Sans ces exemples-là, un modèle à qui on
// n'a montré que des réponses trouvées apprend qu'il faut TOUJOURS répondre, et
// invente quand le texte est muet. On lui enseigne donc les deux gestes : citer,
// et constater l'absence.
//
// CONTRAINTE DE TAILLE, À NE PAS PERDRE DE VUE. La fenêtre vaut 256 tokens, soit
// environ un millier d'octets pour contexte + question + réponse réunis. Un
// exemple plus long serait tronqué à l'entraînement, et elle apprendrait des
// réponses coupées. Les tailles ci-dessous ne sont donc pas décoratives.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_ILYANA_CITATION_H
#define NK_ILYANA_CITATION_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

#include <cstring>

namespace ilyana {
	using namespace nkentseu;

	// La réponse exacte attendue quand les mots ne figurent pas dans le texte.
	// Une seule formulation, délibérément : on enseigne un réflexe, pas un style.
	inline const char *kReponseAbsente() { return "Je ne trouve pas cela dans ce texte."; }

	// -------------------------------------------------------------------------
	// Découpage en phrases. Grossier et assumé : le point suivi d'une espace
	// suffit pour du texte courant. Les abréviations (« M. Dupont ») produiront
	// quelques coupures fautives — sans conséquence ici, puisque la réponse reste
	// une COPIE du texte, donc vraie même mal découpée.
	// -------------------------------------------------------------------------
	inline void DecouperPhrases(const NkString &passage, NkVector<NkString> &out) {
		out.Clear();
		nk_size deb = 0;
		for (nk_size i = 0; i < passage.Size(); ++i) {
			const char c = passage.Data()[i];
			if (c == '.' || c == '!' || c == '?') {
				const bool finit = (i + 1 >= passage.Size()) || passage.Data()[i + 1] == ' ' ||
								   passage.Data()[i + 1] == '\n';
				if (finit) {
					while (deb < i && (passage.Data()[deb] == ' ' || passage.Data()[deb] == '\n'))
						++deb;
					if (i + 1 > deb)
						out.PushBack(passage.SubStr(deb, i + 1 - deb));
					deb = i + 1;
				}
			}
		}
		while (deb < passage.Size() && (passage.Data()[deb] == ' ' || passage.Data()[deb] == '\n'))
			++deb;
		if (deb < passage.Size())
			out.PushBack(passage.SubStr(deb, passage.Size() - deb));
	}

	// Mots vides : ils sont partout et ne désignent rien. Les retenir comme
	// « question » donnerait un exercice sans solution unique.
	inline bool EstMotVide(const NkString &m) {
		static const char *vides[] = {"dans", "avec", "pour", "cette", "leur",  "elle",  "nous",	 "vous",
									  "sont", "être", "avoir", "plus", "mais",	"comme", "tout",  "tous",
									  "sans", "sous", "entre", "aussi", "meme",	"même",	 "cela",  "donc",
									  "alors", "ainsi", "encore", "toujours", "depuis", "apres", "après"};
		for (nk_size i = 0; i < sizeof(vides) / sizeof(vides[0]); ++i)
			if (m == NkString(vides[i]))
				return true;
		return false;
	}

	// Mots porteurs de sens d'une phrase : assez longs pour désigner quelque
	// chose, et pas dans la liste des mots vides.
	inline void MotsDeContenu(const NkString &phrase, NkVector<NkString> &out) {
		out.Clear();
		nk_size i = 0;
		while (i < phrase.Size()) {
			while (i < phrase.Size() && !EstLettre((unsigned char)phrase.Data()[i]))
				++i;
			const nk_size deb = i;
			while (i < phrase.Size() && EstLettre((unsigned char)phrase.Data()[i]))
				++i;
			if (i > deb) {
				NkString m = phrase.SubStr(deb, i - deb);
				// Au moins six octets : en UTF-8 un mot accentué compte plus
				// d'octets que de lettres, donc ce seuil retient à peu près les
				// mots de cinq lettres et plus.
				if (m.Size() >= 6 && !EstMotVide(m))
					out.PushBack(m);
			}
		}
	}

	// -------------------------------------------------------------------------
	// Fabrique UN exemple. Rend false si le passage ne s'y prête pas — mieux vaut
	// sauter un passage que produire un exercice sans solution.
	//
	// `motsEtrangers` non vide fabrique le cas NÉGATIF : la question porte sur des
	// mots venus d'ailleurs, et la réponse juste est de constater leur absence.
	// -------------------------------------------------------------------------
	inline bool FabriquerExemple(const NkString &passage, nk_size graine, const NkVector<NkString> *motsEtrangers,
								 NkString &out, nk_size maxContexte = 600, nk_size maxReponse = 220) {
		if (passage.Size() < 120)
			return false;

		// ⚠️ REFUS D'UN PASSAGE QUI CONTIENT DÉJÀ LES MARQUEURS. « Question: » et
		// « Reponse: » ne sont pas des mots ordinaires ici : ce sont les repères
		// dont l'entraînement se sert pour savoir à partir d'où la perte compte.
		// Un contexte qui en contient déplacerait ce repère, et le modèle
		// apprendrait à prédire le CONTEXTE au lieu de la réponse — sans qu'aucune
		// erreur ne soit signalée nulle part. Constaté en fabriquant des exemples
		// depuis un corpus lui-même en questions/réponses.
		if (passage.Find("Question:") != NkString::npos || passage.Find("Reponse:") != NkString::npos)
			return false;

		NkString contexte = passage;
		if (contexte.Size() > maxContexte) {
			// On coupe à la fin d'une phrase pour ne pas donner un contexte
			// tronqué au milieu d'un mot.
			nk_size p = maxContexte;
			while (p > maxContexte / 2 && contexte.Data()[p] != '.')
				--p;
			contexte = contexte.SubStr(0, (p > maxContexte / 2) ? p + 1 : maxContexte);
		}

		NkVector<NkString> phrases;
		DecouperPhrases(contexte, phrases);
		if (phrases.Size() == 0)
			return false;

		if (motsEtrangers && motsEtrangers->Size() >= 2) {
			// CAS NÉGATIF. Les mots doivent être VRAIMENT absents, sinon on
			// enseignerait une fausseté — on vérifie, on ne suppose pas.
			NkString question;
			int pris = 0;
			for (nk_size i = 0; i < motsEtrangers->Size() && pris < 3; ++i) {
				const NkString &m = (*motsEtrangers)[(graine + i) % motsEtrangers->Size()];
				if (contexte.Find(m.CStr()) != NkString::npos)
					continue;
				if (pris)
					question.Append(' ');
				question.Append(m);
				++pris;
			}
			if (pris < 2)
				return false;
			out.Clear();
			out.Append("Contexte: ");
			out.Append(contexte);
			out.Append("\nQuestion: ");
			out.Append(question);
			out.Append("\nReponse: ");
			out.Append(kReponseAbsente());
			out.Append("\n");
			return true;
		}

		// CAS POSITIF : une phrase du contexte, et des mots pris dans ELLE.
		//
		// ⚠️ IL EN FAUT PLUSIEURS. Si le contexte ne contient qu'une phrase, la
		// réponse EST le contexte entier, et l'exercice n'enseigne plus à choisir
		// mais à recopier son entrée — ce qui est précisément le contraire du
		// geste visé. Trois phrases au moins pour qu'il y ait quelque chose à
		// discriminer.
		if (phrases.Size() < 3)
			return false;
		const NkString &choisie = phrases[graine % phrases.Size()];
		if (choisie.Size() < 40 || choisie.Size() > maxReponse)
			return false;
		NkVector<NkString> mots;
		MotsDeContenu(choisie, mots);
		if (mots.Size() < 2)
			return false;

		// Les mots retenus doivent désigner CETTE phrase et pas une autre : on
		// écarte ceux qui apparaissent ailleurs dans le contexte, sans quoi
		// l'exercice aurait deux solutions et n'enseignerait rien de net.
		NkString question;
		int pris = 0;
		for (nk_size i = 0; i < mots.Size() && pris < 3; ++i) {
			const NkString &m = mots[(graine + i) % mots.Size()];
			nk_size occurrences = 0;
			nk_size pos = contexte.Find(m.CStr());
			while (pos != NkString::npos) {
				++occurrences;
				pos = contexte.Find(m.CStr(), pos + 1);
			}
			if (occurrences != 1)
				continue;
			if (pris)
				question.Append(' ');
			question.Append(m);
			++pris;
		}
		if (pris < 2)
			return false;

		out.Clear();
		out.Append("Contexte: ");
		out.Append(contexte);
		out.Append("\nQuestion: ");
		out.Append(question);
		out.Append("\nReponse: ");
		out.Append(choisie);
		out.Append("\n");
		return true;
	}

} // namespace ilyana

#endif // NK_ILYANA_CITATION_H
