/**
 * NkHarmonyBridge.ts
 * ─────────────────────────────────────────────────────────────────────────────
 * Bridge ArkTS → C++ pour NKWindow/HarmonyOS.
 *
 * Ce fichier est à placer dans entry/src/main/ets/ de ton projet HarmonyOS
 * et à importer depuis ton EntryAbility.ts.
 *
 * Il gère :
 *   1. Orientation de l'écran (changements système → C++)
 *   2. Safe Area (insets notch/navbar → C++)
 *   3. Clavier virtuel (show/hide/hauteur → C++ + API pour ouvrir depuis C++)
 *   4. Fenêtrage PC 2in1 (minimize/maximize/restore ↔ C++)
 *   5. Cycle de vie fenêtre (focus, foreground/background)
 *
 * Usage dans EntryAbility.ts :
 *
 *   import { NkHarmonyBridge } from './NkHarmonyBridge';
 *
 *   onWindowStageCreate(windowStage: window.WindowStage): void {
 *     NkHarmonyBridge.init(windowStage, this.context);
 *   }
 *
 *   onWindowStageDestroy(): void {
 *     NkHarmonyBridge.destroy();
 *   }
 *
 * API pour le clavier virtuel depuis C++ (via NAPI) :
 *   NkHarmonyBridge.showVirtualKeyboard(hint?: InputType)
 *   NkHarmonyBridge.hideVirtualKeyboard()
 */

import window from '@ohos.window';
import display from '@ohos.display';
import inputMethod from '@ohos.inputMethod';
import { common } from '@kit.AbilityKit';

// Import du module natif C++ (généré par Jenga/hvigor)
// Le nom 'entry' doit correspondre au module déclaré dans NKENTSEU_HARMONY_DEFINE_MODULE
const nkNative = requireNapi('entry');

// ─── Types ───────────────────────────────────────────────────────────────────

interface SafeAreaInsets {
    top: number;
    right: number;
    bottom: number;
    left: number;
}

// ─── Bridge principal ─────────────────────────────────────────────────────────

export class NkHarmonyBridge {
    private static mainWindow: window.Window | null = null;
    private static windowStage: window.WindowStage | null = null;
    private static inputController: inputMethod.InputMethodController | null = null;
    private static isPC: boolean = false;

    // ── Init ────────────────────────────────────────────────────────────────────

    static async init(
        stage: window.WindowStage,
        context: common.UIAbilityContext
    ): Promise<void> {
        NkHarmonyBridge.windowStage = stage;

        try {
            NkHarmonyBridge.mainWindow = await stage.getMainWindow();
        } catch (e) {
            console.error('[NkBridge] getMainWindow failed:', e);
            return;
        }

        // Détecter PC 2in1
        try {
            const d = display.getDefaultDisplaySync();
            // deviceType: UNKNOWN=0 PHONE=1 TABLET=2 DESKTOP=3
            NkHarmonyBridge.isPC = (d.deviceType === 3);
        } catch (_) {}

        NkHarmonyBridge._registerWindowCallbacks();
        NkHarmonyBridge._registerOrientationCallback();
        NkHarmonyBridge._setupInputMethod();
        NkHarmonyBridge._readAndSendInitialSafeArea();

        console.info('[NkBridge] initialized isPC=' + NkHarmonyBridge.isPC);
    }

    static destroy(): void {
        NkHarmonyBridge.mainWindow = null;
        NkHarmonyBridge.windowStage = null;
        NkHarmonyBridge.inputController = null;
    }

    // ── Callbacks fenêtre ────────────────────────────────────────────────────────

    private static _registerWindowCallbacks(): void {
        const win = NkHarmonyBridge.mainWindow;
        if (!win) return;

        // Focus
        win.on('windowFocusChange', (focused: boolean) => {
            nkNative.onWindowFocusChanged?.(focused);
        });

        // Redimensionnement (PC 2in1)
        win.on('windowSizeChange', (size: window.Size) => {
            // Transmis via NkHarmonyOnSurfaceChanged depuis OH_NativeXComponent
            // Ce callback est pour la safe area qui peut changer au resize
            NkHarmonyBridge._readAndSendInitialSafeArea();
        });

        // Cycle de vie PC : minimize / maximize / restore
        if (NkHarmonyBridge.isPC) {
            win.on('windowStatusChange', (status: window.WindowStatusType) => {
                switch (status) {
                case window.WindowStatusType.MINIMIZE:
                    nkNative.onWindowMinimized?.();
                    break;
                case window.WindowStatusType.MAXIMIZE:
                    nkNative.onWindowMaximized?.();
                    break;
                case window.WindowStatusType.FULL_SCREEN:
                    nkNative.onWindowMaximized?.();
                    break;
                case window.WindowStatusType.FLOATING:
                    nkNative.onWindowRestored?.();
                    break;
                default:
                    break;
                }
            });
        }

        // Safe Area — keyboardHeight change (clavier virtuel)
        win.on('keyboardHeightChange', (height: number) => {
            const visible = (height > 0);
            nkNative.onVirtualKeyboardChanged?.(visible, Math.round(height));
        });

        // Avoid Area (safe area pour notch / gestes / navigation bar)
        win.on('avoidAreaChange', (info: window.AvoidAreaOptions) => {
            NkHarmonyBridge._sendSafeArea(win);
        });
    }

    // ── Orientation ──────────────────────────────────────────────────────────────

    private static _registerOrientationCallback(): void {
        try {
            const d = display.getDefaultDisplaySync();
            // Rotation initiale
            nkNative.onOrientationChanged?.(d.rotation * 90);

            display.on('change', (displayId: number) => {
                try {
                const upd = display.getDefaultDisplaySync();
                nkNative.onOrientationChanged?.(upd.rotation * 90);
                } catch (_) {}
            });
        } catch (e) {
            console.warn('[NkBridge] orientation callback failed:', e);
        }
    }

    // ── Safe Area ─────────────────────────────────────────────────────────────────

    private static _readAndSendInitialSafeArea(): void {
        const win = NkHarmonyBridge.mainWindow;
        if (win) NkHarmonyBridge._sendSafeArea(win);
    }

    private static _sendSafeArea(win: window.Window): void {
        try {
            // Zone de coupe système (notch, barre de navigation, etc.)
            const area = win.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM);
            const density = NkHarmonyBridge._getDensity();
            // HarmonyOS retourne les insets en vp (virtual pixels) → convertir en px
            nkNative.onSafeAreaChanged?.(
                area.topRect.height   * density,  // top
                0,                                 // right (pas d'inset standard droit)
                area.bottomRect.height * density,  // bottom
                0                                  // left
            );
        } catch (e) {
            console.warn('[NkBridge] safe area read failed:', e);
        }
    }

    private static _getDensity(): number {
        try {
            return display.getDefaultDisplaySync().densityPixels;
        } catch (_) { 
            return 1.0; 
        }
    }

    // ── Clavier virtuel ───────────────────────────────────────────────────────────
    //
    // Sur HarmonyOS, le clavier virtuel est géré par le système de méthode
    // d'entrée (InputMethod). Il faut un composant TextInput dans l'UI ArkTS
    // pour avoir le focus et déclencher l'ouverture.
    //
    // Deux approches :
    //   A) Composant TextInput caché (overlay invisible) — le plus fiable
    //   B) inputMethodController.attach() — demande de focus programmatique
    //
    // On utilise l'approche B ici, compatible avec les apps full C++.
    // Pour la saisie de texte, les caractères arrivent via OnKeyEvent (C++).

    private static _setupInputMethod(): void {
        try {
            NkHarmonyBridge.inputController = inputMethod.getController();
        } catch (e) {
            console.warn('[NkBridge] InputMethod controller unavailable:', e);
        }
    }

    /**
     * Ouvre le clavier virtuel.
     *
     * À appeler depuis ArkTS quand le jeu veut saisir du texte.
     * Depuis C++, utiliser le bridge NAPI exposé par NkHarmonyBridge.
     *
     * @param inputType  Type d'entrée (texte, numérique, email, mot de passe...)
     *   0 = text (défaut)
     *   2 = number
     *   4 = email
     *   8 = password
     */
    static async showVirtualKeyboard(inputType: number = 0): Promise<void> {
        const ctrl = NkHarmonyBridge.inputController;
        if (!ctrl) return;
        try {
            // attach() demande le focus et ouvre le clavier
            await ctrl.attach(true, {
                inputAttribute: {
                textInputType: inputType,
                enterKeyType: 2,  // 2 = DONE
                }
            });
        } catch (e) {
            console.warn('[NkBridge] showVirtualKeyboard failed:', e);
        }
    }

    /**
     * Ferme le clavier virtuel.
     */
    static async hideVirtualKeyboard(): Promise<void> {
        const ctrl = NkHarmonyBridge.inputController;
        if (!ctrl) return;
        try {
            await ctrl.detach();
        } catch (e) {
            console.warn('[NkBridge] hideVirtualKeyboard failed:', e);
        }
    }

    // ── Fenêtrage PC (2in1) ──────────────────────────────────────────────────────
    // Ces méthodes sont appelées par C++ via les callbacks NkWindow
    // (Minimize/Maximize/Restore → windowStage API)

    static async minimizeWindow(): Promise<void> {
        if (!NkHarmonyBridge.isPC || !NkHarmonyBridge.mainWindow) return;
        try {
            await NkHarmonyBridge.mainWindow.minimize();
        } catch (e) {
            console.warn('[NkBridge] minimize failed:', e);
        }
    }

    static async maximizeWindow(): Promise<void> {
        if (!NkHarmonyBridge.isPC || !NkHarmonyBridge.mainWindow) return;
        try {
            await NkHarmonyBridge.mainWindow.maximize(window.MaximizePresentation.ENTER_IMMERSIVE);
        } catch (e) {
            console.warn('[NkBridge] maximize failed:', e);
        }
    }

    static async restoreWindow(): Promise<void> {
        if (!NkHarmonyBridge.isPC || !NkHarmonyBridge.mainWindow) return;
        try {
            await NkHarmonyBridge.mainWindow.recover();
        } catch (e) {
            console.warn('[NkBridge] restore failed:', e);
        }
    }

    // ── UI système ────────────────────────────────────────────────────────────────

    static async setFullscreen(fullscreen: boolean): Promise<void> {
        const win = NkHarmonyBridge.mainWindow;
        if (!win) return;
        try {
            await win.setWindowLayoutFullScreen(fullscreen);
            if (fullscreen) {
                await win.setWindowSystemBarEnable([]);          // masquer toutes les barres
            } else {
                await win.setWindowSystemBarEnable(['status', 'navigation']);
            }
        } catch (e) {
            console.warn('[NkBridge] setFullscreen failed:', e);
        }
    }

    static async setSystemUIVisible(visible: boolean): Promise<void> {
        const win = NkHarmonyBridge.mainWindow;
        if (!win) return;
        try {
            if (visible) {
                await win.setWindowSystemBarEnable(['status', 'navigation']);
            } else {
                await win.setWindowSystemBarEnable([]);
            }
        } catch (e) {
            console.warn('[NkBridge] setSystemUIVisible failed:', e);
        }
    }
}

// ─── Exports NAPI (appelés depuis C++ via le module natif) ───────────────────
//
// Pour appeler ces fonctions depuis C++, utiliser napi_call_function
// ou passer par un objet JS exposé via le module NAPI.
//
// Exemple côté C++ (dans NkHarmonyOS.h) :
//   // Demander l'ouverture du clavier virtuel
//   void NkShowVirtualKeyboard(int inputType = 0);
//   void NkHideVirtualKeyboard();
//
// Exemple côté ArkTS (EntryAbility.ts) :
//   nkNative.showVirtualKeyboard = (type: number) => NkHarmonyBridge.showVirtualKeyboard(type);
//   nkNative.hideVirtualKeyboard = () => NkHarmonyBridge.hideVirtualKeyboard();