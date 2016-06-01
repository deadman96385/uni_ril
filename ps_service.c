/**
 * cmux.c: channel mux implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <netutils/ifc.h>
#include <net/if.h>

#include "os_api.h"
#include "ps_service.h"
#include "pty.h"
#include "at_tok.h"

#undef  PHS_LOGD
#define PHS_LOGD(x...)  RLOGD(x)

#define SYS_NET_ADDR                "sys.data.net.addr"
#define SYS_NET_ACTIVATING_TYPE     "sys.data.activating.type"
#define SYS_IPV6_LINKLOCAL          "sys.data.ipv6.linklocal"
#define DEFAULT_PUBLIC_DNS2         "204.117.214.10"
// Due to real network limited,
// 2409:8084:8000:0010:2002:4860:4860:8888 maybe not correct
#define DEFAULT_PUBLIC_DNS2_IPV6    "2409:8084:8000:0010:2002:4860:4860:8888"
#define MODEM_CONFIG                "persist.radio.modem.config"

#define GSPS_ETH_UP_PROP            "ril.gsps.eth.up"
#define GSPS_ETH_DOWN_PROP          "ril.gsps.eth.down"
#define GSPS_IPV4_ADDR_HEADER       "192.168."
#define DHCP_DNSMASQ_LEASES_FILE    "/data/misc/dhcp/dnsmasq.leases"

#define SOCKET_NAME_EXT_DATA        "ext_data"

struct PDP_INFO pdp_info[MAX_PDP_NUM];
static char s_SavedDns[IP_ADDR_SIZE] = {0};
static char s_SavedDns_IPV6[IP_ADDR_SIZE * 4] ={0};

int s_extDataFd = -1;
mutex ps_service_mutex;
extern const char *s_modem;

extern int findInBuf(char *buf, int len, char *needle);
static int upNetInterface(int cidIndex, IP_TYPE ipType);

void setSockTimeout() {
    struct timeval writetm, recvtm;
    writetm.tv_sec = 1;  // write timeout: 1s
    writetm.tv_usec = 0;
    recvtm.tv_sec = 10;  // recv timeout: 10s
    recvtm.tv_usec = 0;

    if (setsockopt(s_extDataFd, SOL_SOCKET, SO_SNDTIMEO, &writetm,
                     sizeof(writetm)) == -1) {
        PHS_LOGE("WARNING: Cannot set send timeout value on socket: %s",
                 strerror(errno));
    }
    if (setsockopt(s_extDataFd, SOL_SOCKET, SO_RCVTIMEO, &recvtm,
                     sizeof(recvtm)) == -1) {
        PHS_LOGE("WARNING: Cannot set receive timeout value on socket: %s",
                 strerror(errno));
    }
}

void *listen_ext_data_thread(void) {
    PHS_LOGD("try to connect socket ext_data...");

    do {
        s_extDataFd = socket_local_client(SOCKET_NAME_EXT_DATA,
                ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
        usleep(10 * 1000);  // wait for 10ms, try again
    } while (s_extDataFd < 0);
    PHS_LOGD("connect to ext_data socket success!");

    setSockTimeout();
    return NULL;
}

void sendCmdToExtData(char cmd[]) {
    int ret;
    int retryTimes = 0;

RECONNECT:
    retryTimes = 0;
    while (s_extDataFd < 0 && retryTimes < 10) {
        s_extDataFd = socket_local_client(SOCKET_NAME_EXT_DATA,
                ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
        usleep(10 * 1000);  // wait for 10ms, try again
        retryTimes++;
    }
    if (s_extDataFd >= 0 && retryTimes != 0) {
        setSockTimeout();
    }

    if (s_extDataFd >= 0) {
        int len = strlen(cmd) + 1;
        if (TEMP_FAILURE_RETRY(write(s_extDataFd, cmd, len)) !=
                                      len) {
            PHS_LOGE("Failed to write cmd to ext_data!");
            close(s_extDataFd);
            s_extDataFd = -1;
        } else {
            int error;
            if (TEMP_FAILURE_RETRY(read(s_extDataFd, &error, sizeof(error)))
                    <= 0) {
                PHS_LOGE("read error from ext_data!");
                close(s_extDataFd);
                s_extDataFd = -1;
            }
        }
    }
}

void ps_service_init(void) {
    int i;
    int ret;
    pthread_t tid;
    pthread_attr_t attr;

    memset(pdp_info, 0x0, sizeof(pdp_info));
    for (i = 0; i < MAX_PDP_NUM; i++) {
        pdp_info[i].state = PDP_STATE_IDLE;
        cond_init(&pdp_info[i].cond_timeout, NULL);
        mutex_init(&pdp_info[i].mutex_timeout, NULL);
    }
    mutex_init(&ps_service_mutex, NULL);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid, &attr, (void *)listen_ext_data_thread, NULL);
    if (ret < 0) {
        PHS_LOGE("Failed to create listen_ext_data_thread errno: %d", errno);
        return;
    }
}

int get_ipv6addr(const char *prop, int cidIndex) {
    char netInterface[NET_INTERFACE_LENGTH] = {0};
    const int maxRetry = 120;  // wait 12s
    int retry = 0;
    int setup_success = 0;
    char cmd[MAX_CMD] = {0};
    const int ipv6AddrLen = 32;

    snprintf(netInterface, sizeof(netInterface), "%s%d", prop, cidIndex);
    PHS_LOGD("query interface %s", netInterface);
    while (!setup_success) {
        char rawaddrstr[INET6_ADDRSTRLEN], addrstr[INET6_ADDRSTRLEN];
        unsigned int prefixlen;
        int lasterror = 0, i, j, ret;
        char ifname[NET_INTERFACE_LENGTH];  // Currently, IFNAMSIZ = 16.
        FILE *f = fopen("/proc/net/if_inet6", "r");
        if (!f) {
            return -errno;
        }

        // Format:
        // 20010db8000a0001fc446aa4b5b347ed 03 40 00 01    wlan0
        while (fscanf(f, "%32s %*02x %02x %*02x %*02x %63s\n", rawaddrstr,
                &prefixlen, ifname) == 3) {
            // Is this the interface we're looking for?
            if (strcmp(netInterface, ifname)) {
                continue;
            }

            // Put the colons the address
            // and add ':' to separate every 4 addr char
            for (i = 0, j = 0; i < ipv6AddrLen; i++, j++) {
                addrstr[j] = rawaddrstr[i];
                if (i % 4 == 3) {
                    addrstr[++j] = ':';
                }
            }
            addrstr[j - 1] = '\0';
            PHS_LOGD("getipv6addr found ip %s", addrstr);
            // Don't add the link-local address
            if (strncmp(addrstr, "fe80:", sizeof("fe80:") - 1) == 0) {
                PHS_LOGD("getipv6addr found fe80");
                continue;
            }
            snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_ip %s", prop,
                      cidIndex, addrstr);
            system(cmd);
            PHS_LOGD("getipv6addr propset %s ", cmd);
            setup_success = 1;
            break;
        }

        fclose(f);
        if (!setup_success) {
            usleep(100 * 1000);
            retry++;
        }
        if (retry == maxRetry) {
            break;
        }
    }
    return setup_success;
}

int down_netcard(int cid, char *netinterface) {
    int index = cid - 1;
    int isAutoTest = 0;
    char linker[MAX_CMD] = {0};
    char cmd[MAX_CMD];
    char gspsprop[PROPERTY_VALUE_MAX] = {0};

    if (cid < 1 || cid >= MAX_PDP_NUM || netinterface == NULL) {
        return 0;
    }
    PHS_LOGD("down cid %d, network interface %s ", cid, netinterface);
    snprintf(linker, sizeof(linker), "%s%d", netinterface, index);
    if (ifc_disable(linker)) {
        PHS_LOGE("ifc_disable %s fail: %s\n", linker, strerror(errno));
    }
    if (ifc_clear_addresses(linker)) {
        PHS_LOGE("ifc_clear_addresses %s fail: %s\n", linker, strerror(errno));
    }

    property_get(GSPS_ETH_DOWN_PROP, gspsprop, "0");
    isAutoTest = atoi(gspsprop);

    snprintf(cmd, sizeof(cmd), "<ifdown>%s;%s;%d", linker, "IPV4V6",
              isAutoTest);
    sendCmdToExtData(cmd);
    property_set(GSPS_ETH_DOWN_PROP, "0");

    PHS_LOGD("data_off execute done");
    return 1;
}

int dispose_data_fallback(int masterCid, int secondaryCid) {
    int master_index = masterCid - 1;
    int secondary_index = secondaryCid - 1;
    char cmd[MAX_CMD];
    char prop[PROPERTY_VALUE_MAX];
    char ETH_SP[PROPERTY_NAME_MAX];  // "ro.modem.*.eth"
    int count = 0;

    if (masterCid < 1|| masterCid >= MAX_PDP_NUM || secondaryCid <1 ||
        secondaryCid >= MAX_PDP_NUM) {
    // 1~11 is valid cid
        return 0;
    }
    snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.%s.eth", s_modem);
    property_get(ETH_SP, prop, "veth");
    PHS_LOGD("master ip type %d ,secondary ip type %d",
             pdp_info[master_index].ip_state,
             pdp_info[secondary_index].ip_state);
    // fallback get same type ip with master
    if (pdp_info[master_index].ip_state ==
        pdp_info[secondary_index].ip_state) {
        return 0;
    }
    if (pdp_info[master_index].ip_state == IPV4) {
        // down ipv4, because need set ipv6 firstly
        down_netcard(masterCid, prop);
        // copy secondary ppp to master ppp
        memcpy(pdp_info[master_index].ipv6laddr,
                pdp_info[secondary_index].ipv6laddr,
                sizeof(pdp_info[master_index].ipv6laddr));
        memcpy(pdp_info[master_index].ipv6dns1addr,
                pdp_info[secondary_index].ipv6dns1addr,
                sizeof(pdp_info[master_index].ipv6dns1addr));
        memcpy(pdp_info[master_index].ipv6dns2addr,
                pdp_info[secondary_index].ipv6dns2addr,
                sizeof(pdp_info[master_index].ipv6dns2addr));
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_ip %s", prop,
                  master_index, pdp_info[master_index].ipv6laddr);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_dns1 %s", prop,
                  master_index, pdp_info[master_index].ipv6dns1addr);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_dns2 %s", prop,
                  master_index, pdp_info[master_index].ipv6dns2addr);
        system(cmd);
    } else if (pdp_info[master_index].ip_state == IPV6) {
        // copy secondary ppp to master ppp
        memcpy(pdp_info[master_index].ipladdr,
                pdp_info[secondary_index].ipladdr,
                sizeof(pdp_info[master_index].ipladdr));
        memcpy(pdp_info[master_index].dns1addr,
                pdp_info[secondary_index].dns1addr,
                sizeof(pdp_info[master_index].dns1addr));
        memcpy(pdp_info[master_index].dns2addr,
                pdp_info[secondary_index].dns2addr,
                sizeof(pdp_info[master_index].dns2addr));
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip %s", prop,
                  master_index, pdp_info[master_index].ipladdr);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns1 %s", prop,
                  master_index, pdp_info[master_index].dns1addr);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns2 %s", prop,
                  master_index, pdp_info[secondary_index].dns2addr);
        system(cmd);
    }
    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d", prop,
              master_index, IPV4V6);
    system(cmd);
    pdp_info[master_index].ip_state = IPV4V6;
    return 1;
}

int cvt_cgdata_set_req(AT_CMD_REQ_T * req) {
    cmux_t *mux;
    int cid, pdp_index;
    int err, ret;
    int master_cid = 0;
    int i, ip_state, count = 0;
    char prop[PROPERTY_VALUE_MAX];
    char ipv6_on[PROPERTY_VALUE_MAX];
    char gspsprop[PROPERTY_VALUE_MAX];
    char ETH_SP[PROPERTY_NAME_MAX];  // "ro.modem.*.eth"
    char linker[MAX_CMD] = {0};
    char ipv6_dhcpcd_cmd[MAX_CMD] = {0};
    char *cmdStr, *out;
    char atBuffer[MAX_AT_CMD_LEN], error_str[MAX_AT_CMD_LEN];
    char atCmdStr[MAX_AT_CMD_LEN];

    if (req == NULL) {
        PHS_LOGE("leave cvt_cgdata_set_req AT_RESULT_NG");
        return AT_RESULT_NG;
    }
    PHS_LOGD("enter cvt_cgdata_set_req cmd:%s cmdlen:%d  ",
             req->cmd_str, req->len);
    memset(atBuffer, 0, MAX_AT_CMD_LEN);
    memset(atCmdStr, 0, MAX_AT_CMD_LEN);

    cmdStr = atBuffer;
    strncpy(cmdStr, req->cmd_str, req->len);

    err = at_tok_start(&cmdStr, '=');
    if (err < 0) {
        PHS_LOGD("parse cmd error");
        return AT_RESULT_NG;
    }

    /* get L2P */
    err = at_tok_nextstr(&cmdStr, &out);
    if (err < 0) {
        PHS_LOGD("parse cmd error");
        return AT_RESULT_NG;
    }

    /* Get cid */
    err = at_tok_nextint(&cmdStr, &cid);
    if (err < 0) {
        PHS_LOGD("parse cmd error");
        return AT_RESULT_NG;
    }
    if (at_tok_hasmore(&cmdStr)) {
        err = at_tok_nextint(&cmdStr, &master_cid);
        if (err < 0) {
            PHS_LOGD("parse master cid error");
            master_cid = 0;
        }
    }
    pdp_index = cid - 1;

    mutex_lock(&ps_service_mutex);
    mux = adapter_get_cmux(req->cmd_type, TRUE);
    pdp_info[pdp_index].state = PDP_STATE_ACTING;
    PHS_LOGD("PDP_STATE_ACTING");
    pdp_info[pdp_index].pty = req->recv_pty;
    pdp_info[pdp_index].cmux = mux;
    pdp_info[pdp_index].cid = cid;
    pdp_info[pdp_index].error_num = -1;
    adapter_cmux_register_callback(mux, (void *)cvt_cgdata_set_rsp, pdp_index);
    ret = adapter_cmux_write_for_ps(mux, req->cmd_str, req->len, req->timeout);

    PHS_LOGD("PDP activate result:ret = %d,state=%d", ret,
             pdp_info[pdp_index].state);
    if (ret == AT_RESULT_TIMEOUT) {
        PHS_LOGD("CGDATA PDP activate timeout ");
        goto ERROR;
    }

    if (pdp_info[pdp_index].state != PDP_STATE_CONNECT) {
        PHS_LOGD("PDP activate error :%d", pdp_info[pdp_index].state);
        adapter_pty_end_cmd(req->recv_pty);
        adapter_free_cmux_for_ps(mux);
        if (pdp_info[pdp_index].error_num >= 0) {
            snprintf(error_str, sizeof(error_str), "+CME ERROR: %d\r",
                      pdp_info[pdp_index].error_num);
            adapter_pty_write(req->recv_pty, error_str, strlen(error_str));
        } else {
            adapter_pty_write(req->recv_pty, "ERROR\r", strlen("ERROR\r"));
        }
        pdp_info[pdp_index].state = PDP_STATE_IDLE;
        mutex_unlock(&ps_service_mutex);
        return AT_RESULT_OK;
    }
    pdp_info[pdp_index].state = PDP_STATE_ESTING;
    pdp_info[pdp_index].manual_dns = 0;

    if (isLTE()) {
        snprintf(atCmdStr, sizeof(atCmdStr), "AT+CGCONTRDP=%d\r", cid);
    } else {
        snprintf(atCmdStr, sizeof(atCmdStr), "AT+SIPCONFIG=%d\r", cid);
    }
    adapter_cmux_register_callback(mux, (void *)cvt_cgcontrdp_rsp, pdp_index);
    ret = adapter_cmux_write_for_ps(mux, atCmdStr, strlen(atCmdStr), 10);
    if (ret == AT_RESULT_TIMEOUT) {
        PHS_LOGD("Get IP address timeout ");
        pdp_info[pdp_index].state = PDP_STATE_DEACTING;
        snprintf(atCmdStr, sizeof(atCmdStr), "AT+CGACT=0,%d\r", cid);
        adapter_cmux_register_callback(mux, cvt_cgact_deact_rsp1,
                (uintptr_t)req->recv_pty);
        ret = adapter_cmux_write(mux, atCmdStr, strlen(atCmdStr),
                                 req->timeout);
        if (ret == AT_RESULT_TIMEOUT) {
            PHS_LOGD("PDP deactivate timeout ");
            goto ERROR;
        }
    }

    if (pdp_info[pdp_index].state == PDP_STATE_ACTIVE) {
        PHS_LOGD("PS connected successful");

        // if fallback, need map ipv4 and ipv6 to one net device
        if (dispose_data_fallback(master_cid, cid)) {
            cid = master_cid;
        }

        PHS_LOGD("PS ip_state = %d", pdp_info[pdp_index].ip_state);
        if (upNetInterface(pdp_index, pdp_info[pdp_index].ip_state) == 0) {
            PHS_LOGD("get IPv6 address timeout ");
            goto ERROR;
        }
        PHS_LOGD("data_on execute done");

        adapter_pty_end_cmd(req->recv_pty);
        adapter_free_cmux_for_ps(mux);
        adapter_pty_write(req->recv_pty, "CONNECT\r", strlen("CONNECT\r"));
        mutex_unlock(&ps_service_mutex);
        return AT_RESULT_OK;
    }
ERROR:
    adapter_pty_write(req->recv_pty, "ERROR\r", strlen("ERROR\r"));
    PHS_LOGD("Getting IP addr and PDP activate error :%d",
            pdp_info[pdp_index].state);
    pdp_info[pdp_index].state = PDP_STATE_IDLE;
    adapter_pty_end_cmd(req->recv_pty);
    adapter_free_cmux_for_ps(mux);
    mutex_unlock(&ps_service_mutex);
    return AT_RESULT_OK;
}

void ip_hex_to_str(unsigned int in, char *out, int out_size) {
    int i;
    unsigned int mid;
    char str[IP_ADDR_SIZE];

    for (i = 3; i >= 0; i--) {
        mid = (in & (0xff << 8 * i)) >> 8 * i;
        snprintf(str, sizeof(str), "%u", mid);
        if (i == 3) {
            strncpy(out, str, out_size);
            out[out_size - 1] = '\0';
        } else {
            strncat(out, str, strlen(str));
        }
        if (i != 0) {
            strncat(out, ".", strlen("."));
        }
    }
}

void cvt_reset_dns2(char *out, size_t dataLen) {
    if (strlen(s_SavedDns) > 0) {
        PHS_LOGD("Use saved DNS2 instead.");
        memcpy(out, s_SavedDns, sizeof(s_SavedDns));
    } else {
        PHS_LOGD("Use default DNS2 instead.");
        snprintf(out, dataLen, "%s", DEFAULT_PUBLIC_DNS2);
    }
}

int cvt_sipconfig_rsp(AT_CMD_RSP_T * rsp,
        uintptr_t __attribute__((unused)) user_data) {
    int ret;
    int err;
    char *input;
    int cid, nsapi;
    char *out;
    int ip_hex, dns1_hex, dns2_hex;
    char ip[IP_ADDR_SIZE], dns1[IP_ADDR_SIZE], dns2[IP_ADDR_SIZE];
    char cmd[MAX_CMD];
    char prop[PROPERTY_VALUE_MAX];
    char ETH_SP[PROPERTY_NAME_MAX];  // "ro.modem.*.eth"
    char linker[MAX_CMD] = {0};
    int count = 0;

    PHS_LOGD("%s enter", __FUNCTION__);

    if (rsp == NULL) {
        PHS_LOGD("leave cvt_ipconf_rsp:AT_RESULT_NG");
        return AT_RESULT_NG;
    }

    memset(ip, 0, IP_ADDR_SIZE);
    memset(dns1, 0, IP_ADDR_SIZE);
    memset(dns2, 0, IP_ADDR_SIZE);
    input = rsp->rsp_str;
    input[rsp->len - 1] = '\0';
    if (findInBuf(input, rsp->len, "+SIPCONFIG")) {
        do {
            err = at_tok_start(&input, ':');
            if (err < 0) {
                break;
            }
            err = at_tok_nextint(&input, &cid);
            if (err < 0) {
                break;
            }
            err = at_tok_nextint(&input, &nsapi);
            if (err < 0) {
                break;
            }
            err = at_tok_nexthexint(&input, &ip_hex);  // ip
            if (err < 0) {
                break;
            }
            ip_hex_to_str(ip_hex, ip, sizeof(ip));
            err = at_tok_nexthexint(&input, &dns1_hex);  // dns1
            if (err < 0) {
                break;
            } else if (dns1_hex != 0x0) {
                ip_hex_to_str(dns1_hex, dns1, sizeof(dns1));
            }
            err = at_tok_nexthexint(&input, &dns2_hex);  // dns2
            if (err < 0) {
                break;
            } else if (dns2_hex != 0x0) {
                ip_hex_to_str(dns2_hex, dns2, sizeof(dns2));
            }
            if (cid < MAX_PDP_NUM && (cid >= 1)) {
                /*Save ppp info */
                strlcpy(pdp_info[cid - 1].ipladdr, ip,
                        sizeof(pdp_info[cid - 1].ipladdr));

                strlcpy(pdp_info[cid - 1].dns1addr, dns1,
                        sizeof(pdp_info[cid - 1].dns1addr));

                strlcpy(pdp_info[cid - 1].dns2addr, dns2,
                        sizeof(pdp_info[cid - 1].dns2addr));

                // no return dns
                if (!strncasecmp(dns1, "0.0.0.0", sizeof("0.0.0.0"))) {
                    strlcpy(pdp_info[cid - 1].dns1addr,
                            pdp_info[cid - 1].userdns1addr,
                            sizeof(pdp_info[cid - 1].dns1addr));
                }
                // no return dns
                if (!strncasecmp(dns2, "0.0.0.0", sizeof("0.0.0.0"))) {
                    strlcpy(pdp_info[cid - 1].dns2addr,
                            pdp_info[cid - 1].userdns2addr,
                            sizeof(pdp_info[cid - 1].dns2addr));
                }

                upNetInterface(cid - 1, IPV4);
                snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip %s", prop,
                          cid - 1, ip);
                system(cmd);
                if (dns1_hex != 0x0) {
                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns1 %s",
                              prop, cid - 1, dns1);
                } else {
                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns1 \"\"",
                              prop, cid - 1);
                }
                system(cmd);
                if (dns2_hex != 0x0) {
                    if (!strcmp(dns1, dns2)) {
                        PHS_LOGD("Two DNS are the same,so need to reset dns2!");
                        cvt_reset_dns2(dns2, sizeof(dns2));
                    } else {
                        PHS_LOGD("Backup DNS2");
                        memset(s_SavedDns, 0, sizeof(s_SavedDns));
                        memcpy(s_SavedDns, dns2, sizeof(dns2));
                    }
                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns2 %s",
                              prop, cid - 1, dns2);
                } else {
                    PHS_LOGD("DNS2 is empty!!");
                    memset(dns2, 0, IP_ADDR_SIZE);
                    cvt_reset_dns2(dns2, sizeof(dns2));
                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns2 %s",
                              prop, cid - 1, dns2);
                }
                system(cmd);

                pdp_info[cid - 1].state = PDP_STATE_ACTIVE;
                PHS_LOGD("PDP_STATE_ACTIVE");
                PHS_LOGD("cid=%d ip:%s,dns1:%s,dns2:%s", cid, ip, dns1, dns2);
            } else {
                pdp_info[cid - 1].state = PDP_STATE_EST_UP_ERROR;
                PHS_LOGD("PDP_STATE_EST_UP_ERROR:cid of pdp is error!");
            }
        } while (0);
        return AT_RESULT_OK;
    }
    if (adapter_cmd_is_end(rsp->rsp_str, rsp->len) == TRUE) {
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_wakeup_cmux(rsp->recv_cmux);
        return AT_RESULT_OK;
    }

    return AT_RESULT_NG;
}

int cvt_cgdata_set_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data) {
    int ret;
    int rsp_type;
    int pdp_index;
    char cmd[MAX_CMD];
    char *input;
    int err, error_num;

    if (rsp == NULL) {
        PHS_LOGD("leave cvt_cgdata_set_rsp:AT_RESULT_NG1");
        return AT_RESULT_NG;
    }
    rsp_type = adapter_get_rsp_type(rsp->rsp_str, rsp->len);
    PHS_LOGD("cvt_cgdata_set_rsp rsp_type=%d ", rsp_type);
    if (rsp_type == AT_RSP_TYPE_MID) {
        PHS_LOGD("leave cvt_cgdata_set_rsp:AT_RESULT_NG2");
        return AT_RESULT_NG;
    }
    pdp_index = user_data;
    if (pdp_index < 0 || pdp_index >= MAX_PDP_NUM) {
        PHS_LOGD("leave cvt_cgdata_set_rsp:AT_RESULT_NG3");
        return AT_RESULT_NG;
    }

    if (rsp_type == AT_RSP_TYPE_CONNECT) {
        pdp_info[pdp_index].state = PDP_STATE_CONNECT;
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_wakeup_cmux(rsp->recv_cmux);
    } else if (rsp_type == AT_RSP_TYPE_ERROR) {
        PHS_LOGD("PDP activate error\r");
        pdp_info[pdp_index].state = PDP_STATE_ACT_ERROR;
        input = rsp->rsp_str;
        if (strStartsWith(input, "+CME ERROR:")) {
            err = at_tok_start(&input, ':');
            if (err >= 0) {
                err = at_tok_nextint(&input, &error_num);
                if (err >= 0) {
                    if (error_num >= 0)
                        pdp_info[pdp_index].error_num = error_num;
                }
            }
        }
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_wakeup_cmux(rsp->recv_cmux);
    } else {
        PHS_LOGD("leave cvt_cgdata_set_rsp:AT_RESULT_NG2");
        return AT_RESULT_NG;
    }
    return AT_RESULT_OK;
}

int cvt_cgact_deact_req(AT_CMD_REQ_T * req) {
    cmux_t *mux;
    int status, tmp_cid = -1;
    int err;
    char cmd[MAX_CMD], atCmdStr[MAX_AT_CMD_LEN], cgev_str[MAX_AT_CMD_LEN];
    char *cmdStr;
    char prop[PROPERTY_VALUE_MAX];
    int count = 0;
    int maxPDPNum = MAX_PDP_NUM;
    char ETH_SP[PROPERTY_NAME_MAX];  // "ro.modem.*.eth"
    int tmp_cid2 = -1;
    char ipv6_dhcpcd_cmd[MAX_CMD] = {0};

    if (req == NULL) {
        return AT_RESULT_NG;
    }
    memset(atCmdStr, 0, MAX_AT_CMD_LEN);
    cmdStr = req->cmd_str;
    err = at_tok_start(&cmdStr, '=');
    if (err < 0) {
        return AT_RESULT_NG;
    }
    err = at_tok_nextint(&cmdStr, &status);
    if (err < 0 || status != 0) {
        return AT_RESULT_NG;
    }

    if (at_tok_hasmore(&cmdStr)) {
        err = at_tok_nextint(&cmdStr, &tmp_cid);
        if (err < 0) {
            return AT_RESULT_NG;
        }
        if (at_tok_hasmore(&cmdStr)) {
            err = at_tok_nextint(&cmdStr, &tmp_cid2);
            PHS_LOGD("has more  tmp_cid2 = %d", tmp_cid2);
            if (err < 0) {
                return AT_RESULT_NG;
            }
        }

        mutex_lock(&ps_service_mutex);
        mux = adapter_get_cmux(req->cmd_type, TRUE);
        /* deactivate PDP connection */
        if (0 < tmp_cid && tmp_cid < MAX_PDP_NUM) {
            pdp_info[tmp_cid - 1].state = PDP_STATE_DESTING;
            PHS_LOGD("PDP_STATE_DEACTING");
            pdp_info[tmp_cid - 1].state = PDP_STATE_DEACTING;
            pdp_info[tmp_cid - 1].cmux = mux;
            if (tmp_cid2 != 0) {
                if (tmp_cid2 != -1) {
                    snprintf(atCmdStr, sizeof(atCmdStr), "AT+CGACT=0,%d,%d\r",
                            tmp_cid, tmp_cid2);
                } else {
                    snprintf(atCmdStr, sizeof(atCmdStr), "AT+CGACT=0,%d\r",
                            tmp_cid);
                }
                PHS_LOGD("atCmdStr = %s", atCmdStr);
                adapter_cmux_register_callback(mux, cvt_cgact_deact_rsp2,
                        (uintptr_t)req->recv_pty);
                adapter_cmux_write(mux, atCmdStr, strlen(atCmdStr),
                        req->timeout);
            } else {
                adapter_pty_write(req->recv_pty, "OK\r", strlen("OK\r"));
                adapter_pty_end_cmd(req->recv_pty);
                adapter_free_cmux(mux);
            }
            pdp_info[tmp_cid - 1].state = PDP_STATE_IDLE;

            usleep(200 * 1000);
            snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.%s.eth", s_modem);
            property_get(ETH_SP, prop, "veth");
            down_netcard(tmp_cid, prop);

            if (pdp_info[tmp_cid - 1].ip_state == IPV6
                    || pdp_info[tmp_cid - 1].ip_state == IPV4V6) {
                snprintf(ipv6_dhcpcd_cmd, sizeof(ipv6_dhcpcd_cmd),
                        "dhcpcd_ipv6:%s%d", prop, tmp_cid - 1);
                property_set("ctl.stop", ipv6_dhcpcd_cmd);
            }
            PHS_LOGD("data_off execute done");

            snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d", prop,
                      tmp_cid - 1, UNKNOWN);
            system(cmd);

            if ((tmp_cid2 != -1) && (tmp_cid2 != 0)) {
                down_netcard(tmp_cid2, prop);
                if (pdp_info[tmp_cid - 1].ip_state == IPV6
                        || pdp_info[tmp_cid - 1].ip_state == IPV4V6) {
                    snprintf(ipv6_dhcpcd_cmd, sizeof(ipv6_dhcpcd_cmd),
                            "dhcpcd_ipv6:%s%d", prop, tmp_cid2 - 1);
                    property_set("ctl.stop", ipv6_dhcpcd_cmd);
                }
                snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                          prop, tmp_cid2 - 1, UNKNOWN);
                system(cmd);
            }
        } else {
            PHS_LOGD("Just send AT+CGACT ");
            snprintf(atCmdStr, sizeof(atCmdStr), "AT+CGACT=0,%d\r", tmp_cid);
            adapter_cmux_register_callback(mux, cvt_cgact_deact_rsp,
                    (uintptr_t)req->recv_pty);
            adapter_cmux_write(mux, atCmdStr, strlen(atCmdStr), req->timeout);
        }
        mutex_unlock(&ps_service_mutex);
    } else {
        int i;
        mutex_lock(&ps_service_mutex);
        mux = adapter_get_cmux(req->cmd_type, TRUE);
        snprintf(atCmdStr, sizeof(atCmdStr), "AT+CGACT=0\r");
        adapter_cmux_register_callback(mux, cvt_cgact_deact_rsp,
                (uintptr_t)req->recv_pty);
        adapter_cmux_write(mux, atCmdStr, strlen(atCmdStr), req->timeout);

        for (i = 0; i < maxPDPNum; i++) {  /* deactivate PDP connection */
            PHS_LOGD("context id %d state : %d", i, pdp_info[i].state);
            if (pdp_info[i].state == PDP_STATE_ACTIVE) {
                pdp_info[i].cmux = mux;
                pdp_info[i].state = PDP_STATE_IDLE;

                usleep(200 * 1000);
                snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.%s.eth", s_modem);
                property_get(ETH_SP, prop, "veth");
                down_netcard(tmp_cid, prop);

                if (pdp_info[i].ip_state == IPV6
                        || pdp_info[i].ip_state == IPV4V6) {
                    snprintf(ipv6_dhcpcd_cmd, sizeof(ipv6_dhcpcd_cmd),
                            "dhcpcd_ipv6:%s%d", prop, i);
                    property_set("ctl.stop", ipv6_dhcpcd_cmd);
                }
                PHS_LOGD("data_off execute done");
                snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                          prop, i, UNKNOWN);
                system(cmd);
            }
        }
        mutex_unlock(&ps_service_mutex);
    }
    return AT_RESULT_PROGRESS;
}

int cvt_cgact_deact_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data) {
    int ret;

    if (rsp == NULL) {
        return AT_RESULT_NG;
    }

    if (adapter_cmd_is_end(rsp->rsp_str, rsp->len) == TRUE) {
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_pty_write((pty_t *)user_data, rsp->rsp_str, rsp->len);
        adapter_pty_end_cmd((pty_t *)user_data);
        adapter_free_cmux(rsp->recv_cmux);
        return AT_RESULT_OK;
    }
    return AT_RESULT_NG;
}

int cvt_cgact_deact_rsp1(AT_CMD_RSP_T * rsp,
        uintptr_t __attribute__((unused)) user_data) {
    int ret;
    if (rsp == NULL) {
        return AT_RESULT_NG;
    }
    if (adapter_cmd_is_end(rsp->rsp_str, rsp->len) == TRUE) {
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_wakeup_cmux(rsp->recv_cmux);
        return AT_RESULT_OK;
    }
    return AT_RESULT_NG;
}

int cvt_cgact_deact_rsp2(AT_CMD_RSP_T * rsp, uintptr_t user_data) {
    int ret;

    if (rsp == NULL) {
        return AT_RESULT_NG;
    }

    if (adapter_cmd_is_end(rsp->rsp_str, rsp->len) == TRUE) {
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_pty_write((pty_t *)user_data, rsp->rsp_str, rsp->len);
        adapter_pty_end_cmd((pty_t *)user_data);
        adapter_free_cmux(rsp->recv_cmux);
        return AT_RESULT_OK;
    }
    return AT_RESULT_NG;
}

int cvt_cgact_deact_rsp3(AT_CMD_RSP_T * rsp,
        uintptr_t __attribute__((unused)) user_data) {
    int ret;

    if (rsp == NULL) {
        return AT_RESULT_NG;
    }

    if (adapter_cmd_is_end(rsp->rsp_str, rsp->len) == TRUE) {
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_wakeup_cmux(rsp->recv_cmux);
        return AT_RESULT_OK;
    }
    return AT_RESULT_NG;
}

int cvt_cgact_act_req(AT_CMD_REQ_T * req) {
    cmux_t *mux;

    if (req == NULL) {
        return AT_RESULT_NG;
    }

    mux = adapter_get_cmux(req->cmd_type, TRUE);
    adapter_cmux_register_callback(mux, cvt_cgact_act_rsp,
            (uintptr_t)req->recv_pty);
    adapter_cmux_write(mux, req->cmd_str, req->len, req->timeout);
    return AT_RESULT_PROGRESS;
}

int cvt_cgact_act_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data) {
    int ret;

    if (rsp == NULL) {
        return AT_RESULT_NG;
    }

    if (adapter_cmd_is_end(rsp->rsp_str, rsp->len) == TRUE) {
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_pty_write((pty_t *)user_data, rsp->rsp_str, rsp->len);
        adapter_pty_end_cmd((pty_t *)user_data);
        adapter_free_cmux(rsp->recv_cmux);
        return AT_RESULT_OK;
    }
    return AT_RESULT_NG;
}

int cvt_cgdcont_read_req(AT_CMD_REQ_T * req) {
    cmux_t *mux;

    if (req == NULL) {
        return AT_RESULT_NG;
    }

    mux = adapter_get_cmux(req->cmd_type, TRUE);
    adapter_cmux_register_callback(mux, cvt_cgdcont_read_rsp,
            (uintptr_t)req->recv_pty);
    adapter_cmux_write(mux, req->cmd_str, req->len, req->timeout);
    return AT_RESULT_PROGRESS;
}

int cvt_cgdcont_read_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data) {
    int ret, err;
    int tmp_cid = 0;
    int in_len;
    char *input, *out;
    char atCmdStr[MAX_AT_CMD_LEN], ip[IP_ADDR_MAX], net[IP_ADDR_MAX];

    if (rsp == NULL) {
        return AT_RESULT_NG;
    }
    memset(atCmdStr, 0, sizeof(atCmdStr));
    memset(net, 0, sizeof(net));
    in_len = rsp->len;
    input = rsp->rsp_str;
    input[in_len - 1] = '\0';
    if (findInBuf(input, in_len, "+CGDCONT")) {
        do {
            err = at_tok_start(&input, ':');
            if (err < 0) {
                break;
            }
            err = at_tok_nextint(&input, &tmp_cid);
            if (err < 0) {
                break;
            }
            err = at_tok_nextstr(&input, &out);  // ip
            if (err < 0) {
                break;
            }
            strncpy(ip, out, sizeof(ip));
            ip[sizeof(ip) - 1] = '\0';
            err = at_tok_nextstr(&input, &out);  // cmnet
            if (err < 0) {
                break;
            }
            strncpy(net, out, sizeof(net));
            net[sizeof(net) - 1] = '\0';
            PHS_LOGD("cvt_cgdcont_read_rsp cid =%d", tmp_cid);
            if ((tmp_cid <= MAX_PDP_NUM)
                    && (pdp_info[tmp_cid - 1].state == PDP_STATE_ACTIVE)) {
                if (pdp_info[tmp_cid - 1].manual_dns == 1) {
                    snprintf(atCmdStr, sizeof(atCmdStr),
                        "+CGDCONT:%d,\"%s\",\"%s\",\"%s\",0,0,\"%s\",\"%s\"\r",
                        tmp_cid, ip, net, pdp_info[tmp_cid - 1].ipladdr,
                        pdp_info[tmp_cid - 1].userdns1addr,
                        pdp_info[tmp_cid - 1].userdns2addr);
                } else {
                    snprintf(atCmdStr, sizeof(atCmdStr),
                        "+CGDCONT:%d,\"%s\",\"%s\",\"%s\",0,0,\"%s\",\"%s\"\r",
                        tmp_cid, ip, net, pdp_info[tmp_cid - 1].ipladdr,
                        pdp_info[tmp_cid - 1].dns1addr,
                        pdp_info[tmp_cid - 1].dns2addr);
                }
            } else if (tmp_cid <= MAX_PDP_NUM) {
                snprintf(atCmdStr, sizeof(atCmdStr),
                        "+CGDCONT:%d,\"%s\",\"%s\",\"%s\",0,0,\"%s\",\"%s\"\r",
                        tmp_cid, ip, net, "0.0.0.0", "0.0.0.0", "0.0.0.0");
            } else {
                return AT_RESULT_OK;
            }
            atCmdStr[MAX_AT_CMD_LEN - 1] = '\0';
            PHS_LOGD("cvt_cgdcont_read_rsp pty_write cid =%d resp:%s",
                     tmp_cid, atCmdStr);
            adapter_pty_write((pty_t *)user_data, atCmdStr, strlen(atCmdStr));
        } while (0);
        return AT_RESULT_OK;
    }

    if (adapter_cmd_is_end(rsp->rsp_str, rsp->len) == TRUE) {
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_pty_write((pty_t *)user_data, rsp->rsp_str, rsp->len);
        adapter_pty_end_cmd((pty_t *)user_data);
        adapter_free_cmux(rsp->recv_cmux);
        return AT_RESULT_OK;
    }
    return AT_RESULT_NG;
}

int cvt_cgdcont_set_req(AT_CMD_REQ_T * req) {
    cmux_t *mux;
    int len, tmp_cid = 0;
    char *input;
    char atCmdStr[MAX_AT_CMD_LEN], ip[IP_ADDR_MAX], net[IP_ADDR_MAX],
            ipladdr[IP_ADDR_MAX], hcomp[IP_ADDR_MAX], dcomp[IP_ADDR_MAX];
    char *out;
    int err = 0, ret = 0;
    int maxPDPNum = MAX_PDP_NUM;

    if (req == NULL) {
        return AT_RESULT_NG;
    }
    input = req->cmd_str;

    memset(atCmdStr, 0, MAX_AT_CMD_LEN);
    memset(ip, 0, IP_ADDR_MAX);
    memset(net, 0, IP_ADDR_MAX);
    memset(ipladdr, 0, IP_ADDR_MAX);
    memset(hcomp, 0, IP_ADDR_MAX);
    memset(dcomp, 0, IP_ADDR_MAX);

    err = at_tok_start(&input, '=');
    if (err < 0) {
        return AT_RESULT_NG;
    }

    err = at_tok_nextint(&input, &tmp_cid);
    if (err < 0) {
        return AT_RESULT_NG;
    }
    err = at_tok_nextstr(&input, &out);  // ip
    if (err < 0)
        goto end_req;
    strncpy(ip, out, sizeof(ip));
    ip[sizeof(ip) - 1] = '\0';
    err = at_tok_nextstr(&input, &out);  // cmnet
    if (err < 0)
        goto end_req;
    strncpy(net, out, sizeof(net));
    net[sizeof(net) - 1] = '\0';
    err = at_tok_nextstr(&input, &out);  // ipladdr
    if (err < 0)
        goto end_req;
    strncpy(ipladdr, out, sizeof(ipladdr));
    ipladdr[sizeof(ipladdr) - 1] = '\0';
    err = at_tok_nextstr(&input, &out);  // dcomp
    if (err < 0)
        goto end_req;
    strncpy(dcomp, out, sizeof(dcomp));
    dcomp[sizeof(dcomp) - 1] = '\0';
    err = at_tok_nextstr(&input, &out);  // hcomp
    if (err < 0) {
        goto end_req;
    }
    strncpy(hcomp, out, sizeof(hcomp));
    hcomp[sizeof(hcomp) - 1] = '\0';
    // cp dns to pdp_info ?

    if (tmp_cid <= maxPDPNum) {
        strncpy(pdp_info[tmp_cid - 1].userdns1addr, "0.0.0.0",
                 sizeof("0.0.0.0"));
        strncpy(pdp_info[tmp_cid - 1].userdns2addr, "0.0.0.0",
                 sizeof("0.0.0.0"));
        pdp_info[tmp_cid - 1].manual_dns = 0;
    }

    // dns1 , info used with cgdata
    err = at_tok_nextstr(&input, &out);  // dns1
    if (err < 0)
        goto end_req;
    if (tmp_cid <= maxPDPNum && *out != 0) {
        strncpy(pdp_info[tmp_cid - 1].userdns1addr, out,
                sizeof(pdp_info[tmp_cid - 1].userdns1addr));
        pdp_info[tmp_cid - 1].userdns1addr[
                sizeof(pdp_info[tmp_cid - 1].userdns1addr) - 1] = '\0';
    }

    // dns2  , info used with cgdata
    err = at_tok_nextstr(&input, &out);  // dns2
    if (err < 0) {
        goto end_req;
    }
    if (tmp_cid <= maxPDPNum && *out != 0) {
        strncpy(pdp_info[tmp_cid - 1].userdns2addr, out,
                sizeof(pdp_info[tmp_cid - 1].userdns2addr));
        pdp_info[tmp_cid - 1].userdns2addr[
                sizeof(pdp_info[tmp_cid - 1].userdns2addr)- 1] = '\0';
    }

    // cp dns to pdp_info ?
    end_req:

    if (tmp_cid <= maxPDPNum) {
        if (strncasecmp(pdp_info[tmp_cid - 1].userdns1addr, "0.0.0.0",
                strlen("0.0.0.0"))) {
            pdp_info[tmp_cid - 1].manual_dns = 1;
        }
    }

    // make sure ppp in idle
    mutex_lock(&ps_service_mutex);
    mux = adapter_get_cmux(req->cmd_type, TRUE);
    mutex_unlock(&ps_service_mutex);

    snprintf(atCmdStr, sizeof(atCmdStr),
            "AT+CGDCONT=%d,\"%s\",\"%s\",\"%s\",%s,%s\r", tmp_cid, ip, net,
            ipladdr, dcomp, hcomp);
    len = strlen(atCmdStr);
    PHS_LOGD("PS:%s", atCmdStr);

    req->cmd_str = atCmdStr;
    req->len = len;

    adapter_cmux_register_callback(mux, cvt_cgdcont_set_rsp,
            (uintptr_t)req->recv_pty);
    adapter_cmux_write(mux, req->cmd_str, req->len, req->timeout);
    return AT_RESULT_PROGRESS;
}

int cvt_cgdcont_set_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data) {
    if (rsp == NULL) {
        return AT_RESULT_NG;
    }
    if (adapter_cmd_is_end(rsp->rsp_str, rsp->len) == TRUE) {
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_pty_write((pty_t *)user_data, rsp->rsp_str, rsp->len);
        adapter_pty_end_cmd((pty_t *)user_data);
        adapter_free_cmux(rsp->recv_cmux);
        return AT_RESULT_OK;
    }
    return AT_RESULT_NG;
}

IP_TYPE read_ip_addr(char *raw, char *rsp) {
    int comma_count = 0;
    int num = 0, comma4_num = 0, comma16_num = 0;
    int space_num = 0;
    char *buf = raw;
    int len = 0;
    int ip_type = UNKNOWN;

    if (raw != NULL) {
        len = strlen(raw);
        for (num = 0; num < len; num++) {
            if (raw[num] == '.') {
                comma_count++;
            }

            if (raw[num] == ' ') {
                space_num = num;
                break;
            }

            if (comma_count == 4 && comma4_num == 0) {
                comma4_num = num;
            }

            if (comma_count > 7 && comma_count == 16) {
                comma16_num = num;
                break;
            }
        }

        if (space_num > 0) {
            buf[space_num] = '\0';
            ip_type = IPV6;
            memcpy(rsp, buf, strlen(buf) + 1);
        } else if (comma_count >= 7) {
            if (comma_count == 7) {  // ipv4
                buf[comma4_num] = '\0';
                ip_type = IPV4;
            } else {  // ipv6
                buf[comma16_num] = '\0';
                ip_type = IPV6;
            }
            memcpy(rsp, buf, strlen(buf) + 1);
        }
    }

    return ip_type;
}

int cvt_cgcontrdp_rsp(AT_CMD_RSP_T * rsp,
        uintptr_t __attribute__((unused)) user_data) {
    int ret;
    int err;
    char *input;
    int cid;
    char *local_addr_subnet_mask = NULL, *gw_addr = NULL;
    char *dns_prim_addr = NULL, *dns_sec_addr = NULL;
    char ip[IP_ADDR_SIZE * 4], dns1[IP_ADDR_SIZE * 4], dns2[IP_ADDR_SIZE * 4];
    char cmd[MAX_CMD];
    char prop[PROPERTY_VALUE_MAX];
    int count = 0;
    char *sskip;
    char *tmp;
    int skip;
    static int ip_type_num = 0;
    int ip_type;
    int maxPDPNum = MAX_PDP_NUM;
    char ETH_SP[PROPERTY_NAME_MAX];  // "ro.modem.*.eth"

    if (rsp == NULL) {
        PHS_LOGD("leave cvt_cgcontrdp_rsp:AT_RESULT_NG");
        return AT_RESULT_NG;
    }

    memset(ip, 0, sizeof(ip));
    memset(dns1, 0, sizeof(dns1));
    memset(dns2, 0, sizeof(dns2));
    input = rsp->rsp_str;
    input[rsp->len - 1] = '\0';

    snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.%s.eth", s_modem);
    property_get(ETH_SP, prop, "veth");
    PHS_LOGD("cvt_cgcontrdp_rsp: input = %s", input);
    if (findInBuf(input, rsp->len, "+CGCONTRDP")
            || findInBuf(input, rsp->len, "+SIPCONFIG")) {
        do {
            err = at_tok_start(&input, ':');
            if (err < 0) {
                goto error;
            }
            err = at_tok_nextint(&input, &cid);  // cid
            if (err < 0) {
                goto error;
            }
            err = at_tok_nextint(&input, &skip);  // bearer_id
            if (err < 0) {
                goto error;
            }
            err = at_tok_nextstr(&input, &sskip);  // apn
            if (err < 0) {
                goto error;
            }
            if (at_tok_hasmore(&input)) {
                // local_addr_and_subnet_mask
                err = at_tok_nextstr(&input, &local_addr_subnet_mask);
                if (err < 0) {
                    goto error;
                }
                PHS_LOGD("cvt_cgcontrdp_rsp: after fetch input = %s", input);
                if (at_tok_hasmore(&input)) {
                    err = at_tok_nextstr(&input, &sskip);  // gw_addr
                    if (err < 0) {
                        goto error;
                    }
                    if (at_tok_hasmore(&input)) {
                        // dns_prim_addr
                        err = at_tok_nextstr(&input, &dns_prim_addr);
                        if (err < 0) {
                            goto error;
                        }
                        snprintf(dns1, sizeof(dns1), "%s", dns_prim_addr);
                        if (at_tok_hasmore(&input)) {
                            // dns_sec_addr
                            err = at_tok_nextstr(&input, &dns_sec_addr);
                            if (err < 0) {
                                goto error;
                            }
                            snprintf(dns2, sizeof(dns2), "%s", dns_sec_addr);
                        }
                    }
                }
            }

            if ((cid < maxPDPNum) && (cid >= 1)) {
                ip_type = read_ip_addr(local_addr_subnet_mask, ip);
                PHS_LOGD("PS:cid = %d,ip_type = %d,ip = %s,dns1 = %s,dns2 = %s",
                         cid, ip_type, ip, dns1, dns2);

                if (ip_type == IPV6) {  // ipv6
                    PHS_LOGD("cvt_cgcontrdp_rsp: IPV6");
                    if (!strncasecmp(ip, "0000:0000:0000:0000",
                            strlen("0000:0000:0000:0000"))) {
                        // incomplete address
                        tmp = strchr(ip, ':');
                        if (tmp != NULL) {
                            snprintf(ip, sizeof(ip), "FE80%s", tmp);
                        }
                    }
                    memcpy(pdp_info[cid - 1].ipv6laddr, ip,
                            sizeof(pdp_info[cid - 1].ipv6laddr));
                    memcpy(pdp_info[cid - 1].ipv6dns1addr, dns1,
                            sizeof(pdp_info[cid - 1].ipv6dns1addr));

                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                              prop, cid - 1, IPV6);
                    system(cmd);
                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_ip %s",
                              prop, cid - 1, ip);
                    system(cmd);
                    snprintf(cmd, sizeof(cmd),
                              "setprop net.%s%d.ipv6_dns1 %s", prop, cid - 1,
                              dns1);
                    system(cmd);
                    if (strlen(dns2) != 0) {
                        if (!strcmp(dns1, dns2)) {
                            if (strlen(s_SavedDns_IPV6) > 0) {
                                PHS_LOGD("Use saved DNS2 instead.");
                                memcpy(dns2, s_SavedDns_IPV6,
                                        sizeof(s_SavedDns_IPV6));
                            } else {
                                PHS_LOGD("Use default DNS2 instead.");
                                snprintf(dns2, sizeof(dns2), "%s",
                                          DEFAULT_PUBLIC_DNS2_IPV6);
                            }
                        } else {
                            PHS_LOGD("Backup DNS2");
                            memset(s_SavedDns_IPV6, 0,
                                    sizeof(s_SavedDns_IPV6));
                            memcpy(s_SavedDns_IPV6, dns2, sizeof(dns2));
                        }
                    } else {
                        PHS_LOGD("DNS2 is empty!!");
                        memset(dns2, 0, IP_ADDR_SIZE * 4);
                        snprintf(dns2, sizeof(dns2), "%s",
                                  DEFAULT_PUBLIC_DNS2_IPV6);
                    }
                    memcpy(pdp_info[cid - 1].ipv6dns2addr, dns2,
                            sizeof(pdp_info[cid - 1].ipv6dns2addr));
                    snprintf(cmd, sizeof(cmd),
                              "setprop net.%s%d.ipv6_dns2 %s", prop, cid - 1,
                              dns2);
                    system(cmd);

                    pdp_info[cid - 1].ip_state = IPV6;
                    ip_type_num++;
                } else if (ip_type == IPV4) {  // ipv4
                    PHS_LOGD("cvt_cgcontrdp_rsp: IPV4");
                    memcpy(pdp_info[cid - 1].ipladdr, ip,
                            sizeof(pdp_info[cid - 1].ipladdr));
                    memcpy(pdp_info[cid - 1].dns1addr, dns1,
                            sizeof(pdp_info[cid - 1].dns1addr));

                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                              prop, cid - 1, IPV4);
                    system(cmd);
                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip %s", prop,
                              cid - 1, ip);
                    system(cmd);
                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns1 %s",
                              prop, cid - 1, dns1);
                    system(cmd);
                    if (strlen(dns2) != 0) {
                        if (!strcmp(dns1, dns2)) {
                            PHS_LOGD("Two DNS are the same, so need to reset"
                                     "dns2!!");
                            cvt_reset_dns2(dns2, sizeof(dns2));
                        } else {
                            PHS_LOGD("Backup DNS2");
                            memset(s_SavedDns, 0, sizeof(s_SavedDns));
                            memcpy(s_SavedDns, dns2, IP_ADDR_SIZE);
                        }
                    } else {
                        PHS_LOGD("DNS2 is empty!!");
                        memset(dns2, 0, IP_ADDR_SIZE);
                        cvt_reset_dns2(dns2, sizeof(dns2));
                    }
                    memcpy(pdp_info[cid - 1].dns2addr, dns2,
                            sizeof(pdp_info[cid - 1].dns2addr));
                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns2 %s",
                              prop, cid - 1, dns2);
                    system(cmd);

                    pdp_info[cid - 1].ip_state = IPV4;
                    ip_type_num++;
                } else {  // unknown
                    pdp_info[cid - 1].state = PDP_STATE_EST_UP_ERROR;
                    PHS_LOGD("PDP_STATE_EST_UP_ERROR: unknown ip type!");
                }

                if (ip_type_num > 1) {
                    PHS_LOGD("cvt_cgcontrdp_rsp: IPV4V6");
                    pdp_info[cid - 1].ip_state = IPV4V6;
                    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                             prop, cid - 1, IPV4V6);
                    system(cmd);
                }
                pdp_info[cid - 1].state = PDP_STATE_ACTIVE;
                PHS_LOGD("PDP_STATE_ACTIVE");
            }
        } while (0);
        return AT_RESULT_OK;
    }

    if (adapter_cmd_is_end(rsp->rsp_str, rsp->len) == TRUE) {
        PHS_LOGD("cvt_cgcontrdp_rsp adapter cmd is end");
        adapter_cmux_deregister_callback(rsp->recv_cmux);
        adapter_wakeup_cmux(rsp->recv_cmux);
        ip_type_num = 0;
        return AT_RESULT_OK;
    }

    PHS_LOGD("cvt_cgcontrdp_rsp: AT_RESULT_OK");
    return AT_RESULT_OK;

error:
    return AT_RESULT_NG;
}

bool isLTE(void) {
    if (!strcmp(s_modem, "l") || !strcmp(s_modem, "tl")
            || !strcmp(s_modem, "lf")) {
        return true;
    }
    return false;
}

int ifc_set_noarp(const char *ifname) {
    struct ifreq ifr;
    int fd, err;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);

    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        close(fd);
        return -1;
    }

    ifr.ifr_flags = ifr.ifr_flags | IFF_NOARP;
    err = ioctl(fd, SIOCSIFFLAGS, &ifr);
    close(fd);
    return err;
}

/*
 * return value: 1: success
 *               0: getIpv6 header 64bit failed
 */
static int upNetInterface(int cidIndex, IP_TYPE ipType) {
    char linker[MAX_CMD] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    char ETH_SP[PROPERTY_NAME_MAX] = {0};  // "ro.modem.*.eth"
    char gspsprop[PROPERTY_VALUE_MAX] = {0};
    char cmd[MAX_CMD];
    IP_TYPE actIPType = ipType;
    char ifName[MAX_CMD] = {0};
    int isAutoTest = 0;
    int err = -1;

    snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.%s.eth", s_modem);
    property_get(ETH_SP, prop, "veth");

    /* set net interface name */
    snprintf(ifName, sizeof(ifName), "%s%d", prop, cidIndex);
    property_set(SYS_NET_ADDR, ifName);
    PHS_LOGD("Net interface addr linker = %s", ifName);

    property_get(GSPS_ETH_UP_PROP, gspsprop, "0");
    PHS_LOGD("GSPS up prop = %s", gspsprop);
    isAutoTest = atoi(gspsprop);

    if (ipType != IPV4) {
        actIPType = IPV6;
    }
    do {
        if (actIPType == IPV6) {
            property_set(SYS_IPV6_LINKLOCAL, pdp_info[cidIndex].ipv6laddr);
        }
        snprintf(cmd, sizeof(cmd), "<preifup>%s;%s;%d", ifName,
                  (actIPType == IPV4)? "IPV4" : ((actIPType == IPV6)?
                  "IPV6" : "IPV4V6"), isAutoTest);
        sendCmdToExtData(cmd);

        snprintf(linker, sizeof(linker), "%s%d", prop, cidIndex);
        PHS_LOGD("set IP linker = %s", linker);

        /* config ip addr */
        if (actIPType != IPV4) {
            property_set(SYS_NET_ACTIVATING_TYPE, "IPV6");
            err = ifc_add_address(linker, pdp_info[cidIndex].ipv6laddr, 64);
        } else {
            property_set(SYS_NET_ACTIVATING_TYPE, "IPV4");
            err = ifc_add_address(linker, pdp_info[cidIndex].ipladdr, 32);
        }
        if (err != 0) {
            PHS_LOGE("ifc_add_address %s fail: %s\n", linker, strerror(errno));
        }

        if (ifc_set_noarp(linker)) {
            PHS_LOGE("ifc_set_noarp %s fail: %s\n", linker, strerror(errno));
        }

        /* up the net interface */
        if (ifc_enable(linker)) {
            PHS_LOGE("ifc_enable %s fail: %s\n", linker, strerror(errno));
        }

        snprintf(cmd, sizeof(cmd), "<ifup>%s;%s;%d", ifName,
                  (actIPType == IPV4)? "IPV4" : ((actIPType == IPV6)?
                  "IPV6" : "IPV4V6"), isAutoTest);
        sendCmdToExtData(cmd);

        /* Get IPV6 Header 64bit */
        if (actIPType != IPV4) {
            if (!get_ipv6addr(prop, cidIndex)) {
                PHS_LOGD("get IPv6 address timeout, actIPType = %d", actIPType);
                if (ipType == IPV4V6) {
                    pdp_info[cidIndex].ip_state = IPV4;
                } else {
                    return 0;
                }
            }
        }

        /* if IPV4V6 actived, need set IPV4 again */
        if (ipType == IPV4V6 && actIPType != IPV4) {
            actIPType = IPV4;
        } else {
            break;
        }
    } while (ipType == IPV4V6);

    property_set(GSPS_ETH_UP_PROP, "0");

    return 1;
}
