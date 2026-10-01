#pragma once
// -----------------------------------------------------------------------------
// @File    NkTerminalEmulateur.h
// @Brief   L'EMULATEUR VT (sous-ensemble xterm) : le flux d'octets d'un shell
//          (NkPty) devient une GRILLE de cellules -- caractere, couleurs,
//          attributs -- avec curseur, defilement et historique.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  D'OU IL VIENT, ET CE QUI A CHANGE EN DESCENDANT (2026-10-01)
// =============================================================================
//  C'est le `NkTerm` de NKCode (`NKCode/Project/NkTerm.h`), descendu dans le
//  kit. NKCode garde un alias. Ce qui change, et pourquoi :
//
//  1. LES CELLULES PORTENT UN INDICE DE COULEUR, PLUS UNE COULEUR. L'ancien
//     emulateur figeait le RGB a la lecture (palette GitHub sombre ecrite en
//     dur) : en theme CLAIR, le jaune et le blanc ANSI devenaient illisibles,
//     et rien ne pouvait plus les rattraper. Ici la couleur se RESOUT AU
//     DESSIN, contre la palette du theme (NkTerminalVue.h). Changer de theme
//     recolore tout l'historique.
//  2. ATTRIBUTS : gras, pale, italique, souligne, inverse, barre.
//  3. LE SHELL PEUT POSER DES QUESTIONS, ET IL ATTEND LA REPONSE : position du
//     curseur (DSR 6n), etat (5n), identite du terminal (DA). fish attend deux
//     secondes s'il ne l'obtient pas. Les reponses s'accumulent ici ; l'hote
//     les renvoie au shell (`PrendreReponses`).
//  4. LES MODES QUI CHANGENT CE QUE LE CLAVIER ENVOIE : touches curseur en mode
//     application (?1, vim/less), collage encadre (?2004, bash/zsh/PSReadLine).
//  5. LE TITRE ET LE DOSSIER COURANT que le shell annonce (OSC 0/2, OSC 7,
//     OSC 9;9 de Windows Terminal, OSC 633 de VS Code) : l'en-tete du panneau
//     les affiche.
//  6. L'HISTORIQUE EST UN ANNEAU. L'ancien effacait la ligne la plus vieille
//     par `Erase(Begin())` : 5000 lignes recopiees a CHAQUE nouvelle ligne une
//     fois l'historique plein -- un `jenga build` bavard figeait l'editeur.
//  7. Le jeu de caracteres « dessin de lignes » DEC (ESC ( 0) des cadres de
//     tmux, mc, htop.
//
//  ⚠️ CE QU'IL NE FAIT PAS, ecrit pour que personne ne le cherche : pas de
//     caracteres DOUBLE LARGEUR (CJK, emoji : une cellule chacun), pas de
//     souris (?1000), pas de reflow des lignes au redimensionnement (ConPTY
//     redessine de toute facon l'ecran apres un redimensionnement).
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		// ── LES COULEURS D'UNE CELLULE ─────────────────────────────────────────
		/// Encodage sur 32 bits : 0 = couleur PAR DEFAUT (celle du theme),
		/// 0x01000000 | i = indice de palette (0..255), 0x02RRGGBB = RGB direct.
		inline constexpr uint32 kNkTermDefaut = 0u;
		inline constexpr uint32 NkTermIndice(uint32 i) {
			return 0x01000000u | (i & 0xFFu);
		}
		inline constexpr uint32 NkTermRgb(uint32 r, uint32 g, uint32 b) {
			return 0x02000000u | ((r & 0xFFu) << 16) | ((g & 0xFFu) << 8) | (b & 0xFFu);
		}
		inline constexpr bool NkTermEstIndice(uint32 c) {
			return (c >> 24) == 1u;
		}
		inline constexpr bool NkTermEstRgb(uint32 c) {
			return (c >> 24) == 2u;
		}

		/// Attributs SGR d'une cellule (combinables).
		enum NkTermAttr : uint8 {
			NK_TERM_GRAS = 1,
			NK_TERM_PALE = 2,
			NK_TERM_ITALIQUE = 4,
			NK_TERM_SOULIGNE = 8,
			NK_TERM_INVERSE = 16,
			NK_TERM_BARRE = 32,
		};

		struct NkTermCell {
				uint32 cp = 0x20; ///< point de code Unicode (espace par defaut)
				uint32 fg = kNkTermDefaut;
				uint32 bg = kNkTermDefaut;
				uint8 attr = 0;
		};

		/// Forme du curseur demandee par le shell (DECSCUSR, « CSI Ps SP q »).
		enum class NkTermCurseur : uint8 { Bloc = 0, Souligne, Barre };

		class NkTerm {
			public:
				using Line = NkVector<NkTermCell>;

				NkTerm();

				int16 Cols() const {
					return mCols;
				}
				int16 Rows() const {
					return mRows;
				}

				/// (Re)dimensionne la grille. Garde l'historique ; si l'ecran
				/// RETRECIT sous le curseur, les lignes du haut partent dans
				/// l'historique au lieu d'emporter l'invite hors de vue.
				void Resize(int16 cols, int16 rows);

				/// Lignes affichables (historique + ecran) et acces indexe.
				usize TotalLines() const {
					return mNbScroll + mScreen.Size();
				}
				const Line &LineAt(usize i) const;

				/// Le texte d'une ligne en UTF-8, espaces de fin retires.
				NkString TexteLigne(usize i) const;

				/// Position ABSOLUE du curseur (pour le dessiner).
				usize CursorLine() const {
					return mNbScroll + static_cast<usize>(mCy);
				}
				int16 CursorCol() const {
					return mCx >= mCols ? static_cast<int16>(mCols - 1) : mCx;
				}
				bool CursorVisible() const {
					return mCursorVis;
				}
				NkTermCurseur CursorStyle() const {
					return mCurseurForme;
				}
				/// Le shell veut-il un curseur clignotant ? (vrai par defaut).
				bool CursorBlink() const {
					return mCurseurClignote;
				}

				/// Octets bruts du shell.
				void Feed(const char *data, usize len);

				/// Reinitialisation complete (historique + ecran + modes).
				void Clear();

				/// Les reponses aux questions du shell (DSR, DA), a lui RENVOYER par
				/// NkPty::Write. Vide le tampon.
				void PrendreReponses(NkVector<char> &out);

				bool AppCursorKeys() const {
					return mAppCursor;
				}
				bool BracketedPaste() const {
					return mBracketedPaste;
				}
				bool AltScreen() const {
					return mAlt;
				}

				/// Ce que le shell a annonce (OSC). Vide tant qu'il n'a rien dit.
				const NkString &Titre() const {
					return mTitre;
				}
				const NkString &DossierCourant() const {
					return mDossier;
				}

				/// Augmente a chaque Feed qui a produit quelque chose : la vue s'en
				/// sert pour ne recalculer (recherche, liens) que si ca a bouge.
				uint32 Version() const {
					return mVersion;
				}

				/// Capacite de l'historique, en lignes.
				static constexpr usize kHistorique = 5000;

			private:
				enum State : uint8 { Ground, Esc, Csi, Osc, OscEsc, Dcs, DcsEsc, EscCharset, EscArg };

				void BlankLine(Line &ln) const;
				void EnsureWidth(Line &ln) const;
				NkTermCell Blank() const;
				void PousserHistorique(Line &ln);
				void ScrollUp();
				void ScrollDown();
				void LineFeed();
				void PutGlyph(uint32 cp);
				void Byte(unsigned char b);
				void Ground_(unsigned char b);
				void Esc_(unsigned char b);
				void Csi_(unsigned char b);
				void Osc_(unsigned char b);
				int32 Params(int32 *out, int32 maxN, char *prefixe, char *intermediaire);
				void Dispatch(char fin);
				void Mode(char prefixe, const int32 *p, int32 n, bool set);
				void EraseDisplay(int32 mode);
				void EraseLine(int32 mode);
				void Sgr(const int32 *p, int32 n);
				void Repondre(const char *s);
				void FinOsc();
				void Sauver();
				void Restaurer();

				NkVector<Line> mScreen;	 // lignes visibles (mRows)
				NkVector<Line> mScroll;	 // historique en ANNEAU (voir l'en-tete)
				usize mTeteScroll = 0;	 // indice de la ligne la plus ancienne
				usize mNbScroll = 0;	 // lignes d'historique valides
				NkVector<Line> mAltSave; // ecran principal sauve (ecran alternatif)
				int16 mCols = 80, mRows = 24;
				int16 mCx = 0, mCy = 0;
				int16 mTop = 0, mBot = 23; // region de defilement
				int16 mSaveCx = 0, mSaveCy = 0;
				int16 mAltCx = 0, mAltCy = 0;
				uint32 mFg = kNkTermDefaut, mBg = kNkTermDefaut;
				uint8 mAttr = 0;
				uint32 mSaveFg = kNkTermDefaut, mSaveBg = kNkTermDefaut;
				uint8 mSaveAttr = 0;
				bool mCursorVis = true;
				bool mCurseurClignote = true;
				NkTermCurseur mCurseurForme = NkTermCurseur::Bloc;
				bool mAlt = false;
				bool mAppCursor = false;
				bool mBracketedPaste = false;
				bool mAutoWrap = true;
				bool mDessinLignes = false; // G0 = jeu DEC « dessin de lignes »
				uint32 mDernier = 0x20;		// dernier glyphe (REP, « CSI b »)

				State mState = Ground;
				char mCsi[64] = {};
				int32 mCsiLen = 0;
				char mOsc[1024] = {};
				int32 mOscLen = 0;
				uint32 mU8 = 0;
				int32 mU8need = 0;

				NkVector<char> mReponses;
				NkString mTitre;
				NkString mDossier;
				uint32 mVersion = 0;
		};

	} // namespace editorkit
} // namespace nkentseu
