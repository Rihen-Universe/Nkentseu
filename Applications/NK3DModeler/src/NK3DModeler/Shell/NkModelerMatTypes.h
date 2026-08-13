#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerMatTypes.h
// @Brief   Le CATALOGUE des types de materiau (une seule source de verite), et
//          la specialisation du selecteur de fichiers qui fait choisir ce type
//          AU MOMENT DE LA CREATION.
//
// POURQUOI UNE TABLE A PART
//   Ce catalogue vivait en `static` au milieu du panneau de proprietes, la ou se
//   regle le type d'un materiau EXISTANT. La creation en a besoin aussi -- « je
//   veux que pour la creation d'un nouveau materiau on choisisse son type avant »
//   (Rihen, 13 aout). Recopier la table aurait suffi pour aujourd'hui et aurait
//   diverge au premier type ajoute : les deux listes auraient fini par ne plus
//   proposer les memes choses selon l'endroit d'ou on les ouvre.
//
// POURQUOI ON DERIVE DU SELECTEUR AU LIEU D'ECRIRE UN DIALOGUE
//   NkFilePickerState porte des POINTS DE SPECIALISATION (`PickerExtraHeight`,
//   `DrawPickerExtra`, titre, libelle et etat du bouton de validation) : c'est
//   exactement par la que NKCode greffe son assistant de creation de fichier. Un
//   dialogue parallele redemanderait l'emplacement, le nom, la confirmation, la
//   modalite -- tout ce que le selecteur fait deja, et qu'il faudrait corriger
//   deux fois. « Porte au lieu de reecrire » (Rihen, 12 aout).
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKEditorKit/NkFilePicker.h"

namespace nkentseu {
	namespace nk3d {

		// ── LE CATALOGUE ────────────────────────────────────────────────────────
		// Les types pas encore EPROUVES dans le modeleur restent VISIBLES mais
		// inertes, marques « (bientot) » : Rihen les veut grises et non caches --
		// un choix absent se lit comme un oubli, un choix grise se lit comme une
		// promesse. Verre / Tissu / Carrosserie / Feuillage ont ete eprouves les
		// 11-12 aout (gabarits + paires NkSL modernes). Restent inertes : Peau et
		// Cheveux (chacun demande son propre modele) et Eau (son .vk.glsl vise
		// encore l'ancienne disposition).
		inline constexpr int32 kNkMatTypeCount = 13;

		inline const char *const kNkMatTypeNames[kNkMatTypeCount] = {
			"Standard (PBR)", "Peau (bientot)", "Cheveux (bientot)",
			"Verre",		  "Tissu",			"Carrosserie",
			"Feuillage",	  "Eau (bientot)",	"Emissif",
			"Toon",			  "Toon encre",		"Anime",
			"Sans eclairage"};

		/// Type utilisable tel quel ? (faux = visible mais inerte)
		inline const bool kNkMatTypeOk[kNkMatTypeCount] = {
			true, false, false, true, true, true, true, false, true, true, true, true, true};

		/// Masque GRISE des listes deroulantes : l'exact inverse de `Ok`. Deux
		/// tables plutot qu'une negation a l'appel parce que le combo attend un
		/// tableau, pas un predicat.
		inline const bool kNkMatTypeOff[kNkMatTypeCount] = {
			false, true, true, false, false, false, false, true, false, false, false, false, false};

		/// Valeur MOTEUR (NkMaterialType) derriere chaque entree. L'indice de la
		/// liste et la valeur du moteur ne coincident pas : le moteur numerote par
		/// famille (0 PBR, 3 peau, 4 cheveux, 5 verre... 60 sans eclairage) et la
		/// liste, elle, se lit dans l'ordre ou on veut la proposer.
		inline const int32 kNkMatTypeVal[kNkMatTypeCount] = {0, 3, 4, 5, 6, 7, 8, 9, 11, 20, 21, 22, 60};

		/// Indice de liste correspondant a une valeur moteur (0 par defaut).
		inline int32 NkMatTypeIndexOf(int32 valeurMoteur) {
			for (int32 k = 0; k < kNkMatTypeCount; ++k)
				if (kNkMatTypeVal[k] == valeurMoteur)
					return k;
			return 0;
		}

		// ── LE SELECTEUR, SPECIALISE POUR LA CREATION DE MATERIAU ───────────────
		// En dehors du mode « nouveau materiau », cette classe se comporte
		// EXACTEMENT comme le selecteur generique : toutes les surcharges se
		// replient sur la base des que `matNewMode` est faux. C'est ce qui permet
		// de la substituer partout sans relire chaque appel.
		class NkModelerPicker : public editorkit::NkFilePickerState {
			public:
				/// Le selecteur est-il ouvert pour CREER un materiau ? Ce drapeau
				/// vit ici plutot que dans l'etat de l'application parce que ce
				/// sont les surcharges -- appelees par le kit, qui ne voit que le
				/// picker -- qui ont besoin de le lire.
				bool matNewMode = false;
				/// INDICE dans le catalogue ci-dessus (pas la valeur moteur).
				int32 matNewType = 0;
				/// Focus du champ de nom de l'assistant (0 = aucun, 1 = le nom).
				int32 matExtraFocus = 0;

				/// Arme le mode creation : type par defaut = Standard (PBR).
				void MatNewBegin() {
					matNewMode = true;
					matNewType = 0;
					matExtraFocus = 0;
				}
				/// LE MODE MEURT AVEC LA FENETRE. `PickerCancel` est la porte de
				/// sortie unique du selecteur (confirmation, Annuler, Echap) : c'est
				/// donc le seul endroit ou desarmer, plutot que de le rattraper a
				/// chaque endroit qui referme. `matNewType` n'est PAS efface ici --
				/// l'application le lit APRES la confirmation, qui passe par cette
				/// meme porte ; c'est `MatNewBegin` qui le remet a zero.
				void PickerCancel() override {
					matNewMode = false;
					matExtraFocus = 0;
					editorkit::NkFilePickerState::PickerCancel();
				}
				/// Valeur MOTEUR du type choisi, prete pour Demo3DHostProjMatSetType.
				int32 MatNewTypeValue() const {
					const int32 k = (matNewType < 0 || matNewType >= kNkMatTypeCount) ? 0 : matNewType;
					return kNkMatTypeOk[k] ? kNkMatTypeVal[k] : kNkMatTypeVal[0];
				}

				// ── Points de specialisation du kit ─────────────────────────────
				float32 PickerWindowHeight(float32 S) const override {
					return matNewMode ? 660.f * S : NkFilePickerState::PickerWindowHeight(S);
				}
				/// Reserve du BAS = la place que l'arbre laisse au contenu de l'app.
				/// Elle doit couvrir la region supplementaire ET les boutons, sinon
				/// l'assistant deborderait sur l'arborescence.
				float32 PickerBottomReserve(float32 S) const override {
					return matNewMode ? 258.f * S : NkFilePickerState::PickerBottomReserve(S);
				}
				float32 PickerExtraHeight(float32 S) const override {
					return matNewMode ? 214.f * S : 0.f;
				}
				void PickerClearExtraFocus() override { matExtraFocus = 0; }
				const char *PickerTitle() const override {
					return matNewMode ? "Nouveau materiau - emplacement, nom et type"
									  : NkFilePickerState::PickerTitle();
				}
				const char *PickerConfirmLabel() const override {
					return matNewMode ? "Creer" : NkFilePickerState::PickerConfirmLabel();
				}
				bool PickerConfirmEnabled() const override {
					// En creation, seul le NOM est obligatoire : le type a toujours
					// une valeur (Standard au depart) et l'emplacement est le
					// dossier courant de l'arbre.
					return matNewMode ? (pickerSaveName[0] != '\0')
									  : NkFilePickerState::PickerConfirmEnabled();
				}

				// ── L'ASSISTANT ─────────────────────────────────────────────────
				// ATTENTION : des que `PickerExtraHeight` renvoie autre chose que 0,
				// le kit CESSE de dessiner son champ « nom du fichier » (c'est ainsi
				// qu'il laisse la main a l'app, cf. `saveMode && !hasExtra`). Le
				// champ de nom est donc REDESSINE ici -- sur `pickerSaveName`,
				// c'est-a-dire sur celui-la meme que la confirmation recopiera dans
				// `pickerResultName`. Aucun tampon parallele : ce que l'utilisateur
				// tape est ce que l'application recoit.
				void DrawPickerExtra(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl,
									 const nkgui::NkGuiFont *f, const nkgui::NkRect &region,
									 const editorkit::NkFilePickerStyle &sty, bool click,
									 bool &fieldClicked) override {
					if (!f)
						return;
					const float32 S = ctx.S(1.f), lh = f->LineHeight(), asc = f->Ascent();
					const nkgui::NkVec2 mp = ctx.input.mousePos;
					auto survole = [&](const nkgui::NkRect &r) { return nkgui::NkGuiRectContains(r, mp); };
					auto texte = [&](float32 x, float32 y, const char *s, const nkgui::NkColor &c) {
						dl.AddText(f->Face(), f->TexId(), {x, y + asc}, s, c);
					};
					const float32 cx = region.x, cwid = region.w, ny = region.y;

					// ── NOM ─────────────────────────────────────────────────────
					texte(cx, ny, "Nom du materiau", sty.sub);
					const nkgui::NkRect champ = {cx, ny + 20.f * S, cwid, 30.f * S};
					if (survole(champ) && click) {
						pickerSaveFocus = true;
						matExtraFocus = 1;
						pickerEditing = false;
						pickerNewFocus = false;
						fieldClicked = true;
					}
					editorkit::NkOverlayTextField(ctx, dl, f, champ, pickerSaveName,
												  (int32)sizeof(pickerSaveName), pickerSaveFocus);
					if (pickerSaveName[0] == '\0' && !pickerSaveFocus)
						texte(champ.x + 10.f * S, champ.y + (30.f * S - lh) * 0.5f, "ex: Bois", sty.sub);
					// L'extension ne se tape pas : elle est imposee (.nkmat). Le dire
					// evite qu'on la saisisse a la main et qu'on obtienne « Bois.nkmat.nkmat ».
					const char *rappel = "L'extension .nkmat est ajoutee automatiquement.";
					texte(cx, champ.y + 34.f * S, rappel, sty.sub);

					// ── TYPE ────────────────────────────────────────────────────
					// Le meme catalogue que le panneau de proprietes, en PUCES
					// plutot qu'en liste deroulante : a la creation, tout voir d'un
					// coup vaut mieux qu'un deroulant de plus dans une fenetre qui
					// est deja modale.
					const float32 ty = champ.y + 58.f * S;
					texte(cx, ty, "Type", sty.sub);
					float32 bx = cx, by = ty + 18.f * S;
					for (int32 k = 0; k < kNkMatTypeCount; ++k) {
						const float32 bw = f->MeasureWidth(kNkMatTypeNames[k]) + 16.f * S;
						if (bx + bw > cx + cwid) {
							bx = cx;
							by += 28.f * S;
						}
						const nkgui::NkRect r = {bx, by, bw, 24.f * S};
						const bool actif = kNkMatTypeOk[k];
						const bool sel = (matNewType == k);
						const bool hov = actif && survole(r);
						dl.AddRectFilled(r,
										 sel ? sty.accent : (hov ? sty.rowHover : nkgui::NkColor{24, 28, 34, 255}),
										 5.f * S);
						texte(r.x + 8.f * S, r.y + (24.f * S - lh) * 0.5f, kNkMatTypeNames[k],
							  sel ? sty.textStrong : (actif ? sty.text : sty.sub));
						// Un type inerte ne se choisit pas -- mais le clic est
						// CONSOMME (`fieldClicked`), sinon il serait compris comme
						// un clic « ailleurs » et viderait le focus du champ de nom.
						if (hov && click) {
							matNewType = k;
							fieldClicked = true;
						} else if (actif == false && survole(r) && click) {
							fieldClicked = true;
						}
						bx += bw + 6.f * S;
					}
				}
		};

	} // namespace nk3d
} // namespace nkentseu
