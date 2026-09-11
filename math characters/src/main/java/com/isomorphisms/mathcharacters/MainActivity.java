package com.isomorphisms.mathcharacters;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int padding = dp(20);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(padding, padding, padding, padding);

        TextView title = new TextView(this);
        title.setText("Math Characters");
        title.setTextSize(26);
        root.addView(title);

        TextView description = new TextView(this);
        description.setText("Restricted keyboard for arrows, number sets, i/j/k, ordinary digits, and complete superscript/subscript rows including ⁿ.");
        description.setTextSize(17);
        description.setPadding(0, dp(16), 0, dp(20));
        root.addView(description);

        Button enable = new Button(this);
        enable.setAllCaps(false);
        enable.setText("Enable Math Characters keyboard");
        enable.setOnClickListener(v -> startActivity(new Intent(Settings.ACTION_INPUT_METHOD_SETTINGS)));
        root.addView(enable);

        Button choose = new Button(this);
        choose.setAllCaps(false);
        choose.setText("Choose keyboard now");
        choose.setOnClickListener(v -> {
            InputMethodManager manager = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (manager != null) {
                manager.showInputMethodPicker();
            }
        });
        root.addView(choose);

        setContentView(root);
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
