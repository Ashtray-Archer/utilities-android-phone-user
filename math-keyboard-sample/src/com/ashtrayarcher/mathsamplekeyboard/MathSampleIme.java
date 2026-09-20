package com.ashtrayarcher.mathsamplekeyboard;

import android.inputmethodservice.InputMethodService;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.LinearLayout;

public final class MathSampleIme extends InputMethodService {
    @Override
    public boolean onEvaluateInputViewShown() {
        return true;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private Button key(String label, View.OnClickListener listener, float weight) {
        Button button = new Button(this);
        button.setText(label);
        button.setTextSize(24f);
        button.setAllCaps(false);
        button.setMinWidth(0);
        button.setMinHeight(dp(58));
        button.setLayoutParams(new LinearLayout.LayoutParams(0, dp(62), weight));
        button.setOnClickListener(listener);
        return button;
    }

    private void commit(String text) {
        InputConnection input = getCurrentInputConnection();
        if (input != null) {
            input.commitText(text, 1);
        }
    }

    private void backspace() {
        InputConnection input = getCurrentInputConnection();
        if (input != null) {
            input.deleteSurroundingText(1, 0);
        }
    }

    private void enter() {
        InputConnection input = getCurrentInputConnection();
        if (input != null) {
            input.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
            input.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        }
    }

    private void showKeyboardPicker() {
        InputMethodManager manager =
            (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        if (manager != null) {
            manager.showInputMethodPicker();
        }
    }

    @Override
    public View onCreateInputView() {
        LinearLayout keyboard = new LinearLayout(this);
        keyboard.setOrientation(LinearLayout.VERTICAL);
        keyboard.setPadding(dp(3), dp(3), dp(3), dp(3));

        for (String[] symbolRow : Keyset.ROWS) {
            LinearLayout symbols = new LinearLayout(this);
            symbols.setOrientation(LinearLayout.HORIZONTAL);
            for (final String symbol : symbolRow) {
                symbols.addView(key(symbol, ignored -> commit(symbol), 1f));
            }
            keyboard.addView(symbols);
        }

        LinearLayout editing = new LinearLayout(this);
        editing.setOrientation(LinearLayout.HORIZONTAL);
        editing.addView(key("⌨", ignored -> showKeyboardPicker(), 1f));
        editing.addView(key("space", ignored -> commit(" "), 2.5f));
        editing.addView(key("⌫", ignored -> backspace(), 1f));
        editing.addView(key("↵", ignored -> enter(), 1f));
        keyboard.addView(editing);

        return keyboard;
    }
}
