// =============================================================================
// NkNativeActivity.java
// Activity Java pour l'IME complet Android (composition CJK, dictée, suggestions).
//
// Étend android.app.NativeActivity : le code natif (android_native_app_glue)
// tourne exactement comme avant. On ajoute par-dessus une vue invisible mais
// focusable (NkTextInputView) qui porte un InputConnection. Le texte composé par
// l'IME est remonté au natif via les méthodes natives nkCommitText / nkKeyEvent,
// implémentées en C++ dans NkAndroidTextInputJNI.cpp -> NkTextInputEvent.
//
// Pour l'activer, une app déclare dans son .jenga :
//   androidjavafiles(["%{NKWindow.location}/src/NKWindow/Platform/Android/java/**.java"])
//   androidactivityclass("com.nkentseu.window.NkNativeActivity")
//
// Sans cette classe, le backend retombe sur le clavier natif basique
// (KeyCharacterMap) — voir NkAndroidWindow.cpp.
// =============================================================================
package com.nkentseu.window;

import android.app.NativeActivity;
import android.content.Context;
import android.os.Bundle;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;

public class NkNativeActivity extends NativeActivity {

    private NkTextInputView mInputView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Vue 1x1 px, invisible à l'œil mais capable de recevoir le focus et
        // donc de créer un InputConnection quand l'IME est demandé.
        mInputView = new NkTextInputView(this);
        addContentView(mInputView, new ViewGroup.LayoutParams(1, 1));
    }

    // -------------------------------------------------------------------------
    // Appelées depuis le natif (JNI) — voir NkWindow::ShowSoftKeyboard (Android).
    // Toujours reroutées sur le thread UI (obligatoire pour l'IME).
    // -------------------------------------------------------------------------

    /** @param inputType valeur android.text.InputType calculée côté natif. */
    public void nkShowKeyboard(final int inputType) {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                if (mInputView == null) return;
                mInputView.setNkInputType(inputType);
                mInputView.setFocusableInTouchMode(true);
                mInputView.requestFocus();
                InputMethodManager imm =
                    (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.showSoftInput(mInputView, InputMethodManager.SHOW_FORCED);
                }
            }
        });
    }

    public void nkHideKeyboard() {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                InputMethodManager imm =
                    (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null && mInputView != null) {
                    imm.hideSoftInputFromWindow(mInputView.getWindowToken(), 0);
                }
            }
        });
    }

    // -------------------------------------------------------------------------
    // Méthodes natives — implémentées en C++ (NkAndroidTextInputJNI.cpp).
    // Résolues dynamiquement dans le .so chargé par NativeActivity.
    // -------------------------------------------------------------------------

    /** Texte finalisé par l'IME (peut faire plusieurs caractères : CJK, emoji). */
    public static native void nkCommitText(String text);

    /** Événement touche brut (Backspace, Enter, flèches...) depuis l'IME. */
    public static native void nkKeyEvent(int keyCode, int action, int metaState);
}
