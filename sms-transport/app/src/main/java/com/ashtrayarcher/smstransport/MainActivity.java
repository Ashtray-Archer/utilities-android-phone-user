package com.ashtrayarcher.smstransport;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class MainActivity extends Activity {
    private static final int REQUEST_SEND_SMS = 1;

    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(48, 96, 48, 48);

        TextView title = new TextView(this);
        title.setText("SMS Transport");
        title.setTextSize(24.0f);
        layout.addView(title);

        TextView explanation = new TextView(this);
        explanation.setText(
            "Outbound-only Android SMS transport. "
                + "Scheduling, authorization, and message meaning remain outside this APK."
        );
        explanation.setPadding(0, 32, 0, 32);
        layout.addView(explanation);

        status = new TextView(this);
        layout.addView(status);

        Button permissionButton = new Button(this);
        permissionButton.setText("Grant SMS permission");
        permissionButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                requestPermissions(
                    new String[] {Manifest.permission.SEND_SMS},
                    REQUEST_SEND_SMS
                );
            }
        });
        layout.addView(permissionButton);

        setContentView(layout);
        refreshStatus();
    }

    @Override
    public void onRequestPermissionsResult(
        int requestCode,
        String[] permissions,
        int[] grantResults
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_SEND_SMS) {
            refreshStatus();
        }
    }

    private void refreshStatus() {
        boolean granted =
            checkSelfPermission(Manifest.permission.SEND_SMS)
                == PackageManager.PERMISSION_GRANTED;

        if (granted) {
            status.setText(
                "SEND_SMS granted. Carrier sending is available to the protected test ingress."
            );
        } else {
            status.setText(
                "SEND_SMS not granted. This hard-restricted permission may need an "
                    + "allowlisting installer such as adb install -g on a test device."
            );
        }
    }
}
