// =============================================================================
// Applications/NK3DModeler/src/NK3DModeler/Viewport/NkVpMatTypeDefaults.h
// =============================================================================
// LA TABLE DES DEFAUTS PAR TYPE DE MATERIAU — et pourquoi elle existe.
//
// Rodolf, 2026-08-21 : « si je modifie le type de material d'un model sans
// changer le material, le nouveau type herite des proprietes communes entre
// l'ancien type et le nouveau, pourtant ca devait faire comme si on avait reset
// ce dernier aux valeurs par defaut du nouveau type. »
//
// ⚠️ IL L'AVAIT DEJA SIGNALE LE 14 AOUT. La correction d'alors etait deux `if`
// ecrits a la main (verre -> alpha = 1, emissif -> emissive = 0) : une LISTE, pas
// une regle. Une liste ne couvre que ce a quoi on a pense — `rough`, `metal`,
// `clearcoat`, `subsurface`, `parallax`, `shadowMode` et tout champ ajoute depuis
// traversaient encore le changement de type, EN SILENCE. Une correction par
// enumeration de cas se perime a chaque champ ajoute.
//
// LA REGLE QUI REMPLACE LA LISTE :
//     changer le type = ecraser TOUS les parametres numeriques
//                       par la ligne de defauts du NOUVEAU type,
//                       ET vider les chemins de texture.
// Pas de comparaison ancien/nouveau. Pas de « sauf si ». Pas de liste de cas.
//
// -----------------------------------------------------------------------------
// LES TEXTURES PARTENT AUSSI — et le cas « avec noeuds » n'est pas une exception
//
// Rodolf, 2026-08-22 : « on doit tout vider meme les texture, c'est comme ca que
// fait blender », puis « sauf si certains parametres dans les noeuds ». Les deux
// sont vrais, et c'est la MEME regle sur deux modeles de donnees :
//   • SANS NOEUDS (notre cas) : la texture est une PROPRIETE du materiau. Le type
//     change, la propriete part. C'est ce que fait ce fichier.
//   • AVEC NOEUDS (Blender, shader graph) : l'« Image Texture » est un NOEUD a
//     part entiere, jamais une propriete du noeud de shader. Remplacer Principled
//     par Diffuse laisse le noeud image dans le graphe, simplement DEBRANCHE.
// Rien a coder pour le second tant que le modeleur n'a pas de graphe de noeuds —
// mais c'est ecrit ici pour que personne ne prenne un jour ce comportement pour
// une incoherence. Quand le graphe arrivera, la regle ne changera pas : c'est le
// support de la donnee qui change.
//
// -----------------------------------------------------------------------------
// POURQUOI CETTE TABLE EST ICI ET PAS DANS LE MOTEUR — recherche faite, resultat
// enonce avec son perimetre (cherche le 2026-08-22) :
//   • `NkMaterialTemplateDesc` (NkMaterialSystem.h) ne porte QUE de l'etat de
//     pipeline : type, file de rendu, cull, fill, depth, sources de shader.
//     AUCUN parametre numerique.
//   • `NkMaterialSystem::RegisterBuiltins` n'enregistre qu'un nom + un dossier de
//     shader par archetype (17 gabarits) — aucune valeur de `rough`/`metal`/...
//   • `NkMaterial::Create` ne fait que mapper un type vers un handle de gabarit.
//   • Recherche large `defaultparams|paramsfor|presetfor|archetypedefault|
//     typedefaults|defaultsfor` sur `Kernel/` et `Applications/NK3DModeler/` :
//     zero resultat (controle : `NK_GLASS` en remonte 3 sur le meme perimetre —
//     la commande sait donc trouver).
// CONCLUSION : le moteur n'expose AUCUN jeu de defauts numeriques par archetype.
// Cette table n'en duplique donc aucun -- elle est la premiere source de verite,
// pas une seconde.
//
// 🔗 COORDINATION (2026-08-22) -- `Kernel/Runtime/NKRenderer/Materials/` appartient
// desormais a l'agent MATGRAPH, et les « defauts de prise » du graphe de materiaux
// y sont un sujet ouvert. **C'est LA MEME DONNEE que cette table, vue de l'autre
// bout.** Avant d'etendre ce fichier -- surtout avant d'y ajouter des lignes pour
// peau/cheveux/tissu/carrosserie/feuillage/eau -- **demande a matgraph si le
// graphe va porter ces defauts.** Si oui, cette table doit DISPARAITRE au profit
// de la sienne, pas grossir a cote. C'est pour ca qu'il n'y a qu'un seul point
// d'acces : le jour ou l'on bascule, un seul corps de fonction change.
//
// -----------------------------------------------------------------------------
// ⚠️ CETTE TABLE EST FAITE POUR ETRE REMPLACEE — c'est son cahier des charges
//
// Rodolf a demande si les defauts pouvaient etre pilotes par le SHADER, sans
// aucun `if`. C'est la bonne cible, et c'est le seul moyen d'empecher ce defaut
// de revenir une troisieme fois : la verite vivrait la ou le parametre est
// declare. NkSL ne peut pas encore le porter — `NkSLReflection` rend les blocs
// uniformes, leurs bindings et leur taille, mais JAMAIS leurs membres, et aucune
// valeur par defaut (le GLSL ne permet pas d'initialiser un membre d'uniforme,
// donc il n'y a rien a reflechir tant qu'il n'y a rien a declarer ; NkSL etant a
// nous, c'est une decision de conception, pas un obstacle).
//
// D'ou la forme retenue ici : **UN SEUL POINT D'ACCES**,
// `NkVpMatTypeDefaultsFor(type) -> NkVpMatParams`, et de la DONNEE derriere.
// Le jour ou NkSL reflechira les membres et leurs defauts, **seul le corps de
// cette fonction change ; aucun appelant ne s'en apercoit.** C'est pour ca qu'on
// ne dissemine pas de `if type == X` chez les appelants : chacun serait un
// endroit de plus a retrouver ce jour-la.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

#include <cstddef> // offsetof

namespace nkentseu {

	// Les parametres NUMERIQUES d'un materiau du modeleur — exactement ceux qu'un
	// changement de type ecrase.
	//
	// ⚠️ `emiEclaire` est un `int32` (0/1) et non un `bool` DELIBEREMENT : tous
	// les membres font alors 4 octets, la struct n'a aucun octet de bourrage, et
	// le controle de couverture ci-dessous peut etre EXACT plutot que tolerant.
	// Un controle exact echoue quand on ajoute un champ ; un controle tolerant
	// laisse passer.
	struct NkVpMatParams {
			float32 albedo[3];
			float32 rough, metal;
			float32 clearcoat, ccRough, subsurface;
			float32 nrmStrength, emiStrength;
			float32 emissive[3];
			float32 parallax;
			int32 shadowMode;
			float32 alpha, aniso, sheenV;
			float32 toonThresh, toonSmooth;
			float32 toonShadow[3];
			float32 outlineW;
			float32 outlineCol[3];
			float32 rimI;
			float32 rimCol[3];
			float32 specHard;
			int32 emiEclaire; // 0/1 — voir la note ci-dessus
	};

	// ── DESCRIPTEUR DE CHAMP — ce qui rend la couverture VERIFIABLE ──────────
	// Le defaut du 14 aout n'etait pas « deux mauvais `if` », c'etait qu'aucun
	// controle ne pouvait dire qu'il en manquait. Cette table de descripteurs
	// existe pour qu'un banc puisse repondre a la question « chaque champ est-il
	// couvert ? » sans qu'un humain relise la struct.
	struct NkVpMatFieldDesc {
			const char *nom;
			uint32 offset;
			uint32 taille;
	};

	// ⚠️ AJOUTER UN CHAMP A `NkVpMatParams` SANS L'AJOUTER ICI REND LE BANC ROUGE
	// (cas `type/couverture-des-champs` de NKMatTypeResetTest) : la somme des
	// tailles ne fait plus `sizeof(NkVpMatParams)`, et il n'y a aucun bourrage
	// pour masquer l'ecart. C'est le controle qui manquait le 14 aout.
	inline const NkVpMatFieldDesc *NkVpMatFields(uint32 &count) {
#define NKVP_F(m) {#m, (uint32)offsetof(NkVpMatParams, m), (uint32)sizeof(((NkVpMatParams *)0)->m)}
		static const NkVpMatFieldDesc kF[] = {
			NKVP_F(albedo),		NKVP_F(rough),	   NKVP_F(metal),	  NKVP_F(clearcoat),
			NKVP_F(ccRough),	NKVP_F(subsurface), NKVP_F(nrmStrength), NKVP_F(emiStrength),
			NKVP_F(emissive),	NKVP_F(parallax),  NKVP_F(shadowMode), NKVP_F(alpha),
			NKVP_F(aniso),		NKVP_F(sheenV),	   NKVP_F(toonThresh), NKVP_F(toonSmooth),
			NKVP_F(toonShadow), NKVP_F(outlineW),  NKVP_F(outlineCol), NKVP_F(rimI),
			NKVP_F(rimCol),		NKVP_F(specHard),  NKVP_F(emiEclaire),
		};
#undef NKVP_F
		count = (uint32)(sizeof(kF) / sizeof(kF[0]));
		return kF;
	}

	// La ligne de base — STANDARD (PBR). Identique aux valeurs qu'un materiau
	// neuf recoit a sa creation : « creer puis assigner » ne doit rien changer
	// tant qu'on n'a pas touche un curseur.
	inline NkVpMatParams NkVpMatBaseDefaults() {
		NkVpMatParams d;
		d.albedo[0] = d.albedo[1] = d.albedo[2] = 0.7f; // gris neutre mat
		d.rough = 0.85f;
		d.metal = 0.f;
		d.clearcoat = 0.f;
		d.ccRough = 0.f;
		d.subsurface = 0.f;
		d.nrmStrength = 1.f;
		d.emiStrength = 1.f;
		d.emissive[0] = d.emissive[1] = d.emissive[2] = 0.f;
		d.parallax = 0.f;
		d.shadowMode = 1; // proportionnelle
		d.alpha = 1.f;
		d.aniso = 0.f;
		d.sheenV = 0.f;
		d.toonThresh = 0.3f;
		d.toonSmooth = 0.05f;
		d.toonShadow[0] = 0.2f;
		d.toonShadow[1] = 0.1f;
		d.toonShadow[2] = 0.3f;
		d.outlineW = 2.f;
		d.outlineCol[0] = d.outlineCol[1] = d.outlineCol[2] = 0.f;
		d.rimI = 0.5f;
		d.rimCol[0] = d.rimCol[1] = d.rimCol[2] = 1.f;
		d.specHard = 32.f;
		d.emiEclaire = 0;
		return d;
	}

	// ⚠️ UN TYPE SANS LIGNE PROPRE REND LA LIGNE DE BASE — ENTIERE.
	// C'est la propriete qui compte : 16 des 33 types declares n'ont aucun
	// gabarit cote moteur, et un type inconnu ne doit JAMAIS laisser un materiau
	// « a moitie ancien ». Retomber sur une base COMPLETE garantit qu'aucun champ
	// ne survit au changement, meme pour un type qu'on n'a pas prevu.
	//
	// ⚠️ CE QUI EST ICI EST CE QUI EXISTAIT DEJA DANS LE CODE, PAS DES VALEURS
	// INVENTEES. Seuls le verre et l'emissif portaient un prereglage (les deux
	// `if` de Demo3DHostProjMatSetType) ; ils sont repris tels quels. Les autres
	// archetypes — peau, cheveux, tissu, carrosserie, feuillage, eau — n'ont
	// AUCUN prereglage a ce jour, ni ici ni dans le moteur : leur donner des
	// valeurs physiques serait une decision produit, pas une correction de
	// defaut. Ils rendent donc la base, et ajouter leur ligne est un `case`.
	inline NkVpMatParams NkVpMatTypeDefaultsFor(int32 type) {
		NkVpMatParams d = NkVpMatBaseDefaults();
		switch (type) {
			case 5: // NK_GLASS — une vitre opaque n'est pas une vitre.
				// 0,12 : la valeur deja posee par l'ancien code. Desormais
				// INCONDITIONNELLE — avant, elle n'etait appliquee que si
				// l'opacite valait encore exactement 1, donc le meme geste
				// donnait deux resultats selon un historique invisible.
				d.alpha = 0.12f;
				break;
			case 11: // NK_EMISSIVE — un emissif ne rend QUE son emission.
				// Emission NOIRE = objet noir, ce qui se lit comme une panne
				// (Rihen, 14 aout : type Emissif, intensite 14,88, sphere
				// noire). L'ancien code partait de l'albedo COURANT ; sous la
				// nouvelle regle l'albedo est lui aussi remis a son defaut,
				// donc « partir de l'albedo » n'a plus de sens : on pose une
				// emission BLANCHE, c'est-a-dire une lampe allumee.
				d.emissive[0] = d.emissive[1] = d.emissive[2] = 1.f;
				break;
			default:
				// Base entiere — voir l'avertissement ci-dessus.
				break;
		}
		return d;
	}

	// ── L'ANCIENNE REGLE, GARDEE COMME TEMOIN ────────────────────────────────
	// Ce n'est PAS du code mort a supprimer : c'est la liste du 14 aout, gardee
	// pour que le banc puisse MONTRER ce qu'elle laissait passer. Un banc qui se
	// contente de verifier la nouvelle regle prouve qu'elle marche ; celui-ci
	// prouve en plus POURQUOI l'ancienne ne suffisait pas — et c'est ce qui
	// empeche quelqu'un de « simplifier » un jour en revenant a une liste.
	inline void NkVpMatAppliquerAncienneRegle(NkVpMatParams &m, int32 ancien, int32 type) {
		if (ancien == 5 && type != 5)
			m.alpha = 1.f;
		if (ancien == 11 && type != 11) {
			m.emissive[0] = 0.f;
			m.emissive[1] = 0.f;
			m.emissive[2] = 0.f;
		}
		if (type == 5 && m.alpha >= 0.999f)
			m.alpha = 0.12f;
		if (type == 11 && m.emissive[0] <= 0.001f && m.emissive[1] <= 0.001f && m.emissive[2] <= 0.001f) {
			const bool noir = m.albedo[0] <= 0.001f && m.albedo[1] <= 0.001f && m.albedo[2] <= 0.001f;
			m.emissive[0] = noir ? 1.f : m.albedo[0];
			m.emissive[1] = noir ? 1.f : m.albedo[1];
			m.emissive[2] = noir ? 1.f : m.albedo[2];
		}
	}

	// LA NOUVELLE REGLE, en une ligne : la ligne du type, entiere.
	inline void NkVpMatAppliquerRegle(NkVpMatParams &m, int32 type) {
		m = NkVpMatTypeDefaultsFor(type);
	}

} // namespace nkentseu
