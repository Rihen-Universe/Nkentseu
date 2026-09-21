#pragma once
// -----------------------------------------------------------------------------
// @File    NkConverseOllama.h
// @Brief   LE DORSAL OLLAMA -- un service HTTP local qui tient le modele.
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE DORSAL EXISTE, ET CE QU'IL LEVE
// =============================================================================
//  Le depot portait une dette nommee : « aucune MESURE de modele n'est possible
//  sur cette machine tant que NKDesignLLM exige un GPU ». Ollama expose un
//  service HTTP local, decide LUI-MEME de la repartition carte/processeur, et
//  decharge le modele apres un delai d'inactivite. Le chemin processeur n'a donc
//  plus a etre ecrit ici : il existe, il est juste chez quelqu'un d'autre.
//
//  ⚠️ LE NOM DU MODELE EST UN REGLAGE, JAMAIS UNE CONSTANTE. Le cap est
//     explicite : le modele est un reglage, l'OUTILLAGE est le produit. Un nom
//     grave dans ce fichier en ferait un dorsal-a-un-modele, et il faudrait
//     recompiler pour en essayer un autre.
//
//  ⚠️ TROIS REFUS NOMMES, ET AUCUN REPLI MUET. Un dorsal qui rend une reponse
//     vide en annoncant un succes fait passer un modele ABSENT pour un modele
//     SILENCIEUX -- et on cherche alors le defaut dans l'invite pendant des
//     heures. Les trois cas sont distingues parce qu'ils se reparent a trois
//     endroits differents :
//       (1) le SERVICE ne repond pas     -> on nomme l'hote et le port
//       (2) le MODELE n'est pas la       -> on nomme le modele et le code HTTP
//       (3) la reponse est VIDE          -> ce n'est PAS un succes
//
//  ⚠️ IL INCLUT CE QU'IL UTILISE -- regle 2 du module. La faute inverse a coute
//     20 erreurs a NKPA : un en-tete comptait sur ses consommateurs pour tirer
//     sa dependance, et ca marchait PAR CHANCE chez les deux qu'il avait.
// =============================================================================

#include "NKConverse/NkConverse.h"
#include "NKNetwork/HTTP/NkHTTPClient.h"

#include <cstdio>

namespace nkentseu::converse {

	class NkConverseBackendOllama final : public NkIConverseBackend {
		public:
			/// L'hote du service. UN REGLAGE : un autre poste, un autre port.
			NkString hote = NkString("http://127.0.0.1:11434");
			/// Le modele. UN REGLAGE (voir l'en-tete), jamais une constante.
			NkString modele = NkString("qwen2.5:7b-instruct");
			/// ⚠️ GENEREUX A DESSEIN. Le PREMIER appel charge le modele depuis le
			///    disque : mesure du 17/09, 34,7 s de chargement pour 4,75 Go, sur
			///    69,2 s au total. Le defaut de `NkHTTPClient` est 10 s -- il
			///    aurait fait echouer TOUTES les premieres courses, et on aurait
			///    conclu « Ollama ne marche pas » en mesurant notre propre delai.
			///
			/// ⚠️ ET IL EST REGLABLE PAR `NK_OLLAMA_DELAI`, LU ICI ET NULLE PART
			///    AILLEURS. Le banc le lisait de son cote : deux lecteurs du meme
			///    reglage, donc l'application n'en beneficiait pas. *Un reglage lu
			///    a deux endroits finit par valoir deux choses.*
			///
			/// 🔴 POURQUOI IL DOIT ETRE REGLABLE, MESURE DU 20/09 : sur 30 demandes,
			///    9 ont depasse 300 s. Rejouees, **8 rendent en 10 a 48 secondes**
			///    -- cinq a trente fois sous le plafond. Le plafond refusait donc du
			///    BRUIT DE MACHINE et l'inscrivait au passif de l'outil.
			///    *Un plafond qui refuse du bruit est un plafond qui ment sur
			///    l'outil.* Une seule demande (`e26`) resiste a 900 s.
			///
			/// ⚠️ LE DEFAUT NE BOUGE PAS. Le porter en silence a 900 s ferait
			///    attendre l'utilisateur un quart d'heure devant une fenetre qu'il
			///    croirait gelee -- le defaut ferme ce matin, rouvert par l'autre
			///    bout. Qui veut attendre plus longtemps le DIT.
			nkentseu::uint32 delaiMs = LireDelaiReglage();

			static nkentseu::uint32 LireDelaiReglage() {
				const char *v = std::getenv("NK_OLLAMA_DELAI");
				if (!v || !*v)
					return 300000u;
				// Pas `atoi` : on veut distinguer « absent » de « zero », et un
				// zero rendrait toute requete impossible sans le dire.
				nkentseu::uint32 n = 0u;
				for (const char *p = v; *p; ++p) {
					if (*p < '0' || *p > '9')
						return 300000u;
					n = n * 10u + (nkentseu::uint32)(*p - '0');
				}
				return n > 0u ? n : 300000u;
			}
			/// ⚠️ LA TEMPERATURE EST UN REGLAGE, ET ELLE DECIDE SI UNE MESURE EST UNE
			///    MESURE. Mesure du 19/09 : deux courses du MEME modele, de la MEME
			///    invite et des MEMES douze demandes ont rendu 7/12 puis 5/12. *Une
			///    course n'est pas une mesure quand le dorsal tire au sort.*
			///
			///    A -1, on ne pose rien et le modele garde son reglage (c'est le
			///    comportement d'usage). A 0, la generation devient reproductible et
			///    deux invites peuvent enfin se comparer -- c'est le reglage de BANC.
			float32 temperature = -1.f;
			/// LE BUDGET DE REPONSE (`options.num_predict`), pose par l'EFFORT du panneau
			/// IA (21/09). -1 = aucun plafond ecrit : le service garde le sien.
			nkentseu::int32 numPredict = -1;
			/// LE RAISONNEMENT A VOIX HAUTE (`think`, champ racine d'Ollama) : -1 = non
			/// ecrit, 0 = false, 1 = true. ⚠️ N'a de sens que pour un modele dont
			/// /api/show annonce la capacite « thinking » : le panneau GRISE
			/// l'interrupteur pour les autres, il ne l'ecrit pas en silence.
			nkentseu::int32 penser = -1;
			/// Publies pour que l'appelant puisse les IMPRIMER : un temps de
			/// reponse sans sa condition ne vaut rien.
			mutable nkentseu::uint32 dernierCode = 0u;
			nkentseu::uint64 dernierMs = 0u;
			/// LE CORPS EXACT DE LA DERNIERE REQUETE (21/09, Q5). C'est lui que la
			/// preuve compare : « meme demande, deux reglages, deux requetes
			/// differentes » se mesure sur les octets envoyes, pas sur l'etat du
			/// panneau.
			NkString dernierCorps;
			/// Ce que le service a MESURE du dernier tour (champs de /api/generate) :
			/// la fenetre « Utilisation » les montre, rien n'est estime.
			nkentseu::uint64 derniersJetons = 0u; ///< eval_count
			nkentseu::uint64 derniereGenNs = 0u;  ///< eval_duration
			nkentseu::uint64 dernierChargeNs = 0u; ///< load_duration (0 = deja en memoire)
			/// Le raisonnement rendu a part quand `think` est vrai (champ `thinking`).
			NkString dernierRaisonnement;

			/// Le corps que `Complete` enverrait pour cette invite -- la MEME fonction.
			void CorpsPour(const NkString &invite, NkString &out) const {
				BatirCorps(invite, out);
			}

			bool Complete(const NkConverseRequest &req, NkConverseReply &out) override {
				out.success = false;
				out.text = NkString("");
				out.error = NkString("");
				dernierCode = 0u;
				dernierMs = 0u;
				derniersJetons = derniereGenNs = dernierChargeNs = 0u;
				dernierRaisonnement = NkString();
				if (modele.Length() == 0) {
					out.error = NkString("REFUS : aucun modele nomme (le modele est un reglage, pas un defaut)");
					return false;
				}
				// L'invite COMPLETE, assemblee par la MEME fonction que les deux
				// autres dorsaux. Deux assemblages auraient diverge au premier
				// champ ajoute a `NkConverseRequest`, et une reponse rejouee a la
				// main n'aurait plus correspondu a celle du service.
				NkString full;
				NkConverseBackendProcessus::EcrireRequete(req, full);
				mDerniereInvite = full; // les octets envoyes, seule verite

				NkString corps;
				BatirCorps(full, corps);
				dernierCorps = corps;

				nkentseu::net::NkHTTPClient http;
				nkentseu::net::NkHTTPClient::Config cfg;
				cfg.defaultTimeoutMs = delaiMs;
				http.Configure(cfg);

				NkString url = hote;
				url.Append("/api/generate");
				const nkentseu::net::NkHTTPResponse r = http.Post(url.Data(), corps.Data());
				dernierCode = r.statusCode;
				dernierMs = (nkentseu::uint64)r.timeMs;

				// (1) LE SERVICE NE REPOND PAS. `statusCode == 0` est, d'apres
				//     l'en-tete de `NkHTTPResponse`, « aucune reponse recue
				//     (erreur reseau) » -- a distinguer d'un code HTTP d'erreur,
				//     qui prouve au contraire que quelqu'un a repondu.
				if (r.statusCode == 0u) {
					// 🔴 DEUX SILENCES QUI N'ONT RIEN A VOIR, ET ON LES CONFONDAIT.
					//    « personne n'ecoute » et « NOUS avons cesse d'attendre »
					//    portaient le meme message -- « le service ne repond pas
					//    (le service est-il lance ?) ». Le second est FAUX : le
					//    service tourne, c'est notre plafond qui a tranche.
					//    Mesure du 20/09 : 8 depassements sur 9 rendent en 10 a 48 s
					//    quand on rejoue. *Le plafond est une decision de notre
					//    part, pas un fait sur le modele.*
					const bool plafond = r.error.Find("timeout", 0) != NkString::npos ||
										 r.error.Find("Timeout", 0) != NkString::npos;
					if (plafond) {
						// ⚠️ LE GESTE QUI REPARE EN TETE. Un message se tronque a
						//    l'affichage, et ce qui survit doit etre ce qu'on peut
						//    FAIRE -- pas la plainte. Lecon payee par le modeleur
						//    ce matin.
						// ⚠️ « 1 secondes » : un message d'erreur qui ecorche la
						//    langue se lit comme un message qu'on n'a pas relu.
						//    Sous 10 s on donne une decimale, sinon l'entier, et
						//    l'accord suit.
						char duree[40];
						if (delaiMs < 10000u)
							snprintf(duree, sizeof(duree), "%.1f seconde%s",
									 (double)delaiMs / 1000.0, delaiMs >= 2000u ? "s" : "");
						else
							snprintf(duree, sizeof(duree), "%u secondes",
									 (unsigned)(delaiMs / 1000u));
						char b[256];
						snprintf(b, sizeof(b),
								 "Reessayez : pas de reponse en %s. C'est NOTRE plafond "
								 "d'attente, pas une panne du modele -- il repond souvent au "
								 "second essai. (reglable par NK_OLLAMA_DELAI, en millisecondes)",
								 duree);
						out.error = NkString(b);
						return false;
					}
					out.error = NkString("Verifiez que le service de modeles est lance : "
										 "personne ne repond a ");
					out.error.Append(url);
					if (r.error.Length() > 0) {
						out.error.Append(" -- ");
						out.error.Append(r.error);
					}
					return false;
				}

				// (2) LE MODELE N'EST PAS INSTALLE. On NOMME le modele : « erreur
				//     404 » seul enverrait chercher un defaut de reseau.
				if (r.statusCode != 200u) {
					char b[96];
					snprintf(b, sizeof(b), "REFUS : le service rend %u pour le modele ",
							 (unsigned)r.statusCode);
					out.error = NkString(b);
					out.error.Append(modele);
					out.error.Append(" -- est-il installe ? ");
					AppendBorne(out.error, r.body, 200u);
					return false;
				}

				// ⚠️ LE MOTIF DISAIT « champ `response` absent » POUR UN CHAMP
				//    PRESENT. Mesure du 20/09 : le corps affiche a cote du refus
				//    contenait `"response":"nkuidoc 1\ntitre = Jeu..."`. Le champ
				//    etait la ; c'est sa chaine qui n'etait jamais FERMEE, parce que
				//    le client HTTP s'arretait a la fin des en-tetes. *Un message
				//    d'erreur qui nomme un coupable est une hypothese de
				//    l'instrument, pas une mesure* -- et celui-ci accusait le modele
				//    d'un defaut de notre pile reseau. Les trois cas sont desormais
				//    distingues, et aucun ne designe le modele.
				if (!ExtraireChamp(r.body, "response", out.text)) {
					const bool present = r.body.Find("\"response\":", 0) != NkString::npos;
					if (r.body.Length() == 0u) {
						out.error = NkString("REFUS : 200 avec un corps VIDE -- le service a repondu "
											 "sans rien dire (cadrage de la reponse ?)");
					} else if (present) {
						char b[160];
						snprintf(b, sizeof(b),
								 "REFUS : 200, champ `response` PRESENT mais sa chaine n'est jamais "
								 "fermee sur %u octets de corps -- corps TRONQUE, pas modele muet -- ",
								 (unsigned)r.body.Length());
						out.error = NkString(b);
					} else {
						char b[128];
						snprintf(b, sizeof(b),
								 "REFUS : 200, aucun champ `response` dans %u octets de corps -- ",
								 (unsigned)r.body.Length());
						out.error = NkString(b);
					}
					AppendBorne(out.error, r.body, 200u);
					return false;
				}

				// LES MESURES DU SERVICE, et le raisonnement s'il a ete demande
				derniersJetons = NombreJson(r.body, "eval_count");
				derniereGenNs = NombreJson(r.body, "eval_duration");
				dernierChargeNs = NombreJson(r.body, "load_duration");
				if (penser == 1)
					(void)ExtraireChamp(r.body, "thinking", dernierRaisonnement);
				// (3) UNE REPONSE VIDE N'EST PAS UN SUCCES. Le depot a deja nomme
				//     cette faute plus haut dans la chaine : « un modele qui repond
				//     "Bien sur ! Voici :" sans liste rendrait une reponse non vide
				//     et une specification VIDE ». Ici on tient le PREMIER maillon.
				if (out.text.Length() == 0) {
					out.error = NkString("REFUS : le modele ");
					out.error.Append(modele);
					out.error.Append(" a repondu 200 avec un texte VIDE -- ce n'est pas un succes");
					return false;
				}
				out.success = true;
				return true;
			}

			/// ⚠️ ELLE INTERROGE LE SERVICE, ELLE NE SUPPOSE RIEN. Une
			///    disponibilite qui rendrait `true` parce qu'une chaine est non
			///    vide mentirait exactement quand on a besoin d'elle : service
			///    eteint. Delai court : c'est une question, pas une generation.
			bool IsAvailable() const override {
				nkentseu::net::NkHTTPClient http;
				nkentseu::net::NkHTTPClient::Config cfg;
				cfg.defaultTimeoutMs = 3000u;
				http.Configure(cfg);
				NkString url = hote;
				url.Append("/api/tags");
				const nkentseu::net::NkHTTPResponse r = http.Get(url.Data());
				dernierCode = r.statusCode;
				return r.statusCode == 200u;
			}

			const char *Name() const override {
				return "ollama";
			}

		private:
			static nkentseu::uint64 NombreJson(const NkString &j, const char *cle) {
				NkString motif("\"");
				motif.Append(cle);
				motif.Append("\":");
				const NkString::SizeType i = j.Find(motif.Data(), 0);
				if (i == NkString::npos)
					return 0u;
				nkentseu::uint64 v = 0u;
				for (NkString::SizeType k = i + motif.Length(); k < j.Length() && j[k] >= '0' && j[k] <= '9'; ++k)
					v = v * 10u + (nkentseu::uint64)(j[k] - '0');
				return v;
			}
			/// Le corps JSON. `stream=false` : UNE reponse, pas un flux -- le
			/// contrat de `NkIConverseBackend` est « une invite, une reponse ».
			void BatirCorps(const NkString &invite, NkString &out) const {
				out = NkString("{\"model\":\"");
				Echapper(modele, out);
				out.Append("\",\"prompt\":\"");
				Echapper(invite, out);
				out.Append("\",\"stream\":false");
				// LES PROPRIETES DU PANNEAU AGISSENT ICI, et nulle part ailleurs : le
				// corps est la seule chose que le service lit. Une propriete affichee
				// qui ne changerait pas ces octets serait pire qu'absente.
				if (penser >= 0)
					out.Append(penser ? ",\"think\":true" : ",\"think\":false");
				if (temperature >= 0.f || numPredict > 0) {
					char t[96];
					if (temperature >= 0.f && numPredict > 0)
						snprintf(t, sizeof(t), ",\"options\":{\"temperature\":%.3f,\"num_predict\":%d}",
								 (double)temperature, (int)numPredict);
					else if (temperature >= 0.f)
						snprintf(t, sizeof(t), ",\"options\":{\"temperature\":%.3f}", (double)temperature);
					else
						snprintf(t, sizeof(t), ",\"options\":{\"num_predict\":%d}", (int)numPredict);
					out.Append(t);
				}
				out.Append("}");
			}

			/// ⚠️ L'ECHAPPEMENT N'EST PAS COSMETIQUE. L'invite CONTIENT du
			///    `.nkgui` : des guillemets et un saut de ligne a chaque ligne.
			///    Sans lui le JSON est invalide, le service rend 400, et on met ce
			///    refus sur le compte du modele.
			static void Echapper(const NkString &in, NkString &out) {
				char b[8];
				for (NkString::SizeType i = 0; i < in.Length(); ++i) {
					const char c = in.Data()[i];
					if (c == '"') {
						out.Append("\\\"");
					} else if (c == '\\') {
						out.Append("\\\\");
					} else if (c == '\n') {
						out.Append("\\n");
					} else if (c == '\r') {
						out.Append("\\r");
					} else if (c == '\t') {
						out.Append("\\t");
					} else if ((unsigned char)c < 0x20u) {
						snprintf(b, sizeof(b), "\\u%04X", (unsigned)(unsigned char)c);
						out.Append(b);
					} else {
						const char deux[2] = {c, '\0'};
						out.Append(deux);
					}
				}
			}

			/// Lit UN champ chaine d'un objet JSON plat, en desechappant.
			///
			/// ⚠️ CE N'EST PAS UN ANALYSEUR JSON GENERAL, ET C'EST ASSUME. La
			///    reponse du service est un objet plat dont on ne veut qu'un
			///    champ ; ecrire un analyseur complet ici aurait fait un SECOND
			///    analyseur a tenir d'accord avec celui du depot, pour un gain
			///    nul. Il rend `false` -- jamais une chaine vide qui passerait
			///    pour une reponse -- des qu'il ne trouve pas ce qu'il cherche.
			static bool ExtraireChamp(const NkString &json, const char *cle, NkString &out) {
				out = NkString("");
				NkString motif = NkString("\"");
				motif.Append(cle);
				motif.Append("\":");
				NkString::SizeType i = json.Find(motif.Data(), 0);
				if (i == NkString::npos)
					return false;
				i += motif.Length();
				while (i < json.Length() && (json.Data()[i] == ' ' || json.Data()[i] == '\t'))
					++i;
				if (i >= json.Length() || json.Data()[i] != '"')
					return false;
				++i;
				for (; i < json.Length(); ++i) {
					const char c = json.Data()[i];
					if (c == '"')
						return true; // chaine fermee : ce qu'on a lu est le champ
					if (c != '\\') {
						const char deux[2] = {c, '\0'};
						out.Append(deux);
						continue;
					}
					++i;
					if (i >= json.Length())
						return false;
					const char e = json.Data()[i];
					if (e == 'n') {
						out.Append("\n");
					} else if (e == 't') {
						out.Append("\t");
					} else if (e == 'r') {
						out.Append("\r");
					} else if (e == 'u') {
						// ⚠️ ON SAUTE LES QUATRE CHIFFRES, ET ON LE DIT. Decoder
						//    l'UTF-16 echappe demanderait de gerer les paires de
						//    substitution ; le service rend de l'UTF-8 brut pour
						//    tout le reste, et nos documents sont en ASCII + UTF-8
						//    direct. Le jour ou un modele echappera ses accents,
						//    c'est ICI qu'il faudra regarder.
						i += 4u;
					} else {
						const char deux[2] = {e, '\0'};
						out.Append(deux);
					}
				}
				return false; // chaine jamais fermee : on refuse, on n'invente pas
			}

			/// Ajoute au plus `max` caracteres : un corps d'erreur de 40 ko dans un
			/// message de refus rend le refus illisible, donc inutile.
			static void AppendBorne(NkString &dst, const NkString &src, nkentseu::uint32 max) {
				for (NkString::SizeType i = 0; i < src.Length() && i < (NkString::SizeType)max; ++i) {
					const char deux[2] = {src.Data()[i], '\0'};
					dst.Append(deux);
				}
			}
	};

} // namespace nkentseu::converse
