// =============================================================================
// NkGuiIntrospect.cpp — le releve : ce que NKGui vient de dessiner.
// Critere d'acceptation : Kernel/Runtime/NKGui/INTROSPECTION.md
// =============================================================================
#include "NKGui/Core/NkGuiIntrospect.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKContainers/String/NkString.h"

#include <cstdio>  // snprintf, fopen, fwrite — PAS de formateur a marqueurs
#include <cstring> // strcmp

namespace nkentseu {
	namespace nkgui {

		namespace {

			// Copie bornee, toujours terminee. `dst` de taille `cap`.
			void CopieBornee(char *dst, int32 cap, const char *src) noexcept {
				if (!dst || cap <= 0)
					return;
				int32 i = 0;
				if (src) {
					// ⚠️ LE LIBELLE EST COUPE A `##`, comme a l'affichage. NKGui
					//    utilise la convention `Texte##identifiant` : la partie
					//    apres `##` n'est jamais peinte. Un releve qui la
					//    montrerait ferait chercher a l'ecran un texte qui n'y
					//    est pas — exactement le contraire de ce qu'on demande a
					//    cet instrument.
					for (; i < cap - 1 && src[i]; ++i) {
						if (src[i] == '#' && src[i + 1] == '#')
							break;
						dst[i] = src[i];
					}
				}
				dst[i] = '\0';
			}

			const char *const kNomsEtats[NK_GUI_ETAT_COUNT] = {
				"grise", "coche", "selectionne", "survole", "enfonce",
				"ouvert", "replie", "mixte", "hors-vue", "vide",
			};

			const char *const kNomsNatures[static_cast<int32>(NkGuiNature::Count)] = {
				"inconnu", "fenetre", "panneau", "barre-menus", "menu", "entree",
				"separateur", "bouton", "case", "texte", "element", "onglet",
				"champ", "reglage", "region", "mesure",
			};

		} // namespace

		void NkGuiIntrospectActiver(NkGuiContext &ctx, bool actif) noexcept {
			ctx.introspect.actif = actif;
			if (!actif) {
				ctx.introspect.notes.Clear();
				ctx.introspect.perdues = 0;
			}
		}

		bool NkGuiIntrospectActif(const NkGuiContext &ctx) noexcept {
			return ctx.introspect.actif;
		}

		void NkGuiNoter(NkGuiContext &ctx, NkGuiNature nature, NkGuiId id, const char *libelle, const NkRect &rect,
						uint16 etats, const char *annexe) noexcept {
			if (!ctx.introspect.actif)
				return; // C5 : eteint = rien d'observable, un test de booleen
			if (static_cast<int32>(ctx.introspect.notes.Size()) >= NkGuiIntrospect::Max) {
				++ctx.introspect.perdues; // ⚠️ ca se compte, et l'en-tete le dit
				return;
			}

			// ⚠️ LE MARQUAGE DE L'INVISIBLE — C'EST LE CRITERE C2, ET C'EST ICI
			//    QU'IL SE JOUE. `UiRects` REFUSAIT de publier ce qui n'etait pas
			//    visible ; le releve confondait alors « absent » et « invisible »,
			//    et un controle cable mais jamais peint restait indetectable. On
			//    note TOUJOURS, et on MARQUE.
			//    Le hors-vue se juge sur la vue, pas sur un panneau : un panneau
			//    peut legitimement deborder et se faire rogner, alors qu'un
			//    rectangle entierement dehors n'a aucune chance d'etre vu.
			// ⚠️ SAUF POUR `Mesure` : quatre nombres qui ne sont pas une
			//    geometrie ne se jugent pas comme une geometrie. Un zoom de 1 et un
			//    deplacement nul sortiraient « vide,hors-vue » — une fausse alerte,
			//    et une fausse alerte apprend a ne plus lire l'instrument.
			uint16 e = etats;
			if (nature != NkGuiNature::Mesure) {
				if (rect.w <= 0.f || rect.h <= 0.f)
					e |= NK_GUI_ETAT_VIDE;
				const float32 vw = static_cast<float32>(ctx.viewW);
				const float32 vh = static_cast<float32>(ctx.viewH);
				if (vw > 0.f && vh > 0.f &&
					(rect.x + rect.w <= 0.f || rect.y + rect.h <= 0.f || rect.x >= vw || rect.y >= vh))
					e |= NK_GUI_ETAT_HORS_VUE;
			}

			NkGuiNote n;
			n.id = id;
			n.nature = nature;
			n.etats = e;
			// `curPopupLevel` vaut -1 sur la couche principale, 0..N dans un
			// popup : c'est deja la profondeur d'imbrication qu'on veut lire, et
			// elle ne se recalcule pas.
			n.niveau = static_cast<int16>(ctx.curPopupLevel);
			n.rect = rect;
			CopieBornee(n.libelle, NkGuiNote::LibelleMax, libelle);
			CopieBornee(n.annexe, NkGuiNote::AnnexeMax, annexe);
			ctx.introspect.notes.PushBack(n);
		}

		void NkGuiNoterMesure(NkGuiContext &ctx, const char *cle, float32 a, float32 b, float32 c,
							  float32 d) noexcept {
			if (!ctx.introspect.actif)
				return;
			NkGuiNoter(ctx, NkGuiNature::Mesure, NkGuiHashStr(cle ? cle : ""), cle, {a, b, c, d},
					   NK_GUI_ETAT_AUCUN);
			NkGuiIntrospectCler(ctx, cle);
		}

		void NkGuiIntrospectCler(NkGuiContext &ctx, const char *cle) noexcept {
			if (!ctx.introspect.actif || ctx.introspect.notes.Empty())
				return;
			NkGuiNote &n = ctx.introspect.notes[ctx.introspect.notes.Size() - 1];
			CopieBornee(n.cle, NkGuiNote::CleMax, cle);
		}

		const NkGuiNote *NkGuiIntrospectTrouverCle(const NkGuiContext &ctx, const char *cle) noexcept {
			if (!cle)
				return nullptr;
			const int32 n = static_cast<int32>(ctx.introspect.notes.Size());
			for (int32 i = 0; i < n; ++i) {
				const NkGuiNote &no = ctx.introspect.notes[static_cast<uint32>(i)];
				if (std::strcmp(no.cle, cle) == 0)
					return &no;
			}
			return nullptr;
		}

		const NkGuiNote *NkGuiIntrospectNotes(const NkGuiContext &ctx, int32 &nombre) noexcept {
			nombre = static_cast<int32>(ctx.introspect.notes.Size());
			return nombre > 0 ? &ctx.introspect.notes[0] : nullptr;
		}

		const NkGuiNote *NkGuiIntrospectTrouver(const NkGuiContext &ctx, const char *libelle,
												NkGuiNature nature) noexcept {
			if (!libelle)
				return nullptr;
			const int32 n = static_cast<int32>(ctx.introspect.notes.Size());
			for (int32 i = 0; i < n; ++i) {
				const NkGuiNote &no = ctx.introspect.notes[static_cast<uint32>(i)];
				if (nature != NkGuiNature::Count && no.nature != nature)
					continue;
				if (std::strcmp(no.libelle, libelle) == 0)
					return &no;
			}
			return nullptr;
		}

		const char *NkGuiNatureNom(NkGuiNature nature) noexcept {
			const int32 i = static_cast<int32>(nature);
			if (i < 0 || i >= static_cast<int32>(NkGuiNature::Count))
				return "inconnu";
			return kNomsNatures[i];
		}

		int32 NkGuiEtatsTexte(uint16 etats, char *buf, int32 capacite) noexcept {
			if (!buf || capacite <= 0)
				return 0;
			int32 n = 0;
			for (int32 b = 0; b < NK_GUI_ETAT_COUNT; ++b) {
				if (!(etats & (1u << b)))
					continue;
				const char *nom = kNomsEtats[b];
				if (n > 0 && n < capacite - 1)
					buf[n++] = ',';
				for (int32 k = 0; nom[k] && n < capacite - 1; ++k)
					buf[n++] = nom[k];
			}
			if (n == 0) {
				// ⚠️ « normal » PLUTOT QU'UNE COLONNE VIDE. Une colonne vide se
				//    lit comme une donnee manquante ; ici c'est une donnee
				//    presente qui vaut « aucun etat particulier ».
				const char *nom = "normal";
				for (int32 k = 0; nom[k] && n < capacite - 1; ++k)
					buf[n++] = nom[k];
			}
			buf[n] = '\0';
			return n;
		}

		bool NkGuiIntrospectEcrire(const NkGuiContext &ctx, const char *chemin, bool toujours) noexcept {
			if (!ctx.introspect.actif || !chemin || !*chemin)
				return false;

			// ── Construction du texte, par snprintf sur des champs SIMPLES ─────
			// Le libelle, lui, est ecrit tel quel : il peut contenir n'importe
			// quoi, accolades comprises.
			NkString sortie;
			char tampon[256];
			char etatsTxt[192];

			const int32 n = static_cast<int32>(ctx.introspect.notes.Size());
			int32 nHorsVue = 0, nVides = 0;
			for (int32 i = 0; i < n; ++i) {
				const uint16 e = ctx.introspect.notes[static_cast<uint32>(i)].etats;
				if (e & NK_GUI_ETAT_HORS_VUE)
					++nHorsVue;
				if (e & NK_GUI_ETAT_VIDE)
					++nVides;
			}

			// ⚠️ L'EN-TETE COMPTE, ET C'EST LE CRITERE C1. « 0 controle » doit se
			//    lire comme un VERDICT, pas comme un fichier qu'on soupconne
			//    tronque. C'est la difference entre « l'application ne montre
			//    rien » et « l'application montre autre chose que prevu » —
			//    personne n'a su la faire pendant trois etapes.
			snprintf(tampon, sizeof(tampon),
					 "# NKGui introspection : %d controle(s), %d hors-vue, %d degenere(s), %d perdue(s)\n"
					 "# vue %dx%d\n"
					 "# nature niveau rect=[x y w h] etats id \"libelle\" [annexe] {cle}\n",
					 n, nHorsVue, nVides, ctx.introspect.perdues, ctx.viewW, ctx.viewH);
			sortie.Append(tampon);

			for (int32 i = 0; i < n; ++i) {
				const NkGuiNote &no = ctx.introspect.notes[static_cast<uint32>(i)];
				NkGuiEtatsTexte(no.etats, etatsTxt, static_cast<int32>(sizeof(etatsTxt)));
				snprintf(tampon, sizeof(tampon), "%-11s n%-3d [%7.1f %7.1f %7.1f %7.1f] %-24s 0x%08x \"",
						 NkGuiNatureNom(no.nature), static_cast<int>(no.niveau), no.rect.x, no.rect.y, no.rect.w,
						 no.rect.h, etatsTxt, static_cast<unsigned>(no.id));
				sortie.Append(tampon);
				sortie.Append(no.libelle); // ⚠️ tel quel : jamais a travers un format
				sortie.Append("\"");
				if (no.annexe[0]) {
					sortie.Append(" [");
					sortie.Append(no.annexe);
					sortie.Append("]");
				}
				if (no.cle[0]) {
					sortie.Append(" {");
					sortie.Append(no.cle);
					sortie.Append("}");
				}
				sortie.Append("\n");
			}

			// ── N'ecrire que si ca a change ───────────────────────────────────
			static NkString precedent;
			if (!toujours && precedent.Size() == sortie.Size() &&
				std::strcmp(precedent.Data() ? precedent.Data() : "", sortie.Data() ? sortie.Data() : "") == 0)
				return true;
			precedent = sortie;

			// ⚠️ fwrite, PAS un formateur : cf. l'avertissement de l'en-tete.
			//    NKGui ne depend pas de NKFileSystem et n'a pas a en dependre
			//    pour ecrire un releve de diagnostic — ajouter une dependance de
			//    module pour un fopen serait payer cher une ligne.
			FILE *f = std::fopen(chemin, "wb");
			if (!f)
				return false;
			const char *d = sortie.Data();
			const usize taille = d ? sortie.Size() : 0;
			const bool ok = (taille == 0) || (std::fwrite(d, 1, taille, f) == taille);
			std::fclose(f);
			return ok;
		}

	} // namespace nkgui
} // namespace nkentseu
