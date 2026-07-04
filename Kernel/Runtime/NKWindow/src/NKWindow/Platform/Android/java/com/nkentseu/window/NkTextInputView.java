// =============================================================================
// NkTextInputView.java
// Vue invisible (1x1) mais focusable, cible de l'IME. Déclare qu'elle est un
// éditeur de texte (onCheckIsTextEditor) et fournit un InputConnection.
// =============================================================================
package com.nkentseu.window;

import android.content.Context;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

public class NkTextInputView extends View {

    private int mInputType = InputType.TYPE_CLASS_TEXT;

    public NkTextInputView(Context context) {
        super(context);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    /** Type de clavier (valeur android.text.InputType), fixé par NkNativeActivity. */
    public void setNkInputType(int inputType) {
        mInputType = inputType;
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return true;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        outAttrs.inputType = mInputType;
        // IME_FLAG_NO_EXTRACT_UI : pas de champ plein écran en paysage (on gère
        // l'affichage nous-mêmes). IME_ACTION_DONE : touche « OK » par défaut.
        outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE
                            | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                            | EditorInfo.IME_FLAG_NO_FULLSCREEN;
        return new NkInputConnection(this, false);
    }

    // Touches "dures" reçues directement par la vue focalisée (clavier physique,
    // Chromebook, émulateur, adb input) — la saisie IME "molle" passe, elle, par
    // NkInputConnection.commitText. On remonte la touche + le caractère éventuel.
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        NkNativeActivity.nkKeyEvent(keyCode, event.getAction(), event.getMetaState());
        final int uc = event.getUnicodeChar(event.getMetaState());
        if (uc > 0) {
            NkNativeActivity.nkCommitText(new String(Character.toChars(uc)));
        }
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        NkNativeActivity.nkKeyEvent(keyCode, event.getAction(), event.getMetaState());
        return true;
    }
}
