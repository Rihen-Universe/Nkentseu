// =============================================================================
// NkInputConnection.java
// Pont IME -> natif. BaseInputConnection gère l'édition côté Android ; on
// intercepte commitText / composition / suppression / touches pour les remonter
// au moteur via les méthodes natives de NkNativeActivity (-> NkTextInputEvent).
// =============================================================================
package com.nkentseu.window;

import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;

public class NkInputConnection extends BaseInputConnection {

    public NkInputConnection(View targetView, boolean fullEditor) {
        super(targetView, fullEditor);
    }

    // Texte finalisé (latin, chiffres, mais AUSSI CJK/emoji/dictée après
    // composition) : c'est le point clé qui manque au clavier natif basique.
    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        if (text != null && text.length() > 0) {
            NkNativeActivity.nkCommitText(text.toString());
        }
        return super.commitText(text, newCursorPosition);
    }

    // Suppression demandée par l'IME (ex. autocorrection) : on émet des
    // Backspace équivalents côté moteur.
    @Override
    public boolean deleteSurroundingText(int beforeLength, int afterLength) {
        for (int i = 0; i < beforeLength; i++) {
            NkNativeActivity.nkKeyEvent(KeyEvent.KEYCODE_DEL, KeyEvent.ACTION_DOWN, 0);
            NkNativeActivity.nkKeyEvent(KeyEvent.KEYCODE_DEL, KeyEvent.ACTION_UP, 0);
        }
        return super.deleteSurroundingText(beforeLength, afterLength);
    }

    // Touches « dures » que l'IME renvoie directement (Backspace, Enter, flèches).
    @Override
    public boolean sendKeyEvent(KeyEvent event) {
        if (event != null) {
            NkNativeActivity.nkKeyEvent(event.getKeyCode(), event.getAction(), event.getMetaState());
        }
        return super.sendKeyEvent(event);
    }
}
