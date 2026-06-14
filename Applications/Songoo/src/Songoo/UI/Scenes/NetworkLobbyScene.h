#pragma once
// =============================================================================
// NetworkLobbyScene.h
// -----------------------------------------------------------------------------
// Lobby reseau LAN (Phase 1) — permet a 2 joueurs de se connecter avant
// de lancer un match. 2 actions principales :
//   - HEBERGER : ouvre un serveur sur le port 7777, attend qu'un peer rejoigne.
//   - REJOINDRE : tente de se connecter a une IP/port saisi.
//
// Etats afficher a l'user :
//   - IDLE       : choix initial entre HEBERGER et REJOINDRE
//   - HOSTING    : "EN ATTENTE D'UN JOUEUR..." + port d'ecoute
//   - JOINING    : "CONNEXION EN COURS..." vers IP cible
//   - CONNECTED  : "PEER CONNECTE" + bouton LANCER LE MATCH
//   - ERROR      : message d'erreur
//
// Quand l'user clique LANCER LE MATCH (host) ou recoit "MATCH START" (client),
// le SceneManager push GameplayScene avec le flag reseau actif.
//
// Phase 1 = uniquement la connexion. Phase 2 ajoutera le sync gameplay.
// =============================================================================

#include "Songoo/UI/Scene.h"

namespace nkentseu
{
    namespace songoo
    {

        class NetworkLobbyScene : public Scene
        {
        public:
            static constexpr uint16 kDefaultPort = 7777;

            NetworkLobbyScene()  = default;
            ~NetworkLobbyScene() override = default;

            const char* Name() const noexcept override { return "NetworkLobby"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnEvent (AppContext& ctx, NkEvent& ev) override;
            void OnExit  (AppContext& ctx) override;

        private:
            float mTime      = 0.0f;
            float mEnterAnim = 0.0f;

            // Grace period d'entree de scene : on ignore les events click/touch
            // pendant ce delai pour eviter qu'un release "fantome" du clic qui
            // a pousse cette scene (depuis SelectModeScene, qui declenche sur
            // Press) ne declenche automatiquement un bouton ici si la souris se
            // trouve dessus. 200ms = largement suffisant pour absorber le release
            // suivant.
            float mInputArmDelay = 0.0f;

            // Mode UI :
            //   0 = ecran de choix (Heberger / Rejoindre / Retour)
            //   1 = ecran "Rejoindre" : liste des hosts detectes par scan UDP
            //       (cf Phase C, [[pong_multijoueur_lobby_pays_ville]]).
            //       Pas de saisie IP : le client choisit l'host par son nom.
            int  mView = 0;

            // ── Boutons fixes (coords ECRAN, sync chaque frame) ───────────
            float mBackX = 0.0f, mBackY = 0.0f, mBackW = 0.0f, mBackH = 0.0f;
            float mHostX = 0.0f, mHostY = 0.0f, mHostW = 0.0f, mHostH = 0.0f;
            float mJoinX = 0.0f, mJoinY = 0.0f, mJoinW = 0.0f, mJoinH = 0.0f;
            float mLaunchX = 0.0f, mLaunchY = 0.0f;
            float mLaunchW = 0.0f, mLaunchH = 0.0f;
            float mCancelX = 0.0f, mCancelY = 0.0f;
            float mCancelW = 0.0f, mCancelH = 0.0f;

            // ── Liste hosts detectes (vue Rejoindre, cote CLIENT) ─────────
            // On affiche jusqu'a kMaxListSlots boutons cliquables. Au-dela
            // on scroll. Coordonnees recalculees chaque frame avant render
            // et lues par OnEvent.
            static constexpr int kMaxListSlots = 32;
            float mHostBtnX[kMaxListSlots]  = { 0 };
            float mHostBtnY[kMaxListSlots]  = { 0 };
            float mHostBtnW[kMaxListSlots]  = { 0 };
            float mHostBtnH[kMaxListSlots]  = { 0 };
            // IP + port cible memoise pour chaque slot (sert quand on tape).
            char  mHostBtnIp[kMaxListSlots][48]   = { { 0 } };
            uint16 mHostBtnPort[kMaxListSlots]    = { 0 };
            int   mHostBtnCount = 0;

            // ── Liste challengers (vue Hosting, cote HOST) ────────────────
            // Memoise pour chaque slot affiche : le rect cliquable + le
            // peerId associe (passe a AcceptChallenger).
            float  mChalBtnX[kMaxListSlots]    = { 0 };
            float  mChalBtnY[kMaxListSlots]    = { 0 };
            float  mChalBtnW[kMaxListSlots]    = { 0 };
            float  mChalBtnH[kMaxListSlots]    = { 0 };
            uint64 mChalBtnPeerId[kMaxListSlots] = { 0 };
            int    mChalBtnCount = 0;

            // ── Scroll vertical pour les 2 listes ─────────────────────────
            // mScrollJoin : applique sur la vue Rejoindre (liste hosts).
            // mScrollHost : applique sur la vue Hosting (liste challengers).
            // Mis a jour via mouse wheel (PC) et touch drag (mobile).
            float mScrollJoin = 0.0f;
            float mScrollHost = 0.0f;
            // Hauteur totale du contenu (pour clamper le scroll).
            float mContentHJoin = 0.0f;
            float mContentHHost = 0.0f;
            float mViewportHJoin = 0.0f;   ///< Hauteur visible pour clamp
            float mViewportHHost = 0.0f;

            // Touch tracking pour distinguer drag (scroll) vs tap (click).
            // Si le delta vertical entre Begin et End > kTapMaxDeltaPx, c'est
            // un drag et on ne declenche pas le clic.
            static constexpr float kTapMaxDeltaPx = 8.0f;
            long long mActiveTouchId   = -1;
            float     mTouchStartX     = 0.0f;
            float     mTouchStartY     = 0.0f;
            float     mTouchLastY      = 0.0f;
            float     mTouchTotalDeltaY = 0.0f;
            bool      mTouchIsDragging = false;

            // Helpers
            bool HitTest(float sx, float sy, float bx, float by, float bw, float bh) const;
            void DoHost  (AppContext& ctx);
            void DoOpenJoin(AppContext& ctx);
            void DoJoinSlot(AppContext& ctx, int slotIdx);
            void DoAcceptChallenger(AppContext& ctx, int slotIdx);
            void DoLaunch(AppContext& ctx);
            void DoCancel(AppContext& ctx);
            void DoReturnAfterReject(AppContext& ctx);
            /// Pousse GameplayScene avec le mode reseau actif.
            void PushGameplay(AppContext& ctx);
        };

    } // namespace songoo
} // namespace nkentseu
