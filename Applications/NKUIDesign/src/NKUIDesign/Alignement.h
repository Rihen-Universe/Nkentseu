#pragma once
// -----------------------------------------------------------------------------
// @File    Alignement.h
// @Brief   ALIGNER ET REPARTIR LA SELECTION -- le geste de Lunacy, sur le MODELE
//          (les positions), pas sur l'agencement des enfants.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ CE QUI EXISTAIT S'APPELLE DEJA « ALIGNEMENT », ET CE N'EST PAS CA
// =============================================================================
//  MESURE, avant d'ecrire (question de Rodolf : « est-ce que c'est deja en place ?
//  aligner du texte, aligner un graphique par rapport a un ou plusieurs autres... ») :
//  la section ALIGNEMENT de l'inspecteur ecrit `layout.mainAlign` / `crossAlign` du
//  noeud SELECTIONNE -- c'est-a-dire **comment ce noeud range SES ENFANTS** quand il
//  les agence. Sur une feuille, elle affiche « Feuille : ce noeud n'agence pas
//  d'enfants » et ne fait rien ; sur un cadre, elle deplace les ENFANTS, jamais le
//  noeud lui-meme. Aucune des six lignes ne repond a « aligner ce graphique sur un
//  autre », qui est le geste de Lunacy.
//
//  *Deux idees qui partagent un mot finissent par se prendre l'une pour l'autre.*
//  D'ou un vocabulaire separe : ALIGNEMENT = les enfants d'un cadre (ce qui existait),
//  ALIGNER LA SELECTION = les positions des noeuds choisis (ce fichier).
//
// =============================================================================
//  LES REGLES, ET CE QU'ELLES REFUSENT
// =============================================================================
//  - LA REFERENCE : deux noeuds ou plus -> la BOITE ENGLOBANTE de la selection ;
//    un seul -> LA PAGE qui le contient (c'est ce que Lunacy fait, et c'est le seul
//    sens qu'un alignement puisse avoir a un objet) ; l'option « sur le dernier
//    selectionne » prend la boite du DERNIER entre dans la selection (l'« objet
//    cle » de Lunacy) et ne bouge pas celui-la.
//  - CE QUI NE PEUT PAS BOUGER LE DIT : un noeud dont le PARENT agence ses enfants
//    (colonne, ligne, grille) n'a pas de position propre -- `posX/posY` y sont
//    ignores. On ne l'ecrit pas en silence : il est compte a part et la phrase le
//    nomme. Meme regle que le deplacement au clavier.
//  - REPARTIR exige TROIS noeuds (avec deux, il n'y a rien a repartir) : les
//    extremes ne bougent pas, les autres se posent a intervalle egal ENTRE LES
//    CENTRES. C'est la definition la plus simple, et elle est dite : repartir les
//    ESPACES (celle de Figma « distribute spacing ») donnerait d'autres nombres sur
//    des objets de tailles differentes.
//  - LA ROTATION N'EST PAS PRISE EN COMPTE : on aligne les BOITES DE DISPOSITION,
//    pas les quatre coins tournes. Un rectangle tourne de 30 degres s'aligne donc
//    sur sa boite droite. Dit ici, et a l'ecran ca se voit -- c'est aussi ce que
//    font Figma et Lunacy sur leur cadre de selection.
// -----------------------------------------------------------------------------

#include "Layout.h"
#include "Selection.h"

namespace nkuidesign {

	enum class NkAlignGeste : nkentseu::uint8 {
		Gauche = 0,
		CentreH,
		Droite,
		Haut,
		Milieu,
		Bas,
		RepartirH,
		RepartirV,
		NB
	};

	inline const char *NkAlignGesteNom(NkAlignGeste g) {
		switch (g) {
			case NkAlignGeste::Gauche: return "aligner à gauche";
			case NkAlignGeste::CentreH: return "centrer horizontalement";
			case NkAlignGeste::Droite: return "aligner à droite";
			case NkAlignGeste::Haut: return "aligner en haut";
			case NkAlignGeste::Milieu: return "centrer verticalement";
			case NkAlignGeste::Bas: return "aligner en bas";
			case NkAlignGeste::RepartirH: return "répartir horizontalement";
			case NkAlignGeste::RepartirV: return "répartir verticalement";
			default: return "?";
		}
	}
	inline bool NkAlignEstVertical(NkAlignGeste g) {
		return g == NkAlignGeste::Haut || g == NkAlignGeste::Milieu || g == NkAlignGeste::Bas
			   || g == NkAlignGeste::RepartirV;
	}
	inline bool NkAlignEstRepartir(NkAlignGeste g) {
		return g == NkAlignGeste::RepartirH || g == NkAlignGeste::RepartirV;
	}

	/// Le resultat d'un geste, tel que le pied le dira.
	struct NkAlignResultat {
			nkentseu::uint32 bouges = 0;  ///< noeuds effectivement deplaces
			nkentseu::uint32 refuses = 0; ///< noeuds dont le parent agence : position ignoree
			char message[220] = {};
			bool ok = false;
	};

	namespace aligndetail {

		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;

		/// Le parent LIT-IL `posX`/`posY` de ses enfants ? **`free` seulement.**
		///
		/// 🔴 `anchor` A ETE RETIRE LE 07/09, et c'est un ARBITRAGE, pas un reglage.
		///    La branche `Anchor` du solveur (`Layout.h`) calcule la place depuis les
		///    BORDS ancres : elle ne lit jamais `posX`/`posY`. Ecrire une position
		///    la-bas revenait a poser une valeur que rien ne relit -- et le geste
		///    COMPTAIT alors un deplacement qui n'avait pas lieu (temoin 93b :
		///    « 1 annonce bouge, 0 boite reellement deplacee »).
		///
		/// ⚠️ DEUX ENDROITS ENONCAIENT DES REGLES CONTRAIRES SUR UN MEME FAIT : la
		///    section DISPOSITION de l'inspecteur traite deja la position comme
		///    CALCULEE sous un parent `Anchor` (elle affiche des boites statiques et
		///    ecrit « calculee — jamais ecrite dans le document ») ; ce fichier la
		///    traitait comme ECRITE. **Quand deux endroits se contredisent, celui qui
		///    S'EXECUTE a raison** -- ici le solveur -- et l'autre ment. L'utilisateur
		///    etait deja trompe : l'aligner sur le solveur ne lui retire rien, ca rend
		///    visible ce qui etait deja vrai.
		///
		/// ⚠️ CE QUE CET ARBITRAGE FERME, ET IL FAUT LE SAVOIR MAINTENANT : si l'on
		///    veut un jour que l'alignement agisse REELLEMENT sous un parent en
		///    ancrage, ce sera un chantier de SOLVEUR (apprendre a composer un decalage
		///    avec des bords ancres), pas un branchement de champ ici.
		inline bool ParentPlaceLibrement(const NkUIDocument &doc, int32 i) {
			if (!doc.IsValidIndex(i))
				return false;
			const int32 p = doc.nodes[(uint32)i].parent;
			if (!doc.IsValidIndex(p))
				return false;
			return doc.nodes[(uint32)p].layout.kind == NkLayoutKind::Free;
		}
		/// La page d'un noeud : son ancetre direct sous la racine.
		inline int32 PageDe(const NkUIDocument &doc, int32 i) {
			if (!doc.IsValidIndex(i) || i == 0)
				return -1;
			while (doc.nodes[(uint32)i].parent > 0)
				i = doc.nodes[(uint32)i].parent;
			return i;
		}

	} // namespace aligndetail

	/// ALIGNER (ou REPARTIR) LA SELECTION. Rend ce qui a bouge, ce qui a refuse, et
	/// la phrase a dire. N'ecrit rien quand il n'y a rien a faire -- et le dit.
	///
	/// ⚠️ LES POSITIONS SE LISENT DANS LA DISPOSITION, PAS DANS `posX/posY` : un noeud
	///    peut declarer une position et se voir place ailleurs (min/max, ancrage). On
	///    calcule donc l'ecart voulu sur les RECTANGLES calcules, puis on l'ajoute a
	///    `posX/posY` -- exactement ce que fait le deplacement a la souris.
	/// ⚠️ UN PATRON, PAS UNE DEPENDANCE : `DesignState` est declare dans `Panels.h`, qui
	///    inclut ce fichier -- l'ecrire ici en dur ferait une boucle. Le geste ne demande
	///    a l'etat que ce qu'il utilise (`doc`, `sel`, `selected`, `layout`, `Recompute`,
	///    `host`), et c'est le compilateur qui le verifie au premier appel.
	template <class TEtat>
	inline NkAlignResultat NkAlignerSelection(TEtat &st, NkAlignGeste g, bool surDernier) {
		using namespace nkentseu;
		using namespace aligndetail;
		NkAlignResultat r;
		NkVector<int32> cibles;
		for (uint32 k = 0; k < st.sel.Count(); ++k) {
			const int32 i = st.sel.items[k];
			if (st.doc.IsValidIndex(i) && i != 0)
				cibles.PushBack(i);
		}
		if (cibles.Empty() && st.doc.IsValidIndex(st.selected) && st.selected != 0)
			cibles.PushBack(st.selected);
		if (cibles.Empty()) {
			snprintf(r.message, sizeof(r.message), "Aligner : rien n'est sélectionné.");
			return r;
		}
		// la disposition A JOUR : le geste lit des rectangles, pas des intentions
		st.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
		const NkLayoutResult &lay = st.layout;
		if (NkAlignEstRepartir(g) && cibles.Size() < 3u) {
			snprintf(r.message, sizeof(r.message),
					 "Répartir : il faut au moins TROIS éléments (avec deux, il n'y a rien à répartir).");
			return r;
		}
		// ── LA REFERENCE ────────────────────────────────────────────────────
		NkPaintRect ref = {0.f, 0.f, 0.f, 0.f};
		int32 cle = -1;
		char quoi[80];
		if (!NkAlignEstRepartir(g)) {
			if (surDernier && cibles.Size() >= 2u) {
				cle = cibles[(uint32)cibles.Size() - 1u];
				if (!lay.Has(cle)) {
					snprintf(r.message, sizeof(r.message), "Aligner : le dernier sélectionné n'a pas de boîte.");
					return r;
				}
				ref = lay.At(cle);
				snprintf(quoi, sizeof(quoi), "le dernier sélectionné");
			} else if (cibles.Size() >= 2u) {
				bool premier = true;
				for (uint32 k = 0; k < (uint32)cibles.Size(); ++k) {
					if (!lay.Has(cibles[k]))
						continue;
					const NkPaintRect q = lay.At(cibles[k]);
					if (premier) {
						ref = q;
						premier = false;
						continue;
					}
					const float32 x1 = (ref.x + ref.w > q.x + q.w) ? ref.x + ref.w : q.x + q.w;
					const float32 y1 = (ref.y + ref.h > q.y + q.h) ? ref.y + ref.h : q.y + q.h;
					if (q.x < ref.x)
						ref.x = q.x;
					if (q.y < ref.y)
						ref.y = q.y;
					ref.w = x1 - ref.x;
					ref.h = y1 - ref.y;
				}
				if (premier) {
					snprintf(r.message, sizeof(r.message), "Aligner : la sélection n'a pas de boîte.");
					return r;
				}
				snprintf(quoi, sizeof(quoi), "la sélection");
			} else {
				const int32 page = PageDe(st.doc, cibles[0]);
				if (page < 0 || !lay.Has(page)) {
					snprintf(r.message, sizeof(r.message),
							 "Aligner : un seul élément s'aligne sur SA PAGE, et il n'en a pas.");
					return r;
				}
				ref = lay.At(page);
				snprintf(quoi, sizeof(quoi), "la page « %s »",
						 st.doc.nodes[(uint32)page].label.Empty() ? "sans nom"
																  : st.doc.nodes[(uint32)page].label.Data());
			}
		} else
			snprintf(quoi, sizeof(quoi), "les centres, à intervalle égal");

		// ── LE DEPLACEMENT VOULU, PAR NOEUD ─────────────────────────────────
		auto deplacer = [&](int32 i, float32 dx, float32 dy) {
			if (dx == 0.f && dy == 0.f)
				return;
			if (!ParentPlaceLibrement(st.doc, i)) {
				++r.refuses;
				return;
			}
			NkUINode &n = st.doc.nodes[(uint32)i];
			if (n.refusPosition) {
				++r.refuses;
				return;
			}
			n.posX += dx;
			n.posY += dy;
			st.doc.MarkHumanEdit(i);
			++r.bouges;
		};

		if (!NkAlignEstRepartir(g)) {
			for (uint32 k = 0; k < (uint32)cibles.Size(); ++k) {
				const int32 i = cibles[k];
				if (i == cle || !lay.Has(i))
					continue;
				const NkPaintRect q = lay.At(i);
				float32 dx = 0.f, dy = 0.f;
				switch (g) {
					case NkAlignGeste::Gauche: dx = ref.x - q.x; break;
					case NkAlignGeste::CentreH: dx = (ref.x + ref.w * 0.5f) - (q.x + q.w * 0.5f); break;
					case NkAlignGeste::Droite: dx = (ref.x + ref.w) - (q.x + q.w); break;
					case NkAlignGeste::Haut: dy = ref.y - q.y; break;
					case NkAlignGeste::Milieu: dy = (ref.y + ref.h * 0.5f) - (q.y + q.h * 0.5f); break;
					case NkAlignGeste::Bas: dy = (ref.y + ref.h) - (q.y + q.h); break;
					default: break;
				}
				deplacer(i, dx, dy);
			}
		} else {
			// REPARTIR : les extremes tiennent, les autres se posent a intervalle egal
			// entre les CENTRES (dit dans l'en-tete : ce n'est pas « repartir les espaces »).
			const bool vertical = (g == NkAlignGeste::RepartirV);
			enum { kMax = 256 };
			int32 ordre[kMax];
			uint32 nb = 0;
			for (uint32 k = 0; k < (uint32)cibles.Size() && nb < kMax; ++k)
				if (lay.Has(cibles[k]))
					ordre[nb++] = cibles[k];
			if (nb < 3u) {
				snprintf(r.message, sizeof(r.message), "Répartir : il faut au moins trois éléments placés.");
				return r;
			}
			auto centre = [&](int32 i) {
				const NkPaintRect q = lay.At(i);
				return vertical ? q.y + q.h * 0.5f : q.x + q.w * 0.5f;
			};
			for (uint32 a = 1; a < nb; ++a) { // tri par insertion sur le centre
				const int32 v = ordre[a];
				uint32 b = a;
				while (b > 0 && centre(ordre[b - 1]) > centre(v)) {
					ordre[b] = ordre[b - 1];
					--b;
				}
				ordre[b] = v;
			}
			const float32 c0 = centre(ordre[0]), c1 = centre(ordre[nb - 1u]);
			const float32 pas = (c1 - c0) / (float32)(nb - 1u);
			for (uint32 k = 1; k + 1 < nb; ++k) {
				const float32 vise = c0 + pas * (float32)k;
				const float32 d = vise - centre(ordre[k]);
				deplacer(ordre[k], vertical ? 0.f : d, vertical ? d : 0.f);
			}
		}
		st.host.SyncTo(st.doc);
		st.Recompute(NkPaintRect{0.f, 0.f, 1400.f, 900.f});
		r.ok = r.bouges > 0u;
		if (r.bouges == 0u && r.refuses == 0u)
			snprintf(r.message, sizeof(r.message), "%s : rien à déplacer (tout est déjà en place).", NkAlignGesteNom(g));
		else if (r.refuses > 0u)
			snprintf(r.message, sizeof(r.message),
					 "%s sur %s : %u déplacé(s), %u laissé(s) — leur parent place ses enfants lui-même.",
					 NkAlignGesteNom(g), quoi, r.bouges, r.refuses);
		else
			snprintf(r.message, sizeof(r.message), "%s sur %s : %u déplacé(s).", NkAlignGesteNom(g), quoi, r.bouges);
		return r;
	}

} // namespace nkuidesign
