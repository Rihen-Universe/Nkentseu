#pragma once
// =============================================================================
// NetProtocol.h
// -----------------------------------------------------------------------------
// Protocole reseau minimal Phase 2 pour Pong 1v1 LAN.
//
// Architecture :
//   - HOST   : simulation autoritative (paddle L = local, paddle R = input client,
//              ball + goals + scores = simules). Broadcast l'etat complet a 30 Hz.
//   - CLIENT : aucune simulation. Envoie son input vertical a 30 Hz. Applique
//              tel quel le snapshot recu (paddles + ball + scores).
//
// Sans prediction client ni interpolation : OK localhost / LAN faible latence.
// Phase 3 ajoutera la prediction du paddle local + reconciliation.
//
// Format wire :
//   - Toutes les structures sont packed (#pragma pack 1) pour eviter le padding.
//   - Encodage suppose little-endian (x86_64 partout chez nous). A revoir si on
//     porte sur ARM big-endian (rare aujourd'hui mais NkHToN* dispo si besoin).
//   - 1er octet = type (uint8 NetMsgType). Le reste depend du type.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu
{
    namespace pong
    {
        namespace netproto
        {

            // =============================================================
            // NetMsgType — discriminateur de message (1er octet du paquet)
            // =============================================================
            enum NetMsgType : uint8
            {
                kMsgInput         = 1,  ///< Client -> Host : position paddle desiree
                kMsgState         = 2,  ///< Host   -> Client : snapshot complet du monde
                kMsgPauseToggle   = 3,  ///< Client -> Host : demande de toggle pause
                kMsgStartMatch    = 4,  ///< Host   -> Client : signal de demarrage du match
                kMsgReplayRequest = 5,  ///< Client -> Host : demande de rejouer
                kMsgParticleBurst = 6,  ///< Host   -> Client : event emission burst
                kMsgParticleGoal  = 7,  ///< Host   -> Client : event emission goal (explosion ligne de but)
                kMsgHello         = 8,  ///< Echange auto a la connexion : porte pseudo Pays/Ville
                kMsgAccept        = 9,  ///< Host   -> Client : valide le challenger (Phase B)
                kMsgReject        = 10  ///< Host   -> Client : refuse le challenger (Phase B)
            };

            // =============================================================
            // PktHello — echange d'identite Pays/Ville (Phase A multijoueur)
            // =============================================================
            // Envoye en RELIABLE_ORDERED par CHAQUE peer (Host ET Client) des
            // que la connexion NkConnectionManager passe Connected. Permet a
            // chaque cote de connaitre l'identifiant memorable de l'autre
            // joueur (cf [[pong_multijoueur_lobby_pays_ville]]).
            //
            // Format compact : 1 + 32 + 32 = 65 octets. Strings null-terminated,
            // safe meme si l'emetteur n'ecrit pas le terminator (le destinataire
            // applique un memset(0) avant copie).
            //
            // Phase A : echange symetrique seulement (auto-acceptation).
            // Phase B : ajoute PktAccept/PktReject pour validation explicite.
            // =============================================================
            #pragma pack(push, 1)
            struct PktHello
            {
                uint8  type;           ///< Doit valoir kMsgHello
                char   country[32];    ///< Nom du pays (ex: "Cameroun"), null-term
                char   city[32];       ///< Nom de la ville (ex: "Douala"), null-term
                char   code[16];       ///< Code 9 chiffres "012345678", null-term
            };
            #pragma pack(pop)
            static_assert(sizeof(PktHello) == 1 + 32 + 32 + 16, "PktHello layout cassee");

            // =============================================================
            // PktAccept / PktReject — validation host (Phase B multijoueur)
            // =============================================================
            // Envoye en RELIABLE quand l'host clique Accepter / Refuser un
            // challenger affiche dans la liste hosting. Apres Accept, le
            // client peut entrer en SelectMatchConfigScene. Apres Reject, il
            // est shutdown cote host et reload le lobby cote client.
            //
            // Pour Phase A, les peers sont auto-accepted des l'echange Hello
            // (compatibilite ascendante avec le lobby actuel).
            // =============================================================
            #pragma pack(push, 1)
            struct PktAccept { uint8 type; };
            struct PktReject { uint8 type; };
            #pragma pack(pop)
            static_assert(sizeof(PktAccept) == 1, "PktAccept layout cassee");
            static_assert(sizeof(PktReject) == 1, "PktReject layout cassee");

            // =============================================================
            // PktParticleBurst — Host envoie un event burst de particules
            // =============================================================
            // Envoye en UNRELIABLE (perte d'1 burst = mineur visuellement).
            // Position normalisee, couleur packee RGBA8888, parametres quantifies.
            // Le CLIENT emet localement via mParticles.EmitBurst des reception.
            // =============================================================
            #pragma pack(push, 1)
            struct PktParticleBurst
            {
                uint8  type;        ///< Doit valoir kMsgParticleBurst
                float  xN;          ///< Position X normalisee sur arenaW
                float  yN;          ///< Position Y normalisee sur arenaH
                uint32 colorRGBA;   ///< RGBA8888 packed (LE : R=byte0)
                uint8  count;       ///< Nombre de particules a emettre (0..255)
                uint8  speedMin_q;  ///< speedMin en px/sec, 0..255
                uint8  speedMax_q;  ///< speedMax en px/sec, 0..255 (* 2 = 0..510)
                uint8  lifeMin_q;   ///< lifeMin en s/100, 0..255 (= 0..2.55s)
                uint8  lifeMax_q;   ///< lifeMax en s/100
                uint8  sizeMin_q;   ///< sizeMin en px, 0..255
                uint8  sizeMax_q;   ///< sizeMax en px
            };
            #pragma pack(pop)
            static_assert(sizeof(PktParticleBurst) == 1 + 4 + 4 + 4 + 1 + 6,
                          "PktParticleBurst layout cassee");

            // =============================================================
            // PktParticleGoal — Host envoie une explosion de goal (mur)
            // =============================================================
            // Envoye en RELIABLE (event rare et important visuellement).
            // CLIENT appelle mParticles.EmitGoal(wallX = wallXN * arenaW, arenaH, color).
            // =============================================================
            #pragma pack(push, 1)
            struct PktParticleGoal
            {
                uint8  type;       ///< Doit valoir kMsgParticleGoal
                float  wallXN;     ///< Position du mur normalisee : 0.0 = gauche, 1.0 = droite
                uint32 colorRGBA;  ///< Couleur RGBA8888 du marqueur
            };
            #pragma pack(pop)
            static_assert(sizeof(PktParticleGoal) == 1 + 4 + 4,
                          "PktParticleGoal layout cassee");

            // =============================================================
            // PktReplayRequest — client demande au host de relancer une manche
            // =============================================================
            // Envoye en RELIABLE quand le client clique REJOUER apres un game
            // over. Le HOST autoritatif relance et le snapshot propagera
            // mGameOver=false + scores=0 au client.
            // =============================================================
            #pragma pack(push, 1)
            struct PktReplayRequest
            {
                uint8 type;   ///< Doit valoir kMsgReplayRequest
            };
            #pragma pack(pop)
            static_assert(sizeof(PktReplayRequest) == 1, "PktReplayRequest layout cassee");

            // =============================================================
            // PktStartMatch — Host signale au Client de demarrer le match
            // =============================================================
            // Envoye en RELIABLE quand l'host appuie sur LANCER LE MATCH apres
            // la config. Le client recoit -> push GameplayScene (et applique
            // les settings de match envoyes par l'host pour rester coherent).
            // =============================================================
            #pragma pack(push, 1)
            struct PktStartMatch
            {
                uint8  type;           ///< Doit valoir kMsgStartMatch
                uint16 maxScore;       ///< Score pour gagner (0 = pas de limite)
                uint16 timeLimitSec;   ///< Limite de temps en secondes (0 = pas de limite)
                float  ballSpeedMul;   ///< Multiplicateur de vitesse balle [1..2]
                uint8  flags;          ///< bit0 winByTwo, bit1 powerUpsEnabled
                uint32 obstacleSeed;   ///< Seed RNG pour spawn deterministe obstacles
                uint8  obsActive[8];   ///< Toggle des 8 types d'obstacles (settings)
            };
            #pragma pack(pop)
            static_assert(sizeof(PktStartMatch) == 1 + 2 + 2 + 4 + 1 + 4 + 8,
                          "PktStartMatch layout cassee");

            enum StartMatchFlag : uint8
            {
                kStartFlagWinByTwo    = 1 << 0,
                kStartFlagPowerUpsOn  = 1 << 1
            };

            // =============================================================
            // PktPauseToggle — client signale qu'il veut toggle la pause
            // =============================================================
            // Envoye en RELIABLE_ORDERED (1 message ponctuel, pas spammé).
            // Le HOST inverse son mPaused et le broadcast via flag dans PktState.
            // =============================================================
            #pragma pack(push, 1)
            struct PktPauseToggle
            {
                uint8 type;   ///< Doit valoir kMsgPauseToggle
            };
            #pragma pack(pop)
            static_assert(sizeof(PktPauseToggle) == 1, "PktPauseToggle layout cassee");

            // =============================================================
            // PktInput — position desiree du paddle client (envoye a 30 Hz)
            // =============================================================
            // Encodage : float normalise [0..1] = Y haut-gauche du paddle
            // souhaite par le client (relatif a arenaH).
            //
            // Choix d'envoyer une POSITION absolue (pas une direction -1/+1)
            // pour supporter clavier ET souris ET touch :
            //   - Clavier W/S : le client maintient localement la position
            //     en bougeant selon paddleSpd, puis l'envoie.
            //   - Souris drag : le mouvement vertical modifie la position
            //     locale, qui est envoyee telle quelle.
            //   - Touch drag : idem souris.
            //
            // 5 octets contre 2 pour dir, mais on gagne en precision (pas de
            // saccade due au timer 30 Hz) et on supporte tous les inputs.
            // =============================================================
            #pragma pack(push, 1)
            struct PktInput
            {
                uint8 type;        ///< Doit valoir kMsgInput
                float paddleYN;    ///< Position Y desiree, normalisee [0..1]
            };
            #pragma pack(pop)
            static_assert(sizeof(PktInput) == 5, "PktInput layout cassee");

            // =============================================================
            // PktState — snapshot autoritative du monde (Host -> Client)
            // =============================================================
            // Coordonnees NORMALISEES [0..1] relatives a l'arene du host. Le
            // client les multiplie par SON propre arenaW/arenaH pour rester
            // visuellement coherent meme si les viewports different (laptop
            // 1366x768 cote host, desktop 1920x1080 cote client, etc.).
            //
            // Velocites en px/frame@60fps RELATIVES a la taille de l'arene host
            // (en fait : ratio par rapport a arenaW). Le client multiplie par
            // son arenaW pour reconstruire une velocite locale equivalente.
            // =============================================================
            // =============================================================
            // PktDropEntry — un orbe power-up dans le PktState
            // =============================================================
            // Position normalisee [0..1] x [0..1] sur arenaW x arenaH.
            // isBonusKind : bit7 = isBonus (1=bonus/0=malus), bits 0..6 = kind
            // (BonusType ou MalusType selon isBonus). Compact, 9 bytes.
            // =============================================================
            #pragma pack(push, 1)
            struct PktDropEntry
            {
                uint8 isBonusKind;   ///< bit7 isBonus, 0..6 kind (Bonus/MalusType)
                float xN;            ///< centre X normalise
                float yN;            ///< centre Y normalise
            };
            #pragma pack(pop)
            static_assert(sizeof(PktDropEntry) == 9, "PktDropEntry layout cassee");

            /// Nombre max de drops simultanes en vol (capacite snapshot).
            constexpr uint8 kMaxDropsSync = 4;

            // =============================================================
            // PktObstacleEntry — un obstacle dans le PktState (sync HOST->CLIENT)
            // =============================================================
            // Le HOST est autoritatif sur les obstacles : positions, rotations,
            // visibilite et collision. Le CLIENT n'execute PAS son Update()
            // obstacle, il applique simplement les positions du snapshot.
            //
            // Layout compact (14 bytes par obstacle) :
            //   typeShape  : bit7..4 = type (ObstacleType 0..15)
            //                bit3..0 = shape (ObstacleShape 0..15)
            //   flags      : bit0 active, bit1 visible
            //   xN, yN     : centre obstacle normalise [0..1] sur arenaW/arenaH
            //   wN_q, hN_q : largeur/hauteur normalisees, 0..255 -> 0..1
            //                (precision ~0.4% du viewport, suffisant)
            //   rot_q      : rotation en radians, mapped -PI..PI -> -32767..32767
            //                (precision ~0.0001 rad, soit ~0.006°)
            // =============================================================
            #pragma pack(push, 1)
            struct PktObstacleEntry
            {
                uint8  typeShape;   ///< bit7..4 type, bit3..0 shape
                uint8  flags;       ///< bit0 active, bit1 visible
                float  xN;          ///< centre X normalise sur arenaW
                float  yN;          ///< centre Y normalise sur arenaH
                uint8  wN_q;        ///< largeur normalisee × 255
                uint8  hN_q;        ///< hauteur normalisee × 255
                int16  rot_q;       ///< rotation rad × (32767/PI)
            };
            #pragma pack(pop)
            static_assert(sizeof(PktObstacleEntry) == 14,
                          "PktObstacleEntry layout cassee");

            /// Bits du champ flags dans PktObstacleEntry.
            enum ObstacleFlag : uint8
            {
                kObsActive  = 1 << 0,
                kObsVisible = 1 << 1
            };

            /// Nombre max d'obstacles synchronises dans un snapshot.
            /// 8 types × ~4 instances max = 32. On ajoute un peu de marge.
            constexpr uint8 kMaxObstaclesSync = 40;

            #pragma pack(push, 1)
            struct PktState
            {
                uint8  type;        ///< Doit valoir kMsgState
                float  paddleLYN;   ///< Y haut-gauche paddle gauche, normalise [0..1] sur arenaH
                float  paddleRYN;   ///< Y haut-gauche paddle droit,  normalise [0..1] sur arenaH
                float  ballXN;      ///< Centre X balle,   normalise [0..1] sur arenaW
                float  ballYN;      ///< Centre Y balle,   normalise [0..1] sur arenaH
                float  ballVXN;     ///< Velocite X, normalisee par arenaW (fraction / frame@60)
                float  ballVYN;     ///< Velocite Y, normalisee par arenaH
                uint16 scoreL;      ///< Score gauche
                uint16 scoreR;      ///< Score droit
                uint8  flags;       ///< bit0 paused, bit1 gameOver, bit2 goalFlash
                int8   winner;      ///< +1 P1 / -1 P2 / 0 pas encore
                // === Sync power-ups (Phase 5) =================================
                // Multiplicateurs visuels de taille raquette, quantifies sur 8
                // bits (0..255 maps a 0.0..4.0, soit pas 4/255). 64 = 1.0 (normal),
                // 128 = 2.0 (giant), 32 = 0.5 (mini).
                uint8  paddleHMulL_q;
                uint8  paddleHMulR_q;
                // Flags visuels par cote (effets actifs : freeze, blind, etc.)
                uint8  effFlagsL;   ///< bit0 frozen, bit1 blind, bit2 inverted
                uint8  effFlagsR;   ///< idem
                // Drops actifs en vol (positions seulement, sans collision cote
                // client -- la collision est decidee par le HOST).
                uint8       numDrops;
                PktDropEntry drops[kMaxDropsSync];
                // ── Notifications power-up actives (Phase 5b) ─────────────────
                // Une slot par cote. Packing :
                //   notifL: bit7 = isBonus (1) ou isMalus (0)
                //           bits 0..6 = kind (BonusType ou MalusType, 0..15)
                //   notifLeftL_q : timeLeft restant quantifie sur 8 bits
                //                  (0..255 -> 0..2.55s, suffisant pour 1.5s anim).
                //   0 = pas de notif active sur ce cote.
                uint8 notifL;
                uint8 notifR;
                uint8 notifLeftL_q;
                uint8 notifLeftR_q;
                // ── Sync obstacles (Phase 6) ──────────────────────────────────
                // Le HOST est autoritatif sur les obstacles : leurs positions,
                // rotations, visibilite et collision. Chaque snapshot porte
                // l'etat courant de TOUS les obstacles actifs. Le CLIENT
                // n'execute PAS son ObstacleSystem::Update(), il applique
                // simplement les valeurs recues.
                //
                // Payload : 1 + 40*14 = 561 bytes (a 60 Hz = 33.7 KB/s). OK en LAN.
                uint8            numObstacles;
                PktObstacleEntry obstacles[kMaxObstaclesSync];
            };
            #pragma pack(pop)
            static_assert(sizeof(PktState) ==
                              1                                  // type
                            + 4*6                                // 4 floats paddles + 4 floats balle (6 au total)
                            + 2*2                                // scores
                            + 1                                  // flags
                            + 1                                  // winner
                            + 4                                  // paddleHMulL_q + paddleHMulR_q + effFlagsL + effFlagsR
                            + 1 + 9 * 4                          // numDrops + drops[4]
                            + 4                                  // notifs L/R + timeLeft L/R
                            + 1 + 14 * 40,                       // numObstacles + obstacles[40]
                          "PktState layout cassee");

            /// Bits du champ effFlagsL/R dans PktState
            enum EffectFlag : uint8
            {
                kEffFrozen   = 1 << 0,
                kEffBlind    = 1 << 1,
                kEffInverted = 1 << 2,
                kEffShield   = 1 << 3
            };

            // =============================================================
            // Bits du champ flags dans PktState
            // =============================================================
            enum StateFlag : uint8
            {
                kFlagPaused    = 1 << 0,
                kFlagGameOver  = 1 << 1,
                kFlagGoalFlash = 1 << 2
            };

            // =============================================================
            // Cadences d'envoi reseau
            // =============================================================
            /// Frequence d'envoi du PktState par le host (Hz).
            /// 60 Hz = un snapshot par frame de simulation, plus fluide cote
            /// client. Couple a la prediction balle (extrapolation par
            /// velocite chaque frame), on atteint une experience visuelle
            /// quasi identique a celle du HOST. Bande passante : 31 octets
            /// * 60 = 1.9 ko/s par client. Negligeable en LAN.
            constexpr float kStateSendHz = 60.0f;

            /// Frequence d'envoi du PktInput par le client (Hz).
            /// 60 Hz pour rester en phase avec le state du HOST (les paddles
            /// du client sont mis a jour a la meme cadence).
            constexpr float kInputSendHz = 60.0f;

        } // namespace netproto
    }     // namespace pong
}         // namespace nkentseu
