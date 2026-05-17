#pragma once
// =============================================================================
// Fichier : NkRasterizer.h
// Description : Rasteriseur logiciel 2D pour le dessin de primitives graphiques
// =============================================================================
// Primitives disponibles :
//   - Lignes        : DrawLine (algorithme de Bresenham), avec épaisseur variable
//   - Cercles       : DrawCircle (contour), FillCircle (rempli)
//   - Rectangles    : DrawRect (contour), FillRect (rempli)
//   - Triangles     : DrawTriangle (contour), FillTriangle (rempli par scanline)
//
// Effets spéciaux :
//   - BlendPixel    : Fusion alpha standard (Porter-Duff SRC_OVER)
//   - AdditivePixel : Fusion additive pour effets de lueur/éclairage
//   - DrawGlow      : Effet de lueur radiale avec atténuation alpha quadratique
//
// Texte :
//   - Police bitmap 3×5 pixels pour chiffres et ASCII de base
//   - DrawText / DrawNumber / DrawTextCentered avec mise à l'échelle
//
// Toutes les opérations de dessin ciblent NkRenderer::BackBuffer().
// Coordonnées entières par défaut ; les surcharges float tronquent vers int.
// =============================================================================

#include "NkFramebuffer.h"
#include "NKMath/NkMat.h"
#include <cmath>
#include <cstring>
#include <cstdio>

namespace nkentseu {
    // =============================================================================
    // Espace de noms : ColorUtils
    // Description : Fonctions utilitaires pour la manipulation et la fusion de couleurs
    // =============================================================================
    namespace ColorUtils {
        /// Fusionne la couleur src par-dessus dst avec transparence alpha (Porter-Duff SRC_OVER)
        /// Formule : résultat = src * alpha + dst * (1 - alpha)
        inline math::NkColor Blend(const math::NkColor& dst, const math::NkColor& src) noexcept {
            // Cas optimisés : opacité totale ou transparence totale
            if (src.a == 255) {
                return src;
            }

            if (src.a == 0) {
                return dst;
            }

            // Calcul des facteurs de pondération
            uint32_t a = src.a;
            uint32_t ia = 255u - a;

            // Application de la formule de fusion pour chaque composante RGB
            // Utilisation du décalage >> 8 pour diviser par 256 (approximation de /255)
            return math::NkColor(
                static_cast<uint8>((a * src.r + ia * dst.r) >> 8),
                static_cast<uint8>((a * src.g + ia * dst.g) >> 8),
                static_cast<uint8>((a * src.b + ia * dst.b) >> 8),
                static_cast<uint8>(src.a + ((ia * dst.a) >> 8))
            );
        }

        /// Multiplie le canal alpha d'une couleur par un facteur de transparence [0..1]
        /// Utile pour atténuer progressivement une couleur sans modifier ses composantes RGB
        inline math::NkColor WithAlpha(const math::NkColor& c, float t) noexcept {
            // Conversion du facteur float en valeur alpha 8 bits
            uint8 a = static_cast<uint8>(static_cast<float>(c.a) * t);
            return math::NkColor(c.r, c.g, c.b, a);
        }

        /// Fusion additive : additionne les composantes RGB (sans saturation par défaut)
        /// Idéal pour les effets de lueur, particules, ou éclairages
        /// Les valeurs dépassant 255 sont clampées pour éviter le débordement
        inline math::NkColor Additive(const math::NkColor& dst, const math::NkColor& src) noexcept {
            return math::NkColor(
                static_cast<uint8>(math::NkMin(255u, static_cast<uint32_t>(dst.r) + src.r)),
                static_cast<uint8>(math::NkMin(255u, static_cast<uint32_t>(dst.g) + src.g)),
                static_cast<uint8>(math::NkMin(255u, static_cast<uint32_t>(dst.b) + src.b)),
                255u // Alpha toujours à 100% pour la fusion additive
            );
        }
    } // namespace ColorUtils

    // =============================================================================
    // Classe : NkRasterizer
    // Description : Moteur de dessin 2D logiciel avec primitives vectorielles et texte
    // =============================================================================
    class NkRasterizer {
        public:
            // Constructeur : lie le rasterizer à un renderer existant
            explicit NkRasterizer(NkRenderer& renderer) noexcept : mRenderer(renderer) {
            }

            // Retourne une référence vers le renderer associé
            NkRenderer& GetRenderer() noexcept {
                return mRenderer;
            }

            // ── Dessin de pixel et modes de fusion ─────────────────────────────────
            // Dessine un pixel opaque à la position spécifiée
            inline void DrawPixel(int x, int y, const math::NkColor& c) noexcept {
                mRenderer.SetPixel(x, y, c);
            }

            // Dessine un pixel avec fusion alpha (transparence respectée)
            inline void BlendPixel(int x, int y, const math::NkColor& c) noexcept {
                // Optimisation : ignorer les pixels totalement transparents
                if (c.a == 0) {
                    return;
                }

                // Récupérer la couleur actuelle du pixel de destination
                math::NkColor dst = mRenderer.GetPixel(x, y);

                // Appliquer la fusion alpha et écrire le résultat
                mRenderer.SetPixel(x, y, ColorUtils::Blend(dst, c));
            }

            // Dessine un pixel avec fusion additive (pour effets lumineux)
            inline void AdditivePixel(int x, int y, const math::NkColor& c) noexcept {
                // Récupérer la couleur actuelle du pixel de destination
                math::NkColor dst = mRenderer.GetPixel(x, y);

                // Appliquer la fusion additive et écrire le résultat
                mRenderer.SetPixel(x, y, ColorUtils::Additive(dst, c));
            }

            // ── Lignes : algorithme de Bresenham ───────────────────────────────────
            // Trace une ligne entre deux points en coordonnées entières
            void DrawLine(int x0, int y0, int x1, int y1, const math::NkColor& c) noexcept {
                // Calcul des différences absolues en X et Y
                int dx = std::abs(x1 - x0);
                int dy = std::abs(y1 - y0);

                // Détermination du sens de parcours pour chaque axe
                int sx = (x0 < x1) ? 1 : -1;
                int sy = (y0 < y1) ? 1 : -1;

                // Initialisation de la variable d'erreur de l'algorithme
                int err = dx - dy;

                // Boucle principale de tracé jusqu'au point d'arrivée
                for (;;) {
                    // Dessiner le pixel courant
                    DrawPixel(x0, y0, c);

                    // Condition d'arrêt : point final atteint
                    if (x0 == x1 && y0 == y1) {
                        break;
                    }

                    // Mise à jour de l'erreur et progression sur les axes
                    int e2 = err * 2;

                    if (e2 > -dy) {
                        err -= dy;
                        x0 += sx;
                    }

                    if (e2 < dx) {
                        err += dx;
                        y0 += sy;
                    }
                }
            }

            /// Trace une ligne épaisse en dessinant plusieurs lignes parallèles décalées
            /// Paramètre thickness : épaisseur en pixels (doit être >= 1)
            void DrawLine(float x0, float y0, float x1, float y1, const math::NkColor& c, int thickness = 1) noexcept {
                // Cas simple : épaisseur de 1 pixel, utiliser la version entière
                if (thickness <= 1) {
                    DrawLine(
                        static_cast<int>(x0),
                        static_cast<int>(y0),
                        static_cast<int>(x1),
                        static_cast<int>(y1),
                        c
                    );
                    return;
                }

                // Calcul du vecteur directeur de la ligne
                float dx = x1 - x0;
                float dy = y1 - y0;

                // Calcul de la longueur euclidienne de la ligne
                float len = math::NkSqrt(dx * dx + dy * dy);

                // Protection contre les lignes de longueur nulle
                if (len < 0.001f) {
                    return;
                }

                // Calcul de la normale unitaire perpendiculaire à la ligne
                float nx = -dy / len;
                float ny = dx / len;

                // Décalage maximal de part et d'autre du centre de la ligne
                int half = thickness / 2;

                // Tracer plusieurs lignes parallèles pour simuler l'épaisseur
                for (int t = -half; t <= half; ++t) {
                    DrawLine(
                        static_cast<int>(x0 + nx * t),
                        static_cast<int>(y0 + ny * t),
                        static_cast<int>(x1 + nx * t),
                        static_cast<int>(y1 + ny * t),
                        c
                    );
                }
            }

            // ── Cercles : algorithme du point milieu (symétrie octante) ────────────
            // Trace le contour d'un cercle en utilisant la symétrie 8-directionnelle
            void DrawCircle(int cx, int cy, int r, const math::NkColor& c) noexcept {
                // Initialisation des variables de l'algorithme
                int x = r;
                int y = 0;
                int err = 0;

                // Parcours du premier octant jusqu'à la diagonale
                while (x >= y) {
                    // Tracer les 8 points symétriques du cercle
                    DrawPixel(cx + x, cy + y, c);
                    DrawPixel(cx + y, cy + x, c);
                    DrawPixel(cx - y, cy + x, c);
                    DrawPixel(cx - x, cy + y, c);
                    DrawPixel(cx - x, cy - y, c);
                    DrawPixel(cx - y, cy - x, c);
                    DrawPixel(cx + y, cy - x, c);
                    DrawPixel(cx + x, cy - y, c);

                    // Incrémenter Y et mettre à jour l'erreur de décision
                    ++y;

                    if (err <= 0) {
                        err += 2 * y + 1;
                    }
                    else {
                        --x;
                        err += 2 * (y - x) + 1;
                    }
                }
            }

            // Remplit un cercle en traçant des lignes horizontales pour chaque scanline
            void FillCircle(int cx, int cy, int r, const math::NkColor& c) noexcept {
                // Initialisation des variables de l'algorithme
                int x = r;
                int y = 0;
                int err = 0;

                // Parcours du premier octant jusqu'à la diagonale
                while (x >= y) {
                    // Remplir les 4 paires de scanlines symétriques
                    FillRow(cx - x, cx + x, cy + y, c);
                    FillRow(cx - x, cx + x, cy - y, c);
                    FillRow(cx - y, cx + y, cy + x, c);
                    FillRow(cx - y, cx + y, cy - x, c);

                    // Incrémenter Y et mettre à jour l'erreur de décision
                    ++y;

                    if (err <= 0) {
                        err += 2 * y + 1;
                    }
                    else {
                        --x;
                        err += 2 * (y - x) + 1;
                    }
                }
            }

            // ── Rectangles ─────────────────────────────────────────────────────────
            // Trace le contour d'un rectangle en dessinant 4 lignes
            void DrawRect(int x, int y, int w, int h, const math::NkColor& c) noexcept {
                // Tracer les 4 côtés du rectangle (attention aux coordonnées inclusives)
                DrawLine(x, y, x + w - 1, y, c);             // Haut
                DrawLine(x + w - 1, y, x + w - 1, y + h - 1, c); // Droite
                DrawLine(x + w - 1, y + h - 1, x, y + h - 1, c); // Bas
                DrawLine(x, y + h - 1, x, y, c);             // Gauche
            }

            // Remplit un rectangle avec clipping automatique aux bords de l'écran
            void FillRect(int x, int y, int w, int h, const math::NkColor& c) noexcept {
                // Calcul des bornes avec clipping pour éviter les accès hors mémoire
                int x1 = math::NkMax(x, 0);
                int y1 = math::NkMax(y, 0);
                int x2 = math::NkMin(x + w, static_cast<int>(mRenderer.Width()));
                int y2 = math::NkMin(y + h, static_cast<int>(mRenderer.Height()));

                // Remplir ligne par ligne entre les bornes valides
                for (int row = y1; row < y2; ++row) {
                    FillRow(x1, x2 - 1, row, c);
                }
            }

            // Surcharge float pour DrawRect : conversion implicite vers entier
            void DrawRect(float x, float y, float w, float h, const math::NkColor& c) noexcept {
                DrawRect(
                    static_cast<int>(x),
                    static_cast<int>(y),
                    static_cast<int>(w),
                    static_cast<int>(h),
                    c
                );
            }

            // Surcharge float pour FillRect : conversion implicite vers entier
            void FillRect(float x, float y, float w, float h, const math::NkColor& c) noexcept {
                FillRect(
                    static_cast<int>(x),
                    static_cast<int>(y),
                    static_cast<int>(w),
                    static_cast<int>(h),
                    c
                );
            }

            // ── Triangles ──────────────────────────────────────────────────────────
            // Trace le contour d'un triangle en dessinant 3 lignes
            void DrawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, const math::NkColor& c) noexcept {
                DrawLine(x0, y0, x1, y1, c);
                DrawLine(x1, y1, x2, y2, c);
                DrawLine(x2, y2, x0, y0, c);
            }

            /// Remplit un triangle par balayage horizontal (scanline filling)
            /// Algorithme : tri des sommets par Y, interpolation linéaire des bords
            void FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, const math::NkColor& c) noexcept {
                // Étape 1 : Trier les sommets par ordonnée croissante (y0 <= y1 <= y2)
                if (y0 > y1) {
                    std::swap(x0, x1);
                    std::swap(y0, y1);
                }

                if (y0 > y2) {
                    std::swap(x0, x2);
                    std::swap(y0, y2);
                }

                if (y1 > y2) {
                    std::swap(x1, x2);
                    std::swap(y1, y2);
                }

                // Calcul de la hauteur totale du triangle
                int totalH = y2 - y0;

                // Triangle dégénéré (hauteur nulle) : rien à dessiner
                if (totalH == 0) {
                    return;
                }

                // ── Partie supérieure du triangle [y0, y1] ───────────────────────
                int segH = y1 - y0;

                for (int y = y0; y <= y1; ++y) {
                    // Interpolation du facteur alpha sur la hauteur totale
                    float alpha = static_cast<float>(y - y0) / totalH;

                    // Interpolation du facteur beta sur le segment supérieur
                    float beta = (segH == 0) ? 0.f : static_cast<float>(y - y0) / segH;

                    // Calcul des coordonnées X sur les bords gauche et droit
                    int ax = x0 + static_cast<int>((x2 - x0) * alpha);
                    int bx = x0 + static_cast<int>((x1 - x0) * beta);

                    // Garantir que ax <= bx pour FillRow
                    if (ax > bx) {
                        std::swap(ax, bx);
                    }

                    // Remplir la scanline courante
                    FillRow(ax, bx, y, c);
                }

                // ── Partie inférieure du triangle [y1, y2] ───────────────────────
                segH = y2 - y1;

                for (int y = y1; y <= y2; ++y) {
                    // Interpolation du facteur alpha sur la hauteur totale
                    float alpha = static_cast<float>(y - y0) / totalH;

                    // Interpolation du facteur beta sur le segment inférieur
                    float beta = (segH == 0) ? 1.f : static_cast<float>(y - y1) / segH;

                    // Calcul des coordonnées X sur les bords gauche et droit
                    int ax = x0 + static_cast<int>((x2 - x0) * alpha);
                    int bx = x1 + static_cast<int>((x2 - x1) * beta);

                    // Garantir que ax <= bx pour FillRow
                    if (ax > bx) {
                        std::swap(ax, bx);
                    }

                    // Remplir la scanline courante
                    FillRow(ax, bx, y, c);
                }
            }

            // ── Effet de lueur radiale (Glow) ──────────────────────────────────────
            // Dessine un effet de lueur avec atténuation alpha quadratique
            void DrawGlow(int cx, int cy, int r, const math::NkColor& c, float intensity = 1.0f) noexcept {
                // Protection contre les rayons invalides
                if (r <= 0) {
                    return;
                }

                // Pré-calcul du rayon au carré pour les tests de distance
                float r2 = static_cast<float>(r * r);

                // Parcours de la boîte englobante du cercle
                for (int dy = -r; dy <= r; ++dy) {
                    for (int dx = -r; dx <= r; ++dx) {
                        // Calcul de la distance au carré depuis le centre
                        float d2 = static_cast<float>(dx * dx + dy * dy);

                        // Ignorer les pixels en dehors du cercle
                        if (d2 > r2) {
                            continue;
                        }

                        // Calcul du facteur d'atténuation : 1.0 au centre, 0.0 au bord
                        float t = 1.0f - math::NkSqrt(d2) / static_cast<float>(r);

                        // Appliquer une courbe quadratique pour un falloff plus doux
                        t = t * t * intensity;

                        // Dessiner le pixel avec fusion additive et alpha modulé
                        AdditivePixel(
                            cx + dx,
                            cy + dy,
                            ColorUtils::WithAlpha(c, t)
                        );
                    }
                }
            }

            // ── Ligne en pointillés ────────────────────────────────────────────────
            // Trace une ligne avec un motif de pointillés de longueur variable
            void DrawDashedLine(int x0, int y0, int x1, int y1, const math::NkColor& c, int dashLen = 6) noexcept {
                // Initialisation de l'algorithme de Bresenham
                int dx = std::abs(x1 - x0);
                int dy = std::abs(y1 - y0);
                int sx = (x0 < x1) ? 1 : -1;
                int sy = (y0 < y1) ? 1 : -1;
                int err = dx - dy;
                int step = 0;

                // Boucle de tracé avec alternance dash/space
                for (;;) {
                    // Dessiner uniquement sur les segments "actifs" du motif
                    if ((step / dashLen) % 2 == 0) {
                        DrawPixel(x0, y0, c);
                    }

                    // Condition d'arrêt : point final atteint
                    if (x0 == x1 && y0 == y1) {
                        break;
                    }

                    // Mise à jour de l'erreur et progression
                    int e2 = err * 2;

                    if (e2 > -dy) {
                        err -= dy;
                        x0 += sx;
                    }

                    if (e2 < dx) {
                        err += dx;
                        y0 += sy;
                    }

                    // Incrémenter le compteur de pas pour le motif de pointillés
                    ++step;
                }
            }

            // ── Police bitmap 3×5 pour l'affichage de texte ───────────────────────
            // Format : chaque caractère est codé sur 5 octets (une ligne par octet)
            // Bit 0 = colonne de gauche, Bit 2 = colonne de droite
            void DrawChar(int x, int y, char ch, const math::NkColor& c, int scale = 2) noexcept {
                // Table de caractères : 96 glyphes de l'ASCII 32 (espace) à 127 (DEL)
                static const uint8 FONT[96][5] = {
                    // ASCII 32 : espace
                    {0x00, 0x00, 0x00, 0x00, 0x00},
                    // ASCII 33-47 : ! " # $ % & ' ( ) * + , - . /
                    {0x02, 0x02, 0x02, 0x00, 0x02}, {0x05, 0x05, 0x00, 0x00, 0x00},
                    {0x05, 0x07, 0x05, 0x07, 0x05}, {0x07, 0x06, 0x07, 0x03, 0x07},
                    {0x05, 0x01, 0x02, 0x04, 0x05}, {0x06, 0x05, 0x06, 0x05, 0x06},
                    {0x02, 0x02, 0x00, 0x00, 0x00}, {0x02, 0x04, 0x04, 0x04, 0x02},
                    {0x02, 0x01, 0x01, 0x01, 0x02}, {0x05, 0x02, 0x07, 0x02, 0x05},
                    {0x00, 0x02, 0x07, 0x02, 0x00}, {0x00, 0x00, 0x00, 0x02, 0x04},
                    {0x00, 0x00, 0x07, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x02},
                    {0x01, 0x01, 0x02, 0x04, 0x04},
                    // ASCII 48-57 : chiffres 0-9
                    {0x07, 0x05, 0x05, 0x05, 0x07}, {0x02, 0x06, 0x02, 0x02, 0x07},
                    {0x07, 0x01, 0x07, 0x04, 0x07}, {0x07, 0x01, 0x07, 0x01, 0x07},
                    {0x05, 0x05, 0x07, 0x01, 0x01}, {0x07, 0x04, 0x07, 0x01, 0x07},
                    {0x07, 0x04, 0x07, 0x05, 0x07}, {0x07, 0x01, 0x01, 0x01, 0x01},
                    {0x07, 0x05, 0x07, 0x05, 0x07}, {0x07, 0x05, 0x07, 0x01, 0x07},
                    // ASCII 58-64 : : ; < = > ? @
                    {0x00, 0x02, 0x00, 0x02, 0x00}, {0x00, 0x02, 0x00, 0x02, 0x04},
                    {0x01, 0x02, 0x04, 0x02, 0x01}, {0x00, 0x07, 0x00, 0x07, 0x00},
                    {0x04, 0x02, 0x01, 0x02, 0x04}, {0x07, 0x01, 0x03, 0x00, 0x02},
                    {0x07, 0x05, 0x03, 0x04, 0x07},
                    // ASCII 65-90 : lettres majuscules A-Z
                    {0x02, 0x05, 0x07, 0x05, 0x05}, {0x06, 0x05, 0x06, 0x05, 0x06},
                    {0x07, 0x04, 0x04, 0x04, 0x07}, {0x06, 0x05, 0x05, 0x05, 0x06},
                    {0x07, 0x04, 0x07, 0x04, 0x07}, {0x07, 0x04, 0x07, 0x04, 0x04},
                    {0x07, 0x04, 0x05, 0x05, 0x07}, {0x05, 0x05, 0x07, 0x05, 0x05},
                    {0x07, 0x02, 0x02, 0x02, 0x07}, {0x01, 0x01, 0x01, 0x05, 0x07},
                    {0x05, 0x05, 0x06, 0x05, 0x05}, {0x04, 0x04, 0x04, 0x04, 0x07},
                    {0x05, 0x07, 0x05, 0x05, 0x05}, {0x05, 0x07, 0x07, 0x05, 0x05},
                    {0x07, 0x05, 0x05, 0x05, 0x07}, {0x07, 0x05, 0x07, 0x04, 0x04},
                    {0x07, 0x05, 0x05, 0x07, 0x01}, {0x07, 0x05, 0x07, 0x06, 0x05},
                    {0x07, 0x04, 0x07, 0x01, 0x07}, {0x07, 0x02, 0x02, 0x02, 0x02},
                    {0x05, 0x05, 0x05, 0x05, 0x07}, {0x05, 0x05, 0x05, 0x05, 0x02},
                    {0x05, 0x05, 0x05, 0x07, 0x05}, {0x05, 0x05, 0x02, 0x05, 0x05},
                    {0x05, 0x05, 0x02, 0x02, 0x02}, {0x07, 0x01, 0x02, 0x04, 0x07},
                    // ASCII 91-96 : [ \ ] ^ _ `
                    {0x06, 0x04, 0x04, 0x04, 0x06}, {0x04, 0x04, 0x02, 0x01, 0x01},
                    {0x03, 0x01, 0x01, 0x01, 0x03}, {0x02, 0x05, 0x00, 0x00, 0x00},
                    {0x00, 0x00, 0x00, 0x00, 0x07}, {0x04, 0x02, 0x00, 0x00, 0x00},
                    // ASCII 97-122 : lettres minuscules a-z (simplifiées)
                    {0x00, 0x06, 0x05, 0x05, 0x07}, {0x04, 0x06, 0x05, 0x05, 0x06},
                    {0x00, 0x07, 0x04, 0x04, 0x07}, {0x01, 0x03, 0x05, 0x05, 0x07},
                    {0x00, 0x07, 0x05, 0x06, 0x07}, {0x03, 0x04, 0x06, 0x04, 0x04},
                    {0x00, 0x07, 0x05, 0x07, 0x01}, {0x04, 0x06, 0x05, 0x05, 0x05},
                    {0x02, 0x00, 0x02, 0x02, 0x07}, {0x01, 0x00, 0x01, 0x05, 0x07},
                    {0x04, 0x05, 0x06, 0x05, 0x05}, {0x06, 0x02, 0x02, 0x02, 0x07},
                    {0x00, 0x06, 0x07, 0x05, 0x05}, {0x00, 0x06, 0x05, 0x05, 0x05},
                    {0x00, 0x07, 0x05, 0x05, 0x07}, {0x00, 0x06, 0x05, 0x06, 0x04},
                    {0x00, 0x03, 0x05, 0x03, 0x01}, {0x00, 0x07, 0x04, 0x04, 0x04},
                    {0x00, 0x07, 0x06, 0x01, 0x07}, {0x02, 0x07, 0x02, 0x02, 0x03},
                    {0x00, 0x05, 0x05, 0x05, 0x07}, {0x00, 0x05, 0x05, 0x05, 0x02},
                    {0x00, 0x05, 0x05, 0x07, 0x05}, {0x00, 0x05, 0x02, 0x02, 0x05},
                    {0x00, 0x05, 0x07, 0x01, 0x07}, {0x00, 0x07, 0x02, 0x04, 0x07},
                    // ASCII 123-127 : { | } ~ DEL
                    {0x03, 0x02, 0x06, 0x02, 0x03}, {0x02, 0x02, 0x02, 0x02, 0x02},
                    {0x06, 0x02, 0x03, 0x02, 0x06}, {0x00, 0x06, 0x03, 0x00, 0x00},
                    {0x07, 0x05, 0x07, 0x05, 0x07},
                };

                // Validation : ignorer les caractères hors plage ASCII imprimable
                if (ch < 32 || ch > 127) {
                    return;
                }

                // Calcul de l'index dans la table de glyphes
                int idx = ch - 32;

                // Protection supplémentaire contre les indices hors limites
                if (idx >= 96) {
                    return;
                }

                // Récupérer le pointeur vers les données du glyphe courant
                const uint8* glyph = FONT[idx];

                // Parcours des 5 lignes du glyphe (ordre inversé pour corriger l'inversion verticale)
                for (int row = 4; row >= 0; --row) {
                    // Parcours des 3 colonnes du glyphe (de gauche à droite en x)
                    for (int col = 0; col < 3; ++col) {
                        // Tester si le bit correspondant est activé dans la ligne courante
                        // On utilise (2 - col) pour inverser horizontalement : col 0 -> bit 2 (droite), col 2 -> bit 0 (gauche)
                        if (glyph[row] & (1 << (2 - col))) {
                            // Dessin à l'échelle 1:1 ou avec agrandissement
                            if (scale == 1) {
                                DrawPixel(x + col, y + row, c);
                            }
                            else {
                                FillRect(
                                    x + col * scale,
                                    y + row * scale,
                                    scale,
                                    scale,
                                    c
                                );
                            }
                        }
                    }
                }
            }

            // Affiche une chaîne de caractères à la position spécifiée
            void DrawText(int x, int y, const char* text, const math::NkColor& c, int scale = 2) noexcept {
                // Protection contre les pointeurs null
                if (!text) {
                    return;
                }

                // Position courante horizontale pour l'espacement des caractères
                int cx = x;

                // Parcours de la chaîne caractère par caractère
                for (const char* p = text; *p; ++p) {
                    // Dessiner le caractère courant
                    DrawChar(cx, y, *p, c, scale);

                    // Avancer de la largeur du glyphe + 1 pixel d'espacement
                    cx += (3 + 1) * scale;
                }
            }

            // Affiche un nombre entier en le convertissant d'abord en chaîne
            void DrawNumber(int x, int y, int n, const math::NkColor& c, int scale = 4) noexcept {
                // Buffer temporaire pour la conversion entier → chaîne
                char buf[32];

                // Formatage sécurisé avec snprintf
                snprintf(buf, sizeof(buf), "%d", n);

                // Délégation à DrawText pour l'affichage
                DrawText(x, y, buf, c, scale);
            }

            // ── Helpers pour le centrage de texte ─────────────────────────────────
            // Calcule la largeur en pixels d'une chaîne pour un facteur d'échelle donné
            int TextWidth(const char* text, int scale = 2) noexcept {
                // Protection contre les pointeurs null
                if (!text) {
                    return 0;
                }

                // Comptage du nombre de caractères
                int len = 0;

                for (const char* p = text; *p; ++p) {
                    ++len;
                }

                // Calcul : (largeur glyphe 3px + espacement 1px) × échelle × nombre de caractères
                return len * (3 + 1) * scale;
            }

            // Affiche une chaîne centrée horizontalement autour de cx
            void DrawTextCentered(int cx, int y, const char* text, const math::NkColor& c, int scale = 2) noexcept {
                // Calcul de la position de départ pour centrer le texte
                DrawText(
                    cx - TextWidth(text, scale) / 2,
                    y,
                    text,
                    c,
                    scale
                );
            }

        private:
            // ── Fonctions internes d'optimisation ──────────────────────────────────
            // Remplit une ligne horizontale entre x0 et x1 avec clipping automatique
            inline void FillRow(int x0, int x1, int y, const math::NkColor& c) noexcept {
                // Garantir que x0 <= x1 pour la boucle de remplissage
                if (x0 > x1)
                {
                    std::swap(x0, x1);
                }

                // Appliquer le clipping horizontal aux bords de l'écran
                int left = math::NkMax(x0, 0);
                int right = math::NkMin(x1, static_cast<int>(mRenderer.Width()) - 1);

                // Vérifier que la ligne Y est dans les limites verticales de l'écran
                int row = y;

                if (row < 0 || row >= static_cast<int>(mRenderer.Height()))
                {
                    return;
                }

                // Accès direct au tampon arrière pour écriture optimisée
                auto& buf = mRenderer.BackBuffer();
                uint8* rowPtr = buf.RowPtr(static_cast<uint32_t>(row));

                // Boucle de remplissage pixel par pixel
                for (int x = left; x <= right; ++x)
                {
                    // Calcul de l'adresse mémoire du pixel courant (RGBA = 4 octets)
                    uint8* p = rowPtr + static_cast<uint32_t>(x) * 4u;

                    // Écriture directe des composantes RGBA
                    p[0] = c.r;
                    p[1] = c.g;
                    p[2] = c.b;
                    p[3] = c.a;
                }
            }

            // Référence vers le renderer associé (ne possède pas l'objet)
            NkRenderer& mRenderer;
    }; // class NkRasterizer

} // namespace nkentseu


// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================

/*
 * ── Exemple 1 : Initialisation et dessin de base ─────────────────────────────
 *
 * // Dans votre code d'initialisation
 * nkentseu::NkWindow window;
 * // ... configuration de la fenêtre ...
 *
 * nkentseu::NkRenderer renderer;
 * if (!renderer.Init(window)) { return -1; }
 *
 * nkentseu::NkRasterizer rasterizer(renderer);
 *
 * // Boucle de rendu principale
 * while (window.IsOpen())
 * {
 *     // Effacer l'écran en bleu foncé
 *     renderer.Clear(math::NkColor(20, 40, 80, 255));
 *
 *     // Dessiner un rectangle rouge rempli
 *     rasterizer.FillRect(50, 50, 200, 100, math::NkColor(255, 0, 0, 255));
 *
 *     // Dessiner un cercle vert en contour
 *     rasterizer.DrawCircle(150, 100, 40, math::NkColor(0, 255, 0, 255));
 *
 *     // Dessiner une ligne blanche épaisse
 *     rasterizer.DrawLine(0.0f, 0.0f, 320.0f, 240.0f,
 *                         math::NkColor(255, 255, 255, 255), 3);
 *
 *     // Afficher le frame
 *     renderer.Present();
 * }
 */


/*
 * ── Exemple 2 : Effets de transparence et de lueur ───────────────────────────
 *
 * // Dessiner avec fusion alpha (demi-transparence)
 * math::NkColor semiTransparent = math::NkColor(255, 255, 255, 128);
 * rasterizer.FillRect(100, 100, 150, 80, semiTransparent);
 *
 * // Dessiner un effet de lueur jaune au centre de l'écran
 * math::NkColor glowColor = math::NkColor(255, 255, 100, 255);
 * rasterizer.DrawGlow(
 *     renderer.Width() / 2,
 *     renderer.Height() / 2,
 *     60,                    // rayon de la lueur
 *     glowColor,
 *     1.5f                   // intensité > 1 pour une lueur plus vive
 * );
 *
 * // Dessiner un triangle avec fusion additive (effet lumineux)
 * rasterizer.FillTriangle(
 *     200, 50,   // sommet
 *     180, 120,  // bas gauche
 *     220, 120,  // bas droit
 *     math::NkColor(255, 200, 50, 200) // orange avec transparence
 * );
 */


/*
 * ── Exemple 3 : Affichage de texte et de nombres ─────────────────────────────
 *
 * // Afficher un titre centré en haut de l'écran
 * rasterizer.DrawTextCentered(
 *     renderer.Width() / 2,
 *     10,
 *     "MON JEU 2D",
 *     math::NkColor(255, 255, 255, 255),
 *     3  // échelle 3x pour un texte plus grand
 * );
 *
 * // Afficher un score en bas à gauche
 * int score = 1250;
 * rasterizer.DrawText(
 *     10,
 *     renderer.Height() - 20,
 *     "Score: ",
 *     math::NkColor(200, 200, 200, 255),
 *     2
 * );
 * rasterizer.DrawNumber(
 *     10 + 6 * 8,  // décalage après "Score: " (6 chars × 8px à l'échelle 2)
 *     renderer.Height() - 20,
 *     score,
 *     math::NkColor(255, 255, 0, 255),  // jaune pour le nombre
 *     2
 * );
 *
 * // Afficher un message d'état avec couleur conditionnelle
 * bool isPaused = true;
 * math::NkColor statusColor = isPaused
 *     ? math::NkColor(255, 100, 100, 255)   // rouge si pause
 *     : math::NkColor(100, 255, 100, 255);  // vert si actif
 *
 * rasterizer.DrawText(
 *     renderer.Width() / 2,
 *     renderer.Height() / 2,
 *     isPaused ? "PAUSE" : "EN JEU",
 *     statusColor,
 *     4
 * );
 */


/*
 * ── Exemple 4 : Animation de primitives ──────────────────────────────────────
 *
 * // Variables d'animation
 * float angle = 0.0f;
 * int circleRadius = 30;
 *
 * while (window.IsOpen())
 * {
 *     renderer.Clear(math::NkColor(10, 10, 30, 255));
 *
 *     // Calculer une trajectoire circulaire pour un objet
 *     float cx = renderer.Width() / 2 + cosf(angle) * 80;
 *     float cy = renderer.Height() / 2 + sinf(angle) * 60;
 *
 *     // Dessiner une lueur derrière l'objet
 *     rasterizer.DrawGlow(
 *         static_cast<int>(cx),
 *         static_cast<int>(cy),
 *         circleRadius + 10,
 *         math::NkColor(100, 200, 255, 255),
 *         0.8f
 *     );
 *
 *     // Dessiner l'objet principal (cercle rempli)
 *     rasterizer.FillCircle(
 *         static_cast<int>(cx),
 *         static_cast<int>(cy),
 *         circleRadius,
 *         math::NkColor(150, 220, 255, 255)
 *     );
 *
 *     // Dessiner un contour pour plus de définition
 *     rasterizer.DrawCircle(
 *         static_cast<int>(cx),
 *         static_cast<int>(cy),
 *         circleRadius,
 *         math::NkColor(255, 255, 255, 255)
 *     );
 *
 *     // Mettre à jour l'angle pour l'animation suivante
 *     angle += 0.03f;
 *
 *     renderer.Present();
 * }
 */


/*
 * ── Exemple 5 : Dessin de formes complexes avec composition ──────────────────
 *
 * // Fonction utilitaire pour dessiner une étoile à 5 branches
 * void DrawStar(nkentseu::NkRasterizer& rasterizer,
 *               int cx, int cy, int outerR, int innerR,
 *               const math::NkColor& color)
 * {
 *     const int points = 5;
 *     const float step = 3.14159f / points;  // 36° en radians
 *
 *     int vertices[points * 2][2];  // [x, y] pour chaque sommet
 *
 *     // Calculer les coordonnées des sommets extérieurs et intérieurs
 *     for (int i = 0; i < points * 2; ++i)
 *     {
 *         float angle = i * step - 3.14159f / 2;  // Commencer vers le haut
 *         int radius = (i % 2 == 0) ? outerR : innerR;
 *
 *         vertices[i][0] = cx + static_cast<int>(cosf(angle) * radius);
 *         vertices[i][1] = cy + static_cast<int>(sinf(angle) * radius);
 *     }
 *
 *     // Dessiner les triangles formant les branches de l'étoile
 *     for (int i = 0; i < points * 2; i += 2)
 *     {
 *         int next = (i + 1) % (points * 2);
 *         int prev = (i - 1 + points * 2) % (points * 2);
 *
 *         rasterizer.FillTriangle(
 *             cx, cy,  // centre de l'étoile
 *             vertices[prev][0], vertices[prev][1],
 *             vertices[i][0], vertices[i][1],
 *             color
 *         );
 *     }
 * }
 *
 * // Utilisation dans la boucle de rendu
 * DrawStar(rasterizer, 160, 120, 50, 20, math::NkColor(255, 215, 0, 255)); // Étoile dorée
 */


/*
 * ── Bonnes pratiques et recommandations ─────────────────────────────────────
 *
 * 1. Privilégier FillRect/FillCircle plutôt que des boucles de SetPixel
 *    pour de meilleures performances (accès mémoire séquentiel optimisé)
 *
 * 2. Utiliser BlendPixel uniquement quand la transparence est nécessaire
 *    car elle implique une lecture + calcul + écriture (3x plus lent que DrawPixel)
 *
 * 3. Pour les animations fluides, pré-calculer les positions en float
 *    puis convertir en int au dernier moment pour le dessin
 *
 * 4. Le paramètre 'scale' de DrawText permet d'agrandir le texte sans
 *    perte de qualité, mais augmente proportionnellement le coût de rendu
 *
 * 5. DrawGlow est coûteux (O(r²)) : limiter le rayon et l'utiliser
 *    avec parcimonie, ou pré-rendre dans un sprite pour les effets récurrents
 *
 * ── Limitations connues ─────────────────────────────────────────────────────
 *
 * • Pas d'anti-aliasing : les lignes et cercles présentent un escalier visible
 * • Police bitmap limitée à ASCII 32-127, pas d'Unicode ou de polices externes
 * • Pas de support natif des dégradés linéaires/radiaux (à implémenter manuellement)
 * • FillTriangle ne gère pas l'interpolation de couleurs ou de coordonnées UV
 *
 * ── Extensions possibles ────────────────────────────────────────────────────
 *
 * • Ajouter un algorithme de ligne anti-aliasée (Xiaolin Wu)
 * • Implémenter le clipping polygonal pour des formes complexes
 * • Ajouter un système de sprites avec gestion de transparence par pixel
 * • Implémenter un moteur de particules utilisant AdditivePixel
 * • Ajouter le support de polices TrueType via une bibliothèque externe
 */