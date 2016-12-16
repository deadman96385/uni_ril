/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.sprd.customizedNet;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Process;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;
import com.sprd.customizedNet.CustomizedNetManager;

/**
 * Class that receives intent BOOT_COMPLETED,
 * then start the service CustomizedNetManager.
 */

public class CustomizedNetBootReceiver extends BroadcastReceiver {

    private static final String TAG = "CNBR";
    private boolean DBG = true;

    @Override
    public void onReceive(Context context, Intent intent) {
        String prop;
        String action = intent.getAction();

        if (DBG) Log.d(TAG, "Receive action : " + action);

        if (Intent.ACTION_BOOT_COMPLETED.equals(action)) {
            prop = SystemProperties.get("persist.sys.generalsecure", "1");

            if (DBG) Log.d(TAG, "prop = " + prop);
            if (prop.equals("1")) {
                //CustomizedNetManager CNManager = new CustomizedNetManager(context);
                //ServiceManager.addService("customizednet", CNManager);
                context.startService(new Intent(context, CustomizedNetManager.class));
            } else if (prop.equals("0")) {
                //kill myself
                Process.killProcess(Process.myPid());
            }
        } else {
           //nothing to do !
        }
    }
}

