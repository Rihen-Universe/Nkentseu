// =============================================================================
// NkUnkenyComposants.h — les composants d'une scene 2D
//
// A QUOI SERT CE FICHIER
//   Il declare ce qu'une entite PEUT porter. Rien d'autre : pas de logique, pas
//   de dessin, pas de systeme. Ce sont des DONNEES, et elles doivent le rester —
//   c'est ce qui permet de les serialiser, de les inspecter et de les editer.
//
// ⚠️ LA REGLE QUI TIENT TOUT LE FICHIER
//   Un composant ne porte AUCUN type d'interface et AUCUN pointeur vers un
//   systeme. Test : *ce fichier compile-t-il sans NKGui et sans NKCanvas ?* Oui.
//   C'est ce qui a rendu possible le portage des panneaux de Nogee, et c'est ce
//   qui permettra a un editeur d'inspecter une scene sans la faire tourner.
//
// ⚠️ ET LA REGLE QUI EVITE LA DIVERGENCE
//   `NkCorps2D` ne stocke PAS la position : il stocke un `NkBodyId`. La position
//   d'un corps physique appartient au monde physique, une seule fois. Deux
//   copies — une dans le composant, une dans le solveur — divergent au premier
//   pas de simulation, et le defaut se presente comme « le sprite est a cote du
//   collisionneur ».
//   Le systeme de physique RECOPIE le resultat dans NkTransform2D apres chaque
//   pas ; c'est un sens unique, et il est ecrit.
//
// LE PLAN 2D
//   Unkeny travaille dans le plan XY. NKPhysics et NKCollision sont capables de
//   3D ; on y entre avec z = 0 et on n'expose que x, y et l'angle autour de Z.
//   Le pont fait la conversion en UN seul endroit (NkUnkenyPhysique).
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une donnee portee par une entite -> ici, en struct simple
//   - un comportement                  -> un systeme, jamais un composant
//   - un besoin propre a UN jeu        -> chez ce jeu ; NkWorld accepte
//                                         n'importe quel type
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		using math::NkVec2f;

		/// Position, rotation, echelle. C'est la VERITE d'affichage d'une entite.
		///
		/// ⚠️ Pas de matrice stockee : elle se recalcule au dessin. Une matrice
		/// en cache est une seconde source de verite, et il faut alors se rappeler
		/// de l'invalider — ce dont on ne se rappelle jamais.
		struct NkTransform2D {
				NkVec2f position{0.f, 0.f};
				float32 rotation = 0.f; ///< radians, sens trigonometrique
				NkVec2f echelle{1.f, 1.f};

				/// L'ordre est T * R * S : on tourne autour de l'ORIGINE de
				/// l'entite, pas autour du coin de l'ecran.
				NkVec2f VersMonde(const NkVec2f &local) const noexcept {
					const float32 c = math::NkCos(rotation);
					const float32 s = math::NkSin(rotation);
					const NkVec2f e(local.x * echelle.x, local.y * echelle.y);
					return NkVec2f(position.x + e.x * c - e.y * s, position.y + e.x * s + e.y * c);
				}
		};

		/// Un rectangle colore ou texture. `texId` a 0 = forme pleine, sans image.
		struct NkSprite2D {
				NkVec2f taille{1.f, 1.f}; ///< en unites de monde, avant echelle
				NkVec2f pivot{0.5f, 0.5f}; ///< 0,0 = coin haut-gauche ; 0.5,0.5 = centre
				uint32 couleur = 0xFFFFFFFFu;
				uint32 texId = 0;
				/// Region de l'atlas, en coordonnees normalisees. Tout l'atlas par defaut.
				NkVec2f uv0{0.f, 0.f};
				NkVec2f uv1{1.f, 1.f};
				int32 couche = 0; ///< ordre de dessin ; le plus grand passe devant
				bool visible = true;
		};

		/// Forme de collision, en donnees pures. Le pont la traduit en NkShape.
		/// (2026-10-01) AJOUTEES A LA FIN : le POLYGONE (convexe) et la CHAINE
		/// (un contour exact, ouvert ou ferme, pour le decor). Un fichier ecrit
		/// avant elles porte 0, 1 ou 2 et se relit tel quel.
		enum class NkForme2D : uint8 { NK_CERCLE = 0, NK_BOITE, NK_CAPSULE, NK_POLYGONE, NK_CHAINE };

		/// Les sommets qu'un collisionneur peut porter (polygone, chaine). C'est
		/// aussi la copie que NKPhysics garde d'un corps (NK_SOMMETS_2D_MAX).
		static constexpr uint32 NK_COLLISION_SOMMETS_MAX = 32u;
		/// ⚠️ UN POLYGONE CONVEXE N'A PAS PLUS DE 8 SOMMETS : le choc polygone /
		/// polygone de NKCollision (NkColClip, NkPoly2) n'en lit que 8. Au-dela,
		/// le neuvieme serait ignore EN SILENCE et la forme touchee ne serait
		/// plus celle qu'on voit. Les generateurs reduisent donc a 8 (enveloppe).
		static constexpr uint32 NK_POLYGONE_CONVEXE_MAX = 8u;

		struct NkCollisionneur2D {
				NkForme2D forme = NkForme2D::NK_BOITE;
				NkVec2f demiTaille{0.5f, 0.5f}; ///< boite : demi-extents ; capsule : x = demi-longueur
				float32 rayon = 0.5f;			///< cercle et capsule
				/// Par rapport au transform, dans le REPERE DE L'ENTITE (il tourne
				/// avec elle). Sans rotation, c'est aussi le repere du monde.
				NkVec2f decalage{0.f, 0.f};

				/// Couches et masque : deux entites n'entrent en collision que si
				/// chacune est dans le masque de l'autre. C'est ce qui permet aux
				/// balles du joueur d'ignorer le joueur sans code special.
				/// (2026-10-01) La MATRICE des calques de la scene (NkCalquesCollision2D)
				/// s'y ajoute : le masque donne au solveur est `masque` ET la ligne
				/// du calque. Par defaut elle laisse tout passer.
				uint32 couche = 0x1u;
				uint32 masque = 0xFFFFFFFFu;

				/// Un declencheur detecte sans repousser. Une zone de fin de
				/// niveau, un ramassage, un capteur.
				bool declencheur = false;

				// --- (2026-10-01) AJOUTES A LA FIN : polygone, chaine, rotation ---
				/// Radians, la rotation PROPRE du collisionneur dans le repere de
				/// l'entite (une boite penchee sur un sprite droit).
				float32 rotation = 0.f;
				/// Polygone : CONVEXE, au plus NK_POLYGONE_CONVEXE_MAX sommets.
				/// Chaine : au plus NK_COLLISION_SOMMETS_MAX (un de moins fermee).
				uint8 nbSommets = 0;
				/// Chaine FERMEE : le dernier sommet rejoint le premier (le contour
				/// d'une etoile, d'une colline). Ouverte : une ligne, une pente.
				bool boucle = false;
				/// Repere du COLLISIONNEUR : decalage puis rotation appliques.
				NkVec2f sommets[NK_COLLISION_SOMMETS_MAX] = {};
		};

		/// Un point du repere du collisionneur, en MONDE (rotation du transform et
		/// propre, decalage dans le repere de l'entite, echelle 1 -- l'editeur cuit
		/// l'echelle dans les dimensions, NkEditeurMettreAEchelle).
		inline NkVec2f NkPointCollision2D(const NkTransform2D &t, const NkCollisionneur2D &c, const NkVec2f &l) noexcept {
			const float32 ce = math::NkCos(t.rotation), se = math::NkSin(t.rotation);
			const float32 cc = math::NkCos(c.rotation), sc = math::NkSin(c.rotation);
			const float32 x = c.decalage.x + l.x * cc - l.y * sc;
			const float32 y = c.decalage.y + l.x * sc + l.y * cc;
			return NkVec2f(t.position.x + x * ce - y * se, t.position.y + x * se + y * ce);
		}

		/// Le nombre de sommets UTILISES d'un polygone ou d'une chaine, borne.
		inline uint32 NkNbSommetsCollision2D(const NkCollisionneur2D &c) noexcept {
			const uint32 cap = c.forme == NkForme2D::NK_POLYGONE ? NK_POLYGONE_CONVEXE_MAX : NK_COLLISION_SOMMETS_MAX;
			return c.nbSommets < cap ? c.nbSommets : cap;
		}

		/// Distance d'un point au segment [a, b].
		inline float32 NkDistanceSegment2D(const NkVec2f &p, const NkVec2f &a, const NkVec2f &b) noexcept {
			const float32 abx = b.x - a.x, aby = b.y - a.y;
			const float32 l2 = abx * abx + aby * aby;
			float32 t = l2 > 1.0e-12f ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / l2 : 0.f;
			t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
			const float32 dx = p.x - (a.x + abx * t), dy = p.y - (a.y + aby * t);
			return math::NkSqrt(dx * dx + dy * dy);
		}

		/// Distance SIGNEE d'un point a un contour de `n` points (negative dedans
		/// si `ferme` ; jamais sinon : une ligne n'a pas d'interieur). Le test
		/// d'interieur est celui du rayon (pair-impair) : juste pour un contour
		/// concave, comme une etoile.
		inline float32 NkDistanceContour2D(const NkVec2f *pts, uint32 n, bool ferme, const NkVec2f &p) noexcept {
			if (n == 0u) {
				return 1.0e9f;
			}
			if (n == 1u) {
				const float32 dx = p.x - pts[0].x, dy = p.y - pts[0].y;
				return math::NkSqrt(dx * dx + dy * dy);
			}
			float32 d = 1.0e9f;
			bool dedans = false;
			const uint32 nbSeg = ferme ? n : n - 1u;
			for (uint32 i = 0; i < nbSeg; ++i) {
				const NkVec2f &a = pts[i];
				const NkVec2f &b = pts[(i + 1u) % n];
				const float32 di = NkDistanceSegment2D(p, a, b);
				d = di < d ? di : d;
				if (ferme && ((a.y > p.y) != (b.y > p.y)) && (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)) {
					dedans = !dedans;
				}
			}
			return dedans ? -d : d;
		}

		/// Distance SIGNEE d'un point (monde) a la forme d'un collisionneur :
		/// negative dedans. Rotation et echelle 1 du transform prises en compte.
		/// C'est le test de « ce qui est sous le curseur » pour les formes sans
		/// sprite (decor, balles, obstacles) — partage par la demo et l'editeur.
		/// (2026-10-01) Le decalage est lu dans le repere de l'entite, comme le
		/// dessin et le solveur : sans rotation, rien ne change.
		inline float32 NkDistanceForme2D(const NkTransform2D &t, const NkCollisionneur2D &c, const NkVec2f &p) noexcept {
			const float32 co = math::NkCos(-t.rotation);
			const float32 si = math::NkSin(-t.rotation);
			const float32 gx = p.x - t.position.x;
			const float32 gy = p.y - t.position.y;
			// Repere de l'entite, puis celui du collisionneur (decalage, rotation).
			const float32 ex = gx * co - gy * si - c.decalage.x;
			const float32 ey = gx * si + gy * co - c.decalage.y;
			const float32 cr = math::NkCos(-c.rotation);
			const float32 sr = math::NkSin(-c.rotation);
			const float32 lx = ex * cr - ey * sr;
			const float32 ly = ex * sr + ey * cr;
			switch (c.forme) {
				case NkForme2D::NK_CERCLE:
					return math::NkSqrt(lx * lx + ly * ly) - c.rayon;
				case NkForme2D::NK_CAPSULE: {
					const float32 x = math::NkClamp(lx, -c.demiTaille.x, c.demiTaille.x);
					return math::NkSqrt((lx - x) * (lx - x) + ly * ly) - c.rayon;
				}
				case NkForme2D::NK_POLYGONE:
					return NkDistanceContour2D(c.sommets, NkNbSommetsCollision2D(c), true, NkVec2f(lx, ly));
				case NkForme2D::NK_CHAINE:
					return NkDistanceContour2D(c.sommets, NkNbSommetsCollision2D(c), c.boucle, NkVec2f(lx, ly));
				default: {
					const float32 qx = math::NkAbs(lx) - c.demiTaille.x;
					const float32 qy = math::NkAbs(ly) - c.demiTaille.y;
					const float32 ex2 = qx > 0.f ? qx : 0.f;
					const float32 ey2 = qy > 0.f ? qy : 0.f;
					const float32 dedans = qx > qy ? qx : qy;
					return math::NkSqrt(ex2 * ex2 + ey2 * ey2) + (dedans < 0.f ? dedans : 0.f);
				}
			}
		}

		// =====================================================================
		// LES CALQUES DE COLLISION (2026-10-01) — « qui touche qui »
		//
		// Demande de Rihen : une MATRICE, comme les reglages de physique d'Unity
		// et les canaux d'UE5. Seize calques nommes ; le bit j de `matrice[i]`
		// dit que le calque i touche le calque j (Poser la garde SYMETRIQUE).
		// Un collisionneur appartient aux calques de ses bits `couche` 0 a 15 ;
		// le solveur recoit `masque & NkMasqueCalques2D(...)`.
		// ⚠️ PAR DEFAUT TOUT TOUCHE TOUT : une scene d'avant se simule a
		//    l'identique, et s'ecrit a l'octet pres (NkCalquesParDefaut).
		// =====================================================================
		static constexpr uint32 NK_CALQUES_COLLISION = 16u;

		struct NkCalquesCollision2D {
				char noms[NK_CALQUES_COLLISION][24] = {};
				uint16 matrice[NK_CALQUES_COLLISION] = {0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
														0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
														0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu};

				/// Le calque i touche-t-il le calque j ?
				bool Touche(uint32 i, uint32 j) const noexcept {
					return i < NK_CALQUES_COLLISION && j < NK_CALQUES_COLLISION && (matrice[i] & (1u << j)) != 0u;
				}
				/// Pose la case (i, j) ET (j, i) : la matrice reste symetrique.
				void Poser(uint32 i, uint32 j, bool oui) noexcept {
					if (i >= NK_CALQUES_COLLISION || j >= NK_CALQUES_COLLISION) {
						return;
					}
					if (oui) {
						matrice[i] = static_cast<uint16>(matrice[i] | (1u << j));
						matrice[j] = static_cast<uint16>(matrice[j] | (1u << i));
					} else {
						matrice[i] = static_cast<uint16>(matrice[i] & ~(1u << j));
						matrice[j] = static_cast<uint16>(matrice[j] & ~(1u << i));
					}
				}
		};

		/// Le masque des calques que TOUCHE un collisionneur de `couche` : l'union
		/// des lignes de ses calques 0 a 15. Les bits 16 a 31 (hors matrice) ne
		/// sont jamais retires, et une couche sans aucun bit bas n'est pas
		/// concernee par la matrice (tout passe, comme avant).
		inline uint32 NkMasqueCalques2D(const NkCalquesCollision2D &k, uint32 couche) noexcept {
			uint32 ligne = 0u;
			for (uint32 i = 0; i < NK_CALQUES_COLLISION; ++i) {
				if ((couche & (1u << i)) != 0u) {
					ligne |= k.matrice[i];
				}
			}
			if ((couche & 0xFFFFu) == 0u) {
				ligne = 0xFFFFu;
			}
			return 0xFFFF0000u | ligne;
		}

		/// Vrai si les calques `k` sont le reglage par defaut (rien n'est ecrit
		/// au fichier : une scene d'avant ressort a l'octet pres).
		inline bool NkCalquesParDefaut(const NkCalquesCollision2D &k) noexcept {
			for (uint32 i = 0; i < NK_CALQUES_COLLISION; ++i) {
				if (k.matrice[i] != 0xFFFFu || k.noms[i][0] != '\0') {
					return false;
				}
			}
			return true;
		}

		enum class NkTypeCorps : uint8 {
			NK_STATIQUE = 0, ///< ne bouge jamais : murs, sol
			NK_CINEMATIQUE,	 ///< bouge, mais rien ne le pousse : plateforme mobile
			NK_DYNAMIQUE	 ///< subit forces et chocs
		};

		/// ⚠️ NE STOCKE PAS LA POSITION. Voir l'en-tete de fichier : elle
		/// appartient au monde physique, une seule fois.
		struct NkCorps2D {
				NkTypeCorps type = NkTypeCorps::NK_DYNAMIQUE;
				float32 masse = 1.f;
				float32 amortissementLineaire = 0.f;
				float32 amortissementAngulaire = 0.05f;
				float32 echelleGravite = 1.f;
				bool rotationBloquee = false; ///< un personnage de plateforme ne bascule pas
				/// Materiau de contact (2026-09-29). Avant, AjouterCorps ne les
				/// transmettait PAS : tout corps frottait a 0,4 et ne rebondissait
				/// jamais, quoi que demande le jeu.
				float32 friction = 0.5f;
				float32 rebond = 0.f;

				/// Rempli par la scene a la creation. 0 = pas encore enregistre.
				/// ⚠️ Ne pas l'ecrire a la main : c'est le lien vers le solveur.
				uint32 corpsId = 0;
		};

		/// Un CORPS MOU, un FLUIDE, du sable, un cristal, un tissu, une corde : tout
		/// ce que simule `physics::NkParticules2D` (2026-09-29).
		///
		/// ⚠️ MEME REGLE QUE NkCorps2D : il ne stocke PAS la matiere. Ses
		/// particules, ses liens et ses parametres appartiennent au monde de
		/// particules, une seule fois ; le composant n'en tient que l'IDENTIFIANT
		/// stable (`NkCorpsP2D::id`). La scene recopie le centre du corps dans
		/// NkTransform2D apres chaque pas -- sens unique, comme pour les rigides.
		struct NkCorpsMou2D {
				uint32 corpsId = 0; ///< 0 = pas encore enregistre
				uint32 couleur = 0xFFFFFFFFu; ///< RGBA, pour le rendu
				bool visible = true;
		};

		/// Un nom lisible. Sert a l'editeur, aux journaux et au debogage — pas au
		/// jeu : chercher une entite par son nom a chaque trame est un piege de
		/// performance ET de renommage.
		struct NkEtiquette {
				char nom[32] = {};
		};

		/// Vitesse imposee a la main, pour ce qui n'a pas besoin de physique.
		/// Un jeu de plateau, un menu qui glisse, un decor qui defile.
		struct NkVitesse2D {
				NkVec2f lineaire{0.f, 0.f};
				float32 angulaire = 0.f;
		};

		// =====================================================================
		// ECLAIRAGE 2D ET EFFETS (2026-09-30) — FACULTATIFS, ETEINTS PAR DEFAUT
		//
		// Demande de Rihen : « on doit aussi pouvoir ajouter l'eclairage mais
		// facultatif en fonction du type de jeu, le feu aussi ». Un jeu de dames
		// n'a que faire d'une nuit ; un jeu de plateforme dans une grotte en vit.
		// D'ou la regle : une scene SANS lumiere se dessine exactement comme
		// avant (NkEclairage2D::actif vaut faux), et une entite sans ces
		// composants ne paie rien. Le dessin est dans Rendu/NkUnkenyEclairage.h,
		// la simulation des particules dans Effets/NkUnkenyEffets.h.
		// =====================================================================

		/// Les trois natures d'une lumiere 2D.
		enum class NkTypeLumiere2D : uint8 {
			NK_PONCTUELLE = 0, ///< lampe, torche : un disque de rayon `portee`
			NK_SPOT,		   ///< projecteur : le disque restreint a un cone
			NK_DIRECTIONNELLE  ///< soleil, lune : partout, venue d'une direction
		};

		/// Une lumiere portee par une entite. Donnees pures, comme le reste.
		struct NkLumiere2D {
				NkTypeLumiere2D type = NkTypeLumiere2D::NK_PONCTUELLE;
				uint32 couleur = 0xFFE2B8FFu; ///< RGBA ; l'alpha n'est pas lu
				float32 intensite = 1.f;	  ///< multiplie la couleur ; 0 = eteinte
				/// En metres. Ponctuelle et spot : au-dela, la lumiere ne donne
				/// RIEN (la decroissance s'annule exactement au bord, voir
				/// `attenuation`). Directionnelle : la longueur maximale des ombres.
				float32 portee = 5.f;
				/// Exposant de (1 - d / portee) : 1 = lineaire, 2 = douce. C'est la
				/// loi du shader 2D de NKRenderer (render2d.frag.nksl), en exposant
				/// libre : un bord qui tombe a zero, et non une queue infinie en
				/// 1/d² qu'il faudrait couper au hasard.
				float32 attenuation = 2.f;
				/// Radians, sens OU VA la lumiere (spot, directionnelle), dans le
				/// repere de l'entite : il tourne avec son transform.
				float32 direction = -1.5707963f;
				float32 ouverture = 0.6f; ///< radians, demi-angle du cone (spot)
				float32 douceur = 0.25f;  ///< part du bord du cone en fondu (0 = bord net)
				NkVec2f decalage{0.f, 0.f}; ///< par rapport au transform
				/// Eclat ADDITIF autour de la source (0..1) : ce que le mode
				/// multiplie ne peut pas donner, une zone PLUS claire que la
				/// couleur de l'objet eclaire.
				float32 halo = 0.f;
				bool ombres = true; ///< porte des ombres, si la scene les permet
				bool actif = true;
		};

		/// Comment la carte de lumiere est composee sur l'image.
		enum class NkModeEclairage2D : uint8 {
			/// L'image est MULTIPLIEE par la carte (ambiante + lumieres) : ce qui
			/// est hors de toute lumiere prend la teinte ambiante, exactement.
			NK_MULTIPLIE = 0,
			/// Le repli, pour un dorsal qui ne saurait pas multiplier : un VOILE
			/// sombre en melange alpha (exact pour une lumiere blanche) et des
			/// halos additifs pour la couleur. Jamais un ecran noir.
			NK_VOILE
		};

		/// Les reglages d'eclairage d'une SCENE (l'onglet Monde de l'editeur).
		///
		/// ⚠️ ETEINT PAR DEFAUT, ET C'EST LA REGLE DU CHANTIER : `actif` a faux,
		/// le dessin n'emet pas un sommet de plus qu'avant — une scene ancienne se
		/// dessine au pixel pres comme hier, et se reecrit a l'octet pres.
		struct NkEclairage2D {
				bool actif = false;
				/// La lumiere de ce qui n'est eclaire par rien. Une NUIT par defaut :
				/// on n'allume l'eclairage que pour avoir de l'ombre, et une
				/// ambiante blanche n'en laisserait voir aucune.
				uint32 ambiante = 0x2A3148FFu;
				bool ombres = true; ///< les ombres de TOUTES les lumieres (faux : aucune)
				/// Les collisionneurs dont `couche & masqueOcculteurs` est non nul
				/// portent ombre ; les declencheurs, jamais.
				uint32 masqueOcculteurs = 0xFFFFFFFFu;
				NkModeEclairage2D mode = NkModeEclairage2D::NK_MULTIPLIE;
				/// Taille de la maille de la carte, en PIXELS d'ecran (4 a 16).
				/// La maille est affinee jusqu'a 2 px la ou une ombre passe.
				float32 maille = 8.f;
		};

		/// Vrai si `e` est le reglage par defaut : la sauvegarde ne l'ecrit alors
		/// pas, et un fichier ancien ressort a l'octet pres.
		inline bool NkEclairageParDefaut(const NkEclairage2D &e) noexcept {
			const NkEclairage2D d;
			return e.actif == d.actif && e.ambiante == d.ambiante && e.ombres == d.ombres &&
				   e.masqueOcculteurs == d.masqueOcculteurs && e.mode == d.mode && e.maille == d.maille;
		}

		/// D'ou part un emetteur (informatif : les parametres font foi).
		enum class NkPresetEffet2D : uint8 {
			NK_PERSONNALISE = 0,
			NK_FEU,
			NK_FUMEE,
			NK_ETINCELLES,
			NK_PLUIE,
			NK_NEIGE,
			NK_EXPLOSION,
			NK_COUNT
		};

		/// L'allure d'une particule a l'ecran.
		enum class NkFormeParticule2D : uint8 {
			NK_DOUCE = 0, ///< disque au bord fondu : flamme, fumee, neige
			NK_TRAIT,	  ///< trait le long de la vitesse : etincelle, goutte de pluie
			NK_PLEINE	  ///< disque net : debris, confettis
		};

		/// Ou naissent les particules, dans le repere de l'entite.
		enum class NkZoneEmission2D : uint8 {
			NK_POINT = 0,
			NK_DISQUE, ///< rayon `rayonZone`
			NK_LIGNE   ///< segment horizontal de `largeurZone` (pluie, neige)
		};

		/// Un EMETTEUR de particules VISUELLES : feu, fumee, etincelles, pluie,
		/// neige, explosion.
		///
		/// ⚠️ A NE PAS CONFONDRE AVEC NkCorpsMou2D. Les particules de NKPhysics
		/// (NkParticules2D) sont de la MATIERE : elles pesent, poussent et se
		/// touchent. Celles-ci ne touchent rien et ne changent rien a la
		/// simulation (temoin f6 de la feuille de route) : ce sont des images.
		/// Leur etat (positions, ages) vit dans NkEffets2D, pas ici : ce
		/// composant ne porte que la RECETTE.
		struct NkEmetteur2D {
				NkPresetEffet2D preset = NkPresetEffet2D::NK_PERSONNALISE;
				bool actif = true;
				bool boucle = true;
				float32 duree = 1.f;  ///< s : sans boucle, combien de temps il emet
				uint32 rafale = 0u;	  ///< particules lachees d'un coup au depart
				float32 debit = 20.f; ///< particules par seconde
				float32 vieMin = 0.6f;
				float32 vieMax = 1.f;
				float32 vitesseMin = 0.5f; ///< m/s
				float32 vitesseMax = 1.5f;
				float32 direction = 1.5707963f; ///< radians, repere de l'entite (vers le haut)
				float32 dispersion = 0.3f;		///< radians, demi-angle autour de `direction`
				/// m/s², PROPRE aux particules : une flamme monte (y > 0), une
				/// etincelle retombe (y < 0). Pas la gravite de la scene : la fumee
				/// n'a aucune raison d'obeir a celle des caisses.
				NkVec2f gravite{0.f, 0.f};
				float32 frein = 0.f;		///< 1/s : la resistance de l'air
				float32 tailleDebut = 0.2f; ///< m, diametre a la naissance
				float32 tailleFin = 0.05f;	///< m, diametre a la mort
				/// La couleur sur la vie : debut -> milieu (a mi-vie) -> fin, RGBA.
				uint32 couleurDebut = 0xFFFFFFFFu;
				uint32 couleurMilieu = 0xFFFFFF80u;
				uint32 couleurFin = 0xFFFFFF00u;
				/// ADDITIF = la particule EMET (feu, etincelles) : elle s'ajoute a
				/// l'image et la nuit ne l'assombrit pas. Sinon, melange alpha : la
				/// fumee cache ce qui est derriere, et la nuit l'assombrit.
				bool additif = false;
				NkFormeParticule2D forme = NkFormeParticule2D::NK_DOUCE;
				NkZoneEmission2D zone = NkZoneEmission2D::NK_POINT;
				float32 rayonZone = 0.f;
				float32 largeurZone = 0.f;
				NkVec2f decalage{0.f, 0.f};
				/// La GRAINE : meme graine, memes particules (les bancs en vivent).
				uint32 graine = 1u;
				uint32 maxParticules = 256u; ///< plafond de CET emetteur
				// --- La lumiere liee (un feu eclaire) ------------------------------
				// Lue seulement si l'eclairage de la scene est actif.
				bool eclaire = false;
				uint32 couleurLumiere = 0xFF9A48FFu;
				float32 intensiteLumiere = 1.f;
				float32 porteeLumiere = 4.f;
				float32 scintillement = 0.f; ///< 0..1 : amplitude du vacillement
				bool ombresLumiere = true;
		};

	} // namespace unkeny
} // namespace nkentseu
