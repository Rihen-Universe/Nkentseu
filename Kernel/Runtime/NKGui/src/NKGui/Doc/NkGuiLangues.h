#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiLangues.h
// @Brief   LE mecanisme multilingue de la famille : une cle -> un texte, dans la
//          langue courante, a chaud.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  D'OU CA VIENT, ET CE QUI EST REPRIS
// =============================================================================
//  Origine : `Applications/NKCode/src/NKCode/Shell/NkI18n.h`, ecrit pour NKCode.
//  Repris ici le 27/09 sur decision de Rodolf : « vu que NKUIDesign doit aussi
//  utiliser le multilingue, il sera le premier a recevoir le fait de decentrer
//  donc ce systeme dans NKGui ».
//
//  Ce qui etait DEJA juste chez NKCode, et qu'on garde sans y toucher :
//    - l'indexation PAR CLE (`NkT("sb.main")`), jamais par le texte affiche ;
//    - la bascule A CHAUD, sans redemarrage ;
//    - la priorite SURCHARGE > table compilee > anglais > la cle elle-meme ;
//    - le repli sur la CLE plutot que sur une chaine vide -- un libelle manquant
//      affiche `sb.main`, ce qui est laid et se corrige ; une chaine vide
//      affiche un bouton sans texte, ce qui ne se remarque pas.
//
//  Mesure du 27/09 : 8 langues (fr, en, es, pt, de, it, ru, Ghɔmáláʼ), ~1 541
//  entrees et 807 appels dans NKCode. Ce n'est donc pas un mecanisme a eprouver :
//  c'est un mecanisme eprouve, qui etait au mauvais etage.
//
// =============================================================================
//  ⚠️ CE QUI RESTE DEHORS, ET LE GRAPHE DE DEPENDANCES L'IMPOSE
// =============================================================================
//  La version NKCode charge des fichiers `<code>.lang` depuis `data/lang/` et
//  `~/.nkcode/lang/`. Elle a besoin de `NKFileSystem` -- que NKGui N'A PAS
//  (`NKGui.jenga` : NKPlatform, NKCore, NKMemory, NKMath, NKThreading, NKLogger,
//  NKContainers, NKEvent, NKFont, NKImage).
//
//  Meme mur que pour les images, le meme jour, et la meme reponse :
//
//     ICI        le mecanisme -- langues, table(s), recherche, surcharges EN
//                MEMOIRE. Aucune lecture de fichier.
//     L'HOTE     lit les `.lang` et POUSSE les surcharges par `NkGuiSurcharger`.
//
//  Elargir la coupe du noyau pour lire un fichier de traduction aurait fait
//  payer NKFileSystem a tout ce qui affiche un bouton.
//
// =============================================================================
//  ⚠️ LA TABLE APPARTIENT A L'APPLICATION, PAS AU NOYAU
// =============================================================================
//  Les 1 541 entrees de NKCode sont les libelles de NKCode : `sb.main`,
//  `ai.send`... NKUIDesign aura les siens. Les coudre ensemble dans NKGui
//  donnerait un noyau qui grossit a chaque application, et des cles qui
//  finiraient par se marcher dessus.
//
//  Chaque application POSE donc sa table -- exactement comme elle pose ses
//  actions, ses zones, ses jetons, ses icones et ses ecouteurs. Cinq tables
//  posees par l'hote ; une sixieme ne demande rien de nouveau a apprendre.
//
//  📌 PLUSIEURS TABLES PEUVENT COEXISTER (le commun de la famille + celle de
//     l'application). La recherche les parcourt dans l'ordre de pose, la
//     PREMIERE qui repond gagne : une application peut donc redefinir une cle
//     commune sans toucher au commun.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKGUI_DOC_NKGUILANGUES_H__
#define __NKENTSEU_NKGUI_DOC_NKGUILANGUES_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nkgui {

		/// Les langues de la famille, dans l'ordre des colonnes d'une table.
		///
		/// ⚠️ EN AJOUT SEUL, ET STRICTEMENT. Chaque table declaree par une
		///    application porte ses colonnes DANS CET ORDRE : inserer une langue au
		///    milieu decalerait toutes les traductions de toutes les tables, et
		///    afficherait de l'espagnol la ou on attend du portugais -- sans une
		///    seule erreur de compilation.
		static const int32 kNkGuiLangues = 8;

		const char *NkGuiCodeLangue(int32 lang) noexcept;
		const char *const *NkGuiNomsLangues() noexcept;

		/// La langue courante (indice). Bascule A CHAUD : rien a redemarrer.
		int32 NkGuiLangue() noexcept;
		void NkGuiPoserLangue(int32 lang) noexcept;

		/// Une ligne de table : une cle, et ses traductions dans l'ordre des
		/// colonnes.
		struct NkGuiTraduction {
				const char *cle;
				const char *s[kNkGuiLangues];
		};

		/// Pose une table. Plusieurs peuvent coexister ; la premiere posee qui
		/// repond gagne (voir l'en-tete).
		///
		/// ⚠️ LA TABLE N'EST PAS COPIEE. Elle doit vivre aussi longtemps que
		///    l'interface -- en pratique un `static` de l'application. Une table
		///    locale a une fonction rendrait des pointeurs pendouillants au premier
		///    libelle demande apres son retour, et le texte affiche serait ce qui
		///    traine en memoire : parfois juste, ce qui est le pire des cas.
		bool NkGuiPoserTableLangues(const NkGuiTraduction *table, int32 nb) noexcept;
		void NkGuiOublierTablesLangues() noexcept;

		/// Une surcharge POUSSEE par l'hote. Remplace une entree de table pour une
		/// langue donnee.
		bool NkGuiSurcharger(const char *cle, int32 lang, const char *texte) noexcept;
		void NkGuiOublierSurcharges() noexcept;

		/// Analyse un tampon `cle=valeur`, une paire par ligne, et pose chaque
		/// paire comme surcharge de `lang`. Rend le nombre de paires retenues.
		///
		/// 🔴 ANALYSER N'EST PAS LIRE, ET C'ETAIT MA CONFUSION. J'avais conclu que
		///    NKGui ne pouvait pas porter les surcharges parce qu'il n'a pas
		///    `NKFileSystem`. Faux : ce qui lui est interdit, c'est d'OUVRIR un
		///    fichier -- pas d'analyser des octets. Rodolf, 27/09 : « s'il ne lit pas
		///    les fichiers il peut charger du texte avec son propre langage ».
		///
		///    Le partage juste est donc : **l'hote OUVRE, NKGui ANALYSE.** Et
		///    l'analyse -- la seule partie ou l'on peut se tromper -- est ecrite UNE
		///    fois pour toutes les applications, au lieu d'une fois par application.
		///
		/// Format : `cle=valeur` par ligne. Une ligne vide ou commencant par `#` ou
		/// `//` est ignoree. Les espaces autour de la cle et de la valeur sont
		/// retires ; ceux a l'INTERIEUR de la valeur sont gardes -- un libelle a le
		/// droit de contenir des espaces, et c'est meme le cas courant.
		uint32 NkGuiChargerSurcharges(const char *texte, uint32 taille, int32 lang) noexcept;

		/// 🔴 LE TEXTE D'UNE CLE, DANS LA LANGUE COURANTE.
		///
		/// Priorite : surcharge > table posee > anglais > **la cle elle-meme**.
		///
		/// ⚠️ LE REPLI EST LA CLE, PAS UNE CHAINE VIDE, et c'est un choix repris de
		///    NKCode. Un libelle manquant affiche `design.enregistrer` : c'est laid,
		///    ca se voit, et ca se corrige en trente secondes. Une chaine vide
		///    afficherait un bouton SANS TEXTE -- que personne ne signale, parce que
		///    ca ressemble a une icone.
		const char *NkGuiTexteLangue(const char *cle) noexcept;

		/// 🔴 LA FORME STABLE D'UN TEXTE : sa CLE s'il est traduit, lui-meme sinon.
		///
		/// C'est la parade que Rodolf a vue en demandant que le mecanisme vive ici :
		/// *« ça vit à la racine et ça pourrait trouver une parade aux identifiants
		/// qui changent à chaud. »*
		///
		/// Le defaut : six sites du monteur fabriquent l'identite d'interaction d'un
		/// widget a partir de son LIBELLE (`ctx.GetId(titre)`). Avec une bascule de
		/// langue A CHAUD, cette identite change SOUS LES DOIGTS de l'utilisateur --
		/// les sections repliees se rouvrent au moment meme ou il change de langue.
		///
		/// La parade tient parce que le mecanisme est DANS NKGui : lui seul sait
		/// qu'un libelle est une traduction, et peut donc rendre la partie qui NE
		/// BOUGE PAS -- la cle. `@t:design.enregistrer` rend toujours
		/// `design.enregistrer`, dans les huit langues.
		///
		/// ⚠️ ELLE NE REGLE PAS LE CAS DES LIBELLES NON TRADUITS, et il ne faut pas
		///    croire le contraire : un libelle ecrit en dur rend lui-meme, donc reste
		///    l'identite. Mais un libelle en dur NE CHANGE PAS -- le defaut est donc
		///    inerte pour lui. La parade couvre exactement les cas ou le defaut peut
		///    se produire.
		NkString NkGuiFormeStable(const NkString &texte) noexcept;

		/// Le releve, pour qu'une sonde puisse juger sans deviner.
		struct NkGuiLanguesRapport {
				int32 tables = 0;
				int32 entrees = 0;
				int32 surcharges = 0;
				uint32 demandes = 0;
				uint32 serviesParSurcharge = 0;
				uint32 serviesParTable = 0;
				uint32 repliAnglais = 0;
				/// 🔴 LE CHIFFRE QUI COMPTE. Une cle qui retombe sur elle-meme est un
				///    libelle qu'on a oublie de traduire ; sans ce compteur, il ne se
				///    voit qu'a l'ecran, et seulement par quelqu'un qui parle la
				///    langue.
				uint32 repliCle = 0;
				NkString derniereSansTraduction;
		};
		const NkGuiLanguesRapport &NkGuiLanguesReleve() noexcept;
		void NkGuiLanguesRemiseAZero() noexcept;

	} // namespace nkgui
} // namespace nkentseu

#endif // __NKENTSEU_NKGUI_DOC_NKGUILANGUES_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
