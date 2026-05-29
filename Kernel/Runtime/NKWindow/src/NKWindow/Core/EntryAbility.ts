/**
 * EntryAbility.ts
 * =============================================================================
 * Point d'entrée ArkTS de la NativeAbility HarmonyOS pour les projets NKWindow.
 *
 * Ce fichier est généré/patché automatiquement par Jenga (HarmonyOsBuilder)
 * lors du build. Il peut aussi être fourni manuellement via :
 *   files(["harmony/ets/EntryAbility.ts"])
 *
 * Responsabilités :
 *   1. Charger le module natif C++ (.so) via NAPI
 *   2. Initialiser NkHarmonyBridge (orientation, safe area, clavier, PC)
 *   3. Gérer le cycle de vie de l'Ability (foreground/background)
 *   4. Configurer la fenêtre (plein écran, barres système, orientation)
 *
 * Architecture :
 *   EntryAbility (ArkTS)
 *     ↓ initialise
 *   NkHarmonyBridge.ts          ← callbacks vers C++ (orientation, safe area, etc.)
 *     ↓ appelle
 *   nkNative (module NAPI)      ← le .so compilé par Jenga
 *     ↓ démarre
 *   nkmain() (C++)              ← ton code NKWindow
 *
 * Cycle de vie HarmonyOS :
 *   onCreate → onWindowStageCreate → [app en cours] → onWindowStageDestroy → onDestroy
 *   onForeground / onBackground : app visible ou en arrière-plan
 *
 * @see NkHarmonyBridge.ts pour les callbacks système → C++
 * @see NkHarmonyOS.h pour le point d'entrée NAPI côté C++
 */

import UIAbility from '@ohos.app.ability.UIAbility';
import window from '@ohos.window';
import hilog from '@ohos.hilog';
import { AbilityConstant, Want } from '@kit.AbilityKit';
import { NkHarmonyBridge } from '../NkHarmonyBridge';

// ─── Constantes de logging ────────────────────────────────────────────────────

const NK_DOMAIN = 0x0000;
const NK_TAG    = 'NkEntryAbility';

// ─── EntryAbility ─────────────────────────────────────────────────────────────

export default class EntryAbility extends UIAbility {

    // ── Données de configuration (lues depuis harmonywindowconfig() si disponible) ──
    private mHideSystemUI:    boolean = true;   // Masquer status bar + navigation bar
    private mScreenOrientation: string = 'unspecified'; // 'landscape' | 'portrait' | 'unspecified'
    private mFullscreen:      boolean = true;   // Plein écran (phone/tablet toujours true)

    // ──────────────────────────────────────────────────────────────────────────────
    // Cycle de vie de l'Ability
    // ──────────────────────────────────────────────────────────────────────────────

    /**
     * Appelé à la création de l'Ability, avant l'affichage de la fenêtre.
     *
     * Note : le module natif C++ (nkmain) est démarré dans NkHarmonyOS.h
     * via NAPI_MODULE et NkHarmonyNapiInit() — il n'est PAS démarré ici.
     * L'initialisation NAPI est automatique au chargement de la .so.
     */
    onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
        hilog.info(NK_DOMAIN, NK_TAG, 'EntryAbility onCreate');
    }

    /**
     * Appelé quand la fenêtre ArkTS est prête.
     *
     * C'est ici que :
     *   1. NkHarmonyBridge est initialisé (callbacks vers C++)
     *   2. La configuration de la fenêtre est appliquée (plein écran, barres système)
     *   3. Le contenu ArkTS est chargé (XComponent qui héberge le rendu natif)
     */
    onWindowStageCreate(windowStage: window.WindowStage): void {
        hilog.info(NK_DOMAIN, NK_TAG, 'EntryAbility onWindowStageCreate');

        // ── 1. Initialiser NkHarmonyBridge ────────────────────────────────────────
        // IMPORTANT : initialiser AVANT loadContent() pour que les callbacks
        // (orientation, safe area, clavier) soient actifs dès le premier frame.
        NkHarmonyBridge.init(windowStage, this.context).then(() => {
            hilog.info(NK_DOMAIN, NK_TAG, 'NkHarmonyBridge initialized');
        }).catch((err: Error) => {
            hilog.error(NK_DOMAIN, NK_TAG, 'NkHarmonyBridge init failed: %{public}s', err.message);
        });

        // ── 2. Configurer la fenêtre ──────────────────────────────────────────────
        this._configureWindow(windowStage);

        // ── 3. Charger le contenu ArkTS ───────────────────────────────────────────
        // Index.ets contient le XComponent qui héberge la surface native C++.
        // OH_NativeXComponent_Callback::OnSurfaceCreated sera appelé après loadContent.
        windowStage.loadContent('pages/Index', (err) => {
            if (err.code) {
                hilog.error(NK_DOMAIN, NK_TAG, 'Failed to load content. Cause: %{public}s', JSON.stringify(err) ?? '');
                return;
            }
            hilog.info(NK_DOMAIN, NK_TAG, 'Content loaded successfully');
        });
    }

    /**
     * Appelé quand la fenêtre est détruite (app fermée, rotée vers une autre Ability).
     * NkHarmonyBridge est arrêté pour libérer les listeners ArkTS.
     */
    onWindowStageDestroy(): void {
        hilog.info(NK_DOMAIN, NK_TAG, 'EntryAbility onWindowStageDestroy');
        NkHarmonyBridge.destroy();
    }

    /**
     * Appelé quand l'app passe au premier plan (visible).
     * Le rendu C++ reprend son cours normal.
     */
    onForeground(): void {
        hilog.info(NK_DOMAIN, NK_TAG, 'EntryAbility onForeground');
        // NkWESystem du côté C++ émet automatiquement NkWindowFocusGainedEvent
        // via NkHarmonyOnWindowFocusChanged(true) appelé depuis NkHarmonyBridge.
    }

    /**
     * Appelé quand l'app passe en arrière-plan (cachée).
     * Le rendu C++ devrait suspendre les opérations coûteuses.
     */
    onBackground(): void {
        hilog.info(NK_DOMAIN, NK_TAG, 'EntryAbility onBackground');
        // NkWESystem du côté C++ émet automatiquement NkWindowFocusLostEvent.
    }

    /**
     * Appelé juste avant la destruction de l'Ability.
     */
    onDestroy(): void {
        hilog.info(NK_DOMAIN, NK_TAG, 'EntryAbility onDestroy');
    }

    // ──────────────────────────────────────────────────────────────────────────────
    // Configuration de la fenêtre
    // ──────────────────────────────────────────────────────────────────────────────

    /**
     * Configure la fenêtre système pour l'affichage natif C++ :
     *   - Plein écran (layout derrière les barres système)
     *   - Masquage des barres système (status bar + navigation bar)
     *   - Orientation de l'écran
     *
     * Appelé depuis onWindowStageCreate, AVANT loadContent().
     */
    private async _configureWindow(windowStage: window.WindowStage): Promise<void> {
        let mainWindow: window.Window;
        try {
            mainWindow = await windowStage.getMainWindow();
        } catch (e) {
            hilog.error(NK_DOMAIN, NK_TAG, 'getMainWindow failed: %{public}s', (e as Error).message);
            return;
        }

        // ── Plein écran : le layout C++ s'étend derrière les barres système ───────
        // Sur phone/tablet : nécessaire pour que le XComponent occupe tout l'écran.
        // Sur PC 2in1     : peut être false si on veut une vraie fenêtre.
        try {
            await mainWindow.setWindowLayoutFullScreen(this.mFullscreen);
            hilog.info(NK_DOMAIN, NK_TAG, 'setWindowLayoutFullScreen: %{public}s', String(this.mFullscreen));
        } catch (e) {
            hilog.warn(NK_DOMAIN, NK_TAG, 'setWindowLayoutFullScreen failed: %{public}s', (e as Error).message);
        }

        // ── Barres système : masquer pour un rendu plein écran immersif ───────────
        // Les insets de safe area sont transmis au C++ via NkHarmonyBridge
        // même quand les barres sont masquées (notch, coin arrondis, etc.).
        if (this.mHideSystemUI) {
            try {
                await mainWindow.setWindowSystemBarEnable([]);
                hilog.info(NK_DOMAIN, NK_TAG, 'System bars hidden (immersive mode)');
            } catch (e) {
                hilog.warn(NK_DOMAIN, NK_TAG, 'setWindowSystemBarEnable failed: %{public}s', (e as Error).message);
            }
        } else {
            // Garder les barres visibles mais leur appliquer un fond transparent
            // pour que le rendu C++ apparaisse derrière.
            try {
                await mainWindow.setWindowSystemBarProperties({
                statusBarColor: '#00000000',
                navigationBarColor: '#00000000',
                statusBarContentColor: '#FFFFFFFF',
                navigationBarContentColor: '#FFFFFFFF',
                });
            } catch (e) {
                hilog.warn(NK_DOMAIN, NK_TAG, 'setWindowSystemBarProperties failed: %{public}s', (e as Error).message);
            }
        }

        // ── Orientation de l'écran ────────────────────────────────────────────────
        // Configurer ici l'orientation demandée par le projet C++.
        // Les changements d'orientation en runtime sont gérés par NkHarmonyBridge
        // qui appelle NkHarmonyOnOrientationChanged() côté C++.
        try {
            let orientationEnum: window.Orientation;
            switch (this.mScreenOrientation) {
                case 'portrait':
                    orientationEnum = window.Orientation.PORTRAIT;
                break;
                case 'landscape':
                    orientationEnum = window.Orientation.LANDSCAPE;
                break;
                case 'portrait_inverted':
                    orientationEnum = window.Orientation.PORTRAIT_INVERTED;
                break;
                case 'landscape_inverted':
                    orientationEnum = window.Orientation.LANDSCAPE_INVERTED;
                break;
                case 'auto_rotation':
                    orientationEnum = window.Orientation.AUTO_ROTATION;
                break;
                default:
                    orientationEnum = window.Orientation.UNSPECIFIED;
                break;
            }
            await mainWindow.setPreferredOrientation(orientationEnum);
            hilog.info(NK_DOMAIN, NK_TAG, 'Orientation set: %{public}s', this.mScreenOrientation);
        } catch (e) {
            hilog.warn(NK_DOMAIN, NK_TAG, 'setPreferredOrientation failed: %{public}s', (e as Error).message);
        }
    }
}