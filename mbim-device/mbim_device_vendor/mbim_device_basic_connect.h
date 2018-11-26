#ifndef MBIM_DEVICE_BASIC_CONNECT_H_
#define MBIM_DEVICE_BASIC_CONNECT_H_

#include "at_channel.h"
#include "mbim_enums.h"
#include "mbim_device_config.h"
/**
 * MBIM_CID_BASIC_CONNECT_DEVICE_CAPS
 */
typedef struct {
    MbimDeviceType          deviceType;
    MbimCellularClass       cellularClass;
    MbimVoiceClass          voiceClass;
    MbimSimClass            simClass;
    uint32_t                dataClass;      // MbimDataClass
    uint32_t                smsCaps;        // MbimSmsCaps
    uint32_t                controlCaps;    // MbimCtrlCaps
    uint32_t                maxSessions;
    char *                  customDataClass;
    char *                  deviceId;       // for GSM, deviceId represents IMEI
    char *                  firmwareInfo;
    char *                  hardwareInfo;
} MbimDeviceCapsInfo_2;

/**
 * MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS
 */
typedef struct  {
    MbimSubscriberReadyState    readyState;
    char *                      subscriberId;
    char *                      simIccId;
    MbimReadyInfoFlag           readyInfo;
    uint32_t                    elementCount;
    char **                     telephoneNumbers;
} MbimSubscriberReadyInfo_2;

/**
 * MBIM_CID_BASIC_CONNECT_PIN
 */
typedef struct  {
    MbimPinType         pinType;
    MbimPinOperation    pinOperation;
    char *              pin;
    char *              newPin;
} MbimSetPin_2;

/**
 * MBIM_CID_BASIC_CONNECT_HOME_PROVIDER
 */
typedef struct  {
    char *              providerId;
    MbimProviderState   providerState;
    char *              providerName;
    MbimCellularClass   celluarClass;
    uint32_t            rssi;
    uint32_t            errorRate;
} MbimProvider_2;

/**
 * MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS
 */
typedef struct {
    uint32_t                elementCount;
    MbimProvider_2 **       provider;
} MbimProviders_2;

/**
 * MBIM_CID_BASIC_CONNECT_REGISTER_STATE
 */
typedef struct {
    char *              providerId;
    MbimRegisterAction  registerAction;
    MbimDataClass       dataClass;
} MbimSetRegistrationState_2;

typedef struct {
    MbimNwError             nwError;
    MbimRegisterState       registerState;
    MbimRegisterMode        registerMode;
    MbimDataClass           availableDataClasses;
    MbimCellularClass       currentCellularClass;
    char *                  providerId;
    char *                  providerName;
    char *                  roamingText;
    MbimRegistrationFlag    registrationFlag;
} MbimRegistrationStateInfo_2;

/**
 * MBIM_CID_BASIC_CONNECT_CONNECT
 */
typedef struct  {
    uint32_t                sessionId;
    MbimActivationCommand   activationCommand;
    char *                  accessString;
    char *                  username;
    char *                  password;
    MbimCompression         compression;
    MbimAuthProtocol        authProtocol;
    MbimContextIpType       IPType;
    MbimContextType         contextType;
} MbimSetConnect_2;

/**
 * MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS
 */
typedef struct {
    uint32_t            contextId;
    MbimContextType     contextType;
    char *              accessString;
    char *              username;
    char *              password;
    MbimCompression     compression;
    MbimAuthProtocol    authProtocol;
    char *              providerId;
} MbimSetProvisionedContext_2;

typedef struct {
    uint32_t            contextId;
    MbimContextType     contextType;
    char *              accessString;
    char *              username;
    char *              password;
    MbimCompression     compression;
    MbimAuthProtocol    authProtocol;
} MbimContext_2;

typedef struct {
    uint32_t                elementCount;
    MbimContext_2 **        context;
} MbimProvisionedContextsInfo_2;

/**
 * MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION
 */
typedef struct  {
    MbimNwError         nwError;
    void *              vendorSpecificBuffer;
} MbimServiceActivationInfo_2;

/**
 * MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION
 */
typedef struct {
    uint32_t    onLinkPrefixLength;
    char *      IPv4Address;
} MbimIPv4Element_2;

typedef struct {
    uint32_t    onLinkPrefixLength;
    char *      IPv6Address;
} MbimIPv6Element_2;

typedef struct  {
    uint32_t                sessionId;
    uint32_t                IPv4ConfigurationAvailable;
    uint32_t                IPv6ConfigurationAvailable;
    uint32_t                IPv4AddressCount;
    MbimIPv4Element_2 **    IPv4Element;
    uint32_t                IPv6AddressCount;
    MbimIPv6Element_2 **    IPv6Element;
    char *                  IPv4Gateway;
    char *                  IPv6Gateway;
    uint32_t                IPv4DnsServerCount;
    char **                 IPv4DnsServer;
    uint32_t                IPv6DnsServerCount;
    char **                 IPv6DnsServer;
    uint32_t                IPv4Mtu;
    uint32_t                IPv6Mtu;
} MbimIPConfigurationInfo_2;

/**
 * MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES
 */
typedef struct {
    MbimService     deviceServiceId;
    uint32_t        dssPayload;
    uint32_t        maxDssInstances;
    uint32_t        cidCount;
    uint32_t *      cids;
} MbimDeviceServiceElement_2;

typedef struct  {
    uint32_t                        deviceServicesCount;
    uint32_t                        maxDssSessions;
    MbimDeviceServiceElement_2 **    deviceServiceElements;
} MbimDeviceServicesInfo_2;

/**
 * MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST
 */
typedef struct {
    MbimService     deviceServiceId;
    uint32_t        cidCount;
    uint32_t *      cidList;
} MbimEventEntry_2;

typedef struct  {
    uint32_t                elementCount;
    MbimEventEntry_2 **     eventEntries;
} MbimDeviceServiceSubscribeList_2;

/**
 * MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS
 */
typedef struct {
    uint32_t        filterSize;
    uint8_t *       packetFilter;
    uint8_t *       packetMask;
} MbimSinglePacketFilter_2;

typedef struct  {
    uint32_t                    sessionId;
    uint32_t                    packetFilterCount;
    MbimSinglePacketFilter_2 ** singlePacketFiltes;
} MbimPacketFilters_2;

/*****************************************************************************/
#define MAX_PDP_CP                  11
#define MAX_PDP     6
#define PDP_STATE_IDLE              1
#define PDP_STATE_ACTING            2
#define PDP_STATE_CONNECT           3
#define PDP_STATE_ESTING            4
#define PDP_STATE_ACTIVE            5
#define PDP_STATE_DESTING           6
#define PDP_STATE_DEACTING          7
#define PDP_STATE_ACT_ERROR         8
#define PDP_STATE_EST_ERROR         9
#define PDP_STATE_EST_UP_ERROR      10
#define DOWN_LINK_SPEED             0
#define UP_LINK_SPEED               1

#define IP_ADDR_SIZE                16
#define IPV6_ADDR_SIZE              64
#define PROPERTY_NAME_MAX           32
//#define PROPERTY_VALUE_MAX          92
#define SYS_NET_ADDR                "sys.data.net.addr"
#define SYS_NET_ACTIVATING_TYPE     "sys.data.activating.type"
#define SYS_IPV6_LINKLOCAL          "sys.data.ipv6.linklocal"
#define DEFAULT_PUBLIC_DNS2         "204.117.214.10"
// 2409:8084:8000:0010:2002:4860:4860:8888 maybe not correct
#define DEFAULT_PUBLIC_DNS2_IPV6    "2409:8084:8000:0010:2002:4860:4860:8888"

#define GSPS_ETH_UP_PROP            "ril.gsps.eth.up"

#define SOCKET_NAME_EXT_DATA        "ext_data"

typedef enum {
    PDP_IDLE,
    PDP_ACTIVATING,
    PDP_DEACTIVATING,
    PDP_ACTIVATED,
} PDPState;

typedef struct {
    int socketId;
    int cid;
    int secondary_cid;  // for fallback cid
    bool isPrimary;
    PDPState state;
    MbimContextType apnType;
    pthread_mutex_t mutex;
} PDPInfo;

typedef struct {
    int nCid;
    MbimContextIpType iPType;
    MbimContextType apnType;
    char* strApn;
} PDNInfo;

typedef struct IP_INFO {
    char dns1addr[IP_ADDR_SIZE];            /* IPV4 Primary MS DNS entries */
    char dns2addr[IP_ADDR_SIZE];            /* IPV4 secondary MS DNS entries */
    char ipv4netmask[IP_ADDR_SIZE];         /* IPV4 net mask */
    char ipladdr[IP_ADDR_SIZE];             /* IPV4 address local */
    char ipraddr[IP_ADDR_SIZE];             /* IPV4 address remote */
    char ipv6dns1addr[IPV6_ADDR_SIZE];      /* IPV6 Primary MS DNS entries */
    char ipv6dns2addr[IPV6_ADDR_SIZE];      /* IPV6 secondary MS DNS entries */
    char ipv6laddr[IPV6_ADDR_SIZE];         /* IPV6 address local */
    char ipv6raddr[IPV6_ADDR_SIZE];         /* IPV6 address remote */
    MbimContextIpType ip_type;
    int state;
    int cid;
} IP_INFO;

typedef enum {
    IPType_UNKNOWN = 0,
    IPV4    = 1,
    IPV6    = 2,
    IPV4V6  = 3
} IPType;

typedef enum {
    UNLOCK_PIN   = 0,
    UNLOCK_PIN2  = 1,
    UNLOCK_PUK   = 2,
    UNLOCK_PUK2  = 3
} PinPukType;

typedef enum {
  RIL_APPTYPE_UNKNOWN = 0,
  RIL_APPTYPE_SIM     = 1,
  RIL_APPTYPE_USIM    = 2,
  RIL_APPTYPE_RUIM    = 3,
  RIL_APPTYPE_CSIM    = 4,
  RIL_APPTYPE_ISIM    = 5
} RIL_AppType;

extern MbimDeviceServiceSubscribeList_2 *s_deviceServiceSubscribeList;

void
mbim_device_basic_connect(Mbim_Token                token,
                          uint32_t                  cid,
                          MbimMessageCommandType    commandType,
                          void *                    data,
                          size_t                    dataLen);

int
processBasicConnectUnsolicited(const char *s);

/*****************************************************************************/
/* basic_connect_device_caps */
void
basic_connect_device_caps(Mbim_Token         token,
                          MbimMessageCommandType    commandType,
                          void *                    data,
                          size_t                    dataLen);
void
query_device_caps        (Mbim_Token         token,
                          void *                    data,
                          size_t                    dataLen);

/* basic_connect_subscriber_ready_status */
void
basic_connect_subscriber_ready_status(Mbim_Token                token,
                                      MbimMessageCommandType    commandType,
                                      void *                    data,
                                      size_t                    dataLen);
void
query_subscriber_ready_status(Mbim_Token            token,
                              void *                data,
                              size_t                dataLen);

/* basic_connect_radio_state */
void
basic_connect_radio_state(Mbim_Token                token,
                          MbimMessageCommandType    commandType,
                          void *                    data,
                          size_t                    dataLen);
void
query_radio_state        (Mbim_Token                token,
                          void *                    data,
                          size_t                    dataLen);
void
set_radio_state          (Mbim_Token                token,
                          void *                    data,
                          size_t                    dataLen);

/* basic_connect_pin */
void
basic_connect_pin   (Mbim_Token                 token,
                     MbimMessageCommandType     commandType,
                     void *                     data,
                     size_t                     dataLen);
void
query_pin           (Mbim_Token                 token,
                     void *                     data,
                     size_t                     dataLen);
void
set_pin             (Mbim_Token                 token,
                     void *                     data,
                     size_t                     dataLen);

/* basic_connect_pin_list */
void
basic_connect_pin_list  (Mbim_Token                 token,
                         MbimMessageCommandType     commandType,
                         void *                     data,
                         size_t                     dataLen);
void
query_pin_list          (Mbim_Token                 token,
                         void *                     data,
                         size_t                     dataLen);

/* basic_connect_home_provider */
void
basic_connect_home_provider  (Mbim_Token                 token,
                              MbimMessageCommandType     commandType,
                              void *                     data,
                              size_t                     dataLen);
void
query_home_provider          (Mbim_Token                 token,
                              void *                     data,
                              size_t                     dataLen);

/* basic_connect_preferred_providers */
void
basic_connect_preferred_providers   (Mbim_Token                 token,
                                     MbimMessageCommandType     commandType,
                                     void *                     data,
                                     size_t                     dataLen);
void
query_preferred_providers           (Mbim_Token                 token,
                                     void *                     data,
                                     size_t                     dataLen);

/* basic_connect_visible_providers */
void
basic_connect_visible_providers (Mbim_Token                 token,
                                 MbimMessageCommandType     commandType,
                                 void *                     data,
                                 size_t                     dataLen);
void
query_visible_providers         (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);

/* basic_connect_register_state */
void
basic_connect_register_state    (Mbim_Token                 token,
                                 MbimMessageCommandType     commandType,
                                 void *                     data,
                                 size_t                     dataLen);
void
query_register_state            (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);

void
set_register_state              (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);

/* basic_connect_packet_service */
void
basic_connect_packet_service    (Mbim_Token                 token,
                                 MbimMessageCommandType     commandType,
                                 void *                     data,
                                 size_t                     dataLen);
void set_packet_service         (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);
void query_packet_service       (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);

/* basic_signal_state_service */
void
basic_signal_state_service      (Mbim_Token                 token,
                                 MbimMessageCommandType     commandType,
                                 void *                     data,
                                 size_t                     dataLen);
void
query_signal_state              (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);

/* basic_connect_connect */
void
basic_connect_connect           (Mbim_Token                 token,
                                 MbimMessageCommandType     commandType,
                                 void *                     data,
                                 size_t                     dataLen);
void
set_connect                     (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);
void
query_connect_info              (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);

/* basic_connect_provisioned_contexts */
void
basic_connect_provisioned_contexts  (Mbim_Token                 token,
                                     MbimMessageCommandType     commandType,
                                     void *                     data,
                                     size_t                     dataLen);
void
query_provisioned_contexts          (Mbim_Token                 token,
                                     void *                     data,
                                     size_t                     dataLen);
void
set_provisioned_contexts            (Mbim_Token                 token,
                                     void *                     data,
                                     size_t                     dataLen);

/* basic_connect_ip_configuration */
void
basic_connect_ip_configuration(Mbim_Token                token,
                               MbimMessageCommandType    commandType,
                               void *                    data,
                               size_t                    dataLen);

/* basic_connect_device_services */
void
basic_connect_device_services(Mbim_Token                token,
                              MbimMessageCommandType    commandType,
                              void *                    data,
                              size_t                    dataLen);
void
query_device_services        (Mbim_Token                token,
                              void *                    data,
                              size_t                    dataLen);

/* basic_connect_device_service_subscribe_list */
void
basic_connect_device_service_subscribe_list(Mbim_Token              token,
                                            MbimMessageCommandType  commandType,
                                            void *                  data,
                                            size_t                  dataLen);
void
set_device_service_subscribe_list(Mbim_Token            token,
                                  void *                data,
                                  size_t                dataLen);

/* basic_connect_network_idle_hint */
void
basic_connect_network_idle_hint (Mbim_Token                 token,
                                 MbimMessageCommandType     commandType,
                                 void *                     data,
                                 size_t                     dataLen);
void
query_network_idle_hint         (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);
void
set_network_idle_hint           (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);

/* basic_connect_emergency_mode */
void
basic_connect_emergency_mode    (Mbim_Token                 token,
                                 MbimMessageCommandType     commandType,
                                 void *                     data,
                                 size_t                     dataLen);
void
query_emergency_mode            (Mbim_Token                 token,
                                 void *                     data,
                                 size_t                     dataLen);
/*****************************************************************************/

void *  signal_process          ();

void *  init_apn_process        ();

void    cesq_unsol_rsp          (char *             line,
                                 RIL_SOCKET_ID      socket_id,
                                 char *             newLine);

void    queryAllActivePDNInfos  (int                channelID);

bool    isStrEmpty              (char *             str);

bool    isStrEqual              (char *             newStr,
                                 char *             oldStr);

bool    isProtocolEqual         (MbimContextIpType  newIPType,
                                 MbimContextIpType  oldIPType);

bool    isApnEqual              (char *             newStr,
                                 char *             oldStr);

int     getPDP                  (int                sessionId);

int     cgcontrdp_set_cmd_rsp   (ATResponse *       p_response);

int     getOnLinkPrefixLength   (char *             address);

int     getPDNCid               (int                index);

int     setPDPByIndex           (int                index,
                                 int                cid,
                                 PDPState           state,
                                 MbimContextType    apnType);

char *  getPDNAPN               (int                index);

PDPState getPDPState            (int                index);

MbimContextIpType getPDNIPType  (int                index);


#endif  // MBIM_DEVICE_BASIC_CONNECT_H_
