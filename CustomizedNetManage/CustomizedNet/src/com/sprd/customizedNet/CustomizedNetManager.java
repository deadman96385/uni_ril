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

import android.app.Application;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;

import android.system.Os;
import android.os.SystemProperties;
import android.os.ServiceManager;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.HandlerThread;
import java.io.IOException;
import android.os.Handler;
import android.os.SystemClock;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.os.Trace;
import java.io.InputStreamReader;
import java.io.LineNumberReader;
import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.InputStream;
import java.lang.Process;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import java.util.List;
import java.util.ArrayList;
import java.util.Arrays;
import android.util.Log;
import android.content.ContentUris;
import android.net.Uri;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.res.Resources;
import android.database.ContentObserver;
import android.database.Cursor;
import android.provider.Settings;
import android.provider.Settings.SettingNotFoundException;
import android.telephony.SubscriptionManager;

import com.android.internal.telephony.IGeneralSecureManager;
//import com.generalsecure.networkcontrol.IGeneralSecureManager;

/**
 * Class that designed to be a common module to implement certain net requirements.
 * It can be expanded for various requirements.
 * Through local socket, the service communicates with the native service named "cndaemon",
 * which executes iptables commands indeed. Also,the service creat a stub service and add it
 * to ServiceManager for remote call. In the stub service,the special customized needs will
 * be performed.
 */

public class CustomizedNetManager extends Service {
    public static final String TAG = "CustomizedNet";
    private Context mContext;

    //protected AlarmManager mAlarmManager;
    //protected PendingIntent mAlarmIntentForInit = null;

    protected LocalSocket mSocket;
    protected HandlerThread mSenderThread;
    protected DaemonSender mSender;
    protected Thread mReceiverThread;
    protected DaemonReceiver mDaemonReceiver;
    protected String mCmdRunning = null;
    protected GeneralSecureManager mBinder = null;

    protected final int IPV4 = 0;
    protected final int IPV6 = 1;

    private final Object mDaemonLock = new Object();

    protected final int SOCKET_RETRY_MILLIS = 2 * 1000;
    protected final int MAX_SOCKET_RETRY_TIMES = 30;

    private final int EVENT_SEND = 1;

    @Override
    public void onCreate() {
        super.onCreate();
        mContext = this.getApplicationContext();

        Log.d(TAG, "onCreate: start cndaemon");
        SystemProperties.set("ctl.start", "cndaemon");

        mSenderThread = new HandlerThread("DaemonSender");
        mSenderThread.start();
        Looper looper = mSenderThread.getLooper();
        mSender = new DaemonSender(looper);

        mDaemonReceiver= new DaemonReceiver();
        mReceiverThread = new Thread(mDaemonReceiver, "DaemonReceiver");
        mReceiverThread.start();

        /*IntentFilter filter = new IntentFilter();
        mContext.registerReceiver(mIntentReceiver,filter);*/

        /*mAlarmManager = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);*/

        mBinder = new GeneralSecureManager();
        if (mBinder == null) {
            Log.e(TAG, "onCreate : mBinder is null");
        }
        ServiceManager.addService("generalsecure", mBinder);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "onStartCommand start");
        return 0;
    }

    @Override
     public IBinder onBind(Intent arg0) {
         // TODO Auto-generated method stub
         Log.d(TAG, "onBind ");
         if (mBinder == null) {
             Log.e(TAG, "onBind : mBinder is null");
         }
         return mBinder;
     }


    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy()");
        mContext.getContentResolver().unregisterContentObserver(mBinder.mDataSubObserver);
        mContext = null;
        super.onDestroy();
    }

    public class  GeneralSecureManager extends IGeneralSecureManager.Stub {
        private InternalHandler mHandler;

        /** Watches for changes to the Data Sub */
        protected DataSubChangeObserver mDataSubObserver;
        protected int mInitTimes = 0;
        protected int mWaitDatasubTimes = 0;
        protected int mWaitDatabaseTimes = 0;
        protected Object mSendCmdLock = new Object();

        protected ArrayList<String> conIptablesWlan = new ArrayList<String>(2);
        protected ArrayList<String> conIptablesSim1 = new ArrayList<String>(3);
        protected ArrayList<String> conIptablesSim2 = new ArrayList<String>(3);
        protected ArrayList<String> conIp6tablesWlan = new ArrayList<String>(2);
        protected ArrayList<String> conIp6tablesSim1 = new ArrayList<String>(3);
        protected ArrayList<String> conIp6tablesSim2 = new ArrayList<String>(3);
        protected ArrayList<String> suspendedCmds = new ArrayList<String>();
        protected boolean mInitDone = false;

        protected final int MAX_INIT_TRY_TIMES = 30;
        protected final int MAX_WAIT_DATASUB_TIMES = 2;

        protected final String mCmdHeadAppend = "iptables -w -A ";
        protected final String mCmdHeadDelete = "iptables -w -D ";
        protected final String mCmdHeadAppendV6 = "ip6tables -w -A ";
        protected final String mCmdHeadDeleteV6 = "ip6tables -w -D ";
        protected final String mWlanChain = "droidwall-wlan ";
        protected final String mSim1Chain = "droidwall-sim1 ";
        protected final String mSim2Chain = "droidwall-sim2 ";

        protected final int WIFI_ALLOWED_ONLY = 1; //00000001
        protected final int SIM1_ALLOWED_ONLY = 2; //00000010
        protected final int SIM2_ALLOWED_ONLY = 4; //00000100

        public static final int TYPE_WIFI  =  0;
        public static final int TYPE_SIM1_MOBILE_DATA  =  1;
        public static final int TYPE_SIM2_MOBILE_DATA  =  2;

        protected final int INIT_RETRY_DELAY = 2*1000;

        private final int EVENT_INIT_BLOCKED_PACKAGES = 1;
        private final int EVENT_DATA_SUB_CHANGED = 2;
        private final int EVENT_INIT_APP_DATABASE = 3;

        private final Uri PACKAGE_BLOCK_URI =
                    Uri.parse("content://com.sprd.generalsecurity.blockstateprovider/blockstate");


        public GeneralSecureManager() {
             Log.d(TAG, " GeneralSecureManager constructor ");
             HandlerThread handlerThread = new HandlerThread("GeneralSecureManagerThread");
             handlerThread.start();
             mHandler = new InternalHandler(handlerThread.getLooper());

             mDataSubObserver = new DataSubChangeObserver();
             mContext.getContentResolver().registerContentObserver(
                     Settings.Global.getUriFor(Settings.Global.MULTI_SIM_DATA_CALL_SUBSCRIPTION),
                     false, mDataSubObserver);

             initConstantCmds();
             Message msg = mHandler.obtainMessage(EVENT_INIT_BLOCKED_PACKAGES);
             mHandler.sendMessageDelayed(msg, INIT_RETRY_DELAY);
             Log.d(TAG, " GeneralSecureManager msg sent ");
         }


         /**
         * @ param packageUid: uid of package
         * @ param networkType:
         * 0 - TYPE_WIFI
         * 1 - TYPE_SIM1_MOBILE_DATA
         * 2 - TYPE_SIM2_MOBILE_DATA
         * @ param allowed:
         * true - allowed on the networkType
         * false - not allowed on the networkType
         */
         @Override
         public void setPackageBlockState(int packageUid, int networkType, boolean allowed) {
             String iptablesCmd;
             String ip6tablesCmd;

             Log.d(TAG, "setPackageBlockState: E, packageUid = " + packageUid +
                             ", networkType = " + networkType + ", allowed = " + allowed);
             synchronized (mSendCmdLock) {
                 //if init is done,send cmd imediately, or buffer them until init done
                 if (mInitDone) {
                     assembleAndSetUidCmds(networkType,packageUid,(!allowed),IPV4,true);
                     assembleAndSetUidCmds(networkType,packageUid,(!allowed),IPV6,true);
                 } else {
                     Log.d(TAG,"setPackageBlockState: init not done,cmd is suspended");
                     iptablesCmd = assembleAndSetUidCmds(networkType,packageUid,(!allowed),IPV4,false);
                     suspendedCmds.add(iptablesCmd);
                     ip6tablesCmd = assembleAndSetUidCmds(networkType,packageUid,(!allowed),IPV6,false);
                     suspendedCmds.add(ip6tablesCmd);
                 }
             }
         }

         /*
         *@param packageUid: uid of package
         */
         @Override
         public void deleteBlockPackage (int packageUid) {
              String iptablesCmd;
              String ip6tablesCmd;

              //iptables -w -D droidwall-sim1 -m owner --uid-owner 10000 -j DROP
              //iptables -w -D droidwall-sim2 -m owner --uid-owner 10000 -j DROP
              //iptables -w -D droidwall-wlan -m owner --uid-owner 10000 -j DROP
              Log.d(TAG, "deleteBlockPackage: E, packageUid = " + packageUid);

              synchronized (mSendCmdLock) {
                  //if init is done,send cmd imediately, or buffer them until init done
                  if (mInitDone) {
                      assembleAndSetUidCmds(TYPE_WIFI, packageUid, false,IPV4,true);
                      assembleAndSetUidCmds(TYPE_SIM1_MOBILE_DATA, packageUid, false,IPV4,true);
                      assembleAndSetUidCmds(TYPE_SIM2_MOBILE_DATA, packageUid, false,IPV4,true);
                      assembleAndSetUidCmds(TYPE_WIFI, packageUid, false,IPV6,true);
                      assembleAndSetUidCmds(TYPE_SIM1_MOBILE_DATA, packageUid, false,IPV6,true);
                      assembleAndSetUidCmds(TYPE_SIM2_MOBILE_DATA, packageUid, false,IPV6,true);
                  } else {
                      Log.d(TAG,"deleteBlockPackage: init not done,cmd is suspended");
                      iptablesCmd = assembleAndSetUidCmds(TYPE_WIFI, packageUid, false,IPV4,false);
                      suspendedCmds.add(iptablesCmd);
                      iptablesCmd = assembleAndSetUidCmds(TYPE_SIM1_MOBILE_DATA, packageUid, false,IPV4,false);
                      suspendedCmds.add(iptablesCmd);
                      iptablesCmd = assembleAndSetUidCmds(TYPE_SIM2_MOBILE_DATA, packageUid, false,IPV4,false);
                      suspendedCmds.add(iptablesCmd);

                      ip6tablesCmd = assembleAndSetUidCmds(TYPE_WIFI, packageUid, false,IPV6,false);
                      suspendedCmds.add(ip6tablesCmd);
                      ip6tablesCmd = assembleAndSetUidCmds(TYPE_SIM1_MOBILE_DATA, packageUid, false,IPV6,false);
                      suspendedCmds.add(ip6tablesCmd);
                      ip6tablesCmd = assembleAndSetUidCmds(TYPE_SIM2_MOBILE_DATA, packageUid, false,IPV6,false);
                      suspendedCmds.add(ip6tablesCmd);
                  }
              }
         }

         private class InternalHandler extends Handler {
             public InternalHandler(Looper looper) {
                 super(looper);
             }

             @Override
             public void handleMessage(Message msg) {
                 switch (msg.what) {
                     case EVENT_INIT_BLOCKED_PACKAGES: {
                         Log.d(TAG, " GeneralSecureManager: receive msg EVENT_INIT_BLOCKED_PACKAGES ");
                         initBlockedPakages();
                         break;
                     }
                     case EVENT_DATA_SUB_CHANGED: {
                         Log.d(TAG, " GeneralSecureManager: receive msg EVENT_DATA_SUB_CHANGED ");
                         handleDataSubChange();
                         break;
                     }
                     case EVENT_INIT_APP_DATABASE: {
                         Log.d(TAG, " GeneralSecureManager: receive msg EVENT_INIT_APP_DATABASE ");
                         handleInitAppDatabase();
                         break;
                     }
                 }
             }
         }

         /**
          * Handles changes of the Data Sub.
          */
         protected class DataSubChangeObserver extends ContentObserver {
             public DataSubChangeObserver () {
                 super(mHandler);
             }

             @Override
             public void onChange(boolean selfChange) {
                 Log.d(TAG,"Data Sub changed");
                 Message msg = mHandler.obtainMessage(EVENT_DATA_SUB_CHANGED);
                 mHandler.sendMessage(msg);
             }
         }

         protected void handleDataSubChange() {
             int dataPhoneId;
             int dataSubId = getDataSubscription();

             dataPhoneId = SubscriptionManager.getPhoneId(dataSubId);
             Log.d(TAG,"handleDataSubChange: dataPhoneId = " + dataPhoneId);
             if (!SubscriptionManager.isValidPhoneId(dataPhoneId)) {
                 dataPhoneId = 0;
             }

             synchronized (mSendCmdLock) {
                 setDataSimCmds(dataPhoneId);
             }
         }

         /*maintain constant cmds locally */
         private void initConstantCmds() {
             String conCmd = null;

             //constant ipv4 cmds stored locally
             conCmd = "iptables -w -N droidwall-wlan";
             conIptablesWlan.add(0,conCmd);

             conCmd = "iptables -w -I OUTPUT -o wlan+ -j droidwall-wlan";
             conIptablesWlan.add(1,conCmd);

             conCmd = "iptables -w -N droidwall-sim1";
             conIptablesSim1.add(0,conCmd);

             conCmd = "iptables -w -I OUTPUT -o seth+ -j droidwall-sim1";
             conIptablesSim1.add(1,conCmd);

             conCmd = "iptables -w -D OUTPUT -o seth+ -j droidwall-sim1";
             conIptablesSim1.add(2,conCmd);

             conCmd = "iptables -w -N droidwall-sim2";
             conIptablesSim2.add(0,conCmd);

             conCmd = "iptables -w -I OUTPUT -o seth+ -j droidwall-sim2";
             conIptablesSim2.add(1,conCmd);

             conCmd = "iptables -w -D OUTPUT -o seth+ -j droidwall-sim2";
             conIptablesSim2.add(2,conCmd);

             //constant ipv6 cmds stored locally
             conCmd = "ip6tables -w -N droidwall-wlan";
             conIp6tablesWlan.add(0,conCmd);

             conCmd = "ip6tables -w -I OUTPUT -o wlan+ -j droidwall-wlan";
             conIp6tablesWlan.add(1,conCmd);

             conCmd = "ip6tables -w -N droidwall-sim1";
             conIp6tablesSim1.add(0,conCmd);

             conCmd = "ip6tables -w -I OUTPUT -o seth+ -j droidwall-sim1";
             conIp6tablesSim1.add(1,conCmd);

             conCmd = "ip6tables -w -D OUTPUT -o seth+ -j droidwall-sim1";
             conIp6tablesSim1.add(2,conCmd);

             conCmd = "ip6tables -w -N droidwall-sim2";
             conIp6tablesSim2.add(0,conCmd);

             conCmd = "ip6tables -w -I OUTPUT -o seth+ -j droidwall-sim2";
             conIp6tablesSim2.add(1,conCmd);

             conCmd = "ip6tables -w -D OUTPUT -o seth+ -j droidwall-sim2";
             conIp6tablesSim2.add(2,conCmd);
         }

         /*set constant cmds in advance */
         private void setConstantCmds() {
             //creat new iptables chain droidwall-wlan
             send(conIptablesWlan.get(0));
             //insert chain droidwall-wlan to OUTPUT
             send(conIptablesWlan.get(1));
             //creat new iptables chain droidwall-sim1
             send(conIptablesSim1.get(0));
             //creat new iptables chain droidwall-sim2
             send(conIptablesSim2.get(0));

             //creat new ip6tables chain droidwall-wlan
             send(conIp6tablesWlan.get(0));
             //insert ip6tables chain droidwall-wlan to OUTPUT
             send(conIp6tablesWlan.get(1));
             //creat new ip6tables chain droidwall-sim1
             send(conIp6tablesSim1.get(0));
             //creat new ip6tables chain droidwall-sim2
             send(conIp6tablesSim2.get(0));
         }

         /*enable data network chain according to data subscription*/
         private void setDataSimCmds(int dataSim) {
             if (0 == dataSim) {
                 //delete chain droidwall-sim2 and insert chain droidwall-sim1
                 send(conIptablesSim2.get(2));
                 send(conIptablesSim1.get(1));
                 //do the same for ipv6
                 send(conIp6tablesSim2.get(2));
                 send(conIp6tablesSim1.get(1));
             } else if (1 == dataSim) {
                 //delete chain droidwall-sim1 and insert chain droidwall-sim2
                 send(conIptablesSim1.get(2));
                 send(conIptablesSim2.get(1));
                 //do the same for ipv6
                 send(conIp6tablesSim1.get(2));
                 send(conIp6tablesSim2.get(1));
             }
         }

         /*set uid related cmd, if the package is blocked by any network*/
         private void setUidCmds(int packageUid, int blockedState) {
             //iptables -w -A droidwall-sim1 -m owner --uid-owner 10000 -j DROP
             if ((blockedState & WIFI_ALLOWED_ONLY) == 0) {
                 assembleAndSetUidCmds(TYPE_WIFI,packageUid,true,IPV4,true);
                 assembleAndSetUidCmds(TYPE_WIFI,packageUid,true,IPV6,true);
             }
             if ((blockedState & SIM1_ALLOWED_ONLY) == 0) {
                 assembleAndSetUidCmds(TYPE_SIM1_MOBILE_DATA,packageUid,true,IPV4,true);
                 assembleAndSetUidCmds(TYPE_SIM1_MOBILE_DATA,packageUid,true,IPV6,true);
             }
             if ((blockedState & SIM2_ALLOWED_ONLY) == 0) {
                 assembleAndSetUidCmds(TYPE_SIM2_MOBILE_DATA,packageUid,true,IPV4,true);
                 assembleAndSetUidCmds(TYPE_SIM2_MOBILE_DATA,packageUid,true,IPV6,true);
             }
         }

         private String assembleAndSetUidCmds(int netType, int packUid, boolean blocked, int target, boolean sendImediately) {
             StringBuilder cmdAssembled = new StringBuilder();
             String ownerUid = "-m owner --uid-owner ";
             String jumpTo = " -j DROP";
             String cmd;

             if (blocked) {
                 if (IPV4 == target) {
                     cmdAssembled.append(mCmdHeadAppend);
                 } else if (IPV6 == target) {
                     cmdAssembled.append(mCmdHeadAppendV6);
                 }
             } else {
                 if (IPV4 == target) {
                     cmdAssembled.append(mCmdHeadDelete);
                 } else if (IPV6 == target) {
                     cmdAssembled.append(mCmdHeadDeleteV6);
                 }
             }

             switch (netType) {
                 case TYPE_WIFI:
                     cmdAssembled.append(mWlanChain);
                     break;
                 case TYPE_SIM1_MOBILE_DATA:
                     cmdAssembled.append(mSim1Chain);
                     break;
                 case TYPE_SIM2_MOBILE_DATA:
                     cmdAssembled.append(mSim2Chain);
                     break;
             }

             cmdAssembled.append(ownerUid);
             cmdAssembled.append(Integer.toString(packUid));
             cmdAssembled.append(jumpTo);
             cmd = cmdAssembled.toString();

             if (sendImediately) {
                 send(cmd);
             }

             return cmd;
         }

         /*When the service is created, we should query the blocked states of all packages,
             and then set corresponding cmds.*/
         private void initBlockedPakages() {
             int dataSubId = SubscriptionManager.INVALID_SUBSCRIPTION_ID;
             int dataPhoneId;
             Message msg;

             Log.d(TAG,"initBlockedPakages: E ");

             //if mSocket is null,init again after INIT_RETRY_DELAY
             if (mSocket == null) {

                 mInitTimes++;
                 Log.d(TAG,"initBlockedPakages: mInitTimes = " + mInitTimes);

                 if (mInitTimes == MAX_INIT_TRY_TIMES) {
                     Log.d(TAG,"initBlockedPakages: come to max times,stop and send suspended cmds");
                     mInitDone = true;
                     mInitTimes = 0;

                     synchronized (mSendCmdLock) {
                         if (suspendedCmds.size() > 0) {
                             sendSuspendedCmds();
                         }
                     }
                     return;
                 }

                 msg = mHandler.obtainMessage(EVENT_INIT_BLOCKED_PACKAGES);
                 mHandler.sendMessageDelayed(msg, INIT_RETRY_DELAY);
                 return;
             } else if (mSocket != null) {

                 //query data sub
                 dataSubId = getDataSubscription();
                 Log.d(TAG,"initBlockedPakages: dataSubId = " + dataSubId);

                 if (dataSubId == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
                     mWaitDatasubTimes++;
                     if (mWaitDatasubTimes < MAX_WAIT_DATASUB_TIMES) {
                         Log.d(TAG,"initBlockedPakages: wait for a short time,then set dataPhoneId to 0 for default");
                         msg = mHandler.obtainMessage(EVENT_INIT_BLOCKED_PACKAGES);
                         mHandler.sendMessageDelayed(msg, INIT_RETRY_DELAY);
                         return;
                     } else if (mWaitDatasubTimes == MAX_WAIT_DATASUB_TIMES) {
                         mWaitDatasubTimes = 0;
                     }
                 }
             }

             if (dataSubId == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
                 dataPhoneId = 0;
             } else {
                 dataPhoneId = SubscriptionManager.getPhoneId(dataSubId);
                 Log.d(TAG,"initBlockedPakages: dataPhoneId = " + dataPhoneId);
                 if (!SubscriptionManager.isValidPhoneId(dataPhoneId)) {
                     dataPhoneId = 0;
                 }
             }

             synchronized (mSendCmdLock) {
                 setConstantCmds();
                 setDataSimCmds(dataPhoneId);

                 handleInitAppDatabase();

                 mInitDone = true;

                 Log.d(TAG, "initBlockedPakages: suspendedCmds.size() = " + suspendedCmds.size());
                 if (suspendedCmds.size() > 0) {
                     sendSuspendedCmds();
                 }
             }
         }

         private void sendSuspendedCmds() {
             Log.d(TAG, "sendSuspendedCmds: E");
             for(String cmd : suspendedCmds) {
                 send(cmd);
             }
             suspendedCmds.clear();
         }

        private void handleInitAppDatabase() {
            Cursor cursor = null;
            Message msg;
            int uid = 0xffff;
            int blockedValue = 0;
            int INDEX_LABEL = 0;
            int INDEX_UID = 1;
            int INDEX_STATE = 2;
            String COLUMN_PKG_NAME = "packagename";
            String COLUMN_UID = "uid";
            String COLUMN_BLOCK_STATE = "blockstate";
            String[] QUERY_PRJECTION = {COLUMN_PKG_NAME, COLUMN_UID, COLUMN_BLOCK_STATE};

            //query app provider
            cursor = mContext.getContentResolver().query(PACKAGE_BLOCK_URI, QUERY_PRJECTION, null, null, null);
            Log.d(TAG, "handleInitAppDatabase: E");

            if (cursor != null) {
                 mWaitDatabaseTimes = 0;
                 //assemble and send iptables according to results queried
                 try{
                     while(cursor.moveToNext()) {

                         uid = cursor.getInt(INDEX_UID);
                         blockedValue = cursor.getInt(INDEX_STATE);

                         Log.d(TAG, "handleInitAppDatabase: label = " + cursor.getString(INDEX_LABEL) + ", UID="
                                   + uid + ", state=" + blockedValue);
                         setUidCmds(uid,blockedValue);
                         }
                 } finally {
                     cursor.close();
                 }
             } else {
                 mWaitDatabaseTimes++;
                 Log.d(TAG,"handleInitAppDatabase: database is empty, mWaitDatabaseTimes = " + mWaitDatabaseTimes);

                 if (mWaitDatabaseTimes < MAX_INIT_TRY_TIMES) {
                     msg = mHandler.obtainMessage(EVENT_INIT_APP_DATABASE);
                     mHandler.sendMessageDelayed(msg, INIT_RETRY_DELAY);
                 } else if (mWaitDatabaseTimes == MAX_INIT_TRY_TIMES) {
                     mWaitDatabaseTimes = 0;
                 }
             }
        }

    }



    /* Gets User preferred Data subscription setting*/
    protected int getDataSubscription() {
        int subId = SubscriptionManager.INVALID_SUBSCRIPTION_ID;

        try {
            subId = Settings.Global.getInt(mContext.getContentResolver(),
                    Settings.Global.MULTI_SIM_DATA_CALL_SUBSCRIPTION);
        } catch (SettingNotFoundException snfe) {
            Log.e(TAG, "Settings Exception Reading Data Sub Values");
        }

        return subId;
    }

    private int readDaemonMessage(InputStream is, byte[] buffer)
            throws IOException {
        int countRead;
        int offset;
        int remaining;
        int messageLength;

        // First, read in the length of the message
        offset = 0;
        remaining = 4;
        do {
            countRead = is.read(buffer, offset, remaining);

            if (countRead < 0 ) {
                Log.e(TAG, "Hit EOS reading message length");
                return -1;
            }

            offset += countRead;
            remaining -= countRead;
        } while (remaining > 0);

        messageLength = ((buffer[0] & 0xff) << 24)
                                   | ((buffer[1] & 0xff) << 16)
                                   | ((buffer[2] & 0xff) << 8)
                                   | (buffer[3] & 0xff);

        // Then, re-use the buffer and read in the message itself
        offset = 0;
        remaining = messageLength;
        do {
            countRead = is.read(buffer, offset, remaining);

            if (countRead < 0 ) {
                Log.e(TAG, "Hit EOS reading message.  messageLength=" + messageLength
                        + " remaining=" + remaining);
                return -1;
            }

            offset += countRead;
            remaining -= countRead;
        } while (remaining > 0);

        return messageLength;
    }

    private class DaemonReceiver implements Runnable{
        byte[] buff;

        DaemonReceiver() {
            buff = new byte[30];
        }

        @Override
        public void
        run() {
            int retryCount = 0;
            String daemonSocket = "iptablesserver";

            try {for (;;) {
                LocalSocket s = null;
                LocalSocketAddress l;

                try {
                    s = new LocalSocket();
                    l = new LocalSocketAddress(daemonSocket,
                            LocalSocketAddress.Namespace.RESERVED);
                    s.connect(l);
                } catch (IOException ex){
                    Log.e(TAG,"DaemonReceiver: socket connect exception");
                    ex.printStackTrace();

                    try {
                        if (s != null) {
                            s.close();
                        }
                    } catch (IOException ex2) {
                        //ignore failure to close after failure to connect
                    }

                    retryCount++;
                    if (retryCount < MAX_SOCKET_RETRY_TIMES) {
                        Log.d (TAG, "DaemonReceiver: Couldn't find '" + daemonSocket
                            + "' socket; retrying after timeout");

                        try {
                            Thread.sleep(SOCKET_RETRY_MILLIS);
                        } catch (InterruptedException er) {
                        }

                        continue;
                    }
                }

                if (retryCount < MAX_SOCKET_RETRY_TIMES) {
                    retryCount = 0;
                    mSocket = s;
                    Log.d(TAG, "DaemonReceiver: Connected to '" + daemonSocket + "' socket");

                    int length = 0;
                    String resultReceived;
                    try {
                        InputStream is = mSocket.getInputStream();

                        for (;;) {
                            Arrays.fill(buff, (byte)0);
                            length = readDaemonMessage(is, buff);
                            if (length < 0) {
                                // End-of-stream reached
                                break;
                            }

                            resultReceived = new String(buff, 0, length);
                            Log.d(TAG, "DaemonReceiver: resultReceived  = " + resultReceived);
                        }
                    } catch (java.io.IOException ex) {
                        Log.d(TAG, "'" + daemonSocket + "' socket closed", ex);
                    } catch (Throwable tr) {
                        Log.e(TAG, "DaemonReceiver: Uncaught exception " + tr.toString());
                    }
                }

                Log.d(TAG, "DaemonReceiver: Disconnected from '" + daemonSocket + "' socket");
                retryCount = 0;
                try {
                    mSocket.close();
                } catch (IOException ex) {
                }
                mSocket = null;
                //stopUdpDataStallAlarm();
            }} catch (Throwable tr) {
                Log.e(TAG,"Uncaught exception", tr);
            }
        }
    }

    private void send(String iptablesRule) {
        Message msg;
        String cmdSent;

        if (mSocket == null ) {
            Log.d(TAG, "send error--mSocket is null ");
            return;
        }

        mCmdRunning = iptablesRule;

        msg = mSender.obtainMessage(EVENT_SEND, mCmdRunning);
        msg.sendToTarget();
    }

    private class DaemonSender extends Handler implements Runnable{
        public DaemonSender(Looper looper) {
            super(looper);
        }

        byte[] dataLength = new byte[2];

        //***** Runnable implementation
        @Override
        public void
        run() {
            //setup if needed
        }

        //***** Handler implementation
        @Override public void
        handleMessage(Message msg) {
            String iptablesCommand = (String)msg.obj;

            switch (msg.what) {
                case EVENT_SEND:
                    try {
                        LocalSocket s;
                        s = mSocket;

                        if (s == null) {
                            Log.d(TAG, "DaemonSender: error--mSocket is null ");
                            return;
                        }
                        synchronized (mDaemonLock) {
                            byte[] data;
                            data = iptablesCommand.getBytes();
                            Log.d(TAG,"EVENT_SEND: iptablesCommand = " + iptablesCommand);

                            // parcel length in big endian
                            dataLength[0] = (byte) (data.length & 0xff);
                            dataLength[1] = (byte) ((data.length >> 8) & 0xff);

                            s.getOutputStream().write(dataLength, 0 ,2);
                            s.getOutputStream().write(data, 0, data.length);
                        }
                    } catch (IOException ex) {
                        Log.e(TAG, "IOException " + ex);
                    } catch (RuntimeException exc) {
                        Log.e(TAG, "Uncaught exception " + exc);
                    }

                    break;
            }
        }
    }

 }
