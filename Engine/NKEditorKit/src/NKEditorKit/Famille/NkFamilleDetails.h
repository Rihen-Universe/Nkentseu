#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamilleDetails.h
// @Brief   LE PANNEAU DETAILS d'Unreal 5 de la famille : en-tete (icone de
//          l'acteur, case « active », NOM, « + Ajouter » au plus vert), arbre
//          des composants (« Cube (Instance) » puis ses composants), recherche
//          dans les proprietes, pastilles de categorie, puis les CARTES de
//          composants (repli, icone, case, titre en gras, menu « ⋮ ») et leurs
//          rangees « libellé | valeur » au rythme de 24 px, colonne des noms
//          alignee et TIRABLE, LISERES rouge X / vert Y / bleu Z collés au bord
//          des champs, verrou de l'echelle, fleche de remise.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// D'OU CA VIENT : UnkenyEditor, NkEditeurDetails.cpp (Entete, Fin, Rangee,
// Frotter, Nombre, Paire, Unique, Remettre, Choix, Info, Boutons, OngletDetails),
// que Rihen a jugé « très beau » le 01/10. Ce n'est PAS NkEditorInspector :
// celui-la est pilote par NKReflection ; celui-ci dessine ce que l'application
// lui donne, des nombres et des cases, quel que soit son modele.
//
// UTILISATION (une trame) :
//   NkFamilleInspecteur I(c, etat, zone);
//   if (!selection) { I.Vide("Sélectionnez...", "Cliquez-la..."); return; }
//   bool ajouter = I.Entete(nom, sizeof(nom), &actif, "type : maillage", ...);
//   if (I.Debut("e42", "Cube", composants, n)) {
//       if (I.Carte(0, "Transform", NkFamilleIconeCarte::Transform, 2)) {
//           I.Vecteur("Position", pos, 0.01f, 3, ...);
//           I.FinCarte();
//       }
//       ...
//       I.Fin("Ajouter un composant");
//   }
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleStyle.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editorkit {

		/// Les icones des cartes, tracees (la police n'a pas ces glyphes).
		enum class NkFamilleIconeCarte : uint8 {
			Aucune = 0,
			Transform,
			Sprite,
			Collisionneur,
			Corps,
			CorpsMou,
			Son,
			Animation,
			Animateur,
			Lumiere,
			Emetteur,
			Hierarchie,
			Camera,
			Maillage,
			Materiau,
			Count
		};
		void NkFamillePeindreIconeCarte(nkgui::NkGuiDrawList &dl, NkFamilleIconeCarte icone, const nkgui::NkRect &r,
										const nkgui::NkColor &col) noexcept;
		/// L'ACTEUR (l'en-tete et la racine de l'arbre) : le cube d'Unreal.
		void NkFamillePeindreIconeActeur(nkgui::NkGuiDrawList &dl, const nkgui::NkRect &r, const nkgui::NkColor &col,
										 const nkgui::NkColor &accent) noexcept;

		/// Les pastilles de categorie d'Unreal. 0 = Tout (en dernier a l'ecran).
		enum class NkFamilleCategorie : uint8 { Tout = 0, General, Acteur, Physique, Rendu, Animation, Audio, Count };
		const char *NkFamilleNomCategorie(NkFamilleCategorie c) noexcept;

		/// L'etat du panneau, a garder d'une trame a l'autre.
		struct NkFamilleDetailsEtat {
				uint32 cartesRepliees = 0;	 ///< bit = l'identifiant de carte de l'application (< 32)
				float32 colonne = 0.40f;	 ///< la colonne des noms, en fraction de la rangee
				bool cloisonTenue = false;
				float32 cloisonX = 0.f;		 ///< ou elle est, a la derniere rangee dessinee
				float32 rangeeX = 0.f, rangeeW = 0.f;
				char recherche[64] = {};
				bool rechercheFocus = false;
				NkString rechercheVue;		 ///< la recherche de la trame d'avant
				uint32 cartesTrouvees = 0;	 ///< les cartes qui ont montre une rangee (recherche)
				int32 categorie = 0;		 ///< NkFamilleCategorie
				int32 composant = -1;		 ///< -1 = l'acteur entier ; sinon l'identifiant de carte
				int32 arbreDefil = 0;
				/// Le libelle qu'on FROTTE (Unity : le nom d'un nombre se tire).
				uint32 frotteId = 0;
				float32 frotteX = 0.f;
				/// Le champ du nom (il suit l'entite choisie hors saisie).
				char nom[64] = {};
				bool nomFocus = false;
				uint64 nomDe = ~0ull;
				/// Ce que le panneau a releve a la derniere trame (bancs, captures).
				nkgui::NkRect ajouter{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect caseActif{0.f, 0.f, 0.f, 0.f};
				/// (2026-10-02) L'EN-TETE FIXE (nom, arbre, recherche, pastilles : il ne
				/// defile pas), ses pieces, et la zone des CARTES, qui seule defile.
				nkgui::NkRect entete{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect cartes{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect arbre{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect rechercheRect{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect pastilles[7] = {};
				/// Le haut de la PREMIERE carte dessinee a la trame (il descend et monte
				/// avec le defilement des cartes ; l'en-tete, lui, ne bouge pas).
				float32 premiereCarteY = 0.f;
				int32 liseres = 0;			 ///< combien de liseres d'axe ont ete peints
				/// Rien n'est tape dans le panneau (les raccourcis de l'editeur passent).
				bool Libre() const noexcept {
					return !rechercheFocus && !nomFocus && frotteId == 0;
				}
		};

		/// Un composant de l'arbre (« Transform », « Maillage »...).
		struct NkFamilleComposant {
				int32 id = 0; ///< l'identifiant de carte (< 32)
				const char *nom = "";
				NkFamilleIconeCarte icone = NkFamilleIconeCarte::Aucune;
		};

		/// Le dessin d'UNE trame du panneau Details.
		class NkFamilleInspecteur {
			public:
				NkFamilleInspecteur(NkFamilleCtx &c, NkFamilleDetailsEtat &etat, const nkgui::NkRect &zone) noexcept;

				/// L'etat vide qui PARLE : il dit quoi faire.
				void Vide(const char *ligne1, const char *ligne2);

				/// L'en-tete d'Unreal. `cleEntite` : l'identite de l'entite (le champ
				/// du nom suit l'entite choisie) ; `nom` : son nom actuel. `actif`
				/// (facultatif) : la case « active ». Rend vrai si « + Ajouter » est
				/// clique ; `nouveauNom` recoit le nom tape s'il a change.
				bool Entete(uint64 cleEntite, const char *nom, bool *actif, const char *ligneType, bool ajouterOuvert,
							NkString *nouveauNom);

				/// Pose l'en-tete FIXE -- l'arbre des composants (l'acteur `nomActeur`
				/// puis `composants`), la recherche et les pastilles -- puis ouvre la
				/// zone DEFILABLE des cartes, sous lui (2026-10-02 : seules les cartes
				/// defilent). Faux = rien a dessiner (zone trop petite). Toujours suivi
				/// de `Fin` si vrai.
				bool Debut(const char *cle, const char *nomActeur, const NkFamilleComposant *composants, int32 n);

				/// Une carte : rend vrai si son CORPS est a peindre (depliee et
				/// montree par les filtres) -- alors `FinCarte` suit. `menu` recoit le
				/// clic sur « ⋮ ».
				bool Carte(int32 id, const char *titre, NkFamilleIconeCarte icone, NkFamilleCategorie categorie,
						   bool *actif = nullptr, bool *menu = nullptr);
				void FinCarte();

				// ── Les rangees ────────────────────────────────────────────────
				bool Nombre(const char *libelle, float32 &v, float32 pas, float32 vmin = -1.0e9f, float32 vmax = 1.0e9f);
				bool Glissiere(const char *libelle, float32 &v, float32 vmin, float32 vmax);
				bool Entier(const char *libelle, int32 &v, int32 vmin = -1000000, int32 vmax = 1000000);
				bool Case(const char *libelle, bool &v);
				/// Une couleur RGBA, quatre canaux 0..1.
				bool Couleur(const char *libelle, float32 *rgba);
				/// Un VECTEUR (2 ou 3 composantes) avec les LISERES d'axe X/Y/Z ;
				/// `remise` (facultatif) recoit le clic sur la fleche de remise, qui ne
				/// parait que si `modifie` ; `verrou` (facultatif) : le cadenas des
				/// proportions dans la colonne du nom.
				bool Vecteur(const char *libelle, float32 *v, int32 composantes, float32 pas, bool *remise = nullptr,
							 bool modifie = false, bool *verrou = nullptr);
				/// Des boutons SEGMENTES (Mobilité d'Unreal) : rend vrai si la valeur change.
				bool Choix(const char *libelle, const char *const *noms, int32 n, int32 &valeur);
				/// Une valeur en LECTURE seule.
				void Info(const char *libelle, const char *valeur);
				/// Une ligne de texte sur toute la largeur de la carte.
				void Ligne(const char *texte, bool vif = false);
				/// Un ou deux boutons sur la largeur de la carte ; rend celui clique.
				int32 Boutons(const char *a, const char *b = nullptr, bool actifA = true, bool actifB = true);

				/// Ferme la zone defilable ; `ajouter` non nul : le bouton « Ajouter un
				/// composant » en bas (Unity) -- rend vrai s'il est clique.
				bool Fin(const char *ajouter, bool ajouterOuvert, nkgui::NkRect *ajouterRect = nullptr);

			private:
				bool Repond(const char *libelle) const;
				bool Rangee(const char *libelle, float32 h, nkgui::NkRect &champ, nkgui::NkRect &lib);
				bool Frotter(const nkgui::NkRect &lib, float32 &v, float32 pas, float32 vmin, float32 vmax);
				bool Survol(const nkgui::NkRect &r) const;
				bool Clic(const nkgui::NkRect &r) const;
				bool Remettre(const nkgui::NkRect &champ, bool modifie);

				NkFamilleCtx &mC;
				NkFamilleDetailsEtat &mE;
				nkgui::NkRect mZone;
				nkgui::NkRect mCorps{0.f, 0.f, 0.f, 0.f};
				float32 mHaut = 0.f;		 ///< sous l'en-tete
				float32 mEspacement = 0.f;	 ///< l'espacement NKGui d'avant (rendu par Fin)
				bool mOuvert = false;		 ///< entre Debut et Fin
				bool mPremiere = false;		 ///< la premiere carte de la trame est relevee
				bool mCherche = false;
				bool mNouvelle = false;		 ///< la recherche vient de changer : toutes les cartes se redessinent
				int32 mCarte = -1;
				bool mCarteRepond = false;
				nkgui::NkColor mFondEntete, mFondEnteteSurvol, mFondCorps;
		};

	} // namespace editorkit
} // namespace nkentseu
