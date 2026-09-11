package com.isomorphisms.mathcharacters;

import android.inputmethodservice.InputMethodService;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputConnection;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;

public final class MathCharactersImeService extends InputMethodService {
    private static final String[][] ROWS = {
        {"←", "↑", "↓", "→", "↔", "↦", "⇒"},
        {"ℂ", "ℝ", "ℚ", "ℤ", "ℕ", "∞"},
        {"i", "j", "k"},
        {"0", "1", "2", "3", "4", "5"},
        {"6", "7", "8", "9", ".", "+", "−"},
        {"⁰", "¹", "²", "³", "⁴", "⁵"},
        {"⁶", "⁷", "⁸", "⁹", "⁺", "⁻"},
        {"⁼", "⁽", "⁾", "ⁱ", "ʲ", "ᵏ", "ˡ", "ⁿ"},
        {"₀", "₁", "₂", "₃", "₄", "₅"},
        {"₆", "₇", "₈", "₉", "₊", "₋"},
        {"₌", "₍", "₎", "ᵢ", "ⱼ", "ₖ", "ₗ"},
        {"ABC", "space", "⌫", "↵"}
    };

    @Override
    public View onCreateInputView() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);

        LinearLayout keyboard = new LinearLayout(this);
        keyboard.setOrientation(LinearLayout.VERTICAL);
        keyboard.setPadding(dp(3), dp(3), dp(3), dp(3));
        scroll.addView(keyboard);

        for (String[] keys : ROWS) {
            LinearLayout row = new LinearLayout(this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER);
            keyboard.addView(row, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ));

            for (String key : keys) {
                Button button = new Button(this);
                button.setAllCaps(false);
                button.setText(key);
                button.setTextSize(18);
                button.setMinWidth(0);
                button.setMinimumWidth(0);
                button.setMinHeight(dp(42));
                button.setMinimumHeight(dp(42));
                button.setPadding(dp(2), 0, dp(2), 0);
                button.setOnClickListener(v -> press(key));

                LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    0,
                    dp(48),
                    1f
                );
                params.setMargins(dp(1), dp(1), dp(1), dp(1));
                row.addView(button, params);
            }
        }

        return scroll;
    }

    private void press(String key) {
        InputConnection connection = getCurrentInputConnection();
        if (connection == null) {
            return;
        }

        switch (key) {
            case "ABC":
                switchToNextInputMethod(false);
                break;
            case "space":
                connection.commitText(" ", 1);
                break;
            case "⌫":
                connection.deleteSurroundingTextInCodePoints(1, 0);
                break;
            case "↵":
                connection.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
                connection.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
                break;
            default:
                connection.commitText(key, 1);
                break;
        }
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
