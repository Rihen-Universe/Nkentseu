// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurExemplePortes.cpp
// DESCRIPTION: Le projet d'exemple « Portes » : une porte ouverte par un
//              Blueprint, une autre par un script C++.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkEditeurExemplePortes.h"

#include "Script/NkBpCatalogue.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Effets/NkUnkenyEffets.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Script/NkUnkenyScript.h"

namespace nkentseu {
	namespace editeur {

		using namespace nkentseu::unkeny;

		const char *NkEditeurSourcePorteCpp() noexcept {
			return "// =============================================================================\n"
				   "// PorteCpp.cpp -- la porte ROUGE, ouverte par un script C++ (exemple Portes)\n"
				   "//\n"
				   "// Pose sur la « Zone rouge » (Details > Scripts). Quand le Joueur entre dans\n"
				   "// la zone, la porte rouge monte de « hauteur » metres (variable exposee, a\n"
				   "// regler dans les Details) et lance ses etincelles.\n"
				   "//\n"
				   "// ESSAYEZ : pendant Jouer, changez 2.f en 4.f dans NK_UNKENY_REEL ci-dessous\n"
				   "// ou le texte d'Afficher, puis ENREGISTREZ : l'editeur recompile et recharge\n"
				   "// a chaud ; l'etat (« ouverte ») est garde par NK_UNKENY_GARDER. Une faute de\n"
				   "// frappe : l'erreur s'affiche dans le Journal (fichier:ligne), l'ancienne\n"
				   "// version continue de tourner.\n"
				   "// =============================================================================\n"
				   "#include \"Unkeny/Script/NkUnkenyScriptABI.h\"\n"
				   "\n"
				   "class PorteCpp : public nkunk::Script {\n"
				   "\tpublic:\n"
				   "\t\tvoid Debut() override {\n"
				   "\t\t\tAfficher(\"Zone rouge prete (C++)\");\n"
				   "\t\t}\n"
				   "\n"
				   "\t\tvoid ZoneEntree(NkUnkEntite autre, bool soiEstLaZone) override {\n"
				   "\t\t\tif (!soiEstLaZone || !NomEst(autre, \"Joueur\") || etat.ouverte) {\n"
				   "\t\t\t\treturn;\n"
				   "\t\t\t}\n"
				   "\t\t\tNkUnkEntite porte = ParNom(\"Porte rouge\");\n"
				   "\t\t\tTeleporter(porte, Position(porte) + nkunk::Vec2(0.f, Variable(\"hauteur\", 2.f)));\n"
				   "\t\t\tJouerEffet(porte);\n"
				   "\t\t\tetat.ouverte = true;\n"
				   "\t\t\tAfficher(\"La porte rouge s'ouvre (C++)\");\n"
				   "\t\t}\n"
				   "\n"
				   "\t\t// L'etat PRIVE garde a travers le rechargement a chaud.\n"
				   "\t\tstruct Etat {\n"
				   "\t\t\t\tbool ouverte = false;\n"
				   "\t\t} etat;\n"
				   "\t\tNK_UNKENY_GARDER(etat)\n"
				   "};\n"
				   "\n"
				   "NK_UNKENY_CLASSE_VARIABLES(PorteCpp, NK_UNKENY_REEL(\"hauteur\", 2.f))\n";
		}

		namespace {
			ecs::NkEntityId Boite(NkScene &s, const char *nom, const NkVec2f &p, const NkVec2f &demi, uint32 couleur,
								  NkTypeCorps type, bool declencheur = false) {
				const ecs::NkEntityId id = s.Creer(nom, p);
				NkSprite2D sp;
				sp.taille = NkVec2f(demi.x * 2.f, demi.y * 2.f);
				sp.couleur = couleur;
				sp.couche = declencheur ? -1 : 0;
				s.Monde().Add<NkSprite2D>(id, sp);
				NkCollisionneur2D c;
				c.forme = NkForme2D::NK_BOITE;
				c.demiTaille = demi;
				c.declencheur = declencheur;
				s.Monde().Add<NkCollisionneur2D>(id, c);
				NkCorps2D k;
				k.type = type;
				s.AjouterCorps(id, k);
				return id;
			}
			void Porte(NkScene &s, const char *nom, float32 x, uint32 couleur) {
				const ecs::NkEntityId p = Boite(s, nom, NkVec2f(x, 1.5f), NkVec2f(0.3f, 1.5f), couleur, NkTypeCorps::NK_STATIQUE);
				// Ses etincelles, ETEINTES : le script les joue a l'ouverture (R34).
				NkEmetteur2D e = NkPresetEmetteur2D(NkPresetEffet2D::NK_ETINCELLES);
				e.actif = false;
				e.boucle = false;
				e.duree = 0.4f;
				e.rafale = 40u;
				s.Monde().Add<NkEmetteur2D>(p, e);
			}
			void Zone(NkScene &s, const char *nom, float32 x, uint32 couleur, const char *script) {
				const ecs::NkEntityId z = Boite(s, nom, NkVec2f(x, 1.f), NkVec2f(1.f, 1.f), couleur, NkTypeCorps::NK_STATIQUE, true);
				NkScript2D sc;
				NkScriptAjouter(sc, script);
				s.Monde().Add<NkScript2D>(z, sc);
			}
		} // namespace

		void NkEditeurScenePortes(NkScene &s) {
			NkSceneConfig cfg;
			cfg.physique = true;
			s.Init(cfg);
			Boite(s, "Sol", NkVec2f(0.f, -0.5f), NkVec2f(14.f, 0.5f), 0x4A4E57FFu, NkTypeCorps::NK_STATIQUE);
			Boite(s, "Mur gauche", NkVec2f(-14.f, 3.f), NkVec2f(0.5f, 3.f), 0x4A4E57FFu, NkTypeCorps::NK_STATIQUE);
			Boite(s, "Mur droit", NkVec2f(14.f, 3.f), NkVec2f(0.5f, 3.f), 0x4A4E57FFu, NkTypeCorps::NK_STATIQUE);
			// Le Joueur : un controleur de personnage (fleches ou A / D, Espace).
			const ecs::NkEntityId j = s.Creer("Joueur", NkVec2f(0.f, 0.6f));
			NkSprite2D sp;
			sp.taille = NkVec2f(0.8f, 0.8f);
			sp.couleur = 0xF2C14EFFu;
			s.Monde().Add<NkSprite2D>(j, sp);
			NkCollisionneur2D cj;
			cj.forme = NkForme2D::NK_CERCLE;
			cj.rayon = 0.4f;
			s.Monde().Add<NkCollisionneur2D>(j, cj);
			NkCorps2D kj;
			kj.rotationBloquee = true;
			kj.friction = 0.2f;
			s.AjouterCorps(j, kj);
			s.Monde().Add<NkControleRigide2D>(j, NkControleRigide2D());
			// A gauche le Blueprint, a droite le C++.
			Zone(s, "Zone bleue", -5.f, 0x3A7BFF40u, "Contenu/Scripts/PorteBlueprint.nkbp");
			Porte(s, "Porte bleue", -8.f, 0x3A7BFFFFu);
			Zone(s, "Zone rouge", 5.f, 0xFF4A3A40u, "cpp:PorteCpp");
			Porte(s, "Porte rouge", 8.f, 0xE8473AFFu);
			s.Camera().Cadrer(NkVec2f(0.f, 2.f), NkVec2f(30.f, 10.f));
		}

		NkString NkEditeurEcrireExemplePortes(const char *dossier, NkString *erreur) {
			auto echec = [&](const NkString &pourquoi) {
				if (erreur != nullptr) {
					*erreur = pourquoi;
				}
				return NkString();
			};
			const NkString racine(dossier != nullptr ? dossier : "");
			const NkString scenes = racine + "Contenu/Scenes";
			const NkString scripts = racine + "Contenu/Scripts";
			NkDirectory::CreateRecursive(scenes.CStr());
			NkDirectory::CreateRecursive(scripts.CStr());
			const NkString cpp = scripts + "/PorteCpp.cpp";
			if (!NkFile::WriteAllText(cpp.CStr(), NkEditeurSourcePorteCpp())) {
				return echec(NkString("ecriture impossible : ") + cpp);
			}
			// (2026-10-01) La porte REECRITE avec de vraies declarations (variables
			// « ouverte », « porte », « hauteur » et la fonction OuvrirPorte) :
			// l'ancienne forme (NkBpGraphePorte) reste lue telle quelle (banc u6).
			NkDocumentBp g;
			NkBpDocumentPorte(g, "Porte bleue");
			NkErreurBp err;
			const NkString bp = scripts + "/PorteBlueprint.nkbp";
			if (!NkBpEnregistrerDocument(bp.CStr(), g, err)) {
				return echec(NkString("Blueprint de la porte : ") + err.message);
			}
			NkScene *s = memory::NkGetDefaultAllocator().New<NkScene>();
			NkEditeurScenePortes(*s);
			const NkString scene = scenes + "/Portes.nkscene";
			const bool ok = NkSauverSceneFichier(*s, scene.CStr());
			memory::NkGetDefaultAllocator().Delete(s);
			if (!ok) {
				return echec(NkString("ecriture impossible : ") + scene);
			}
			return scene;
		}

		NkString NkEditeurDossierExemplePortes() {
			NkString base = NkDirectory::GetUserFolder(NkDirectory::NkUserFolder::Documents).ToString();
			if (base.Empty()) {
				base = NkDirectory::GetAppDataDirectory().ToString();
			}
			if (!base.Empty() && !base.EndsWith("/") && !base.EndsWith("\\")) {
				base += "/";
			}
			return base + "Unkeny/Exemples/Portes/";
		}

	} // namespace editeur
} // namespace nkentseu
