package com.ashtrayarcher.mathsamplekeyboard;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class MainActivity extends Activity {
    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private Button button(String label) {
        Button button = new Button(this);
        button.setText(label);
        button.setAllCaps(false);
        return button;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setSoftInputMode(
            WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE |
            WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE
        );

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(20), dp(24), dp(20), dp(24));

        TextView title = new TextView(this);
        title.setText("Compact Math Keyboard");
        title.setTextSize(26f);
        layout.addView(title);

        TextView explanation = new TextView(this);
        explanation.setText(
            "ℕ ℤ ℚ ℝ ℂ\n" +
            "= ≠ ≟ ∧ →\n" +
            "λ π ∂ ∫ ∞\n" +
            "ⁿ ᵢ ² − –\n\n" +
            "Enable this keyboard, choose it, then type in the field below."
        );
        explanation.setTextSize(18f);
        explanation.setPadding(0, dp(16), 0, dp(12));
        layout.addView(explanation);

        final EditText sample = new EditText(this);
        sample.setHint("Type here");
        sample.setContentDescription("sample_target");
        sample.setTextSize(22f);
        sample.setMinHeight(dp(64));
        layout.addView(sample);

        Button settings = button("Enable keyboard");
        settings.setOnClickListener(ignored ->
            startActivity(new Intent(Settings.ACTION_INPUT_METHOD_SETTINGS))
        );
        layout.addView(settings);

        Button choose = button("Choose keyboard");
        choose.setOnClickListener(ignored -> {
            InputMethodManager manager =
                (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (manager != null) {
                manager.showInputMethodPicker();
            }
        });
        layout.addView(choose);

        setContentView(layout);
        sample.requestFocus();
        sample.postDelayed(new Runnable() {
            @Override
            public void run() {
                InputMethodManager manager =
                    (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
                if (manager != null) {
                    manager.showSoftInput(sample, InputMethodManager.SHOW_FORCED);
                }
            }
        }, 500);
    }
}
