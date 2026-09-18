package com.ashtrayarcher.smstransport;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.telephony.SmsManager;
import android.util.Log;

public final class SendResultReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        String requestId =
            intent.getStringExtra(SendActivity.EXTRA_REQUEST_ID);
        if (requestId == null || requestId.length() == 0) {
            requestId = "request";
        }

        int result = getResultCode();
        if (result == Activity.RESULT_OK) {
            Log.i(
                SendActivity.TAG,
                "PASS sent request_id=" + requestId + " Android modem accepted the SMS"
            );
            return;
        }

        Log.e(
            SendActivity.TAG,
            "FAIL sent request_id="
                + requestId
                + " "
                + explain(result)
                + " result_code="
                + result
        );
    }

    private static String explain(int result) {
        switch (result) {
            case SmsManager.RESULT_ERROR_GENERIC_FAILURE:
                return "generic SMS send failure";
            case SmsManager.RESULT_ERROR_RADIO_OFF:
                return "cellular radio is off";
            case SmsManager.RESULT_ERROR_NULL_PDU:
                return "Android produced no SMS protocol data unit";
            case SmsManager.RESULT_ERROR_NO_SERVICE:
                return "no cellular SMS service is available";
            case SmsManager.RESULT_ERROR_LIMIT_EXCEEDED:
                return "SMS sending limit was exceeded";
            case SmsManager.RESULT_ERROR_FDN_CHECK_FAILURE:
                return "fixed-dialing-number policy rejected the destination";
            default:
                return "Android reported an SMS send failure";
        }
    }
}
