

#ifndef MBIM_SERVICE_BASIC_CONNECT_H_
#define MBIM_SERVICE_BASIC_CONNECT_H_

#include "mbim_message.h"
#include "mbim_uuid.h"
#include "mbim_cid.h"
#include "mbim_enums.h"
#include "mbim_error.h"


/*****************************************************************************/
/* 'Connect' struct */


/**
 * MBIM_CID_BASIC_CONNECT_DEVICE_CAPS:
 *            Set        Query          Notification
 * Command    NA         Empty               NA
 * Response   NA  MBIM_DEVICE_CAPS_INFO      NA
 *
 */
struct _MbimDeviceCapsInfo {
    MbimDeviceType          deviceType;
    MbimCellularClass       cellularClass;
    MbimVoiceClass          voiceClass;
    MbimSimClass            simClass;
    uint32_t                dataClass;      // MbimDataClass
    uint32_t                smsCaps;        // MbimSmsCaps
    uint32_t                controlCaps;    // MbimCtrlCaps
    uint32_t                maxSessions;
    uint32_t                customDataClassOffset;
    uint32_t                customDataClassSize;
    uint32_t                deviceIdOffset;
    uint32_t                deviceIdSize;
    uint32_t                firmwareInfoOffset;
    uint32_t                firmwareInfoSize;
    uint32_t                hardwareInfoOffset;
    uint32_t                hardwareInfoSize;
    uint8_t                 dataBuffer[];   /* customDataClass, deviceId,
                                               firwareInfo, hardwareInfo */
} __attribute__((packed));
typedef struct _MbimDeviceCapsInfo MbimDeviceCapsInfo;


/**
 * MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS:
 *            Set           Query                  Notification
 * Command    NA            Empty                       NA
 * Response   NA  MBIM_SUBSCRIBER_READY_INFO  MBIM_SUBSCRIBER_READY_INFO
 *
 */
struct _MbimSubscriberReadyInfo {
    MbimSubscriberReadyState    readyState;
    uint32_t                    subscriberIdOffset; /* for GSM-based functions,
                                                       subscriberId represents IMSI */
    uint32_t                    subscriberIdSize;
    uint32_t                    simIccIdOffset;
    uint32_t                    simIccIdSize;
    MbimReadyInfoFlag           readyInfo;
    uint32_t                    elementCount;
    OffsetLengthPairList        telephoneNumbersRefList[];
 //   uint8_t                     dataBuffer[];   /* subscriberId, simIccId, telephoneNumber */
} __attribute__((packed));
typedef struct _MbimSubscriberReadyInfo MbimSubscriberReadyInfo;


/**
 * MBIM_CID_BASIC_CONNECT_RADIO_STATE:
 *                    Set                     Query            Notification
 * Command    MBIM_SET_RADIO_STATE            Empty                 NA
 * Response   MBIM_RADIO_STATE_INFO  MBIM_RADIO_STATE_INFO  MBIM_RADIO_STATE_INFO
 *
 */
struct _MbimSetRadioState {
    MbimRadioSwitchState    radioState;
} __attribute__((packed));
typedef struct _MbimSetRadioState MbimSetRadioState;

struct _MbimRadioStateInfo {
    MbimRadioSwitchState    hwRadioState;
    MbimRadioSwitchState    swRadioState;
} __attribute__((packed));
typedef struct _MbimRadioStateInfo MbimRadioStateInfo;

/**
 * MBIM_CID_BASIC_CONNECT_PIN:
 *                Set              Query        Notification
 * Command    MBIM_SET_PIN         Empty            NA
 * Response   MBIM_PIN_INFO    MBIM_PIN_INFO        NA
 *
 */
struct _MbimSetPin {
    MbimPinType         pinType;
    MbimPinOperation    pinOperation;
    uint32_t            pinOffset;
    uint32_t            pinSize;
    uint32_t            newPinOffset;
    uint32_t            newPinSize;
    uint8_t             dataBuffer[];   /* pin, newPin */
} __attribute__((packed));
typedef struct _MbimSetPin MbimSetPin;

struct _MbimPinInfo {
    MbimPinType         pinType;
    MbimPinState        pinState;
    uint32_t            remainingAttemps;
} __attribute__((packed));
typedef struct _MbimPinInfo MbimPinInfo;


/**
 * MBIM_CID_BASIC_CONNECT_PIN_LIST:
 *            Set      Query        Notification
 * Command    NA       Empty            NA
 * Response   NA  MBIM_PIN_LIST_INFO    NA
 *
 */
struct _MbimPinDesc {
    MbimPinMode         pinMode;
    MbimPinFormat       pinFormat;
    uint32_t            pinLengthMin;
    uint32_t            pinLengthMax;
} __attribute__((packed));
typedef struct _MbimPinDesc MbimPinDesc;

struct _MbimPinListInfo {
    MbimPinDesc         pinDescPin1;
    MbimPinDesc         pinDescPin2;
    MbimPinDesc         pinDescDeviceSimPin;
    MbimPinDesc         pinDescDeviceFirstPin;
    MbimPinDesc         pinDescNetwrokPin;
    MbimPinDesc         pinDescNetworkSubsetPin;
    MbimPinDesc         pinDescServiceProviderPin;
    MbimPinDesc         pinDescCorporatePin;
    MbimPinDesc         pinDescSubsidyLock;
    MbimPinDesc         pinDescCustom;
} __attribute__((packed));
typedef struct _MbimPinListInfo MbimPinListInfo;


/**
 * MBIM_CID_BASIC_CONNECT_HOME_PROVIDER:
 *                 Set            Query      Notification
 * Command    MBIM_PROVIDER       Empty           NA
 * Response   MBIM_PROVIDER   MBIM_PROVIDER       NA
 *
 */
struct _MbimProvider {
    uint32_t            providerIdOffset;   /* for GSM-based networks, this string
                                               consists of mcc and mnc */
    uint32_t            providerIdSize;
    MbimProviderState   providerState;
    uint32_t            providerNameOffset; /* ignored when sets the preferred provider list */
    uint32_t            providerNameSize;
    MbimCellularClass   celluarClass;
    uint32_t            rssi;           /* ignored on set requests */
    uint32_t            errorRate;      /* ignored on set requests */
    uint8_t             dataBuffer[];   /* provideId, providerName */
} __attribute__((packed));
typedef struct _MbimProvider MbimProvider;


/**
 * MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS:
 *                 Set             Query       Notification
 * Command    MBIM_PROVIDERS       Empty            NA
 * Response   MBIM_PROVIDERS   MBIM_PROVIDERS       NA
 *
 */
struct _MbimProviders {
    uint32_t                elementCount;
    OffsetLengthPairList    providerRefList[];
//    uint8_t                 dataBuffer[];   /* MbimProvider */
} __attribute__((packed));
typedef struct _MbimProviders MbimProviders;


/**
 * MBIM_CID_BASIC_CONNECT_VISIBLE_PROVIDERS:
 *            Set              Query               Notification
 * Command    NA      MBIM_VISBLE_PROVIDERS_REQ        NA
 * Response   NA          MBIM_PROVIDERS               NA
 *
 */
struct _MbimVisibleProvidersReq {
    MbimVisibleProvidersAction  action;
} __attribute__((packed));
typedef struct _MbimVisibleProvidersReq MbimVisibleProvidersReq;

/**
 * MBIM_CID_BASIC_CONNECT_REGISTER_STATE:
 *                      Set                         Query               Notification
 * Command  MBIM_SET_REGISTRATION_STATE             Empty                    NA
 * Response MBIM_REGISTRATION_STATE_INFO  MBIM_REGISTRATION_STATE_INFO  MBIM_REGISTRATION_STATE_INFO
 *
 */
struct _MbimSetRegistrationState {
    uint32_t            providerIdOffset;   /* for GSM-based networks,
                                               this string consists of mcc and mnc*/
    uint32_t            providerIdSize;
    MbimRegisterAction  registerAction;
    MbimDataClass       dataClass;
    uint8_t             dataBuffer[];       /* providerId */
} __attribute__((packed));
typedef struct _MbimSetRegistrationState MbimSetRegistrationState;

struct _MbimRegistrationStateInfo {
    MbimNwError             nwError;
    MbimRegisterState       registerState;
    MbimRegisterMode        registerMode;
    MbimDataClass           availableDataClasses;
    MbimCellularClass       currentCellularClass;
    uint32_t                providerIdOffset;
    uint32_t                providerIdSize;
    uint32_t                providerNameOffset;
    uint32_t                providerNameSize;
    uint32_t                roamingTextOffset;
    uint32_t                roamingTextSize;
    MbimRegistrationFlag    registrationFlag;
    uint8_t                 dataBuffer[];   /* providerId, ProviderName, RoamingText */
} __attribute__((packed));
typedef struct _MbimRegistrationStateInfo MbimRegistrationStateInfo;


/**
 * MBIM_CID_BASIC_CONNECT_PACKET_SERVICE:
 *                      Set                        Query                Notification
 * Command    MBIM_SET_PACKET_SERVICE              Empty                    NA
 * Response   MBIM_PACKET_SERVICE_INFO    MBIM_PACKET_SERVICE_INFO  MBIM_PACKET_SERVICE_INFO
 *
 */
struct _MbimSetPacketService {
    MbimPacketServiceAction     packetServiceAction;
} __attribute__((packed));
typedef struct _MbimSetPacketService MbimSetPacketService;

struct _MbimPacketServiceInfo {
    MbimNwError                 nwError;
    MbimPacketServiceState      packetServiceState;
    MbimDataClass               highestAvailableDataClass;
    uint64_t                    upLinkSpeed;
    uint64_t                    downLinkSpeed;
} __attribute__((packed));
typedef struct _MbimPacketServiceInfo MbimPacketServiceInfo;


/**
 * MBIM_CID_BASIC_CONNECT_SIGNAL_STATE:
 *                     Set                    Query               Notification
 * Command    MBIM_SET_SIGNAL_STATE           Empty                   NA
 * Response   MBIM_SIGNAL_STATE_INFO  MBIM_SIGNAL_STATE_INFO  MBIM_SIGNAL_STATE_INFO
 *
 */
struct _MbimSetSignalState {
    uint32_t        signalStrengthInterval;
    uint32_t        rssiThreshold;
    uint32_t        errorRateThreshold;
} __attribute__((packed));
typedef struct _MbimSetSignalState MbimSetSignalState;

struct _MbimSignalStateInfo {
    uint32_t        rssi;
    uint32_t        errorRate;
    uint32_t        signalStrengthInterval;
    uint32_t        rssiThreshold;
    uint32_t        errorRateThreshold;
} __attribute__((packed));
typedef struct _MbimSignalStateInfo MbimSignalStateInfo;


/**
 * MBIM_CID_BASIC_CONNECT_CONNECT:
 *              Set                Query          Notification
 * Command  MBIM_SET_CONNECT   MBIM_CONNECT_INFO       NA
 * Response MBIM_CONNECT_INFO  MBIM_CONNECT_INFO  MBIM_CONNECT_INFO
 *
 */
struct _MbimSetConnect {
    uint32_t                sessionId;
    MbimActivationCommand   activationCommand;
    uint32_t                accessStringOffset;
    uint32_t                accessStringSize;
    uint32_t                usernameOffset;
    uint32_t                usernameSize;
    uint32_t                passwordOffset;
    uint32_t                passwordSize;
    MbimCompression         compression;
    MbimAuthProtocol        authProtocol;
    MbimContextIpType       IPType;
    MbimUuid                contextType;
    uint8_t                 dataBuffer[];   /* accessString, userName, password */
} __attribute__((packed));
typedef struct _MbimSetConnect MbimSetConnect;

struct _MbimConnectInfo {
    uint32_t                sessionId;
    MbimActivationState     activationState;
    MbimVoiceCallState      voiceCallState;
    MbimContextIpType       IPType;
    MbimUuid                contextType;
    MbimNwError             nwError;
} __attribute__((packed));
typedef struct _MbimConnectInfo MbimConnectInfo;


/**
 * MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS:
 *                      Set                              Query                    Notification
 * Command  MBIM_SET_PROVISIONED_CONTEXT                 Empty                         NA
 * Response MBIM_PROVISIONED_CONTEXTS_INFO  MBIM_PROVISIONED_CONTEXTS_INFO  MBIM_PROVISIONED_CONTEXTS_INFO
 *
 */
struct _MbimContext {
    uint32_t            contextId;
    MbimUuid            contextType;
    uint32_t            accessStringOffset;
    uint32_t            accessStringSize;
    uint32_t            usernameOffset;
    uint32_t            usernameSize;
    uint32_t            passwordOffset;
    uint32_t            passwordSize;
    MbimCompression     compression;
    MbimAuthProtocol    authProtocol;
    uint8_t             dataBuffer[];   /* accessString, username, password */
} __attribute__((packed));
typedef struct _MbimContext MbimContext;

struct _MbimSetProvisionedContext {
    uint32_t            contextId;
    MbimUuid            contextType;
    uint32_t            accessStringOffset;
    uint32_t            accessStringSize;
    uint32_t            usernameOffset;
    uint32_t            usernameSize;
    uint32_t            passwordOffset;
    uint32_t            passwordSize;
    MbimCompression     compression;
    MbimAuthProtocol    authProtocol;
    uint32_t            providerIdOffset;
    uint32_t            providerIdSize;
    uint8_t             dataBuffer[];   /* accessString, username, password, providerId */
} __attribute__((packed));
typedef struct _MbimSetProvisionedContext MbimSetProvisionedContext;

struct _MbimProvisionedContextsInfo {
    uint32_t                elementCount;
    OffsetLengthPairList    provisionedContestRefList[];
//    uint8_t                 dataBuffer[];   /* MbimContext */
} __attribute__((packed));
typedef struct _MbimProvisionedContextsInfo MbimProvisionedContextsInfo;


/**
 * MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION:
 *                      Set                  Query    Notification
 * Command  MBIM_SET_SERVICE_ACTIVIATION      NA          NA
 * Response MBIM_SERVICE_ACTIVIATION_INFO     NA          NA
 *
 */
struct _MbimSetServiceActivation {
//    uint8_t     dataBuffer[];   /* vendorSpecificBuffer is a placeholder for
//                                   sending the carrier-specific data to activate the service */
} __attribute__((packed));
typedef struct _MbimSetServiceActivation MbimSetServiceActivation;

struct _MbimServiceActivationInfo {
    MbimNwError         nwError;
    uint8_t             dataBuffer[];   /* vendorSpecificBuffer */
} __attribute__((packed));
typedef struct _MbimServiceActivationInfo MbimServiceActivationInfo;


/**
 * MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION:
 *            Set                Query            Notification
 * Command    NA     MBIM_IP_CONFIGURATION_INFO        NA
 * Response   NA     MBIM_IP_CONFIGURATION_INFO   MBIM_IP_CONFIGURATION_INFO
 *
 * For Query, the InformationBuffer contains an MBIM_IP_CONFIGURATION_INFO
 * in which the only relevant field is the SessionId.
 *
 */
/**
 * MbimIPv4:
 * @addr: 4 bytes specifying the IPv4 address.
 *
 * An IPv4 address.
 */
typedef struct _MbimIPv4 MbimIPv4;
struct _MbimIPv4 {
    uint8_t addr[4];
};

typedef struct _MbimIPv4Element MbimIPv4Element;
struct _MbimIPv4Element {
    uint32_t    onLinkPrefixLength;
    MbimIPv4    IPv4Address;
} __attribute__((packed));

/**
 * MbimIPv6:
 * @addr: 16 bytes specifying the IPv6 address.
 *
 * An IPv6 address.
 */
typedef struct _MbimIPv6 MbimIPv6;
struct _MbimIPv6 {
    uint8_t addr[16];
};

typedef struct _MbimIPv6Element MbimIPv6Element;
struct _MbimIPv6Element {
    uint32_t    onLinkPrefixLength;
    MbimIPv6    IPv6Address;
} __attribute__((packed));

struct _MbimIPConfigurationInfo {
    uint32_t        sessionId;
    uint32_t        IPv4ConfigurationAvailable;
    uint32_t        IPv6ConfigurationAvailable;
    uint32_t        IPv4AddressCount;
    uint32_t        IPv4AddressOffset;
    uint32_t        IPv6AddressCount;
    uint32_t        IPv6AddressOffset;
    uint32_t        IPv4GatewayOffset;
    uint32_t        IPv6GatewayOffset;
    uint32_t        IPv4DnsServerCount;
    uint32_t        IPv4DnsServerOffset;
    uint32_t        IPv6DnsServerCount;
    uint32_t        IPv6DnsServerOffset;
    uint32_t        IPv4Mtu;
    uint32_t        IPv6Mtu;
    uint8_t         dataBuffer[];   /* actual address data */
} __attribute__((packed));
typedef struct _MbimIPConfigurationInfo MbimIPConfigurationInfo;

/**
 * MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES:
 *            Set            Query           Notification
 * Command    NA              NA                 NA
 * Response   NA    MBIM_DEVICE_SERVICES_INFO    NA
 *
 */
//#define SUPPORTED_SERVICES_NUMBER   2

struct _MbimDeviceServiceElement {
    MbimUuid        deviceServiceId;
    uint32_t        dssPayload;
    uint32_t        maxDssInstances;
    uint32_t        cidCount;
    uint32_t        cids[];
} __attribute__((packed));
typedef struct _MbimDeviceServiceElement MbimDeviceServiceElement;

struct _MbimDeviceServicesInfo {
    uint32_t                    deviceServicesCount;
    uint32_t                    maxDssSessions;
    OffsetLengthPairList        deviceServicesRefList[];
//    uint8_t                     dataBuffer[];
} __attribute__((packed));
typedef struct _MbimDeviceServicesInfo MbimDeviceServicesInfo;


/**
 * MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST:
 *                           Set                    Query    Notification
 * Command    MBIM_DEVICE_SERVICE_SUBSCRIBE_LIST     NA           NA
 * Response   MBIM_DEVICE_SERVICE_SUBSCRIBE_LIST     NA           NA
 *
 */
struct _MbimEventEntry {
    MbimUuid        deviceServiceId;
    uint32_t        cidCount;
    uint32_t        cids[];     /* cidList */
} __attribute__((packed));
typedef struct _MbimEventEntry MbimEventEntry;

struct _MbimDeviceServiceSubscribeList {
    uint32_t                elementCount;
    OffsetLengthPairList    deviceServiceSubscirbeRefList[];
//    uint8_t                 dataBuffer[];   /* array of MbimEventEntry */
} __attribute__((packed));
typedef struct _MbimDeviceServiceSubscribeList MbimDeviceServiceSubscribeList;


/**
 * MBIM_CID_BASIC_CONNECT_PACKET_STATISTICS:
 *            Set            Query               Notification
 * Command    NA             Empty                    NA
 * Response   NA   MBIM_PACKET_STATISTICS_INFO        NA
 *
 */
struct _MbimPacketStatisticsInfo {
    uint32_t        inDiscards;
    uint32_t        inErrors;
    uint64_t        inOctets;
    uint64_t        inPackets;
    uint64_t        outOctets;
    uint64_t        outPackets;
    uint32_t        outErrors;
    uint32_t        outDiscards;
} __attribute__((packed));
typedef struct _MbimPacketStatisticsInfo MbimPacketStatisticsInfo;


/**
 * MBIM_CID_BASIC_CONNECT_NETWORK_IDLE_HINT:
 *                     Set                    Query             Notification
 * Command    MBIM_NETWORK_IDLE_HINT          Empty                  NA
 * Response   MBIM_NETWORK_IDLE_HINT   MBIM_NETWORK_IDLE_HINT        NA
 *
 */
struct _MbimNetworkIdleHint {
    MbimNetworkIdleHintState    networkIdleHintState;
} __attribute__((packed));
typedef struct _MbimNetworkIdleHint MbimNetworkIdleHint;


/**
 * MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE:
 *            Set          Query                  Notification
 * Command    NA           Empty                       NA
 * Response   NA   MBIM_EMERGENCY_MODE_INFO   MBIM_EMERGENCY_MODE_INFO
 *
 */
struct _MbimEmergencyModeInfo {
    MbimEmergencyModeState      emergencyMode;
} __attribute__((packed));
typedef struct _MbimEmergencyModeInfo MbimEmergencyModeInfo;


/**
 * MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS:
 *                    Set                  Query          Notification
 * Command    MBIM_PACKET_FILTERS   MBIM_PACKET_FILTERS        NA
 * Response   MBIM_PACKET_FILTERS   MBIM_PACKET_FILTERS        NA
 *
 */
struct _MbimSinglePacketFilter {
    uint32_t        filterSize;
    uint32_t        packetFilterOffset;
    uint32_t        packetMaskOffset;
    uint8_t         dataBuffer[];   /* packetFilter, packetMask */
} __attribute__((packed));
typedef struct _MbimSinglePacketFilter MbimSinglePacketFilter;

struct _MbimPacketFilters {
    uint32_t                sessionId;
    uint32_t                packetFilterCount;
    OffsetLengthPairList    packetFilterRefList[];
//    uint8_t                 dataBuffer[];   /* MbimSinglePacketFilter */
} __attribute__((packed));
typedef struct _MbimPacketFilters MbimPacketFilters;


/**
 * MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS:
 *                 Set             Query        Notification
 * Command    MBIM_PROVIDERS       Empty             NA
 * Response   MBIM_PROVIDERS   MBIM_PROVIDERS   MBIM_PROVIDERS
 *
 */

/*****************************************************************************/

void mbim_message_parser_basic_connect(MbimMessageInfo *messageInfo);

extern IndicateStatusInfo
s_basic_connect_indicate_status_info[MBIM_CID_BASIC_CONNECT_LAST];

/*****************************************************************************/
/**
 * basic connect service's packing and unpacking interfaces
 */

/* basic_connect_device_caps */
int
mbim_message_parser_basic_connect_device_caps(MbimMessageInfo * messageInfo,
                                              char *            printBuf);
void
basic_connect_device_caps_query_operation(MbimMessageInfo *     messageInfo,
                                          char *                printBuf);
int
basic_connect_device_caps_query_ready(Mbim_Token                token,
                                      MbimStatusError           error,
                                      void *                    response,
                                      size_t                    responseLen,
                                      char *                    printBuf);

/* basic_connect_subscriber_ready_status */
int
mbim_message_parser_basic_connect_subscriber_ready_status(MbimMessageInfo * messageInfo,
                                                          char *            printBuf);
void
basic_connect_subscriber_ready_status_query_operation(MbimMessageInfo *     messageInfo,
                                                      char *                printBuf);
int
basic_connect_subscriber_ready_status_query_ready(Mbim_Token                token,
                                                  MbimStatusError           error,
                                                  void *                    response,
                                                  size_t                    responseLen,
                                                  char *                    printBuf);
int
basic_connect_subscriber_ready_status_notify(void *     data,
                                             size_t     dataLen,
                                             void **    response,
                                             size_t *   responseLen,
                                             char *     printBuf);
MbimSubscriberReadyInfo *
subscriber_ready_status_response_builder(void *         response,
                                         size_t         responseLen,
                                         uint32_t *     outSize,
                                         char *         printBuf);

/* basic_connect_radio_state */
int
mbim_message_parser_basic_connect_radio_state(MbimMessageInfo * messageInfo,
                                              char *            printBuf);
void
basic_connect_radio_state_query_operation(MbimMessageInfo *     messageInfo,
                                          char *                printBuf);
void
basic_connect_radio_state_set_operation(MbimMessageInfo *       messageInfo,
                                        char *                  printBuf);
int
basic_connect_radio_state_set_or_query_ready(Mbim_Token         token,
                                             MbimStatusError    error,
                                             void *             response,
                                             size_t             responseLen,
                                             char *             printBuf);
int
basic_connect_radio_state_notify(void *      data,
                                 size_t      dataLen,
                                 void **     response,
                                 size_t *    responseLen,
                                 char *      printBuf);

/* basic_connect_pin */
int
mbim_message_parser_basic_connect_pin(MbimMessageInfo * messageInfo,
                                      char *            printBuf);
void
basic_connect_pin_query_operation(MbimMessageInfo *     messageInfo,
                                  char *                printBuf);
void
basic_connect_pin_set_operation(MbimMessageInfo *       messageInfo,
                                char *                  printBuf);
int
basic_connect_pin_set_or_query_ready(Mbim_Token         token,
                                     MbimStatusError    error,
                                     void *             response,
                                     size_t             responseLen,
                                     char *             printBuf);

/* basic_connect_pin_list */
int
mbim_message_parser_basic_connect_pin_list(MbimMessageInfo *    messageInfo,
                                           char *               printBuf);
void
basic_connect_pin_list_query_operation(MbimMessageInfo *        messageInfo,
                                       char *                   printBuf);
int
basic_connect_pin_list_query_ready(Mbim_Token                   token,
                                   MbimStatusError              error,
                                   void *                       response,
                                   size_t                       responseLen,
                                   char *                       printBuf);

/* basic_connect_home_provider */
int
mbim_message_parser_basic_connect_home_provider(MbimMessageInfo *   messageInfo,
                                                char *              printBuf);
void
basic_connect_home_provider_query_operation(MbimMessageInfo *       messageInfo,
                                            char *                  printBuf);
void
basic_connect_home_provider_set_operation(MbimMessageInfo *         messageInfo,
                                          char *                    printBuf);
int
basic_connect_home_provider_set_or_query_ready(Mbim_Token           token,
                                               MbimStatusError      error,
                                               void *               response,
                                               size_t               responseLen,
                                               char *               printBuf);
MbimProvider *
home_provider_response_builder(void *       response,
                               size_t       responseLen,
                               uint32_t *   outSize,
                               char *       printBuf);

/* basic_connect_preferred_providers */
int
mbim_message_parser_basic_connect_preferred_providers(MbimMessageInfo * messageInfo,
                                                      char *            printBuf);
void
basic_connect_preferred_providers_query_operation(MbimMessageInfo *     messageInfo,
                                                  char *                printBuf);
void basic_connect_preferred_providers_set_operation(MbimMessageInfo *  messageInfo,
                                                     char *             printBuf);
int
basic_connect_preferred_providers_set_or_query_ready(Mbim_Token         token,
                                                     MbimStatusError    error,
                                                     void *             response,
                                                     size_t             responseLen,
                                                     char *             printBuf);
int basic_connect_preferred_providers_notify(void *      data,
                                             size_t      dataLen,
                                             void **     response,
                                             size_t *    responseLen,
                                             char *      printBuf);

/* basic_connect_visible_providers */
int
mbim_message_parser_basic_connect_visible_providers(MbimMessageInfo *   messageInfo,
                                                    char *              printBuf);
void
basic_connect_visible_providers_query_operation(MbimMessageInfo *       messageInfo,
                                                char *                  printBuf);
int
basic_connect_visible_providers_query_ready(Mbim_Token                  token,
                                            MbimStatusError             error,
                                            void *                      response,
                                            size_t                      responseLen,
                                            char *                      printBuf);

/* basic_connect_register_state */
int
mbim_message_parser_basic_connect_register_state(MbimMessageInfo *  messageInfo,
                                                 char *             printBuf);
void
basic_connect_register_state_query_operation(MbimMessageInfo *      messageInfo,
                                             char *                 printBuf);
void
basic_connect_register_state_set_operation(MbimMessageInfo *        messageInfo,
                                           char *                   printBuf);
int
basic_connect_register_state_set_or_query_ready(Mbim_Token          token,
                                                MbimStatusError     error,
                                                void *              response,
                                                size_t              responseLen,
                                                char *              printBuf);
int
basic_connect_register_state_notify(void *      data,
                                    size_t      dataLen,
                                    void **     response,
                                    size_t *    responseLen,
                                    char *      printBuf);
MbimRegistrationStateInfo *
register_state_response_builder(void *          response,
                                size_t          responseLen,
                                uint32_t *      outSize,
                                char *          printBuf);

/* basic_connect_register_state */
int
mbim_message_parser_basic_connect_packet_service(MbimMessageInfo *  messageInfo,
                                                 char *             printBuf);
void
basic_connect_packet_service_query_operation(MbimMessageInfo *      messageInfo,
                                             char *                 printBuf);
void
basic_connect_packet_service_set_operation(MbimMessageInfo *        messageInfo,
                                           char *                   printBuf);
int
basic_connect_packet_service_set_or_query_ready(Mbim_Token          token,
                                                MbimStatusError     error,
                                                void *              response,
                                                size_t              responseLen,
                                                char *              printBuf);
int
basic_connect_packet_service_notify(void *      data,
                                    size_t      dataLen,
                                    void **     response,
                                    size_t *    responseLen,
                                    char *      printBuf);

/* basic_connect_register_state */
int
mbim_message_parser_basic_connect_signal_state_service(MbimMessageInfo *    messageInfo,
                                                       char *               printBuf);
void
basic_connect_signal_state_query_operation(MbimMessageInfo *                messageInfo,
                                           char *                           printBuf);
void basic_connect_signal_state_set_operation(MbimMessageInfo *             messageInfo,
                                              char *                        printBuf);
int
basic_connect_signal_state_set_or_query_ready(Mbim_Token                    token,
                                              MbimStatusError               error,
                                              void *                        response,
                                              size_t                        responseLen,
                                              char *                        printBuf);
int
basic_connect_signal_state_notify(void *      data,
                                  size_t      dataLen,
                                  void **     response,
                                  size_t *    responseLen,
                                  char *      printBuf);

/* basic_connect_connect */
int
mbim_message_parser_basic_connect_connect(MbimMessageInfo *     messageInfo,
                                          char *                printBuf);
void
basic_connect_connect_query_operation(MbimMessageInfo *         messageInfo,
                                      char *                    printBuf);
void
basic_connect_connect_set_operation(MbimMessageInfo *           messageInfo,
                                    char *                      printBuf);
int
basic_connect_connect_set_or_query_ready(Mbim_Token             token,
                                         MbimStatusError        error,
                                         void *                 response,
                                         size_t                 responseLen,
                                         char *                 printBuf);
int
basic_connect_connect_notify(void *      data,
                             size_t      dataLen,
                             void **     response,
                             size_t *    responseLen,
                             char *      printBuf);

/* basic_connect_provisioned_contexts */
int
mbim_message_parser_basic_connect_provisioned_contexts(MbimMessageInfo *    messageInfo,
                                                       char *               printBuf);
void
basic_connect_provisioned_contexts_query_operation(MbimMessageInfo *        messageInfo,
                                                   char *                   printBuf);
void
basic_connect_provisioned_contexts_set_operation(MbimMessageInfo *          messageInfo,
                                                 char *                     printBuf);
int
basic_connect_provisioned_contexts_set_or_query_ready(Mbim_Token            token,
                                                      MbimStatusError       error,
                                                      void *                response,
                                                      size_t                responseLen,
                                                      char *                printBuf);
int
basic_connect_provisioned_contexts_notify(void *        data,
                                          size_t        dataLen,
                                          void **       response,
                                          size_t *      responseLen,
                                          char *        printBuf);
MbimContext *
provisioned_context_response_builder(void *             response,
                                     size_t             responseLen,
                                     uint32_t *         outSize,
                                     char *             printBuf);

/* basic_connect_service_activiation */
int
mbim_message_parser_basic_connect_service_activiation(MbimMessageInfo * messageInfo,
                                                      char *            printBuf);
void
basic_connect_service_activiation_set_operation(MbimMessageInfo *   messageInfo,
                                                char *              printBuf);
int
basic_connect_service_activiation_set_ready(Mbim_Token          token,
                                            MbimStatusError     error,
                                            void *              response,
                                            size_t              responseLen,
                                            char *              printBuf);

/* basic_connect_ip_configuratio */
int
mbim_message_parser_basic_connect_ip_configuration(MbimMessageInfo *    messageInfo,
                                                   char *               printBuf);
void
basic_connect_ip_configuration_query_operation(MbimMessageInfo *        messageInfo,
                                               char *                   printBuf);
int
basic_connect_ip_configuration_query_ready(Mbim_Token                   token,
                                           MbimStatusError              error,
                                           void *                       response,
                                           size_t                       responseLen,
                                           char *                       printBuf);
int
basic_connect_ip_configuration_notify(void *        data,
                                      size_t        dataLen,
                                      void **       response,
                                      size_t *      responseLen,
                                      char *        printBuf);
MbimIPConfigurationInfo *
ip_configuration_response_builder(void *            response,
                                  size_t            responseLen,
                                  uint32_t *        outSize,
                                  char *            printBuf);

/* basic_connect_device_services */
int
mbim_message_parser_basic_connect_device_services(MbimMessageInfo * messageInfo,
                                                  char *            printBuf);
void
basic_connect_device_services_query_operation(MbimMessageInfo *     messageInfo,
                                              char *                printBuf);
int
basic_connect_device_services_query_ready(Mbim_Token                token,
                                          MbimStatusError           error,
                                          void *                    response,
                                          size_t                    responseLen,
                                          char *                    printBuf);
MbimDeviceServiceElement *
device_services_response_builder(void *         response,
                                 size_t         responseLen,
                                 uint32_t *     outSize);

/* basic_connect_device_service_subscribe_list */
int
mbim_message_parser_basic_connect_device_service_subscribe_list(MbimMessageInfo *   messageInfo,
                                                                char *              printBuf);
void
basic_connect_device_service_subscribe_list_set_operation(MbimMessageInfo *         messageInfo,
                                                          char *                    printBuf);
int
basic_connect_device_service_subscribe_list_set_ready(Mbim_Token                    token,
                                                      MbimStatusError               error,
                                                      void *                        response,
                                                      size_t                        responseLen,
                                                      char *                        printBuf);
MbimEventEntry *
device_service_subscribe_list_response_builder(void *       response,
                                               size_t       responseLen,
                                               uint32_t *   outSize);

/* basic_connect_device_service_packet_statistics */
int
mbim_message_parser_basic_connect_packet_statistics(MbimMessageInfo *   messageInfo,
                                                    char *              printBuf);
void
basic_connect_packet_statistics_query_operation(MbimMessageInfo *       messageInfo,
                                                char *                  printBuf);
int
basic_connect_packet_statistics_query_ready(Mbim_Token                  token,
                                            MbimStatusError             error,
                                            void *                      response,
                                            size_t                      responseLen,
                                            char *                      printBuf);

/* basic_connect_device_service_network_idle_hint */
int
mbim_message_parser_basic_connect_network_idle_hint(MbimMessageInfo *   messageInfo,
                                                    char *              printBuf);
void
basic_connect_network_idle_hint_query_operation(MbimMessageInfo *       messageInfo,
                                                char *                  printBuf);
void
basic_connect_network_idle_hint_set_operation(MbimMessageInfo *         messageInfo,
                                              char *                    printBuf);
int
basic_connect_network_idle_hint_set_or_query_ready(Mbim_Token           token,
                                                   MbimStatusError      error,
                                                   void *               response,
                                                   size_t               responseLen,
                                                   char *               printBuf);

/* basic_connect_device_service_emergency_mode */
int
mbim_message_parser_basic_connect_emergency_mode(MbimMessageInfo *  messageInfo,
                                                 char *             printBuf);
void
basic_connect_emergency_mode_query_operation(MbimMessageInfo *      messageInfo,
                                             char *                 printBuf);
int
basic_connect_emergency_mode_query_ready(Mbim_Token                 token,
                                         MbimStatusError            error,
                                         void *                     response,
                                         size_t                     responseLen,
                                         char *                     printBuf);
int
basic_connect_emergency_mode_notify(void *      data,
                                    size_t      dataLen,
                                    void **     response,
                                    size_t *    responseLen,
                                    char *      printBuf);

/* basic_connect_ip_packet_filters */
int
mbim_message_parser_basic_connect_ip_packet_filters(MbimMessageInfo *   messageInfo,
                                                    char *              printBuf);
void
basic_connect_ip_packet_filters_query_operation(MbimMessageInfo *       messageInfo,
                                                char *                  printBuf);
void
basic_connect_ip_packet_filters_set_operation(MbimMessageInfo *         messageInfo,
                                              char *                    printBuf);
int
basic_connect_ip_packet_filters_set_or_query_ready(Mbim_Token           token,
                                                   MbimStatusError      error,
                                                   void *               response,
                                                   size_t               responseLen,
                                                   char *               printBuf);
MbimSinglePacketFilter *
ip_packet_filters_response_builder(void *       response,
                                   size_t       responseLen,
                                   uint32_t *   outSize,
                                   char *       printBuf);

/* basic_connect_multicarrier_providers */
int
mbim_message_parser_basic_connect_multicarrier_providers(MbimMessageInfo *  messageInfo,
                                                         char *             printBuf);
void
basic_connect_multicarrier_providers_query_operation(MbimMessageInfo *      messageInfo,
                                                     char *                 printBuf);
void
basic_connect_multicarrier_providers_set_operation(MbimMessageInfo *        messageInfo,
                                                   char *                   printBuf);
int
basic_connect_multicarrier_providers_set_or_query_ready(Mbim_Token          token,
                                                        MbimStatusError     error,
                                                        void *              response,
                                                        size_t              responseLen,
                                                        char *              printBuf) ;
int
basic_connect_multicarrier_providers_notify(void *      data,
                                            size_t      dataLen,
                                            void **     response,
                                            size_t *    responseLen,
                                            char *      printBuf);

/*****************************************************************************/


#endif  // MBIM_SERVICE_BASIC_CONNECT_H_
