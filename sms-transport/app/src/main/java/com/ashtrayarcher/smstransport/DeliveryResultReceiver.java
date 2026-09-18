package com.ashtrayarcher.smstransport;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public final class DeliveryResultReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        String requestId =
            intent.getStringExtra(SendActivity.EXTRA_REQUEST_ID);
        if (requestId == null || requestId.length() == 0) {
            requestId = "request";
        }

        if (getResultCode() == Activity.RESULT_OK) {
            Log.i(
                SendActivity.TAG,
                "PASS delivery_report request_id="
                    + requestId
                    + " carrier reported delivery"
            );
        } else {
            Log.w(
                SendActivity.TAG,
                "NOTE delivery_report request_id="
                    + requestId
                    + " carrier did not report successful delivery result_code="
                    + getResultCode()
            );
        }
    }
}
