// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkWindowAudit.h
//
// CE QUI NE PEUT PAS AGIR DOIT LE DIRE.
//
// `NkWindowConfig` promet vingt-cinq réglages. Aucun dorsal ne les tient tous —
// et c'est normal : un téléphone n'a pas de barre de titre, `xdg-shell` n'a pas
// de « toujours au-dessus ». Ce qui n'est PAS normal, c'est qu'un réglage non
// tenu s'accepte en SILENCE : l'utilisateur pose `resizable = false`, voit sa
// fenêtre se redimensionner quand même, et cherche l'erreur chez lui. Une
// propriété qui s'accepte sans agir est pire qu'une propriété absente —
// l'absence se voit à la compilation, le silence se paie en heures perdues.
//
// Chaque dorsal appelle donc `NkWindowAuditerConfig` dans son `Create`, en
// déclarant ce qu'il TIENT et ce qui est SANS OBJET chez lui. Tout le reste,
// s'il a été demandé, sort au journal sous forme de REFUS NOMMÉ.
//
// ⚠️ RÈGLE DU REFUS : il ne se déclenche QUE si la valeur demandée diffère du
//    défaut. Une application qui ne touche à rien ne doit pas voir une page
//    d'avertissements au démarrage ; le journal ne parle que de ce qui a été
//    *demandé* et ne sera pas *livré*. Et il ne parle QU'UNE FOIS par propriété
//    et par processus : dix fenêtres identiques ne font pas dix lignes.
//
// La table de vérité complète (propriété × dorsal) vit dans
// `wiki/Runtime/NKWindow/Proprietes-par-dorsal.md`.
// =============================================================================

#include "NkTypes.h"

namespace nkentseu {

	struct NkWindowConfig;

	// -------------------------------------------------------------------------
	// NkWindowProp — les réglages auditables. L'ordre fixe le numéro de bit :
	// ne PAS réordonner, seulement ajouter avant `Count`.
	// -------------------------------------------------------------------------
	enum class NkWindowProp : uint32 {
		Resizable = 0,
		Movable,
		Closable,
		Minimizable,
		Maximizable,
		CanFullscreen,
		Fullscreen,
		Modal,
		Centered,
		DropEnabled,
		Frame,
		HasShadow,
		Transparent,
		Visible,
		BgColor,
		AlwaysOnTop,
		ClickThrough,
		Opacity,
		NoActivate,
		MinSize,
		MaxSize,
		ScreenOrientation,
		RespectSafeArea,
		HideSystemUI,
		LockOrientation,
		Count
	};

	inline constexpr uint64 NkWindowPropBit(NkWindowProp p) {
		return 1ull << static_cast<uint32>(p);
	}

/// Masque d'une propriété, à composer par `|` : `NK_WPROP(Resizable) | NK_WPROP(Frame)`.
#define NK_WPROP(x) (::nkentseu::NkWindowPropBit(::nkentseu::NkWindowProp::x))

	/// Nom lisible de la propriété, tel qu'il apparaît dans `NkWindowConfig`.
	const char *NkWindowPropNom(NkWindowProp p);

	// -------------------------------------------------------------------------
	// NkWindowAuditerConfig
	//
	// À appeler UNE FOIS par `NkWindow::Create`, une fois la fenêtre native
	// obtenue (ou juste avant de rendre `false`).
	//
	// @param config      la configuration telle que l'appelant l'a écrite
	// @param plateforme  le nom du dorsal, tel qu'il doit apparaître au journal
	//                    (« Win32 », « Wayland », « Android »…)
	// @param tenues      masque des propriétés que ce dorsal HONORE vraiment.
	//                    Écrire ici une propriété qu'on ne tient pas, c'est
	//                    remettre le silence en place par la porte de service.
	// @param sansObjet   masque des propriétés qui n'ont pas de sens sur cette
	//                    plateforme. Elles sont dites aussi, mais avec leurs
	//                    mots à elles : « sans objet » n'est pas « pas encore
	//                    fait », et l'appelant n'a pas la même décision à
	//                    prendre dans les deux cas.
	// -------------------------------------------------------------------------
	void NkWindowAuditerConfig(const NkWindowConfig &config, const char *plateforme, uint64 tenues,
							   uint64 sansObjet = 0);

	// -------------------------------------------------------------------------
	// NkWindowRefuserUneFois
	//
	// Refus ponctuel, hors audit de création : un SETTER qui ne peut rien faire
	// (`SetScreenOrientation` sur un bureau, `SetAlwaysOnTop` sous Wayland). Il
	// parle une fois par propriété et par processus, comme l'audit.
	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------
	// NkWindowAuditerDorsalCourant
	//
	// L'audit du dorsal REELLEMENT compile, masques compris. Appele une fois par
	// fenetre depuis `NkWESystem::RegisterWindow` — le seul point que les douze
	// `NkWindow::Create` traversent tous. Les masques vivent dans
	// NkWindowAudit.cpp : un seul endroit dit ce que chaque dorsal tient.
	// -------------------------------------------------------------------------
	void NkWindowAuditerDorsalCourant(const NkWindowConfig &config);

	void NkWindowRefuserUneFois(NkWindowProp p, const char *plateforme, const char *raison);

	// ── LE REFUS NOMME DES METHODES, a cote de celui des PROPRIETES ─────────
	// Une propriete se declare a la construction ; une METHODE s'appelle a tout
	// moment. Le refus ne peut donc pas passer par l'audit de config -- il n'y a
	// pas de config a auditer quand on appelle `SetCursor` au milieu d'une image.
	//
	// POURQUOI IL EXISTE (utilisateur reel, 26/09) : « les methodes pour curseur
	// dans NKWindow ne fonctionnent pas ». Elles ne fonctionnaient pas sur SON
	// dorsal, en silence -- `SetCursor` n'est implemente que sur Win32,
	// `CaptureMouse` est vide sur XLib et XCB, et Wayland range deux booleens que
	// personne ne lit. Il a cherche l'erreur chez lui.
	//
	// ⚠️ UNE FOIS PAR METHODE ET PAR PROCESSUS. Ces methodes s'appellent souvent
	//    a chaque image : un message par appel noierait le journal et se ferait
	//    couper par le premier lecteur presse.
	// ⚠️ ET SEULEMENT SI ON A APPELE. Une application qui ne touche pas au
	//    curseur ne doit voir aucun avertissement -- meme regle que les
	//    proprietes : le journal ne parle que de ce qui a ete DEMANDE.
	void NkWindowRefuserMethode(const char *methode, const char *plateforme, const char *raison);

	/// Remet les compteurs « déjà dit » à zéro. RÉSERVÉ AUX BANCS : une sonde qui
	/// mesure plusieurs configurations dans un seul processus doit pouvoir
	/// réentendre les refus de la deuxième.
	void NkWindowAuditReinitialiser();

} // namespace nkentseu
