//
// NkEditeurDeroulement.cpp
// =============================================================================
// Description :
//   Le deroulement d'une construction et son journal (voir l'en-tete).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Livraison/NkEditeurDeroulement.h"

#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKTime/NkChrono.h"

#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			/// La largeur du texte dans une boite de Jenga : _BOX_WIDTH 96, moins
			/// les deux bords et leurs deux espaces (Utils/Reporter.py).
			constexpr int32 NK_LARGEUR_BOITE = 92;

			float64 Maintenant() {
				return NkChrono::Now().ToSeconds();
			}

			int32 Colonnes(const NkString &s) noexcept {
				int32 n = 0;
				for (usize i = 0; i < s.Length(); ++i) {
					const uint8 c = static_cast<uint8>(s[i]);
					n += (c & 0xC0u) == 0x80u ? 0 : 1;
				}
				return n;
			}

			/// Le caractere de cadre U+25xx a la position `i` : E2 b2 b3.
			bool Cadre(const NkString &s, usize i, uint8 b2, uint8 b3) noexcept {
				return i + 2u < s.Length() + 0u && static_cast<uint8>(s[i]) == 0xE2u && static_cast<uint8>(s[i + 1u]) == b2 &&
					   static_cast<uint8>(s[i + 2u]) == b3;
			}

			NkString SansEspaces(const NkString &s) {
				usize d = 0;
				while (d < s.Length() && s[d] == ' ') {
					++d;
				}
				usize f = s.Length();
				while (f > d && s[f - 1u] == ' ') {
					--f;
				}
				return NkString(s.SubStr(d, f - d));
			}

			/// Les suites d'espaces reduites a une (les colonnes de Jenga).
			NkString Resserre(const NkString &s) {
				NkString r;
				bool espace = false;
				for (usize i = 0; i < s.Length(); ++i) {
					if (s[i] == ' ') {
						espace = true;
						continue;
					}
					if (espace && !r.Empty()) {
						r.Append(' ');
					}
					espace = false;
					r.Append(s[i]);
				}
				return r;
			}

			/// Le symbole de tete de Jenga (✓ ✗ ⚠ ℹ ⊘ →) et ses espaces : la
			/// couleur et l'icone de la ligne le disent deja.
			NkString SansSymbole(const NkString &s) {
				usize i = 0;
				for (int32 tour = 0; tour < 3; ++tour) {
					while (i < s.Length() && s[i] == ' ') {
						++i;
					}
					const uint8 c = i < s.Length() ? static_cast<uint8>(s[i]) : 0u;
					if (c == 0xE2u && i + 2u < s.Length()) {
						i += 3u;
						continue;
					}
					break;
				}
				return NkString(s.SubStr(i));
			}

			bool EstChiffres(const NkString &s) noexcept {
				if (s.Empty()) {
					return false;
				}
				for (usize i = 0; i < s.Length(); ++i) {
					if (s[i] < '0' || s[i] > '9') {
						return false;
					}
				}
				return true;
			}

			int32 Entier(const char *p) noexcept {
				return static_cast<int32>(std::strtol(p, nullptr, 10));
			}

			/// Un morceau de boite qui commence FORCEMENT une ligne : un chemin,
			/// un extrait de source (indente), « In file included from »...
			bool DebutDeLigne(const NkString &m) noexcept {
				if (m.Empty() || m[0] == ' ' || m[0] == '/' || m[0] == '\\') {
					return true;
				}
				if (m.Length() > 2u && ((m[0] >= 'A' && m[0] <= 'Z') || (m[0] >= 'a' && m[0] <= 'z')) && m[1] == ':' &&
					(m[2] == '/' || m[2] == '\\')) {
					return true;
				}
				return m.StartsWith("In file included from") || m.StartsWith("In instantiation") ||
					   (m[0] >= '0' && m[0] <= '9' && (m.Find(" error") != NkString::npos || m.Find(" warning") != NkString::npos));
			}

			NkString PremierMot(const NkString &m) {
				const usize e = m.Find(' ');
				return e == NkString::npos ? m : NkString(m.SubStr(0, e));
			}
		} // namespace

		// =====================================================================
		// LE JOURNAL
		// =====================================================================
		void NkJournalConstruction::Vider() {
			*this = NkJournalConstruction();
		}

		void NkJournalConstruction::NouvelleCommande() {
			projetsTotal = 0;
			projetsFaits = 0;
			fichiersTotal = 0;
			fichiersFaits = 0;
			projet = NkString();
			lien = false;
			mBoite = 0;
			mRecousue = NkString();
			mLargeur = 0;
		}

		float32 NkJournalConstruction::Fraction() const noexcept {
			const float32 dansProjet = fichiersTotal > 0 ? static_cast<float32>(fichiersFaits) / static_cast<float32>(fichiersTotal) : 0.f;
			float32 f = -1.f;
			if (projetsTotal > 0) {
				f = (static_cast<float32>(projetsFaits) + (projetsFaits < projetsTotal ? dansProjet : 0.f)) /
					static_cast<float32>(projetsTotal);
			} else if (fichiersTotal > 0) {
				f = dansProjet;
			}
			return f > 1.f ? 1.f : f;
		}

		NkNiveauLigne NkJournalConstruction::NiveauDe(const NkString &t) {
			const NkString s = SansEspaces(t);
			const bool coche = s.StartsWith("\xE2\x9C\x93"); // ✓
			if (s.StartsWith("\xE2\x9C\x97") || s.Find("error:") != NkString::npos || s.Find("Error:") != NkString::npos ||
				s.Find("ECHEC") != NkString::npos || s.Find("FAILED") != NkString::npos || s.Find("Build Failed") != NkString::npos ||
				s.Find("FAILURE") != NkString::npos || s.Find("impossible") != NkString::npos ||
				s.Find("undefined reference") != NkString::npos || s.StartsWith("Errors:")) {
				return NkNiveauLigne::NK_ERREUR;
			}
			if (s.StartsWith("\xE2\x9A\xA0") || s.Find("warning:") != NkString::npos || s.Find("with warnings") != NkString::npos ||
				s.Find("AVERTISSEMENT") != NkString::npos || s.StartsWith("Warnings:")) {
				return NkNiveauLigne::NK_AVERTISSEMENT;
			}
			// « ✓ [3/31] Compiled: x.cpp » est le fond du journal, pas un succes :
			// en vert, il noierait les vrais (Built:, Build Successful, VERIFIE).
			if ((coche && s.Find("Compiled") == NkString::npos) || s.Find("BUILD COMPLETED") != NkString::npos || s.Find("Build Successful") != NkString::npos ||
				s.StartsWith("Kit pret") || s.Find("VERIFIE") != NkString::npos) {
				return NkNiveauLigne::NK_SUCCES;
			}
			if (s.Find("note:") != NkString::npos) {
				return NkNiveauLigne::NK_NOTE;
			}
			return NkNiveauLigne::NK_INFO;
		}

		void NkJournalConstruction::Abreger(const NkString &prefixe, const NkString &par) {
			if (prefixe.Length() < 4u) {
				return;
			}
			// Les deux ecritures : clang cite le chemin tel qu'on le lui donne
			// (C:\Users\...), Jenga et l'editeur l'ecrivent en obliques.
			NkString oblique;
			NkString inverse;
			for (usize i = 0; i < prefixe.Length(); ++i) {
				oblique.Append(prefixe[i] == '\\' ? '/' : prefixe[i]);
				inverse.Append(prefixe[i] == '/' ? '\\' : prefixe[i]);
			}
			mPrefixes.PushBack(oblique);
			mRemplacements.PushBack(par);
			mPrefixes.PushBack(inverse);
			mRemplacements.PushBack(par);
		}

		NkString NkJournalConstruction::Lisible(const NkString &texte) const {
			// Les symboles de Jenga (coche, croix, alerte, info, interdit,
			// fleche), ou qu'ils soient : la police de l'editeur ne les a pas
			// (« Status: ? FAILURE »), et la couleur de la ligne le dit deja.
			static const char *kSymboles[] = {"\xE2\x9C\x93", "\xE2\x9C\x97", "\xE2\x9A\xA0",
											  "\xE2\x84\xB9", "\xE2\x8A\x98", "\xE2\x86\x92"};
			NkString s;
			for (usize i = 0; i < texte.Length(); ++i) {
				bool symbole = false;
				for (const char *k : kSymboles) {
					if (i + 2u < texte.Length() && texte[i] == k[0] && texte[i + 1u] == k[1] && texte[i + 2u] == k[2]) {
						symbole = true;
						break;
					}
				}
				if (symbole) {
					i += 2u;
					continue;
				}
				s.Append(texte[i]);
			}
			// Les chemins connus raccourcis : « Applications/UnkenyPlayer/... »
			// plutot que trois lignes de C:\Users\...
			for (usize k = 0; k < mPrefixes.Size(); ++k) {
				usize p = 0;
				while ((p = s.Find(mPrefixes[k].CStr(), p)) != NkString::npos) {
					s = NkString(s.SubStr(0, p)) + mRemplacements[k] + NkString(s.SubStr(p + mPrefixes[k].Length()));
					p += mRemplacements[k].Length();
				}
			}
			return SansEspaces(s);
		}

		void NkJournalConstruction::Pousser(NkNiveauLigne niveau, const NkString &texte, float32 temps) {
			NkLigneJournal l;
			l.niveau = niveau;
			l.phase = mPhase;
			l.temps = temps;
			l.texte = Lisible(texte);
			if (l.texte.Empty()) {
				return;
			}
			lignes.PushBack(l);
		}

		void NkJournalConstruction::Annonce(const NkString &texte, NkNiveauLigne niveau, float32 temps) {
			Pousser(niveau, texte, temps);
		}

		void NkJournalConstruction::Diagnostiquer(const NkString &t) {
			static const char *kMarques[] = {": fatal error: ", ": error: ", ": warning: "};
			usize pos = NkString::npos;
			usize longueur = 0;
			bool erreur = true;
			for (int32 k = 0; k < 3; ++k) {
				const usize p = t.Find(kMarques[k]);
				if (p != NkString::npos && p < pos) {
					pos = p;
					longueur = std::strlen(kMarques[k]);
					erreur = k < 2;
				}
			}
			NkDiagnostic d;
			if (pos == NkString::npos) {
				const usize u = t.Find("undefined reference to");
				if (u == NkString::npos) {
					return;
				}
				d.message = NkString(t.SubStr(u));
			} else {
				d.erreur = erreur;
				d.message = NkString(t.SubStr(pos + longueur));
				NkString prefixe = SansEspaces(NkString(t.SubStr(0, pos)));
				// « chemin:ligne:colonne », « chemin:ligne » ou un outil (ld, clang++).
				const usize p1 = prefixe.RFind(':');
				if (p1 != NkString::npos && EstChiffres(NkString(prefixe.SubStr(p1 + 1u)))) {
					const usize p2 = p1 > 0u ? prefixe.RFind(':', p1 - 1u) : NkString::npos;
					if (p2 != NkString::npos && EstChiffres(NkString(prefixe.SubStr(p2 + 1u, p1 - p2 - 1u)))) {
						d.fichier = NkString(prefixe.SubStr(0, p2));
						d.ligne = Entier(prefixe.CStr() + p2 + 1u);
						d.colonne = Entier(prefixe.CStr() + p1 + 1u);
					} else {
						d.fichier = NkString(prefixe.SubStr(0, p1));
						d.ligne = Entier(prefixe.CStr() + p1 + 1u);
					}
				} else {
					d.fichier = prefixe;
				}
			}
			(d.erreur ? erreurs : avertissements)++;
			diagnostics.PushBack(d);
		}

		void NkJournalConstruction::Logique(const NkString &texte, float32 temps) {
			if (SansEspaces(texte).Empty()) {
				return;
			}
			NkNiveauLigne n = NkNiveauLigne::NK_NOTE;
			if (texte.Find("error:") != NkString::npos || texte.Find("undefined reference") != NkString::npos) {
				n = NkNiveauLigne::NK_ERREUR;
			} else if (texte.Find("warning:") != NkString::npos) {
				n = NkNiveauLigne::NK_AVERTISSEMENT;
			}
			if (n != NkNiveauLigne::NK_NOTE) {
				Diagnostiquer(texte);
			}
			Pousser(n, texte, temps);
		}

		void NkJournalConstruction::Progression(const NkString &s) {
			if (s.StartsWith("Build Order (")) {
				projetsTotal = Entier(s.CStr() + 13);
				return;
			}
			const usize ouvre = s.Find('[');
			if (s.Find("] Compiled") != NkString::npos && ouvre != NkString::npos) {
				const usize barre = s.Find('/', ouvre);
				if (barre != NkString::npos) {
					fichiersFaits = Entier(s.CStr() + ouvre + 1u);
					fichiersTotal = Entier(s.CStr() + barre + 1u);
				}
				return;
			}
			const usize trouve = s.Find("Found ");
			if (trouve != NkString::npos && s.Find("source file") != NkString::npos) {
				fichiersTotal = Entier(s.CStr() + trouve + 6u);
				fichiersFaits = 0;
				return;
			}
			if (s.Find("All files up to date") != NkString::npos) {
				fichiersFaits = fichiersTotal;
			} else if (s.Find("Linking...") != NkString::npos) {
				lien = true;
			}
		}

		void NkJournalConstruction::FinCommande(float32 temps) {
			if (!mRecousue.Empty()) {
				Logique(mRecousue, temps);
			}
			mRecousue = NkString();
			mLargeur = 0;
			mBoite = 0;
		}

		void NkJournalConstruction::LigneJenga(const NkString &brute, float32 temps) {
			usize i = 0;
			while (i < brute.Length() && brute[i] == ' ') {
				++i;
			}
			// ── Les bords de boite : ╔ ┌ ouvrent, ╠ separe, ╚ └ ferment. ─────────
			if (Cadre(brute, i, 0x95u, 0x94u) || Cadre(brute, i, 0x94u, 0x8Cu)) {
				FinCommande(temps);
				mBoite = Cadre(brute, i, 0x95u, 0x94u) ? 1 : 5;
				return;
			}
			if (Cadre(brute, i, 0x95u, 0xA0u) || Cadre(brute, i, 0x95u, 0x90u) || Cadre(brute, i, 0x94u, 0x80u)) {
				return; // ╠═══╣, ═══ (pied de page), ───
			}
			if (Cadre(brute, i, 0x95u, 0x9Au) || Cadre(brute, i, 0x94u, 0x94u)) {
				FinCommande(temps);
				return;
			}
			const bool double_ = Cadre(brute, i, 0x95u, 0x91u); // ║
			const bool simple = Cadre(brute, i, 0x94u, 0x82u);	// │
			if (double_ || simple) {
				// Le dedans : entre le premier bord et le dernier.
				usize fin = brute.Length();
				while (fin > i + 3u && !(static_cast<uint8>(brute[fin - 3u]) == 0xE2u && static_cast<uint8>(brute[fin - 2u]) == (double_ ? 0x95u : 0x94u) &&
										static_cast<uint8>(brute[fin - 1u]) == (double_ ? 0x91u : 0x82u))) {
					--fin;
				}
				NkString dedans(brute.SubStr(i + 3u, (fin > i + 6u ? fin - 3u : i + 3u) - (i + 3u)));
				if (mBoite == 1) {
					// L'en-tete : la boite d'un projet, d'une erreur, d'un
					// avertissement -- ou la banniere de Jenga.
					const NkString tete = Resserre(NkLigneDeJenga(dedans));
					const usize err = tete.Find("Compilation Error:");
					const usize avt = tete.Find("Warning:");
					if (tete.StartsWith("Project:")) {
						const usize k = tete.Find(" Kind:");
						projet = SansEspaces(NkString(tete.SubStr(9, (k == NkString::npos ? tete.Length() : k) - 9)));
						fichiersTotal = 0;
						fichiersFaits = 0;
						lien = false;
						mBoite = 4;
						Pousser(NkNiveauLigne::NK_ETAPE, NkString("Projet ") + projet, temps);
					} else if (err != NkString::npos) {
						mBoite = 2;
						Pousser(NkNiveauLigne::NK_ERREUR, NkString("Erreur de compilation : ") + SansEspaces(NkString(tete.SubStr(err + 18))), temps);
					} else if (avt != NkString::npos) {
						mBoite = 3;
						Pousser(NkNiveauLigne::NK_AVERTISSEMENT, NkString("Avertissement : ") + SansEspaces(NkString(tete.SubStr(avt + 8))), temps);
					} else {
						mBoite = 4;
						if (!tete.Empty()) {
							Pousser(NkNiveauLigne::NK_NOTE, tete, temps);
						}
					}
					return;
				}
				if (mBoite == 2 || mBoite == 3) {
					// Un MORCEAU : Jenga coupe a 92 colonnes, aux espaces, et coupe
					// net un mot plus long (un chemin). On recoud.
					if (!dedans.Empty() && dedans[0] == ' ') {
						dedans = NkString(dedans.SubStr(1));
					}
					usize f = dedans.Length();
					while (f > 0u && dedans[f - 1u] == ' ') {
						--f;
					}
					const NkString morceau(dedans.SubStr(0, f));
					if (mRecousue.Empty()) {
						mRecousue = morceau;
					} else if (mLargeur >= NK_LARGEUR_BOITE) {
						mRecousue += morceau; // un mot coupe net
					} else if (DebutDeLigne(morceau) ||
							   mLargeur + 1 + Colonnes(PremierMot(morceau)) <= NK_LARGEUR_BOITE) {
						// Le mot aurait tenu sur la ligne d'avant : Jenga ne l'a donc
						// pas coupee la, c'est une ligne a elle.
						Logique(mRecousue, temps);
						mRecousue = morceau;
					} else {
						mRecousue += NkString(" ") + morceau;
					}
					mLargeur = Colonnes(morceau);
					return;
				}
				const NkString texte = Resserre(NkLigneDeJenga(dedans));
				if (texte.Empty()) {
					return;
				}
				if (mBoite == 5 && texte.Find("Build Successful") != NkString::npos) {
					// Le projet est fini : sa part de fichiers ne compte plus.
					++projetsFaits;
					fichiersTotal = 0;
					fichiersFaits = 0;
				}
				Pousser(NiveauDe(texte), SansSymbole(texte), temps);
				return;
			}
			// ── Une ligne hors boite ──────────────────────────────────────────
			FinCommande(temps);
			const NkString propre = NkLigneDeJenga(brute);
			if (propre.Empty()) {
				return;
			}
			const NkString texte = SansSymbole(propre);
			Progression(texte);
			const NkNiveauLigne n = NiveauDe(propre);
			if (texte.Find(": error: ") != NkString::npos || texte.Find(": fatal error: ") != NkString::npos ||
				texte.Find(": warning: ") != NkString::npos) {
				Diagnostiquer(texte);
			}
			Pousser(n, texte, temps);
		}

		NkVector<NkString> NkResumeErreurs(const NkJournalConstruction &j, usize max) {
			NkVector<NkString> r;
			for (usize i = 0; i < j.diagnostics.Size() && r.Size() < max; ++i) {
				const NkDiagnostic &d = j.diagnostics[i];
				if (!d.erreur) {
					continue;
				}
				NkString ligne;
				if (!d.fichier.Empty()) {
					NkString nom = d.fichier;
					const usize barre = nom.RFind('/') != NkString::npos ? nom.RFind('/') : nom.RFind('\\');
					if (barre != NkString::npos) {
						nom = NkString(nom.SubStr(barre + 1u));
					}
					ligne = d.ligne > 0 ? NkString::Format("%s:%d: %s", nom.CStr(), static_cast<int>(d.ligne), d.message.CStr())
										: nom + ": " + d.message;
				} else {
					ligne = d.message;
				}
				bool deja = false;
				for (usize k = 0; k < r.Size() && !deja; ++k) {
					deja = r[k] == ligne;
				}
				if (!deja) {
					r.PushBack(ligne);
				}
			}
			return r;
		}

		// =====================================================================
		// LE DEROULEMENT
		// =====================================================================
		void NkDeroulementConstruction::Commencer(const NkDemandeConstruction &demande, NkPlanConstruction &p,
												  const NkVector<NkString> &preparation) {
			mDemande = demande;
			plan = &p;
			journal.Vider();
			mVerrou.Liberer();
			mEtape = 0;
			mReussi = false;
			mAttenteDite = false;
			mDebut = Maintenant();
			// Les phases deja faites par la preparation ont leur debut a elles.
			for (int32 i = 0; i < NK_NB_PHASES; ++i) {
				if (p.phases[i].debut > 0.0 && p.phases[i].debut < mDebut) {
					mDebut = p.phases[i].debut;
				}
			}
			mFin = 0.0;
			// Les chemins que le journal raccourcit (a l'affichage seulement :
			// construire.log garde le texte brut).
			journal.Abreger(p.depot, NkString());
			journal.Abreger(p.dossierJeu, NkString());
			if (!p.cache.racine.Empty()) {
				journal.Abreger(p.cache.racine, NkString("cache/"));
			}
			for (usize i = 0; i < preparation.Size(); ++i) {
				const NkString &l = preparation[i];
				NkPhaseConstruction ph = NkPhaseConstruction::NK_PREPARER;
				if (l.StartsWith("[cuisson]")) {
					ph = NkPhaseConstruction::NK_CUIRE;
				} else if (l.StartsWith("[icones]")) {
					ph = NkPhaseConstruction::NK_ICONES;
				} else if (l.StartsWith("[moteur]")) {
					ph = NkPhaseConstruction::NK_MOTEUR;
				}
				journal.Phase(ph);
				Annoncer(l, NkJournalConstruction::NiveauDe(l));
			}
			mPas = NkPas::NK_LANCER;
		}

		float32 NkDeroulementConstruction::Temps() const {
			return static_cast<float32>((mFin > 0.0 ? mFin : Maintenant()) - mDebut);
		}

		void NkDeroulementConstruction::Annoncer(const NkString &texte, NkNiveauLigne niveau) {
			journal.Annonce(texte, niveau, Temps());
			if (plan != nullptr && !plan->journal.Empty()) {
				NkFile::AppendAllText(plan->journal.CStr(), (texte + "\n").CStr());
			}
		}

		void NkDeroulementConstruction::Ouvrir(NkPhaseConstruction p) {
			NkPhaseSuivie &s = plan->phases[static_cast<int32>(p)];
			if (s.etat != NkEtatPhase::NK_EN_COURS) {
				s.etat = NkEtatPhase::NK_EN_COURS;
				s.debut = Maintenant();
			}
			journal.Phase(p);
		}

		void NkDeroulementConstruction::Fermer(NkPhaseConstruction p, NkEtatPhase etat, const NkString &detail) {
			NkPhaseSuivie &s = plan->phases[static_cast<int32>(p)];
			if (s.etat == NkEtatPhase::NK_EN_COURS) {
				s.duree = Maintenant() - s.debut;
			}
			s.etat = etat;
			if (!detail.Empty()) {
				s.detail = detail;
			}
		}

		void NkDeroulementConstruction::Echec(const NkString &pourquoi) {
			mVerrou.Liberer();
			for (int32 i = 0; i < NK_NB_PHASES; ++i) {
				if (plan->phases[i].etat == NkEtatPhase::NK_EN_COURS) {
					Fermer(static_cast<NkPhaseConstruction>(i), NkEtatPhase::NK_ECHEC, NkString());
				}
			}
			Annoncer(pourquoi + " -- journal complet : " + plan->journal, NkNiveauLigne::NK_ERREUR);
			annonce = pourquoi;
			mReussi = false;
			mPas = NkPas::NK_FINI;
			mFin = Maintenant();
		}

		void NkDeroulementConstruction::Lancer() {
			if (mEtape >= plan->etapes.Size()) {
				Achever();
				return;
			}
			const NkEtapeConstruction &e = plan->etapes[mEtape];
			if (e.phase == NkPhaseConstruction::NK_MOTEUR && !mVerrou.Tenu()) {
				Ouvrir(NkPhaseConstruction::NK_MOTEUR);
				if (!mVerrou.Prendre(plan->cache.verrou)) {
					// Une autre construction (une autre fenetre, la ligne de
					// commande) construit CE moteur : on attend son sceau.
					mPas = NkPas::NK_VERROU;
					if (!mAttenteDite) {
						mAttenteDite = true;
						annonce = NkString("un autre processus construit ce moteur : on attend...");
						Annoncer(NkString("[moteur] verrou tenu par une autre construction (") + plan->cache.verrou + ") : on attend son sceau",
								 NkNiveauLigne::NK_INFO);
						plan->phases[static_cast<int32>(NkPhaseConstruction::NK_MOTEUR)].detail = NkString("attend une autre construction");
					}
					return;
				}
				NkString pourquoi;
				if (NkCacheScelle(plan->cache, pourquoi)) {
					mVerrou.Liberer();
					plan->cacheTrouve = true;
					Annoncer(NkString("[moteur] scelle par l'autre construction pendant l'attente : reutilise (") + pourquoi + ")",
							 NkNiveauLigne::NK_SUCCES);
					Fermer(NkPhaseConstruction::NK_MOTEUR, NkEtatPhase::NK_FAITE,
						   NkString("cache trouvé (") + NkString(plan->cache.empreinte.SubStr(0, 8)) + ")");
					while (mEtape < plan->etapes.Size() && plan->etapes[mEtape].phase == NkPhaseConstruction::NK_MOTEUR) {
						++mEtape;
					}
					Lancer();
					return;
				}
				if (!NkPreparerChantier(plan->cache, plan->depot)) {
					Echec(NkString("chantier du moteur impossible a ecrire : ") + plan->cache.chantier);
					return;
				}
				mDebutMoteur = Maintenant();
				Annoncer(NkString("[moteur] verrou pris : le moteur ") + plan->cache.empreinte + " se construit dans " + plan->cache.chantier,
						 NkNiveauLigne::NK_ETAPE);
			}
			Ouvrir(e.phase);
			Annoncer(NkString("[etape] ") + e.libelle, NkNiveauLigne::NK_ETAPE);
			Annoncer(NkString("  ") + e.commande, NkNiveauLigne::NK_NOTE);
			annonce = e.libelle + "...";
			journal.NouvelleCommande();
			mErreursAvant = journal.erreurs;
			mProcessus.Environnement("JENGA_NO_IDE_CONFIG", NK_CONSTRUIRE_SANS_IDE);
			if (!mProcessus.Lancer(e.commande, e.dossier.Empty() ? plan->dossierJeu : e.dossier)) {
				Echec(NkString("lancement impossible : ") + e.commande);
				return;
			}
			mPas = NkPas::NK_PROCESSUS;
		}

		void NkDeroulementConstruction::Recolter() {
			NkVector<NkString> lignes;
			mProcessus.Recolter(lignes);
			if (lignes.Empty()) {
				return;
			}
			NkString bloc;
			const float32 t = Temps();
			for (usize i = 0; i < lignes.Size(); ++i) {
				bloc += lignes[i] + "\n";
				journal.LigneJenga(lignes[i], t);
				// Le dernier fichier du JEU compile : la phase Lier commence. On
				// n'attend pas « Linking... » : Jenga ne l'ecrit qu'APRES l'edition
				// de liens (BuildLogger.LogLink), la phase durerait zero seconde.
				// ... sauf si un fichier a echoue : la phase en echec est alors
				// celle de la compilation, pas celle des liens.
				const bool compile = journal.erreurs == mErreursAvant &&
									 (journal.lien || (journal.fichiersTotal > 0 && journal.fichiersFaits >= journal.fichiersTotal));
				if (mPas == NkPas::NK_PROCESSUS && mEtape < plan->etapes.Size() &&
					plan->etapes[mEtape].phase == NkPhaseConstruction::NK_COMPILER && compile && journal.projet == plan->projet &&
					plan->phases[static_cast<int32>(NkPhaseConstruction::NK_LIER)].etat == NkEtatPhase::NK_ATTENTE) {
					Fermer(NkPhaseConstruction::NK_COMPILER, NkEtatPhase::NK_FAITE,
						   NkString::Format("%d fichier(s)", static_cast<int>(journal.fichiersTotal)));
					Ouvrir(NkPhaseConstruction::NK_LIER);
				}
			}
			if (!plan->journal.Empty()) {
				NkFile::AppendAllText(plan->journal.CStr(), bloc.CStr());
			}
		}

		void NkDeroulementConstruction::FinCommande(int32 code) {
			journal.FinCommande(Temps());
			const NkEtapeConstruction &e = plan->etapes[mEtape];
			if (code != 0) {
				Echec(code == 124 ? NkString("construction arretee")
								  : NkString::Format("echec de l'etape « %s » (code %d)", e.libelle.CStr(), static_cast<int>(code)));
				return;
			}
			const NkPhaseConstruction phase = e.phase;
			++mEtape;
			const bool suiteMemePhase = mEtape < plan->etapes.Size() && plan->etapes[mEtape].phase == phase;
			if (phase == NkPhaseConstruction::NK_MOTEUR && !suiteMemePhase) {
				NkVector<NkString> details;
				const float64 duree = Maintenant() - mDebutMoteur;
				const bool scelle = NkScellerMoteur(plan->cache, plan->depot, plan->cache.config.CStr(), plan->cache.systeme.CStr(), NkVersionJenga(), duree, details);
				for (usize i = 0; i < details.Size(); ++i) {
					Annoncer(details[i], NkJournalConstruction::NiveauDe(details[i]));
				}
				mVerrou.Liberer();
				if (!scelle) {
					Echec(NkString("le kit du moteur est incomplet : rien n'est scelle"));
					return;
				}
				Fermer(NkPhaseConstruction::NK_MOTEUR, NkEtatPhase::NK_FAITE,
					   NkString::Format("construit en %.0f s (%s)", duree, NkString(plan->cache.empreinte.SubStr(0, 8)).CStr()));
			} else if (phase == NkPhaseConstruction::NK_COMPILER &&
					   plan->phases[static_cast<int32>(NkPhaseConstruction::NK_COMPILER)].etat == NkEtatPhase::NK_EN_COURS) {
				Fermer(NkPhaseConstruction::NK_COMPILER, NkEtatPhase::NK_FAITE,
					   journal.fichiersTotal > 0 ? NkString::Format("%d fichier(s)", static_cast<int>(journal.fichiersTotal))
												 : NkString("à jour"));
			} else if (phase == NkPhaseConstruction::NK_EMPAQUETER && !suiteMemePhase) {
				Fermer(NkPhaseConstruction::NK_EMPAQUETER, NkEtatPhase::NK_FAITE, NkString("paquet dans Livraison/"));
			}
			mPas = NkPas::NK_LANCER;
			Lancer();
		}

		void NkDeroulementConstruction::Achever() {
			NkVector<NkString> fin;
			const bool range = NkAcheverConstruction(mDemande, *plan, fin);
			journal.Phase(NkPhaseConstruction::NK_LIER);
			for (usize i = 0; i < fin.Size(); ++i) {
				Annoncer(fin[i], NkJournalConstruction::NiveauDe(fin[i]));
			}
			NkPhaseSuivie &lier = plan->phases[static_cast<int32>(NkPhaseConstruction::NK_LIER)];
			if (lier.etat == NkEtatPhase::NK_ATTENTE) {
				// Pas de « Linking... » : l'executable etait a jour.
				Fermer(NkPhaseConstruction::NK_LIER, range ? NkEtatPhase::NK_FAITE : NkEtatPhase::NK_ECHEC, NkString("à jour"));
			} else if (lier.etat == NkEtatPhase::NK_EN_COURS) {
				Fermer(NkPhaseConstruction::NK_LIER, range ? NkEtatPhase::NK_FAITE : NkEtatPhase::NK_ECHEC,
					   NkPath(plan->resultat.CStr()).GetFileName());
			}
			if (!range) {
				Echec(fin.Empty() ? NkString("rien de produit") : fin[fin.Size() - 1u]);
				return;
			}
			if (!plan->verification.Empty()) {
				// Le jeu produit relit SES donnees sans fenetre (temoin l1 sur le
				// vrai produit) : c'est la derniere phase.
				Ouvrir(NkPhaseConstruction::NK_VERIFIER);
				journal.NouvelleCommande();
				Annoncer(NkString("[verifier] ") + plan->verification, NkNiveauLigne::NK_ETAPE);
				if (mProcessus.Lancer(plan->verification, plan->dossierJeu)) {
					annonce = NkString("le jeu construit relit ses donnees...");
					mPas = NkPas::NK_VERIFICATION;
					return;
				}
				Echec(NkString("verification impossible a lancer : ") + plan->verification);
				return;
			}
			mReussi = true;
			mPas = NkPas::NK_FINI;
			mFin = Maintenant();
			annonce = NkString("Construit : ") + plan->resultat;
			Annoncer(annonce, NkNiveauLigne::NK_SUCCES);
		}

		bool NkDeroulementConstruction::Avancer() {
			switch (mPas) {
				case NkPas::NK_LANCER:
				case NkPas::NK_VERROU:
					Lancer();
					return EnCours();
				case NkPas::NK_PROCESSUS:
				case NkPas::NK_VERIFICATION: {
					// EnCours AVANT la recolte : le fil pousse toutes ses lignes
					// avant de se dire fini, donc une recolte faite apres un
					// « fini » a tout.
					const bool encore = mProcessus.EnCours();
					Recolter();
					if (encore) {
						return true;
					}
					const int32 code = mProcessus.Code();
					if (mPas == NkPas::NK_VERIFICATION) {
						journal.FinCommande(Temps());
						if (code != 0) {
							Echec(NkString::Format("le jeu construit ne relit pas la scene de l'editeur (code %d)", static_cast<int>(code)));
							return false;
						}
						Fermer(NkPhaseConstruction::NK_VERIFIER, NkEtatPhase::NK_FAITE, NkString("même empreinte que l'éditeur"));
						mReussi = true;
						mPas = NkPas::NK_FINI;
						mFin = Maintenant();
						annonce = NkString("Construit et verifie : ") + plan->resultat;
						Annoncer(annonce, NkNiveauLigne::NK_SUCCES);
						return false;
					}
					FinCommande(code);
					return EnCours();
				}
				default:
					return false;
			}
		}

		void NkDeroulementConstruction::Arreter() {
			if (mPas == NkPas::NK_VERROU) {
				Echec(NkString("construction arretee pendant l'attente du moteur"));
				return;
			}
			mProcessus.Arreter();
		}

		float32 NkDeroulementConstruction::Progression() const {
			if (plan == nullptr) {
				return 0.f;
			}
			// Le POIDS de chaque phase, a la louche des mesures du 2026-10-01 : un
			// moteur a construire, c'est les trois quarts du temps ; trouve, il
			// ne compte plus. En mode sources, le moteur est dans « Compiler ».
			bool moteurAConstruire = false;
			for (usize k = 0; k < plan->etapes.Size(); ++k) {
				moteurAConstruire |= plan->etapes[k].phase == NkPhaseConstruction::NK_MOTEUR;
			}
			const bool sources = plan->moteur == NkModeMoteur::NK_SOURCES;
			const float32 poids[NK_NB_PHASES] = {1.f, 1.f, 1.f, moteurAConstruire ? 60.f : 1.f, sources ? 70.f : 12.f, 4.f, 2.f, 6.f};
			float32 total = 0.f;
			float32 fait = 0.f;
			for (int32 i = 0; i < NK_NB_PHASES; ++i) {
				const NkPhaseSuivie &s = plan->phases[i];
				if (s.etat == NkEtatPhase::NK_SAUTEE) {
					continue;
				}
				const float32 p = poids[i];
				total += p;
				if (s.etat == NkEtatPhase::NK_FAITE) {
					fait += p;
				} else if (s.etat == NkEtatPhase::NK_EN_COURS) {
					// La part de la phase : ses commandes faites, plus celle en
					// cours au prorata de ce que Jenga en dit.
					uint32 n = 0u;
					uint32 avant = 0u;
					for (usize k = 0; k < plan->etapes.Size(); ++k) {
						if (static_cast<int32>(plan->etapes[k].phase) == i) {
							++n;
							avant += k < mEtape ? 1u : 0u;
						}
					}
					const float32 f = journal.Fraction();
					const float32 dedans = f < 0.f ? 0.f : f;
					fait += p * (n > 0u ? (static_cast<float32>(avant) + dedans) / static_cast<float32>(n) : dedans);
				}
			}
			return total > 0.f ? fait / total : 0.f;
		}

	} // namespace editeur
} // namespace nkentseu
