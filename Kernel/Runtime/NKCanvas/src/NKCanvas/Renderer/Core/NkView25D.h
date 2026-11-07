// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkView25D.h : la vue 2.5D : une NkView2D (centre, taille, rotation) à laquelle on ajoute l'inclinaison de la caméra,
// la lumière et la HAUTEUR z. Créé le 05/11/2026 (Rodolf : « on ne peut pas créer une NkView héritée de NkView2D
// qui a cet effet ? »). Remplace la structure Camera du jeu.
//
// CE FICHIER NE DÉPEND D'AUCUN JEU : il n'inclut que NKMath et l'en-tête des types 2D de NKCanvas. Il est écrit pour
// monter tel quel dans NKCanvas ; MONTÉ le 07/11/2026 (1.5.8) : nkentseu::renderer::NkView25D, le code est celui du jeu, seul l'espace de noms change
// (le jeu garde `nk25d::NkView25D` par un alias, voir FoodHunt/src/NkView25D.h).
//
// POURQUOI UNE VUE LOCALE ET NON NkView2D SEULE : une vue 2D n'a que deux axes, elle ne connaît ni la HAUTEUR z (cubes,
// fourmis qui portent un bloc), ni la profondeur de tri, ni l'ombre portée oblique. Ces trois choses sont l'essence du
// 2.5D ; elles s'ajoutent ici, et seulement elles.
//
// ===== LE MODÈLE (académique) =====
// Le monde a trois axes : x vers la droite, y vers l'avant (vers le bas de l'écran, donc vers le joueur) et z vers le
// haut. On regarde la scène d'en haut, la caméra étant inclinée d'un angle phi au-dessus du plan du sol (phi = 90 degrés :
// vue de dessus pure ; phi = 0 : vue de côté).
//
// 1. PROJECTION ORTHOGRAPHIQUE sur le « plan de projection ». Soit u = (0, cos phi, sin phi) le vecteur unitaire qui
//    pointe vers la caméra et w = (0, sin phi, -cos phi) l'axe vertical de l'écran (vers le bas). Un point P = (x, y, z) a
//        plan_x = x ,   plan_y = P . w = y sin phi - z cos phi .
//    On divise plan_y par cos phi (simple changement d'unité, pour garder une hauteur z « vraie » en pixels) :
//        plan_y = y * tilt - z ,   avec  tilt = tan phi .
//    L'inclinaison `tilt` écrase le sol verticalement (tilt < 1 : un pas en y avance moins vite à l'écran qu'un pas en x)
//    et la hauteur z remonte l'objet d'autant. Pourquoi ORTHOGRAPHIQUE et pas en perspective : tous les blocs gardent la
//    même taille quelle que soit leur distance, ce qui convient à un plateau qu'on lit comme une grille ; une perspective
//    demanderait une division par la profondeur et déformerait la grille.
//
// 2. LA VUE (hérité de NkView2D). `center` est le point du PLAN DE PROJECTION vu au milieu de la fenêtre et `size` la taille
//    de la zone du plan qui est visible ; `window` est la taille de la fenêtre en pixels. Un point du plan p devient le pixel
//        pixel = (p - (center - size / 2)) * (window / size) .
//    Project rend donc des PIXELS DE FENÊTRE : il n'y a pas de second calcul d'échelle à côté. VUE NEUTRE : window == size et
//    center == size / 2 ; alors pixel == plan (échelle 1, aucun décalage) : c'est exactement l'ancienne formule
//    (x, y * tilt - z) de la structure Camera, avec origine nulle. `rotation` (héritée) reste à 0 : une vue qui tourne
//    n'a pas de sens avec une projection oblique dont l'axe vertical est l'axe de la hauteur.
//
// 3. PROFONDEUR (algorithme du peintre). La proximité à la caméra d'un point est son produit scalaire avec u :
//        P . u = y cos phi + z sin phi ;  divisé par cos phi :  y + z tan phi = y + z * tilt .
//    C'est Depth. Plus la valeur est grande, plus le point est proche (plus bas à l'écran, ou plus haut dans l'air). Les
//    commandes de dessin sont triées par cette valeur croissante, puis dessinées dans cet ordre.
//
// 4. OMBRE PORTÉE. Une lumière de direction (lightDir.x, lightDir.y) par unité de hauteur : un point à la hauteur z projette
//    son ombre sur le sol en (x + lightDir.x * z, y + lightDir.y * z), puis on projette ce point du sol avec z = 0. C'est une
//    projection oblique parallèle (la lumière est supposée infiniment lointaine).
//
// ADAPTATION À L'ÉCRAN. NkResizePolicy (même en-tête que NkView2D) nomme cinq réponses à « que devient l'image quand la
// fenêtre change de forme » ; aucune ne convient telle quelle ici : les bandes noires de NK_FIT_LETTERBOX seraient
// laides. `Adapt` fait NK_FIT_LETTERBOX (rapport conservé, tout le monde de référence visible) mais REMPLIT les bandes en
// montrant plus de monde (herbe) au lieu de noir : l'échelle est min(W / refW, H / refH) et la zone visible s'étend sur
// l'autre axe. Le jeu n'a ainsi qu'une seule échelle, celle de la vue.
#pragma once
#include "NKMath/NKMath.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"

namespace nkentseu {
namespace renderer {

struct NkView25D : NkView2D {
    using NkVec2f = nkentseu::math::NkVec2f;

    // Inclinaison de la caméra : tan(phi). 0.65 correspond à phi = 33 degrés environ. Sans unité (rapport de longueurs).
    // Plus la valeur est grande, plus le sol s'étale en hauteur à l'écran ; 1.0 donnerait phi = 45 degrés.
    float tilt = 0.65f;

    // Direction de la lumière, en unités de plan décalées par unité de hauteur : un point à la hauteur z projette son
    // ombre à (x + 0.55 * z, y + 0.30 * z) sur le sol. C'est la lumière qui vient du haut gauche de l'écran.
    NkVec2f lightDir{0.55f, 0.30f};

    // Taille de la fenêtre en pixels (largeur, hauteur). Valeur de départ : une fenêtre portrait 540 x 1000.
    NkVec2f window{540.f, 1000.f};

    // Constructeur : part d'une vue neutre de 540 x 1000 pixels (échelle 1, aucun décalage).
    NkView25D()
    {
        Neutral(540.f, 1000.f);
    }

    // Vue NEUTRE de w x h pixels : échelle 1, aucun décalage (Project rend la même chose que l'ancienne formule
    // (x, y * tilt - z) avec une origine nulle).
    //   w, h : taille de la fenêtre en pixels.
    void Neutral(float w, float h)
    {
        // La fenêtre fait w x h pixels.
        window = NkVec2f(w, h);
        // La zone visible du plan a la même taille que la fenêtre : une unité de plan vaut un pixel.
        size = window;
        // Le centre est au milieu de cette zone : le coin haut gauche visible est donc l'origine (0, 0).
        center = NkVec2f(w * .5f, h * .5f);
        // Pas de rotation : voir l'explication de l'en-tête.
        rotation = 0.f;
    }

    // Échelle d'adaptation : pixels de fenêtre par unité de plan. Formule UNIQUE (la police de l'application l'utilise
    // aussi, dans main.cpp, pour que le texte suive la même échelle que le plateau).
    //   winW, winH : taille de la fenêtre en pixels.   refW, refH : taille du monde de référence en unités de plan.
    // Rend le plus petit des deux rapports (le monde de référence tient entièrement dans la fenêtre), mais jamais moins
    // de 0.2 pour éviter une échelle nulle ou négative sur une fenêtre minuscule.
    static float ScaleFor(float winW, float winH, float refW, float refH)
    {
        // Rapport sur la largeur : combien de pixels par unité de plan si on remplit la largeur.
        float a = winW / refW;
        // Rapport sur la hauteur : idem pour la hauteur.
        float b = winH / refH;
        // On garde le plus petit : l'autre axe aura donc de la marge (qu'Adapt remplit avec plus de monde).
        float s = a < b ? a : b;
        // Plancher à 0.2 : une fenêtre dégénérée ne doit pas écraser l'échelle jusqu'à zéro.
        return s < 0.2f ? 0.2f : s;
    }

    // Adapte la vue à une fenêtre de winW x winH pixels pour un monde de référence refW x refH (voir l'en-tête).
    // Le centre est conservé relativement au coin haut-gauche de la zone visible : on le remet donc à size / 2 ; l'appelant
    // décale ensuite `center` selon sa mise en page (le point du plan qui doit tomber au centre de la fenêtre).
    void Adapt(float winW, float winH, float refW, float refH)
    {
        // Une seule échelle pour toute la vue.
        float s = ScaleFor(winW, winH, refW, refH);
        // La fenêtre garde sa taille réelle en pixels.
        window = NkVec2f(winW, winH);
        // La zone visible du plan s'étend sur l'axe en trop : fenêtre divisée par l'échelle.
        size = NkVec2f(winW / s, winH / s);
        // Centre au milieu de la zone visible (l'appelant le déplacera ensuite).
        center = size * .5f;
        // Toujours sans rotation.
        rotation = 0.f;
    }

    // Pixels de fenêtre par unité de plan (vaut 1 pour une vue neutre).
    float Scale() const
    {
        return window.x / size.x;
    }

    // Point du plan de projection (coordonnées « plan », avant la mise en fenêtre).
    //   x, y, z : point du monde (z = hauteur au-dessus du sol).
    // Rend (x, y * tilt - z) : le sol est écrasé par l'inclinaison, la hauteur remonte le point.
    NkVec2f Plane(float x, float y, float z) const
    {
        return NkVec2f(x, y * tilt - z);
    }

    // Monde -> pixels de fenêtre : on retire le coin haut gauche de la zone visible (center - size / 2) puis on
    // multiplie par l'échelle k.
    //   x, y, z : point du monde.   Rend la position en pixels dans la fenêtre.
    NkVec2f Project(float x, float y, float z) const
    {
        // Échelle plan -> pixels.
        const float k = Scale();
        return NkVec2f((x - (center.x - size.x * .5f)) * k, (y * tilt - z - (center.y - size.y * .5f)) * k);
    }

    // Pixels de fenêtre -> point (x, y) du SOL (z = 0) : l'inverse de Project sur le plan horizontal. Sert à convertir
    // un clic ou un toucher en case du plateau. Le y est divisé par tilt pour défaire l'écrasement vertical.
    //   px, py : position en pixels dans la fenêtre.   Rend le point du sol (x, y).
    NkVec2f Unproject(float px, float py) const
    {
        // Échelle plan -> pixels.
        const float k = Scale();
        return NkVec2f(px / k + (center.x - size.x * .5f), (py / k + (center.y - size.y * .5f)) / tilt);
    }

    // Position à l'écran de l'ombre d'un point en hauteur : on décale le point sur le sol selon la lumière, puis on le
    // projette avec z = 0 (l'ombre est posée sur le sol).
    //   x, y, z : point du monde qui porte l'ombre.   Rend la position de l'ombre en pixels de fenêtre.
    NkVec2f ShadowPos(float x, float y, float z) const
    {
        return Project(x + lightDir.x * z, y + lightDir.y * z, 0.f);
    }

    // Profondeur pour le tri du peintre : plus la valeur est grande, plus le point est proche de la caméra (dessiné
    // après). Le premier paramètre (x) est ignoré : la profondeur ne dépend que de y et de z. Voir l'en-tête, point 3.
    float Depth(float, float y, float z) const
    {
        return y + z * tilt;
    }
};

} // namespace renderer
} // namespace nkentseu
