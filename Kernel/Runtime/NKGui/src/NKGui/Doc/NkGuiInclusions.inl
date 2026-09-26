// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkGuiInclusions.inl — la résolution, et les quatre refus qu'elle sait dire.
// =============================================================================

namespace nkentseu {
	namespace nkgui {

		namespace detail {

			/// Vrai si `chemin` est déjà dans la pile d'inclusion en cours.
			inline bool NkGIDansLaPile(const NkVector<NkString> &pile,
									   const NkString &chemin) noexcept {
				for (uint32 i = 0; i < (uint32)pile.Size(); ++i)
					if (pile[i].Compare(chemin.CStr()) == 0)
						return true;
				return false;
			}

			/// La passe récursive. `pile` porte les chemins en cours d'inclusion,
			/// ce qui rend le cycle détectable sans profondeur maximale.
			inline void NkGIResoudre(NkArchive &doc, const NkString &dossierBase,
									 NkVector<NkString> &pile,
									 NkGuiRapportInclusions &rap) noexcept {
				NkArchiveNode *corps = NkGICorpsMut(doc);
				if (!corps)
					return;

				NkVector<NkArchiveNode> sortie;
				for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
					NkArchiveNode &n = corps->array[i];

					// ⚠️ DEUX ECRITURES, ET LA NATURELLE EST SANS ACCOLADES.
					//    `include "x" { }` est un BLOC : l'archive le range comme
					//    tel. `include "x"` tout court n'en est pas un -- il n'a pas
					//    de corps -- et l'archive le conserve alors en TRANCHE DE
					//    SOURCE VERBATIM (regle T11 du format).
					//
					//    On lit donc les deux. Exiger les accolades vides aurait
					//    laisse une verrue dans CHAQUE document, et un auteur qui
					//    ecrit la forme naturelle aurait vu son inclusion ignoree en
					//    silence -- le defaut que ce fichier existe pour supprimer.
					NkString relatif;
					bool estInclusion = false;

					if (n.IsObject() && n.object
						&& NkGIMotEgal(NkGuiArchive::TypeOf(*n.object), "include")) {
						relatif = NkString(NkGuiArchive::IdOf(*n.object));
						estInclusion = true;
					} else if (!n.IsObject()) {
						// Une tranche brute : `include "chemin"`, eventuellement
						// precedee d'espaces. On extrait ce qui est entre guillemets.
						const NkString brut(n.Lexeme());
						const char *p = brut.CStr();
						uint32 k = 0;
						while (p[k] == ' ' || p[k] == '\t' || p[k] == '\r' || p[k] == '\n')
							++k;
						const char *mot = "include";
						uint32 m = 0;
						while (mot[m] && p[k + m] == mot[m])
							++m;
						if (mot[m] == '\0') {
							k += m;
							while (p[k] == ' ' || p[k] == '\t')
								++k;
							if (p[k] == '"') {
								++k;
								NkString chem;
								while (p[k] && p[k] != '"') {
									const char c[2] = {p[k], '\0'};
									chem += c;
									++k;
								}
								if (p[k] == '"' && chem.Size() > 0u) {
									relatif = chem;
									estInclusion = true;
								}
							}
						}
					}

					if (!estInclusion) {
						sortie.PushBack(n);
						continue;
					}

					// ── C'EST UNE INCLUSION ─────────────────────────────────
					if (relatif.Size() == 0u) {
						++rap.illisibles;
						rap.refuses.PushBack(NkString("include sans chemin (refuse)"));
						continue;
					}

					// ⚠️ SANS PROVENANCE, ON NE DEVINE PAS. `Adopter` prend un arbre
					//    et ne connait aucun chemin : chercher dans le dossier courant
					//    du processus donnerait un resultat qui depend d'ou on a lance
					//    l'application. On COMPTE et on garde le bloc.
					if (dossierBase.Size() == 0u) {
						++rap.nonResolues;
						NkString r("include \"");
						r += relatif;
						r += "\" : aucun dossier de base (document adopte comme arbre)";
						rap.refuses.PushBack(r);
						sortie.PushBack(n);
						continue;
					}

					const NkString chemin = NkGIJoindre(dossierBase, relatif);

					if (NkGIDansLaPile(pile, chemin)) {
						++rap.cycles;
						NkString r("include \"");
						r += relatif;
						r += "\" : cycle d'inclusion (refuse)";
						rap.refuses.PushBack(r);
						continue;
					}

					const NkVector<nk_uint8> octets = NkFile::ReadAllBytes(chemin.CStr());
					if (octets.Size() == 0u) {
						++rap.introuvables;
						NkString r("include \"");
						r += relatif;
						r += "\" : introuvable ou vide -> ";
						r += chemin;
						rap.refuses.PushBack(r);
						continue;
					}

					NkArchive inclus;
					NkGuiDiag err;
					if (!NkGuiArchive::Read((const char *)octets.Data(), (uint32)octets.Size(),
											inclus, err)) {
						++rap.illisibles;
						NkString r("include \"");
						r += relatif;
						r += "\" : illisible (le lecteur refuse)";
						rap.refuses.PushBack(r);
						continue;
					}

					// Les inclusions du document inclus se resolvent DEPUIS SON
					// PROPRE dossier : un chemin ecrit dans une bibliotheque parle
					// du voisinage de la bibliotheque, pas de celui qui l'emploie.
					pile.PushBack(chemin);
					NkGIResoudre(inclus, NkGIDossierDe(chemin.CStr()), pile, rap);
					pile.PopBack();

					// ⚠️ LA LIGNE DE VERSION N'EST PAS DU CONTENU. Elle appartient au
					//    FICHIER inclus ; la recopier poserait deux versions dans un
					//    seul document, et la seconde ecraserait la premiere en
					//    silence.
					const NkArchiveNode *corpsInclus = NkGICorpsMut(inclus);
					if (corpsInclus) {
						for (uint32 k = 0; k < (uint32)corpsInclus->array.Size(); ++k) {
							sortie.PushBack(corpsInclus->array[k]);
							++rap.sectionsApportees;
						}
					}
					++rap.resolues;
				}
				corps->array = sortie;
			}

		} // namespace detail

		inline bool NkGuiResoudreInclusions(NkArchive &doc, const char *dossierBase,
											NkGuiRapportInclusions &rap) noexcept {
			using namespace detail;
			NkVector<NkString> pile;
			NkGIResoudre(doc, NkString(dossierBase ? dossierBase : ""), pile, rap);
			return rap.Propre();
		}

	} // namespace nkgui
} // namespace nkentseu
