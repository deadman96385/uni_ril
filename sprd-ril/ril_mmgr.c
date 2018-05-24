#define LOG_TAG "RIL"

#include "sprd_ril.h"
#include "ril_mmgr.h"

static int freeVoid(void *data, size_t datalen);
static int freeInts(void *data, size_t datalen);
static int freeString(void *data, size_t datalen);
static int freeStrings(void *data, size_t datalen);
static int freeRaw(void *data, size_t datalen);
static int freeDial(void *data, size_t datalen);
static int freeSIM_IO(void *data, size_t datalen);
static int freeCallForwardStatus(void *data, size_t datalen);
static int freeWriteSmsToSim(void *data, size_t datalen);
static int freeSendCdmaSms(void *data, size_t datalen);
static int freeAcknowledgeLastIncomingCdmaSms(void *data, size_t datalen);
static int freeSetGsmBroadcastConfig(void *data, size_t datalen);
static int freeSetCdmaBroadcastConfig(void *data, size_t datalen);
static int freeWriteSmsToRuim(void *data, size_t datalen);
static int freeSetInitialAttachApn(void *data, size_t datalen);
static int freeSendImsSms(void *data, size_t datalen);
static int freeIccApdu(void *data, size_t datalen);
static int freeIccOpenLogicalChannel(void *data, size_t datalen);
static int freeNvReadItem(void *data, size_t datalen);
static int freeNvWriteItem(void *data, size_t datalen);
static int freeSetUiccSubscription(void *data, size_t datalen);
static int freeIccSimAuthentication(void *data, size_t datalen);
static int freeSetDataProfileData(void *data, size_t datalen);
static int freeSetRadioCapability(void *data, size_t datalen);
static int freeSetAllowedCarriers(void *data, size_t datalen);

static int freeVideoPhoneDial(void *data, size_t datalen);
static int freeVideoPhoneCodec(void *data, size_t datalen);

static int freeCallForwardUri(void *data, size_t datalen);
static int freeNotifyIMSNetworkInfoChanged(void *data, size_t datalen);

static int freeSetCarrierImsiEncryption(void *data, size_t datalen);
static int freeStartNetworkScan(void *data, size_t datalen);
static int freeStartKeepAlive(void *data, size_t datalen);

MemoryManager s_memoryManager[] = {
#include "ril_aosp_mmgr.h"
};
MemoryManager s_oemMemoryManager[] = {
#include "ril_oem_mmgr.h"
};
MemoryManager s_imsMemoryManager[] = {
#include "ril_ims_mmgr.h"
};

static void memsetString(char *s) {
    if (s != NULL) {
        memset(s, 0, strlen(s));
    }
}

static int freeVoid(void *data, size_t datalen) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    return 0;
}

static int freeInts(void *data, size_t datalen) {
    memset(data, 0, datalen);
    free(data);

    return 0;
}

static int freeString(void *data, size_t datalen) {
    RIL_UNUSED_PARM(datalen);

    memsetString(data);
    free(data);

    return 0;
}

static int freeStrings(void *data, size_t datalen) {
    int countStrings = datalen / sizeof(char *);
    char **pStrings = (char **)data;

    if (pStrings != NULL) {
        for (int i = 0; i < countStrings; i++) {
            memsetString(pStrings[i]);
            free(pStrings[i]);
        }
        memset(pStrings, 0, datalen);
        free(pStrings);
    }

    return 0;
}

static int freeRaw(void *data, size_t datalen) {
    memset(data, 0, datalen);
    free(data);

    return 0;
}

static int freeDial(void *data, size_t datalen) {
    RIL_UNUSED_PARM(datalen);

    RIL_Dial *pDial = (RIL_Dial *)data;
    RIL_UUS_Info *pUusInfo = pDial->uusInfo;

    if (pUusInfo != NULL) {
        memsetString(pUusInfo->uusData);
        free(pUusInfo->uusData);
        memset(pUusInfo, 0, sizeof(RIL_UUS_Info));
        free(pUusInfo);
    }
    memsetString(pDial->address);
    free(pDial->address);

    memset(pDial, 0, sizeof(RIL_Dial));
    free(pDial);

    return 0;
}

static int freeSIM_IO(void *data, size_t datalen) {
    RIL_SIM_IO_v6 *pSimIO = (RIL_SIM_IO_v6 *)data;

    memsetString(pSimIO->path);
    memsetString(pSimIO->data);
    memsetString(pSimIO->pin2);
    memsetString(pSimIO->aidPtr);

    free(pSimIO->path);
    free(pSimIO->data);
    free(pSimIO->pin2);
    free(pSimIO->aidPtr);

    memset(pSimIO, 0, datalen);
    free(pSimIO);

    return 0;
}

static int freeCallForwardStatus(void *data, size_t datalen) {
    RIL_CallForwardInfo *pCF = (RIL_CallForwardInfo *)data;

    memsetString(pCF->number);
    free(pCF->number);

    memset(pCF, 0, datalen);
    free(pCF);

    return 0;
}

static int freeWriteSmsToSim(void *data, size_t datalen) {
    RIL_SMS_WriteArgs *pArgs = (RIL_SMS_WriteArgs *)data;

    memsetString(pArgs->smsc);
    memsetString(pArgs->pdu);

    free(pArgs->smsc);
    free(pArgs->pdu);

    memset(pArgs, 0, datalen);
    free(pArgs);

    return 0;
}

static int freeSendCdmaSms(void *data, size_t datalen) {
    RIL_CDMA_SMS_Message *rcsm = (RIL_CDMA_SMS_Message *)data;

    memset(rcsm, 0, datalen);
    free(rcsm);

    return 0;
}

static int freeAcknowledgeLastIncomingCdmaSms(void *data, size_t datalen) {
    RIL_CDMA_SMS_Ack *rcsa = (RIL_CDMA_SMS_Ack *)data;

    memset(rcsa, 0, datalen);
    free(rcsa);

    return 0;
}

static int freeSetGsmBroadcastConfig(void *data, size_t datalen) {
    RIL_GSM_BroadcastSmsConfigInfo **gsmBci =
            (RIL_GSM_BroadcastSmsConfigInfo **)data;
    int num = datalen / sizeof(RIL_GSM_BroadcastSmsConfigInfo *);

    int i = 0;
    for (i = 0; i < num; i++) {
        memset(gsmBci[i], 0, sizeof(RIL_GSM_BroadcastSmsConfigInfo));
        free(gsmBci[i]);
    }

    memset(gsmBci, 0, datalen);
    free(gsmBci);

    return 0;
}

static int freeSetCdmaBroadcastConfig(void *data, size_t datalen) {
    RIL_CDMA_BroadcastSmsConfigInfo **cdmaBci =
            (RIL_CDMA_BroadcastSmsConfigInfo **)data;
    int num = datalen / sizeof(RIL_CDMA_BroadcastSmsConfigInfo *);

    int i = 0;
    for (i = 0; i < num; i++) {
        memset(cdmaBci[i], 0, sizeof(RIL_CDMA_BroadcastSmsConfigInfo));
        free(cdmaBci[i]);
    }

    memset(cdmaBci, 0, datalen);
    free(cdmaBci);

    return 0;
}

static int freeWriteSmsToRuim(void *data, size_t datalen) {
    RIL_CDMA_SMS_WriteArgs *rcsw = (RIL_CDMA_SMS_WriteArgs *)data;

    memset(rcsw, 0, datalen);
    free(rcsw);

    return 0;
}

static int freeSetInitialAttachApn(void *data, size_t datalen) {
    if (RIL_VERSION <= 14) {
        RIL_InitialAttachApn *iaa = (RIL_InitialAttachApn *)data;

        memsetString(iaa->apn);
        memsetString(iaa->protocol);
        memsetString(iaa->username);
        memsetString(iaa->password);

        free(iaa->apn);
        free(iaa->protocol);
        free(iaa->username);
        free(iaa->password);

        memset(data, 0, datalen);
        free(iaa);
    } else {
        RIL_InitialAttachApn_v15 *iaa = (RIL_InitialAttachApn_v15 *)data;

        memsetString(iaa->apn);
        memsetString(iaa->protocol);
        memsetString(iaa->roamingProtocol);
        memsetString(iaa->username);
        memsetString(iaa->password);
        memsetString(iaa->mvnoMatchData);

        free(iaa->apn);
        free(iaa->protocol);
        free(iaa->roamingProtocol);
        free(iaa->username);
        free(iaa->password);
        free(iaa->mvnoMatchData);

        memset(data, 0, datalen);
        free(iaa);
    }

    return 0;
}

// TODO: just for GSM SMS
static int freeSendImsSms(void *data, size_t datalen) {
    RIL_UNUSED_PARM(datalen);

    ImsCdmaSms *ics = (ImsCdmaSms *)data;
    free(ics);

    return 0;
}

static int freeIccApdu(void *data, size_t datalen) {
    RIL_SIM_APDU *apdu = (RIL_SIM_APDU *)data;

    memsetString(apdu->data);
    free(apdu->data);

    memset(apdu, 0, datalen);
    free(apdu);

    return 0;
}

static int freeIccOpenLogicalChannel(void *data, size_t datalen) {
    RIL_OpenChannelParams *params = (RIL_OpenChannelParams *)data;

    memsetString(params->aidPtr);
    free(params->aidPtr);

    memset(params, 0, datalen);
    free(params);

    return 0;
}

static int freeNvReadItem(void *data, size_t datalen) {
    RIL_NV_ReadItem *nvri = (RIL_NV_ReadItem *)data;

    memset(nvri, 0, datalen);
    free(nvri);

    return 0;
}

static int freeNvWriteItem(void *data, size_t datalen) {
    RIL_NV_WriteItem *nvwi = (RIL_NV_WriteItem *)data;

    memsetString(nvwi->value);
    free(nvwi->value);

    memset(nvwi, 0, datalen);
    free(nvwi);

    return 0;
}

static int freeSetUiccSubscription(void *data, size_t datalen) {
    RIL_SelectUiccSub *rilUiccSub = (RIL_SelectUiccSub *)data;

    memset(rilUiccSub, 0, datalen);
    free(rilUiccSub);

    return 0;
}

static int freeIccSimAuthentication(void *data, size_t datalen) {
    RIL_SimAuthentication *pf = (RIL_SimAuthentication *)data;

    memsetString(pf->aid);
    memsetString(pf->authData);

    free(pf->aid);
    free(pf->authData);

    memset(pf, 0, datalen);
    free(pf);

    return 0;
}

static int freeSetDataProfileData(void *data, size_t datalen) {
    if (RIL_VERSION <= 14) {
        RIL_DataProfileInfo **dataProfiles = (RIL_DataProfileInfo **)data;
        int num = datalen / sizeof(RIL_DataProfileInfo *);
        int i = 0;
        for (i = 0; i < num; i++) {
            memsetString(dataProfiles[i]->apn);
            memsetString(dataProfiles[i]->protocol);
            memsetString(dataProfiles[i]->user);
            memsetString(dataProfiles[i]->password);

            free(dataProfiles[i]->apn);
            free(dataProfiles[i]->protocol);
            free(dataProfiles[i]->user);
            free(dataProfiles[i]->password);
        }
        memset(dataProfiles[0], 0, num * sizeof(RIL_DataProfileInfo));
        free(dataProfiles[0]);
        memset(dataProfiles, 0, datalen);
        free(dataProfiles);
    } else {
        RIL_DataProfileInfo_v15 **dataProfiles = (RIL_DataProfileInfo_v15 **)data;
        int num = datalen / sizeof(RIL_DataProfileInfo_v15 *);
        int i = 0;
        for (i = 0; i < num; i++) {
            memsetString(dataProfiles[i]->apn);
            memsetString(dataProfiles[i]->protocol);
            memsetString(dataProfiles[i]->roamingProtocol);
            memsetString(dataProfiles[i]->user);
            memsetString(dataProfiles[i]->password);
            memsetString(dataProfiles[i]->mvnoMatchData);

            free(dataProfiles[i]->apn);
            free(dataProfiles[i]->protocol);
            free(dataProfiles[i]->roamingProtocol);
            free(dataProfiles[i]->user);
            free(dataProfiles[i]->password);
            free(dataProfiles[i]->mvnoMatchData);
        }

        memset(dataProfiles[0], 0, num * sizeof(RIL_DataProfileInfo));
        free(dataProfiles[0]);
        memset(dataProfiles, 0, datalen);
        free(dataProfiles);
    }

    return 0;
}

static int freeSetRadioCapability(void *data, size_t datalen) {
    RIL_RadioCapability *rilRc = (RIL_RadioCapability *)data;

    memset(rilRc, 0, datalen);
    free(rilRc);

    return 0;
}

static int freeSetAllowedCarriers(void *data, size_t datalen) {
    RIL_UNUSED_PARM(datalen);

    RIL_CarrierRestrictions *cr = (RIL_CarrierRestrictions *)data;

    memset(cr->allowed_carriers, 0,
           cr->len_allowed_carriers * sizeof(sizeof(RIL_Carrier)));
    memset(cr->excluded_carriers, 0,
           cr->len_excluded_carriers * sizeof(sizeof(RIL_Carrier)));
    free(cr->allowed_carriers);
    free(cr->excluded_carriers);

    free(cr);

    return 0;
}

static int freeVideoPhoneDial(void *data, size_t datalen) {
    RIL_VideoPhone_Dial *vpDial = (RIL_VideoPhone_Dial *)data;

    memsetString(vpDial->address);
    memsetString(vpDial->sub_address);

    free(vpDial->address);
    free(vpDial->sub_address);

    memset(vpDial, 0, datalen);
    free(vpDial);

    return 0;
}

static int freeVideoPhoneCodec(void *data, size_t datalen) {
    RIL_VideoPhone_Codec *vpCodec = (RIL_VideoPhone_Codec *)data;

    memset(vpCodec, 0, datalen);
    free(vpCodec);

    return 0;
}

static int freeCallForwardUri(void *data, size_t datalen) {
    RIL_CallForwardInfoUri *cfInfo = (RIL_CallForwardInfoUri *)data;

    memsetString(cfInfo->number);
    memsetString(cfInfo->ruleset);

    free(cfInfo->number);
    free(cfInfo->ruleset);

    memset(cfInfo, 0, datalen);
    free(cfInfo);

    return 0;
}

static int freeNotifyIMSNetworkInfoChanged(void *data, size_t datalen) {
    IMS_NetworkInfo *nwInfo = (IMS_NetworkInfo *)data;

    memsetString(nwInfo->info);
    free(nwInfo->info);

    memset(nwInfo, 0, datalen);
    free(nwInfo);

    return 0;
}

static int freeSetCarrierImsiEncryption(void *data, size_t datalen) {
    RIL_CarrierInfoForImsiEncryption *info =
            (RIL_CarrierInfoForImsiEncryption *)data;
    memsetString(info->mcc);
    memsetString(info->mnc);
    memsetString(info->keyIdentifier);

    free(info->mcc);
    free(info->mnc);
    free(info->keyIdentifier);

    free(info->carrierKey);

    memset(info, 0, datalen);
    free(info);

    return 0;
}
static int freeStartNetworkScan(void *data, size_t datalen) {
    RIL_NetworkScanRequest *scanRequest = (RIL_NetworkScanRequest *)data;
    memset(scanRequest, 0, datalen);
    free(scanRequest);

    return 0;
}
static int freeStartKeepAlive(void *data, size_t datalen) {
    RIL_KeepaliveRequest *kaReq = (RIL_KeepaliveRequest *)data;
    memset(kaReq, 0, datalen);
    free(kaReq);

    return 0;
}
