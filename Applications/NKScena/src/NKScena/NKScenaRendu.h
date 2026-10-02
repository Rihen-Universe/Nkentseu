#pragma once
// -----------------------------------------------------------------------------
// @File    NKScenaRendu.h
// @Brief   RENDRE LA SEQUENCE EN IMAGES, SANS FENETRE : un peripherique graphique
//          sans surface, un NkRenderer dont la sortie va dans une cible hors
//          ecran (NkOffscreenTarget), le chemin de rendu de Noge (NkTransformSystem
//          puis NkRenderSystem, ceux que NkEngineLayer execute), la camera du
//          plan actif, et NkImageSequenceWriter pour les PNG numerotes.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// QUI S'EN SERT : le banc (`NKScena --selftest`, critere r1) et la ligne de
// commande `NKScena --rendre=SEQUENCE.nkseq [--sortie=DOSSIER]`. Le bouton
// « Rendre » de l'editeur, lui, rend IMAGE PAR IMAGE dans la vue de
// l'application (NKScenaApp.cpp) : la meme sequence, le meme ecrivain, la
// progression au Journal.
//
// ⚠️ Sur OpenGL seul, NKRHI cree lui-meme une fenetre CACHEE de 1x1 pour son
//    contexte (NkOpenglDevice) : DirectX 11 est donc essaye d'abord sous
//    Windows, OpenGL ensuite.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nkscena {

		class NkScenaModele;

		struct NkScenaRenduDesc {
				uint32 largeur = 320;
				uint32 hauteur = 180;
				NkString dossier = "Build/NKScena/Rendu";
				NkString prefixe = "image";
				/// Le nombre d'images ; <= 0 : celles de la plage de lecture (ips x duree).
				int32 images = 0;
				/// CONTRE-EPREUVE : le temps reste a l'instant du debut (toutes les
				/// images doivent alors etre IDENTIQUES).
				bool figer = false;
				/// Ecrire chaque image sur le disque (faux : rendre et relire seulement).
				bool ecrire = true;
		};

		struct NkScenaRenduResultat {
				int32 rendues = 0;	   ///< images rendues et relues
				int32 ecrites = 0;	   ///< fichiers PNG ecrits
				NkString api;		   ///< le dorsal obtenu
				NkString raison;	   ///< pourquoi ca s'est arrete (vide = au bout)
				uint64 premiere = 0;   ///< empreinte (FNV-1a) des pixels de la premiere image
				uint64 derniere = 0;   ///< ... et de la derniere
				uint32 pixelsFond = 0; ///< pixels de la derniere image egaux au fond (rien n'y est dessine)
				uint32 pixels = 0;
				/// Luminance moyenne (0..255) du HAUT et du BAS de la premiere image (un
				/// huitieme de la hauteur chacun) : le ciel est en haut, le sol en bas
				/// -- le temoin que l'image n'est pas a l'envers.
				float32 lumHaut = 0.f;
				float32 lumBas = 0.f;
		};

		/// Rend la sequence du modele. 1 = rendue, 0 = echec (raison), -1 = AUCUN
		/// peripherique graphique sur cette machine (ni vert ni rouge pour un banc).
		int32 NkScenaRendreSansFenetre(NkScenaModele &m, const NkScenaRenduDesc &d, NkScenaRenduResultat &r);


	} // namespace nkscena
} // namespace nkentseu
