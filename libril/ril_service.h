/*
 * Copyright (c) 2016 The Android Open Source Project
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

#ifndef RIL_SERVICE_H
#define RIL_SERVICE_H

#include <telephony/ril.h>
#include <ril_internal.h>

namespace radio {
void registerService(RIL_RadioFunctions *callbacks, android::CommandInfo *commands);

int getIccCardStatusResponse(int slotId, int responseType,
                            int token, RIL_Errno e, void *response, size_t responseLen);

int supplyIccPinForAppResponse(int slotId,
                              int responseType, int serial, RIL_Errno e, void *response,
                              size_t responseLen);

int supplyIccPukForAppResponse(int slotId,
                              int responseType, int serial, RIL_Errno e, void *response,
                              size_t responseLen);

int supplyIccPin2ForAppResponse(int slotId,
                               int responseType, int serial, RIL_Errno e, void *response,
                               size_t responseLen);

int supplyIccPuk2ForAppResponse(int slotId,
                               int responseType, int serial, RIL_Errno e, void *response,
                               size_t responseLen);

int changeIccPinForAppResponse(int slotId,
                              int responseType, int serial, RIL_Errno e, void *response,
                              size_t responseLen);

int changeIccPin2ForAppResponse(int slotId,
                               int responseType, int serial, RIL_Errno e, void *response,
                               size_t responseLen);

int supplyNetworkDepersonalizationResponse(int slotId,
                                          int responseType, int serial, RIL_Errno e,
                                          void *response, size_t responseLen);

int getCurrentCallsResponse(int slotId,
                           int responseType, int serial, RIL_Errno e, void *response,
                           size_t responseLen);

int dialResponse(int slotId,
                int responseType, int serial, RIL_Errno e, void *response, size_t responseLen);

int getIMSIForAppResponse(int slotId, int responseType,
                         int serial, RIL_Errno e, void *response, size_t responseLen);

int hangupConnectionResponse(int slotId, int responseType,
                            int serial, RIL_Errno e, void *response, size_t responseLen);

int hangupWaitingOrBackgroundResponse(int slotId,
                                     int responseType, int serial, RIL_Errno e, void *response,
                                     size_t responseLen);

int hangupForegroundResumeBackgroundResponse(int slotId,
                                            int responseType, int serial, RIL_Errno e,
                                            void *response, size_t responseLen);

int switchWaitingOrHoldingAndActiveResponse(int slotId,
                                           int responseType, int serial, RIL_Errno e,
                                           void *response, size_t responseLen);

int conferenceResponse(int slotId, int responseType,
                      int serial, RIL_Errno e, void *response, size_t responseLen);

int rejectCallResponse(int slotId, int responseType,
                      int serial, RIL_Errno e, void *response, size_t responseLen);

int getLastCallFailCauseResponse(int slotId,
                                int responseType, int serial, RIL_Errno e, void *response,
                                size_t responseLen);

int getSignalStrengthResponse(int slotId,
                              int responseType, int serial, RIL_Errno e,
                              void *response, size_t responseLen);

int getVoiceRegistrationStateResponse(int slotId,
                                     int responseType, int serial, RIL_Errno e, void *response,
                                     size_t responseLen);

int getDataRegistrationStateResponse(int slotId,
                                    int responseType, int serial, RIL_Errno e, void *response,
                                    size_t responseLen);

int getOperatorResponse(int slotId,
                       int responseType, int serial, RIL_Errno e, void *response,
                       size_t responseLen);

int setRadioPowerResponse(int slotId,
                         int responseType, int serial, RIL_Errno e, void *response,
                         size_t responseLen);

int sendDtmfResponse(int slotId,
                    int responseType, int serial, RIL_Errno e, void *response,
                    size_t responseLen);

int sendSmsResponse(int slotId,
                   int responseType, int serial, RIL_Errno e, void *response,
                   size_t responseLen);

int sendSMSExpectMoreResponse(int slotId,
                             int responseType, int serial, RIL_Errno e, void *response,
                             size_t responseLen);

int setupDataCallResponse(int slotId,
                          int responseType, int serial, RIL_Errno e, void *response,
                          size_t responseLen);

int iccIOForAppResponse(int slotId,
                       int responseType, int serial, RIL_Errno e, void *response,
                       size_t responseLen);

int sendUssdResponse(int slotId,
                    int responseType, int serial, RIL_Errno e, void *response,
                    size_t responseLen);

int cancelPendingUssdResponse(int slotId,
                             int responseType, int serial, RIL_Errno e, void *response,
                             size_t responseLen);

int getClirResponse(int slotId,
                   int responseType, int serial, RIL_Errno e, void *response, size_t responseLen);

int setClirResponse(int slotId,
                   int responseType, int serial, RIL_Errno e, void *response, size_t responseLen);

int getCallForwardStatusResponse(int slotId,
                                int responseType, int serial, RIL_Errno e, void *response,
                                size_t responseLen);

int setCallForwardResponse(int slotId,
                          int responseType, int serial, RIL_Errno e, void *response,
                          size_t responseLen);

int getCallWaitingResponse(int slotId,
                          int responseType, int serial, RIL_Errno e, void *response,
                          size_t responseLen);

int setCallWaitingResponse(int slotId,
                          int responseType, int serial, RIL_Errno e, void *response,
                          size_t responseLen);

int acknowledgeLastIncomingGsmSmsResponse(int slotId,
                                         int responseType, int serial, RIL_Errno e, void *response,
                                         size_t responseLen);

int acceptCallResponse(int slotId,
                      int responseType, int serial, RIL_Errno e, void *response,
                      size_t responseLen);

int deactivateDataCallResponse(int slotId,
                              int responseType, int serial, RIL_Errno e, void *response,
                              size_t responseLen);

int getFacilityLockForAppResponse(int slotId,
                                 int responseType, int serial, RIL_Errno e, void *response,
                                 size_t responseLen);

int setFacilityLockForAppResponse(int slotId,
                                 int responseType, int serial, RIL_Errno e, void *response,
                                 size_t responseLen);

int setBarringPasswordResponse(int slotId,
                              int responseType, int serial, RIL_Errno e, void *response,
                              size_t responseLen);

int getNetworkSelectionModeResponse(int slotId,
                                   int responseType, int serial, RIL_Errno e, void *response,
                                   size_t responseLen);

int setNetworkSelectionModeAutomaticResponse(int slotId,
                                            int responseType, int serial, RIL_Errno e,
                                            void *response, size_t responseLen);

int setNetworkSelectionModeManualResponse(int slotId,
                                         int responseType, int serial, RIL_Errno e, void *response,
                                         size_t responseLen);

int getAvailableNetworksResponse(int slotId,
                                int responseType, int serial, RIL_Errno e, void *response,
                                size_t responseLen);

int startDtmfResponse(int slotId,
                     int responseType, int serial, RIL_Errno e, void *response,
                     size_t responseLen);

int stopDtmfResponse(int slotId,
                    int responseType, int serial, RIL_Errno e, void *response,
                    size_t responseLen);

int getBasebandVersionResponse(int slotId,
                              int responseType, int serial, RIL_Errno e, void *response,
                              size_t responseLen);

int separateConnectionResponse(int slotId,
                              int responseType, int serial, RIL_Errno e, void *response,
                              size_t responseLen);

int setMuteResponse(int slotId,
                   int responseType, int serial, RIL_Errno e, void *response,
                   size_t responseLen);

int getMuteResponse(int slotId,
                   int responseType, int serial, RIL_Errno e, void *response,
                   size_t responseLen);

int getClipResponse(int slotId,
                   int responseType, int serial, RIL_Errno e, void *response,
                   size_t responseLen);

int getDataCallListResponse(int slotId,
                            int responseType, int serial, RIL_Errno e,
                            void *response, size_t responseLen);

int setSuppServiceNotificationsResponse(int slotId,
                                       int responseType, int serial, RIL_Errno e, void *response,
                                       size_t responseLen);

int writeSmsToSimResponse(int slotId,
                         int responseType, int serial, RIL_Errno e, void *response,
                         size_t responseLen);

int deleteSmsOnSimResponse(int slotId,
                          int responseType, int serial, RIL_Errno e, void *response,
                          size_t responseLen);

int setBandModeResponse(int slotId,
                       int responseType, int serial, RIL_Errno e, void *response,
                       size_t responseLen);

int getAvailableBandModesResponse(int slotId,
                                 int responseType, int serial, RIL_Errno e, void *response,
                                 size_t responseLen);

int sendEnvelopeResponse(int slotId,
                        int responseType, int serial, RIL_Errno e, void *response,
                        size_t responseLen);

int sendTerminalResponseToSimResponse(int slotId,
                                     int responseType, int serial, RIL_Errno e, void *response,
                                     size_t responseLen);

int handleStkCallSetupRequestFromSimResponse(int slotId,
                                            int responseType, int serial, RIL_Errno e,
                                            void *response, size_t responseLen);

int explicitCallTransferResponse(int slotId,
                                int responseType, int serial, RIL_Errno e, void *response,
                                size_t responseLen);

int setPreferredNetworkTypeResponse(int slotId,
                                   int responseType, int serial, RIL_Errno e, void *response,
                                   size_t responseLen);

int getPreferredNetworkTypeResponse(int slotId,
                                   int responseType, int serial, RIL_Errno e, void *response,
                                   size_t responseLen);

int getNeighboringCidsResponse(int slotId,
                              int responseType, int serial, RIL_Errno e, void *response,
                              size_t responseLen);

int setLocationUpdatesResponse(int slotId,
                              int responseType, int serial, RIL_Errno e, void *response,
                              size_t responseLen);

int setCdmaSubscriptionSourceResponse(int slotId,
                                     int responseType, int serial, RIL_Errno e, void *response,
                                     size_t responseLen);

int setCdmaRoamingPreferenceResponse(int slotId,
                                    int responseType, int serial, RIL_Errno e, void *response,
                                    size_t responseLen);

int getCdmaRoamingPreferenceResponse(int slotId,
                                    int responseType, int serial, RIL_Errno e, void *response,
                                    size_t responseLen);

int setTTYModeResponse(int slotId,
                      int responseType, int serial, RIL_Errno e, void *response,
                      size_t responseLen);

int getTTYModeResponse(int slotId,
                      int responseType, int serial, RIL_Errno e, void *response,
                      size_t responseLen);

int setPreferredVoicePrivacyResponse(int slotId,
                                    int responseType, int serial, RIL_Errno e, void *response,
                                    size_t responseLen);

int getPreferredVoicePrivacyResponse(int slotId,
                                    int responseType, int serial, RIL_Errno e, void *response,
                                    size_t responseLen);

int sendCDMAFeatureCodeResponse(int slotId,
                               int responseType, int serial, RIL_Errno e,
                               void *response, size_t responseLen);

int sendBurstDtmfResponse(int slotId,
                         int responseType, int serial, RIL_Errno e, void *response,
                         size_t responseLen);

int sendCdmaSmsResponse(int slotId,
                       int responseType, int serial, RIL_Errno e, void *response,
                       size_t responseLen);

int acknowledgeLastIncomingCdmaSmsResponse(int slotId,
                                          int responseType, int serial, RIL_Errno e, void *response,
                                          size_t responseLen);

int getGsmBroadcastConfigResponse(int slotId,
                                 int responseType, int serial, RIL_Errno e, void *response,
                                 size_t responseLen);

int setGsmBroadcastConfigResponse(int slotId,
                                 int responseType, int serial, RIL_Errno e, void *response,
                                 size_t responseLen);

int setGsmBroadcastActivationResponse(int slotId,
                                     int responseType, int serial, RIL_Errno e, void *response,
                                     size_t responseLen);

int getCdmaBroadcastConfigResponse(int slotId,
                                  int responseType, int serial, RIL_Errno e, void *response,
                                  size_t responseLen);

int setCdmaBroadcastConfigResponse(int slotId,
                                  int responseType, int serial, RIL_Errno e, void *response,
                                  size_t responseLen);

int setCdmaBroadcastActivationResponse(int slotId,
                                      int responseType, int serial, RIL_Errno e,
                                      void *response, size_t responseLen);

int getCDMASubscriptionResponse(int slotId,
                               int responseType, int serial, RIL_Errno e, void *response,
                               size_t responseLen);

int writeSmsToRuimResponse(int slotId,
                          int responseType, int serial, RIL_Errno e, void *response,
                          size_t responseLen);

int deleteSmsOnRuimResponse(int slotId,
                           int responseType, int serial, RIL_Errno e, void *response,
                           size_t responseLen);

int getDeviceIdentityResponse(int slotId,
                             int responseType, int serial, RIL_Errno e, void *response,
                             size_t responseLen);

int exitEmergencyCallbackModeResponse(int slotId,
                                     int responseType, int serial, RIL_Errno e, void *response,
                                     size_t responseLen);

int getSmscAddressResponse(int slotId,
                          int responseType, int serial, RIL_Errno e, void *response,
                          size_t responseLen);

int setCdmaBroadcastActivationResponse(int slotId,
                                      int responseType, int serial, RIL_Errno e,
                                      void *response, size_t responseLen);

int setSmscAddressResponse(int slotId,
                          int responseType, int serial, RIL_Errno e,
                          void *response, size_t responseLen);

int reportSmsMemoryStatusResponse(int slotId,
                                 int responseType, int serial, RIL_Errno e,
                                 void *response, size_t responseLen);

int reportStkServiceIsRunningResponse(int slotId,
                                      int responseType, int serial, RIL_Errno e,
                                      void *response, size_t responseLen);

int getCdmaSubscriptionSourceResponse(int slotId,
                                     int responseType, int serial, RIL_Errno e, void *response,
                                     size_t responseLen);

int requestIsimAuthenticationResponse(int slotId,
                                     int responseType, int serial, RIL_Errno e, void *response,
                                     size_t responseLen);

int acknowledgeIncomingGsmSmsWithPduResponse(int slotId,
                                            int responseType, int serial, RIL_Errno e,
                                            void *response, size_t responseLen);

int sendEnvelopeWithStatusResponse(int slotId,
                                  int responseType, int serial, RIL_Errno e, void *response,
                                  size_t responseLen);

int getVoiceRadioTechnologyResponse(int slotId,
                                   int responseType, int serial, RIL_Errno e,
                                   void *response, size_t responseLen);

int getCellInfoListResponse(int slotId,
                            int responseType,
                            int serial, RIL_Errno e, void *response,
                            size_t responseLen);

int setCellInfoListRateResponse(int slotId,
                               int responseType, int serial, RIL_Errno e,
                               void *response, size_t responseLen);

int setInitialAttachApnResponse(int slotId,
                               int responseType, int serial, RIL_Errno e,
                               void *response, size_t responseLen);

int getImsRegistrationStateResponse(int slotId,
                                   int responseType, int serial, RIL_Errno e,
                                   void *response, size_t responseLen);

int sendImsSmsResponse(int slotId, int responseType,
                      int serial, RIL_Errno e, void *response, size_t responseLen);

int iccTransmitApduBasicChannelResponse(int slotId,
                                       int responseType, int serial, RIL_Errno e,
                                       void *response, size_t responseLen);

int iccOpenLogicalChannelResponse(int slotId,
                                  int responseType, int serial, RIL_Errno e, void *response,
                                  size_t responseLen);


int iccCloseLogicalChannelResponse(int slotId,
                                  int responseType, int serial, RIL_Errno e,
                                  void *response, size_t responseLen);

int iccTransmitApduLogicalChannelResponse(int slotId,
                                         int responseType, int serial, RIL_Errno e,
                                         void *response, size_t responseLen);

int nvReadItemResponse(int slotId,
                      int responseType, int serial, RIL_Errno e,
                      void *response, size_t responseLen);


int nvWriteItemResponse(int slotId,
                       int responseType, int serial, RIL_Errno e,
                       void *response, size_t responseLen);

int nvWriteCdmaPrlResponse(int slotId,
                          int responseType, int serial, RIL_Errno e,
                          void *response, size_t responseLen);

int nvResetConfigResponse(int slotId,
                         int responseType, int serial, RIL_Errno e,
                         void *response, size_t responseLen);

int setUiccSubscriptionResponse(int slotId,
                               int responseType, int serial, RIL_Errno e,
                               void *response, size_t responseLen);

int setDataAllowedResponse(int slotId,
                          int responseType, int serial, RIL_Errno e,
                          void *response, size_t responseLen);

int getHardwareConfigResponse(int slotId,
                              int responseType, int serial, RIL_Errno e,
                              void *response, size_t responseLen);

int requestIccSimAuthenticationResponse(int slotId,
                                       int responseType, int serial, RIL_Errno e,
                                       void *response, size_t responseLen);

int setDataProfileResponse(int slotId,
                          int responseType, int serial, RIL_Errno e,
                          void *response, size_t responseLen);

int requestShutdownResponse(int slotId,
                           int responseType, int serial, RIL_Errno e,
                           void *response, size_t responseLen);

int getRadioCapabilityResponse(int slotId,
                               int responseType, int serial, RIL_Errno e,
                               void *response, size_t responseLen);

int setRadioCapabilityResponse(int slotId,
                               int responseType, int serial, RIL_Errno e,
                               void *response, size_t responseLen);

int startLceServiceResponse(int slotId,
                           int responseType, int serial, RIL_Errno e,
                           void *response, size_t responseLen);

int stopLceServiceResponse(int slotId,
                          int responseType, int serial, RIL_Errno e,
                          void *response, size_t responseLen);

int pullLceDataResponse(int slotId,
                        int responseType, int serial, RIL_Errno e,
                        void *response, size_t responseLen);

int getModemActivityInfoResponse(int slotId,
                                int responseType, int serial, RIL_Errno e,
                                void *response, size_t responseLen);

int setAllowedCarriersResponse(int slotId,
                              int responseType, int serial, RIL_Errno e,
                              void *response, size_t responseLen);

int getAllowedCarriersResponse(int slotId,
                              int responseType, int serial, RIL_Errno e,
                              void *response, size_t responseLen);

int sendDeviceStateResponse(int slotId,
                              int responseType, int serial, RIL_Errno e,
                              void *response, size_t responseLen);

int setIndicationFilterResponse(int slotId,
                              int responseType, int serial, RIL_Errno e,
                              void *response, size_t responseLen);

int setSimCardPowerResponse(int slotId,
                              int responseType, int serial, RIL_Errno e,
                              void *response, size_t responseLen);

void acknowledgeRequest(int slotId, int serial);

int radioStateChangedInd(int slotId,
                          int indicationType, int token, RIL_Errno e, void *response,
                          size_t responseLen);

int callStateChangedInd(int slotId, int indType, int token,
                        RIL_Errno e, void *response, size_t responseLen);

int networkStateChangedInd(int slotId, int indType,
                                int token, RIL_Errno e, void *response, size_t responseLen);

int newSmsInd(int slotId, int indicationType,
              int token, RIL_Errno e, void *response, size_t responseLen);

int newSmsStatusReportInd(int slotId, int indicationType,
                          int token, RIL_Errno e, void *response, size_t responseLen);

int newSmsOnSimInd(int slotId, int indicationType,
                   int token, RIL_Errno e, void *response, size_t responseLen);

int onUssdInd(int slotId, int indicationType,
              int token, RIL_Errno e, void *response, size_t responseLen);

int nitzTimeReceivedInd(int slotId, int indicationType,
                        int token, RIL_Errno e, void *response, size_t responseLen);

int currentSignalStrengthInd(int slotId,
                             int indicationType, int token, RIL_Errno e,
                             void *response, size_t responseLen);

int dataCallListChangedInd(int slotId, int indicationType,
                           int token, RIL_Errno e, void *response, size_t responseLen);

int suppSvcNotifyInd(int slotId, int indicationType,
                     int token, RIL_Errno e, void *response, size_t responseLen);

int stkSessionEndInd(int slotId, int indicationType,
                     int token, RIL_Errno e, void *response, size_t responseLen);

int stkProactiveCommandInd(int slotId, int indicationType,
                           int token, RIL_Errno e, void *response, size_t responseLen);

int stkEventNotifyInd(int slotId, int indicationType,
                      int token, RIL_Errno e, void *response, size_t responseLen);

int stkCallSetupInd(int slotId, int indicationType,
                    int token, RIL_Errno e, void *response, size_t responseLen);

int simSmsStorageFullInd(int slotId, int indicationType,
                         int token, RIL_Errno e, void *response, size_t responseLen);

int simRefreshInd(int slotId, int indicationType,
                  int token, RIL_Errno e, void *response, size_t responseLen);

int callRingInd(int slotId, int indicationType,
                int token, RIL_Errno e, void *response, size_t responseLen);

int simStatusChangedInd(int slotId, int indicationType,
                        int token, RIL_Errno e, void *response, size_t responseLen);

int cdmaNewSmsInd(int slotId, int indicationType,
                  int token, RIL_Errno e, void *response, size_t responseLen);

int newBroadcastSmsInd(int slotId,
                       int indicationType, int token, RIL_Errno e, void *response,
                       size_t responseLen);

int cdmaRuimSmsStorageFullInd(int slotId,
                              int indicationType, int token, RIL_Errno e, void *response,
                              size_t responseLen);

int restrictedStateChangedInd(int slotId,
                              int indicationType, int token, RIL_Errno e, void *response,
                              size_t responseLen);

int enterEmergencyCallbackModeInd(int slotId,
                                  int indicationType, int token, RIL_Errno e, void *response,
                                  size_t responseLen);

int cdmaCallWaitingInd(int slotId,
                       int indicationType, int token, RIL_Errno e, void *response,
                       size_t responseLen);

int cdmaOtaProvisionStatusInd(int slotId,
                              int indicationType, int token, RIL_Errno e, void *response,
                              size_t responseLen);

int cdmaInfoRecInd(int slotId,
                   int indicationType, int token, RIL_Errno e, void *response,
                   size_t responseLen);

int oemHookRawInd(int slotId,
                  int indicationType, int token, RIL_Errno e, void *response,
                  size_t responseLen);

int indicateRingbackToneInd(int slotId,
                            int indicationType, int token, RIL_Errno e, void *response,
                            size_t responseLen);

int resendIncallMuteInd(int slotId,
                        int indicationType, int token, RIL_Errno e, void *response,
                        size_t responseLen);

int cdmaSubscriptionSourceChangedInd(int slotId,
                                     int indicationType, int token, RIL_Errno e,
                                     void *response, size_t responseLen);

int cdmaPrlChangedInd(int slotId,
                      int indicationType, int token, RIL_Errno e, void *response,
                      size_t responseLen);

int exitEmergencyCallbackModeInd(int slotId,
                                 int indicationType, int token, RIL_Errno e, void *response,
                                 size_t responseLen);

int rilConnectedInd(int slotId,
                    int indicationType, int token, RIL_Errno e, void *response,
                    size_t responseLen);

int voiceRadioTechChangedInd(int slotId,
                             int indicationType, int token, RIL_Errno e, void *response,
                             size_t responseLen);

int cellInfoListInd(int slotId,
                    int indicationType, int token, RIL_Errno e, void *response,
                    size_t responseLen);

int imsNetworkStateChangedInd(int slotId,
                              int indicationType, int token, RIL_Errno e, void *response,
                              size_t responseLen);

int subscriptionStatusChangedInd(int slotId,
                                 int indicationType, int token, RIL_Errno e, void *response,
                                 size_t responseLen);

int srvccStateNotifyInd(int slotId,
                        int indicationType, int token, RIL_Errno e, void *response,
                        size_t responseLen);

int hardwareConfigChangedInd(int slotId,
                             int indicationType, int token, RIL_Errno e, void *response,
                             size_t responseLen);

int radioCapabilityIndicationInd(int slotId,
                                 int indicationType, int token, RIL_Errno e, void *response,
                                 size_t responseLen);

int onSupplementaryServiceIndicationInd(int slotId,
                                        int indicationType, int token, RIL_Errno e,
                                        void *response, size_t responseLen);

int stkCallControlAlphaNotifyInd(int slotId,
                                 int indicationType, int token, RIL_Errno e, void *response,
                                 size_t responseLen);

int lceDataInd(int slotId,
               int indicationType, int token, RIL_Errno e, void *response,
               size_t responseLen);

int pcoDataInd(int slotId,
               int indicationType, int token, RIL_Errno e, void *response,
               size_t responseLen);

int modemResetInd(int slotId,
                  int indicationType, int token, RIL_Errno e, void *response,
                  size_t responseLen);

int sendRequestRawResponse(int slotId,
                           int responseType, int serial, RIL_Errno e,
                           void *response, size_t responseLen);

int sendRequestStringsResponse(int slotId,
                               int responseType, int serial, RIL_Errno e,
                               void *response, size_t responseLen);

pthread_rwlock_t * getRadioServiceRwlock(int slotId);


/******************SPRD EXTENSION REQUESTs' responseFunction******************/

int videoPhoneDialResponse(int slotId, int responseType, int serial,
                           RIL_Errno e, void *response, size_t responseLen);

int videoPhoneCodecResponse(int slotId, int responseType, int serial,
                            RIL_Errno e, void *response, size_t responseLen);

int videoPhoneFallbackResponse(int slotId, int responseType, int serial,
                               RIL_Errno e, void *response, size_t responseLen);

int videoPhoneStringResponse(int slotId, int responseType, int serial,
                             RIL_Errno e, void *response, size_t responseLen);

int videoPhoneLocalMediaResponse(int slotId, int responseType, int serial,
                                 RIL_Errno e, void *response, size_t responseLen);

int videoPhoneControlIFrameResponse(int slotId, int responseType, int serial,
                                    RIL_Errno e, void *response, size_t responseLen);

int switchMultiCallResponse(int slotId, int responseType, int serial,
                            RIL_Errno e, void *response, size_t responseLen);

int setTrafficClassResponse(int slotId, int responseType, int serial,
                            RIL_Errno e, void *response, size_t responseLen);

int enableLTEResponse(int slotId, int responseType, int serial,
                      RIL_Errno e, void *response, size_t responseLen);

int attachDataResponse(int slotId, int responseType, int serial,
                       RIL_Errno e, void *response, size_t responseLen);

int stopQueryNetworkResponse(int slotId, int responseType, int serial,
                             RIL_Errno e, void *response, size_t responseLen);

int forceDeatchResponse(int slotId, int responseType, int serial,
                        RIL_Errno e, void *response, size_t responseLen);

int getHDVoiceStateResponse(int slotId, int responseType, int serial,
                            RIL_Errno e, void *response, size_t responseLen);

int simmgrSimPowerResponse(int slotId, int responseType, int serial,
                           RIL_Errno e, void *response, size_t responseLen);

int enableRauNotifyResponse(int slotId, int responseType, int serial,
                            RIL_Errno e, void *response, size_t responseLen);

int setCOLPResponse(int slotId, int responseType, int serial,
                    RIL_Errno e, void *response, size_t responseLen);

int getDefaultNANResponse(int slotId, int responseType, int serial,
                          RIL_Errno e, void *response, size_t responseLen);

int simGetAtrResponse(int slotId, int responseType, int serial,
                      RIL_Errno e, void *response, size_t responseLen);

int simOpenChannelWithP2Response(int slotId, int responseType, int serial,
                                 RIL_Errno e, void *response, size_t responseLen);

int getSimCapacityResponse(int slotId, int responseType, int serial,
                           RIL_Errno e, void *response, size_t responseLen);

int storeSmsToSimResponse(int slotId, int responseType, int serial,
                          RIL_Errno e, void *response, size_t responseLen);

int querySmsStorageModeResponse(int slotId, int responseType, int serial,
                                RIL_Errno e, void *response, size_t responseLen);

int getSimlockRemaintimesResponse(int slotId, int responseType, int serial,
                                  RIL_Errno e, void *response, size_t responseLen);

int setFacilityLockForUserResponse(int slotId, int responseType, int serial,
                                   RIL_Errno e, void *response, size_t responseLen);

int getSimlockStatusResponse(int slotId, int responseType, int serial,
                             RIL_Errno e, void *response, size_t responseLen);

int getSimlockDummysResponse(int slotId, int responseType, int serial,
                             RIL_Errno e, void *response, size_t responseLen);

int getSimlockWhitelistResponse(int slotId, int responseType, int serial,
                                RIL_Errno e, void *response, size_t responseLen);

int updateEcclistResponse(int slotId, int responseType, int serial,
                          RIL_Errno e, void *response, size_t responseLen);

int getBandInfoResponse(int slotId, int responseType, int serial,
                        RIL_Errno e, void *response, size_t responseLen);

int setBandInfoModeResponse(int slotId, int responseType, int serial,
                            RIL_Errno e, void *response, size_t responseLen);

int setSinglePDNResponse(int slotId, int responseType, int serial,
                         RIL_Errno e, void *response, size_t responseLen);

int setSpecialRatcapResponse(int slotId, int responseType, int serial,
                             RIL_Errno e, void *response, size_t responseLen);

int queryColpResponse(int slotId, int responseType, int serial,
                      RIL_Errno e, void *response, size_t responseLen);

int queryColrResponse(int slotId, int responseType, int serial,
                      RIL_Errno e, void *response, size_t responseLen);

int mmiEnterSimResponse(int slotId, int responseType, int serial,
                        RIL_Errno e, void *response, size_t responseLen);

int updateOperatorNameResponse(int slotId, int responseType, int serial,
                               RIL_Errno e, void *response, size_t responseLen);

int simmgrGetSimStatusResponse(int slotId, int responseType, int serial,
                               RIL_Errno e, void *response, size_t responseLen);

int setXcapIPAddressResponse(int slotId, int responseType, int serial,
                             RIL_Errno e, void *response, size_t responseLen);

int sendCmdAsyncResponse(int slotId, int responseType, int serial,
                         RIL_Errno e, void *response, size_t responseLen);

/**************SPRD EXTENSION UNSOL RESPONSEs' responsFunction****************/

int videoPhoneCodecInd(int slotId, int indicationType, int token,
                       RIL_Errno e, void *response, size_t responseLen);

int videoPhoneDSCIInd(int slotId, int indicationType, int token,
                      RIL_Errno e, void *response, size_t responseLen);

int videoPhoneStringInd(int slotId, int indicationType, int token,
                        RIL_Errno e, void *response, size_t responseLen);

int videoPhoneRemoteMediaInd(int slotId, int indicationType, int token,
                             RIL_Errno e, void *response, size_t responseLen);

int videoPhoneMMRingInd(int slotId, int indicationType, int token,
                        RIL_Errno e, void *response, size_t responseLen);

int videoPhoneReleasingInd(int slotId, int indicationType, int token,
                           RIL_Errno e, void *response, size_t responseLen);

int videoPhoneRecordVideoInd(int slotId, int indicationType, int token,
                             RIL_Errno e, void *response, size_t responseLen);

int videoPhoneMediaStartInd(int slotId, int indicationType, int token,
                            RIL_Errno e, void *response, size_t responseLen);

int eccNetworkListChangedInd(int slotId, int indicationType, int token,
                             RIL_Errno e, void *response, size_t responseLen);

int rauSuccessInd(int slotId, int indicationType, int token,
                  RIL_Errno e, void *response, size_t responseLen);

int clearCodeFallbackInd(int slotId, int indicationType, int token,
                         RIL_Errno e, void *response, size_t responseLen);

int extRILConnectedInd(int slotId, int indicationType, int token,
                       RIL_Errno e, void *response, size_t responseLen);

int simlockStatusChangedInd(int slotId, int indicationType, int token,
                            RIL_Errno e, void *response, size_t responseLen);

int simlockSimExpiredInd(int slotId, int indicationType, int token,
                         RIL_Errno e, void *response, size_t responseLen);

int bandInfoInd(int slotId, int indicationType, int token,
                RIL_Errno e, void *response, size_t responseLen);

int switchPrimaryCardInd(int slotId, int indicationType, int token,
                         RIL_Errno e, void *response, size_t responseLen);

int simPSRejectInd(int slotId, int indicationType, int token,
                   RIL_Errno e, void *response, size_t responseLen);

/*******************IMS EXTENSION REQUESTs' responseFunction******************/

int getIMSCurrentCallsResponse(int slotId, int responseType, int serial,
                               RIL_Errno e, void *response, size_t responseLen);

int setIMSVoiceCallAvailabilityResponse(int slotId, int responseType, int serial,
                                        RIL_Errno e, void *response, size_t responseLen);

int getIMSVoiceCallAvailabilityResponse(int slotId, int responseType, int serial,
                                        RIL_Errno e, void *response, size_t responseLen);

int initISIMResponse(int slotId, int responseType, int serial,
                     RIL_Errno e, void *response, size_t responseLen);

int requestVolteCallMediaChangeResponse(int slotId, int responseType, int serial,
                                        RIL_Errno e, void *response, size_t responseLen);

int responseVolteCallMediaChangeResponse(int slotId, int responseType, int serial,
                                         RIL_Errno e, void *response, size_t responseLen);

int setIMSSmscAddressResponse(int slotId, int responseType, int serial,
                              RIL_Errno e, void *response, size_t responseLen);

int volteCallFallBackToVoiceResponse(int slotId, int responseType, int serial,
                                     RIL_Errno e, void *response, size_t responseLen);

int setIMSInitialAttachApnResponse(int slotId, int responseType, int serial,
                                   RIL_Errno e, void *response, size_t responseLen);

int queryCallForwardStatusResponse(int slotId, int responseType, int serial,
                                   RIL_Errno e, void *response, size_t responseLen);

int setCallForwardUriResponse(int slotId, int responseType, int serial,
                              RIL_Errno e, void *response, size_t responseLen);

int IMSInitialGroupCallResponse(int slotId, int responseType, int serial,
                                RIL_Errno e, void *response, size_t responseLen);

int IMSAddGroupCallResponse(int slotId, int responseType, int serial,
                            RIL_Errno e, void *response, size_t responseLen);

int enableIMSResponse(int slotId, int responseType, int serial,
                      RIL_Errno e, void *response, size_t responseLen);

int disableIMSResponse(int slotId, int responseType, int serial,
                       RIL_Errno e, void *response, size_t responseLen);

int getIMSBearerStateResponse(int slotId, int responseType, int serial,
                              RIL_Errno e, void *response, size_t responseLen);

int setInitialAttachSOSApnResponse(int slotId, int responseType, int serial,
                                   RIL_Errno e, void *response, size_t responseLen);

int IMSHandoverResponse(int slotId, int responseType, int serial,
                        RIL_Errno e, void *response, size_t responseLen);

int notifyIMSHandoverStatusUpdateResponse(int slotId, int responseType, int serial,
                                          RIL_Errno e, void *response, size_t responseLen);

int notifyIMSNetworkInfoChangedResponse(int slotId, int responseType, int serial,
                                        RIL_Errno e, void *response, size_t responseLen);

int notifyIMSCallEndResponse(int slotId, int responseType, int serial,
                             RIL_Errno e, void *response, size_t responseLen);

int getTPMRStateResponse(int slotId, int responseType, int serial,
                         RIL_Errno e, void *response, size_t responseLen);

int setTPMRStateResponse(int slotId, int responseType, int serial,
                         RIL_Errno e, void *response, size_t responseLen);

int notifyVoWifiEnableResponse(int slotId, int responseType, int serial,
                               RIL_Errno e, void *response, size_t responseLen);

int notifyVoWifiCallStateChangedResponse(int slotId, int responseType, int serial,
                                         RIL_Errno e, void *response, size_t responseLen);

int notifyDataRouterUpdateResponse(int slotId, int responseType, int serial,
                                   RIL_Errno e, void *response, size_t responseLen);

int IMSHoldSingleCallResponse(int slotId, int responseType, int serial,
                              RIL_Errno e, void *response, size_t responseLen);

int IMSMuteSingleCallResponse(int slotId, int responseType, int serial,
                              RIL_Errno e, void *response, size_t responseLen);

int IMSSilenceSingleCallResponse(int slotId, int responseType, int serial,
                                 RIL_Errno e, void *response, size_t responseLen);

int IMSEnableLocalConferenceResponse(int slotId, int responseType, int serial,
                                     RIL_Errno e, void *response, size_t responseLen);

int notifyHandoverCallInfoResponse(int slotId, int responseType, int serial,
                                   RIL_Errno e, void *response, size_t responseLen);

int getSrvccCapbilityResponse(int slotId, int responseType, int serial,
                              RIL_Errno e, void *response, size_t responseLen);

int getIMSPcscfAddressResponse(int slotId, int responseType, int serial,
                               RIL_Errno e, void *response, size_t responseLen);

int setIMSPcscfAddressResponse(int slotId, int responseType, int serial,
                               RIL_Errno e, void *response, size_t responseLen);

/****************IMS EXTENSION UNSOL RESPONSEs' responseFunction**************/

int IMSCallStateChangedInd(int slotId, int indicationType, int token,
                           RIL_Errno e, void *response, size_t responseLen);

int videoQualityInd(int slotId, int indicationType, int token,
                    RIL_Errno e, void *response, size_t responseLen);

int IMSBearerEstablished(int slotId, int indicationType, int token,
                       RIL_Errno e, void *response, size_t responseLen);

int IMSHandoverRequestInd(int slotId, int indicationType, int token,
                          RIL_Errno e, void *response, size_t responseLen);

int IMSHandoverStatusChangedInd(int slotId, int indicationType, int token,
                                RIL_Errno e, void *response, size_t responseLen);

int IMSNetworkInfoChangedInd(int slotId, int indicationType, int token,
                             RIL_Errno e, void *response, size_t responseLen);

int IMSRegisterAddressChangedInd(int slotId, int indicationType, int token,
                                 RIL_Errno e, void *response, size_t responseLen);

int IMSWifiParamInd(int slotId, int indicationType, int token,
                                 RIL_Errno e, void *response, size_t responseLen);

int IMSNetworkStateChangedInd(int slotId, int indicationType, int token,
                              RIL_Errno e, void *response, size_t responseLen);

/*****************************************************************************/
}   // namespace radio

#endif  // RIL_SERVICE_H
