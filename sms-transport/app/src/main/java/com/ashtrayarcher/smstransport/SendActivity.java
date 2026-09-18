package com.ashtrayarcher.smstransport;

import android.Manifest;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.telephony.SmsManager;
import android.util.Log;

import java.nio.charset.StandardCharsets;

public final class SendActivity extends Activity {
    static final String TAG = "SmsTransport";

    public static final String EXTRA_DESTINATION = "destination";
    public static final String EXTRA_BODY = "body";
    public static final String EXTRA_REQUEST_ID = "request_id";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent request = getIntent();
        String destination = request.getStringExtra(EXTRA_DESTINATION);
        String body = request.getStringExtra(EXTRA_BODY);
        String requestId = request.getStringExtra(EXTRA_REQUEST_ID);

        if (requestId == null || requestId.length() == 0) {
            requestId = "request";
        }

        if (checkSelfPermission(Manifest.permission.SEND_SMS)
                != PackageManager.PERMISSION_GRANTED) {
            fail(requestId, "SMS permission not granted (SEND_SMS)");
            return;
        }

        if (destination == null || destination.length() == 0) {
            fail(requestId, "destination is missing");
            return;
        }

        if (body == null || body.length() == 0) {
            fail(requestId, "message body is missing");
            return;
        }

        SmsManager smsManager = SmsManager.getDefault();
        if (smsManager == null) {
            fail(requestId, "Android SMS service is unavailable");
            return;
        }

        Intent sentResult = new Intent(this, SendResultReceiver.class);
        sentResult.putExtra(EXTRA_REQUEST_ID, requestId);
        PendingIntent sentIntent = PendingIntent.getBroadcast(
            this,
            positiveRequestCode(requestId, 0),
            sentResult,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        Intent deliveryResult = new Intent(this, DeliveryResultReceiver.class);
        deliveryResult.putExtra(EXTRA_REQUEST_ID, requestId);
        PendingIntent deliveryIntent = PendingIntent.getBroadcast(
            this,
            positiveRequestCode(requestId, 1),
            deliveryResult,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        try {
            smsManager.sendTextMessage(
                destination,
                null,
                body,
                sentIntent,
                deliveryIntent
            );
            int bodyBytes = body.getBytes(StandardCharsets.UTF_8).length;
            Log.i(
                TAG,
                "PASS request_submitted request_id="
                    + requestId
                    + " destination="
                    + redactDestination(destination)
                    + " body_utf8_bytes="
                    + bodyBytes
            );
        } catch (SecurityException exception) {
            fail(
                requestId,
                "Android rejected SMS permission: " + exception.getClass().getSimpleName()
            );
            return;
        } catch (IllegalArgumentException exception) {
            fail(
                requestId,
                "Android rejected SMS arguments: " + exception.getMessage()
            );
            return;
        } catch (RuntimeException exception) {
            fail(
                requestId,
                "Android SMS request failed: " + exception.getClass().getSimpleName()
            );
            return;
        }

        finish();
    }

    private void fail(String requestId, String message) {
        Log.e(TAG, "FAIL request_id=" + requestId + " " + message);
        finish();
    }

    private static int positiveRequestCode(String requestId, int offset) {
        return (requestId.hashCode() & 0x3fffffff) + offset;
    }

    private static String redactDestination(String destination) {
        int length = destination.length();
        if (length <= 4) {
            return "…";
        }
        return "…" + destination.substring(length - 4);
    }
}
