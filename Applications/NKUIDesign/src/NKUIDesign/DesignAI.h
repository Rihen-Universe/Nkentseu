#pragma once
// -----------------------------------------------------------------------------
// @File    DesignAI.h
// @Brief   LA PLACE DE L'IA — elle passe par la MEME PORTE que la main.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI EST LIVRE ICI, ET CE QUI NE L'EST PAS — a lire en premier
// =============================================================================
//  ⚠️ **LE MODELE SPECIALISE N'EXISTE PAS.** Il s'entrainera sur des
//     declarations, et il n'y en a presque pas encore. C'est la boucle posee par
//     Rodolf : l'outil produit le corpus qui entraine le modele. Ce fichier ne
//     contient donc AUCUNE intelligence, et n'en revendique aucune.
//
//  CE QUI EST LIVRE, c'est **la place** — et elle coute quelques dizaines de
//  lignes aujourd'hui contre une refonte plus tard :
//    1. un point d'entree de prompt, en francais ;
//    2. un backend REMPLACABLE (Claude, Ollama, Ilyana demain) derriere une
//       interface qui ne suppose rien de l'un ni de l'autre ;
//    3. ⚠️ **la sortie de l'IA passe par la MEME PORTE que la main** : elle
//       produit un DOCUMENT, qui atterrit dans l'arbre par `GraftFrom`, la
//       fonction qu'utilise aussi le copier-coller. Jamais du code, jamais un
//       artefact a part ;
//    4. la provenance se remplit toute seule (`auteur = ia`), et la correction
//       humaine ensuite aussi (`MarkHumanEdit`) — c'est la que le corpus se
//       fabrique, sans effort supplementaire ;
//    5. **le rejeu comme verificateur** : une proposition qui ne se rejoue pas a
//       l'identique est ECARTEE. Le moteur est le juge, pas l'oeil.
//
//  CE QUI N'EST PAS LIVRE, ET QUI EST NOMME :
//    - **aucun backend reseau.** PV3DE parle a Claude et a Ollama par des
//      sockets ecrites a la main (`Applications/PV3DE/src/PV3DE/AI/Backends/`,
//      « en production : remplacer par NKStream::HttpClient quand disponible »).
//      Recopier ces sockets ici en ferait une troisieme copie, dans une
//      application, alors que la directive du depot est exactement l'inverse.
//      **Le client HTTP doit monter dans un module partage** — porte au canal.
//      En attendant, le backend FICHIER ci-dessous n'est pas un pis-aller : il
//      rend l'outil utilisable des aujourd'hui avec n'importe quel modele, y
//      compris a la main.
//    - aucune conversation multi-tours, aucun streaming, aucun asynchrone. La
//      requete est synchrone et le restera tant qu'aucun backend ne bloque
//      vraiment.
//
// =============================================================================
//  POURQUOI LE REJEU EST LA PIECE QUI COMPTE
// =============================================================================
//  Regle du corpus : *« une paire fausse est apprise fidelement »*. Un corpus
//  synthetique non verifie est pire qu'un petit corpus vrai. Ici le verificateur
//  existe deja et il est mecanique : une declaration se **rejoue**.
//
//      texte propose -> document -> reserialise -> relu -> MEME mise en page ?
//
//  Ce que ce controle attrape reellement : une proposition qui ne survit pas a
//  un aller-retour par le format — reference inventee, structure incoherente,
//  valeur que l'ecrivain ne sait pas reecrire. Ce sont exactement les fautes
//  qu'un modele produit quand il ecrit « a peu pres » le bon format.
//
//  ⚠️ CE QU'IL N'ATTRAPE PAS, ET IL FAUT LE DIRE AVEC LUI : une interface
//     PARFAITEMENT bien formee et LAIDE passe le rejeu sans broncher. Le rejeu
//     verifie la FIDELITE, pas la qualite. Le jugement esthetique reste humain,
//     et c'est precisement pourquoi `corrected` existe.
//
// OU AJOUTER LA PROCHAINE CHOSE :
//   un backend de plus -> une classe qui herite de `NkIDesignBackend`, et rien
//   d'autre a toucher : ni le prompt, ni la porte, ni la provenance, ni la
//   sonde.
// -----------------------------------------------------------------------------

#include "NKFileSystem/NkFile.h"

#include "Layout.h"

namespace nkuidesign {

	// ── LA REQUETE ET LA REPONSE ────────────────────────────────────────────
	struct NkDesignRequest {
			NkString prompt;	 ///< ce que l'utilisateur demande, en francais
			NkString catalog;	 ///< les composants DECLARES, engendres depuis le registre
			NkString currentDoc; ///< le document courant, pour qu'elle puisse le CONTINUER
	};

	struct NkDesignReply {
			NkString text; ///< la reponse brute du backend
			bool success = false;
			NkString error;
	};

	// ── L'INTERFACE DE BACKEND ──────────────────────────────────────────────
	// Meme forme que `NkIConvBackend` de PV3DE, volontairement : c'est le patron
	// qui a deja servi, et un second patron pour la meme chose serait une
	// divergence gratuite.
	class NkIDesignBackend {
		public:
			virtual ~NkIDesignBackend() = default;
			virtual bool Complete(const NkDesignRequest &req, NkDesignReply &out) = 0;
			virtual bool IsAvailable() const = 0;
			virtual const char *Name() const = 0;
	};

	// ── BACKEND FICHIER ─────────────────────────────────────────────────────
	// Il ecrit le prompt complet dans un fichier et lit la reponse dans un autre.
	//
	// ⚠️ CE N'EST PAS UN BOUCHON. C'est le seul backend qui fonctionne
	//    AUJOURD'HUI, sans reseau, avec n'importe quel modele : on donne le
	//    fichier de prompt a qui on veut, on colle la reponse dans le fichier de
	//    reponse, l'outil la verifie et la pose. Le jour ou un backend reseau
	//    arrive, celui-ci reste utile — c'est lui qui permet de rejouer une
	//    reponse a l'identique pour comprendre un rejet.
	class NkFileBackend final : public NkIDesignBackend {
		public:
			NkString promptPath = NkString("nkuidesign_prompt.txt");
			NkString replyPath = NkString("nkuidesign_reponse.txt");

			bool Complete(const NkDesignRequest &req, NkDesignReply &out) override {
				NkString full;
				full.Append(req.prompt);
				full.Append("\n\n--- composants declares ---\n");
				full.Append(req.catalog);
				full.Append("\n--- document courant ---\n");
				full.Append(req.currentDoc);
				nkentseu::NkFile::WriteAllText(promptPath.Data(), full.Data());

				if (!nkentseu::NkFile::Exists(replyPath.Data())) {
					out.success = false;
					out.error = NkString("Prompt ecrit dans ");
					out.error.Append(promptPath);
					out.error.Append(" — collez la reponse dans ");
					out.error.Append(replyPath);
					return false;
				}
				out.text = nkentseu::NkFile::ReadAllText(replyPath.Data());
				out.success = out.text.Length() > 0;
				if (!out.success)
					out.error = NkString("Fichier de reponse vide.");
				return out.success;
			}
			bool IsAvailable() const override {
				return true;
			}
			const char *Name() const override {
				return "fichier";
			}
	};

	// ── BACKEND EN CONSERVE ─────────────────────────────────────────────────
	// ⚠️ CELUI-CI EST UN INSTRUMENT DE MESURE, pas un backend de travail : il rend
	//    la reponse qu'on lui a posee. Il existe pour que la sonde puisse exercer
	//    la chaine complete — prompt, porte, verification, provenance, rejet —
	//    sans reseau et sans modele. **Condition de retrait : aucune.** Le jour ou
	//    un vrai backend existe, celui-ci reste, exactement comme un peintre
	//    enregistreur reste a cote d'un vrai peintre : c'est ce qui permet de
	//    tester le rejet sans avoir a provoquer une mauvaise reponse chez un
	//    modele.
	class NkCannedBackend final : public NkIDesignBackend {
		public:
			NkString canned;
			bool available = true;
			int32 calls = 0;

			bool Complete(const NkDesignRequest &, NkDesignReply &out) override {
				++calls;
				out.text = canned;
				out.success = available && canned.Length() > 0;
				if (!out.success)
					out.error = NkString("backend en conserve : rien a rendre");
				return out.success;
			}
			bool IsAvailable() const override {
				return available;
			}
			const char *Name() const override {
				return "conserve";
			}
	};

	// ── LE VERDICT ──────────────────────────────────────────────────────────
	// Une proposition n'est pas « acceptee ou refusee » : elle est refusee POUR
	// UNE RAISON, et la raison est ce qui permet d'ameliorer le prompt (ou de
	// constater que le modele n'est pas pret). Un booleen aurait rendu tout ca
	// muet.
	enum class NkAIVerdict : uint8 {
		Acceptee = 0,
		BackendMuet,	 ///< le backend n'a rien rendu
		TexteNonConforme, ///< pas de document lisible dans la reponse
		ComposantInconnu, ///< nomme un composant que le registre ignore
		RejeuDivergent,	  ///< se charge, mais ne survit pas a l'aller-retour
		GreffeRefusee,	  ///< la cible n'accepte pas (index invalide, cycle)
		Count
	};

	inline const char *NkAIVerdictName(NkAIVerdict v) {
		switch (v) {
			case NkAIVerdict::Acceptee:
				return "acceptee";
			case NkAIVerdict::BackendMuet:
				return "le backend n'a rien rendu";
			case NkAIVerdict::TexteNonConforme:
				return "aucun document lisible dans la reponse";
			case NkAIVerdict::ComposantInconnu:
				return "nomme un composant que le registre ignore";
			case NkAIVerdict::RejeuDivergent:
				return "ne se rejoue pas a l'identique";
			case NkAIVerdict::GreffeRefusee:
				return "cible de greffe invalide";
			default:
				return "?";
		}
	}

	struct NkAIResult {
			NkAIVerdict verdict = NkAIVerdict::BackendMuet;
			int32 graftedRoot = -1; ///< index du sous-arbre pose, -1 si rejet
			uint32 nodesAdded = 0;
			uint32 unknownComponents = 0;
			uint32 replayDiffs = 0; ///< rectangles qui divergent apres aller-retour
			NkString detail;

			bool Accepted() const {
				return verdict == NkAIVerdict::Acceptee;
			}
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  L'ORCHESTRATEUR
	// ═══════════════════════════════════════════════════════════════════════════
	class NkDesignAI {
		public:
			/// La surface de reference sur laquelle le rejeu se compare. Sa valeur
			/// exacte n'a pas d'importance ; ce qui compte est qu'elle soit LA MEME
			/// des deux cotes de l'aller-retour.
			NkPaintRect replaySurface = {0.f, 0.f, 1200.f, 800.f};

			void SetBackend(NkIDesignBackend *b) {
				mBackend = b;
			}
			NkIDesignBackend *Backend() const {
				return mBackend;
			}

			// ── LE CATALOGUE, ENGENDRE DEPUIS LE REGISTRE ────────────────────
			// ⚠️ IL BOUCLE SUR LE REGISTRE, il n'enumere aucun nom. C'est la meme
			//    exigence que pour la palette, et pour la meme raison : un
			//    catalogue ecrit a la main proposerait a l'IA des composants
			//    inexistants (ou lui cacherait les nouveaux), et l'outil
			//    « marcherait » en produisant des documents impossibles a poser.
			static void BuildCatalog(NkString &out) {
				out = NkString("");
				const uint16 n = NkComponentRegistry::Count();
				for (uint16 i = 0; i < n; ++i) {
					const NkComponentDecl *d = NkComponentRegistry::At(i);
					if (!d)
						continue;
					out.Append("composant ");
					out.Append(d->name);
					out.Append(" : ");
					out.Append(d->summary ? d->summary : "");
					out.Append('\n');
					for (uint16 v = 0; v < d->variantCount; ++v) {
						out.Append("    variante ");
						out.Append(d->variants[v].name);
						out.Append('\n');
					}
					for (uint16 p = 0; p < d->paramCount; ++p) {
						out.Append("    param ");
						out.Append(d->params[p].name);
						out.Append('\n');
					}
					for (uint16 m = 0; m < d->metricCount; ++m) {
						out.Append("    metrique ");
						out.Append(d->metrics[m].name);
						out.Append('\n');
					}
				}
			}

			/// Le prompt. Il dit trois choses, et rien de plus : ce qu'on veut, ce
			/// qui existe, et **le format exact attendu** — engendre depuis les
			/// memes fonctions de nom que l'ecrivain, pour qu'il ne puisse pas
			/// deriver du format reel.
			static void BuildPrompt(const char *userAsk, NkString &out) {
				out = NkString("Tu produis une INTERFACE pour NkUIDesign.\n\n");
				out.Append("Demande : ");
				out.Append(userAsk ? userAsk : "");
				out.Append("\n\nReponds UNIQUEMENT par un document au format ci-dessous,\n");
				out.Append("sans explication, sans balise de code.\n\n");
				out.Append("REGLE ABSOLUE : n'ecris JAMAIS de position ni de coordonnee.\n");
				out.Append("La position se calcule ; tu declares des tailles et un agencement.\n\n");
				out.Append("Format :\n");
				out.Append("nkuidoc 1\n");
				out.Append("titre = <texte>\n");
				out.Append("noeud <numero, en partant de 0 pour la racine>\n");
				out.Append("  libelle = <texte>\n");
				out.Append("  composant = <nom du catalogue, ou vide pour un cadre>\n");
				out.Append("  enfants = <numeros de noeuds, separes par des espaces>\n");
				out.Append("  largeur = <");
				AppendModes(out);
				out.Append("> <valeur> <min> <max>\n");
				out.Append("  hauteur = <idem>\n");
				out.Append("  agencement = <");
				AppendKinds(out);
				out.Append(">\n");
				out.Append("  ecart = <px>\n  marge = <px>\n  colonnes = <n, pour la grille>\n");
				out.Append("  alignement = <");
				AppendAligns(out);
				out.Append(">\n");
				out.Append("  reglage param <nom> = <valeur>   (facultatif)\n");
				out.Append("  reglage variante <nom>           (facultatif)\n\n");
				out.Append("N'emploie que des composants du catalogue ci-dessous.\n");
			}

			// ═══════════════════════════════════════════════════════════════════
			//  DEMANDER, PUIS POSER
			// ═══════════════════════════════════════════════════════════════════
			// ⚠️ `doc` N'EST TOUCHE QU'A LA TOUTE DERNIERE ETAPE, et c'est ce qui
			//    rend « le rejet laisse le document intact » vrai par CONSTRUCTION
			//    plutot que par vigilance. Tout se passe dans `scratch`, un
			//    document de cote. Une version qui aurait pose d'abord et verifie
			//    ensuite aurait eu besoin d'une annulation — et une annulation qui
			//    ne sert que dans le cas d'erreur est une annulation qui n'est
			//    jamais testee.
			NkAIResult Ask(const char *userAsk, NkUIDocument &doc, int32 targetParent) {
				NkAIResult res;
				if (!mBackend) {
					res.detail = NkString("aucun backend branche");
					return res;
				}
				NkDesignRequest req;
				BuildPrompt(userAsk, req.prompt);
				BuildCatalog(req.catalog);
				doc.Save(req.currentDoc);

				NkDesignReply reply;
				if (!mBackend->Complete(req, reply) || reply.text.Length() == 0) {
					res.verdict = NkAIVerdict::BackendMuet;
					res.detail = reply.error;
					return res;
				}
				mLastReply = reply.text;
				return Apply(reply.text.Data(), doc, targetParent, mBackend->Name());
			}

			/// Poser une reponse deja obtenue. Separee de `Ask` pour une raison
			/// pratique : c'est ce qui permet de rejouer un texte suspect autant de
			/// fois qu'on veut, sans rappeler un modele et sans payer un jeton.
			NkAIResult Apply(const char *replyText, NkUIDocument &doc, int32 targetParent,
							 const char *origin) {
				NkAIResult res;
				if (!doc.IsValidIndex(targetParent)) {
					res.verdict = NkAIVerdict::GreffeRefusee;
					return res;
				}

				// 1. Extraire le document de la reponse. Un modele encadre volontiers
				//    sa sortie de commentaires ou de balises : on part de la premiere
				//    ligne `nkuidoc`. Si elle n'y est pas, c'est non — on ne devine
				//    pas ce qu'il a voulu dire.
				const char *body = FindHeader(replyText);
				if (!body) {
					res.verdict = NkAIVerdict::TexteNonConforme;
					res.detail = NkString("pas de ligne `nkuidoc` dans la reponse");
					return res;
				}

				// 2. Charger de cote.
				NkUIDocument scratch;
				uint32 unknown = 0;
				if (!scratch.Load(body, &unknown) || scratch.NodeCount() == 0) {
					res.verdict = NkAIVerdict::TexteNonConforme;
					res.detail = NkString("document illisible ou structure incoherente");
					return res;
				}
				res.unknownComponents = unknown;
				if (unknown > 0 || !NkUIDocument::CanGraft(scratch, 0)) {
					res.verdict = NkAIVerdict::ComposantInconnu;
					return res;
				}

				// 3. LE REJEU. C'est le verificateur, et il est mecanique.
				res.replayDiffs = ReplayDiffs(scratch, replaySurface);
				if (res.replayDiffs > 0) {
					res.verdict = NkAIVerdict::RejeuDivergent;
					return res;
				}

				// 4. La porte — la MEME que la main.
				const uint32 before = doc.NodeCount();
				const int32 root = doc.GraftFrom(scratch, 0, targetParent, true, NkAuthor::IA, origin);
				if (root < 0) {
					res.verdict = NkAIVerdict::GreffeRefusee;
					return res;
				}
				// 5. Le tampon « rejouee » : il est pose parce que le rejeu a EU LIEU
				//    juste au-dessus, pas parce que ca vient de l'IA.
				doc.MarkVerified(root);
				res.verdict = NkAIVerdict::Acceptee;
				res.graftedRoot = root;
				res.nodesAdded = doc.NodeCount() - before;
				return res;
			}

			/// LE REJEU, isole pour etre reutilisable : un document produit A LA
			/// MAIN se verifie exactement pareil. Rend le nombre de rectangles qui
			/// divergent apres un aller-retour par le texte — 0 = fidele.
			///
			/// ⚠️ ET SA CONDITION D'EXISTENCE : si la relecture ne rend aucun noeud,
			///    il n'y a rien a comparer, et « 0 difference » serait un succes A
			///    VIDE. On rend alors un chiffre non nul, qui vaut echec. C'est le
			///    defaut trouve dans la sonde du 18/08, applique ici avant qu'il se
			///    reproduise.
			static uint32 ReplayDiffs(const NkUIDocument &doc, const NkPaintRect &surface) {
				if (doc.NodeCount() == 0)
					return 1u;
				NkString text;
				doc.Save(text);
				NkUIDocument again;
				if (!again.Load(text.Data()))
					return 1u;
				if (again.NodeCount() != doc.NodeCount())
					return 1u;

				NkLayoutResult a, b;
				NkComputeLayout(doc, surface, a);
				NkComputeLayout(again, surface, b);
				if (a.ValidCount() == 0 || a.ValidCount() != b.ValidCount())
					return 1u;
				uint32 diffs = 0;
				for (uint32 i = 0; i < (uint32)doc.NodeCount(); ++i) {
					const bool ha = a.Has((int32)i), hb = b.Has((int32)i);
					if (ha != hb) {
						++diffs;
						continue;
					}
					if (!ha)
						continue;
					if (!SameRect(a.At((int32)i), b.At((int32)i)))
						++diffs;
				}
				return diffs;
			}

			static bool SameRect(const NkPaintRect &a, const NkPaintRect &b) {
				return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.w, b.w) && Near(a.h, b.h);
			}

			const NkString &LastReply() const {
				return mLastReply;
			}

		private:
			static bool Near(float32 a, float32 b) {
				const float32 d = a - b;
				return d < 0.01f && d > -0.01f;
			}
			/// La premiere ligne qui commence par `nkuidoc`, en debut de ligne.
			static const char *FindHeader(const char *s) {
				if (!s)
					return nullptr;
				const char *line = s;
				while (*line) {
					const char *p = line;
					while (*p == ' ' || *p == '\t')
						++p;
					if (p[0] == 'n' && p[1] == 'k' && p[2] == 'u' && p[3] == 'i' && p[4] == 'd' &&
						p[5] == 'o' && p[6] == 'c')
						return p;
					while (*line && *line != '\n')
						++line;
					if (*line)
						++line;
				}
				return nullptr;
			}
			// Les listes de valeurs du prompt sont engendrees depuis les MEMES
			// fonctions de nom que l'ecrivain de fichier. Les taper a la main ferait
			// deux verites, et la seconde vieillirait en silence : le modele
			// recevrait un vocabulaire perime et ses reponses seraient rejetees sans
			// qu'on comprenne pourquoi.
			static void AppendModes(NkString &out) {
				for (uint8 i = 0; i < (uint8)NkSizeMode::Count; ++i) {
					if (i)
						out.Append(" | ");
					out.Append(NkSizeModeName((NkSizeMode)i));
				}
			}
			static void AppendKinds(NkString &out) {
				for (uint8 i = 0; i < (uint8)NkLayoutKind::Count; ++i) {
					if (i)
						out.Append(" | ");
					out.Append(NkLayoutKindName((NkLayoutKind)i));
				}
			}
			static void AppendAligns(NkString &out) {
				for (uint8 i = 0; i < (uint8)NkAlign::Count; ++i) {
					if (i)
						out.Append(" | ");
					out.Append(NkAlignName((NkAlign)i));
				}
			}

			NkIDesignBackend *mBackend = nullptr;
			NkString mLastReply;
	};

} // namespace nkuidesign
