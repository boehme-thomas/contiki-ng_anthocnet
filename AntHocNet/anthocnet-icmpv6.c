//
// Created by thomas on 5/26/25.
//
/**
 * \file
 *      Implementations of handler functions for the use of ICMPv6 messages in AntHocNet.
 */

#include "anthocnet-icmpv6.h"
#include "uip-icmp6.h"
#include "anthocnet.h"
#include "sys/log.h"
#include <stdlib.h>

#define LOG_MODULE "AntHocNet - ICMPv6"

#ifdef LOG_CONF_LEVEL_ANTHOCNET_ICMPV6
#define LOG_LEVEL LOG_CONF_LEVEL_ANTHOCNET_ICMPV6
#else
#define LOG_LEVEL LOG_LEVEL_NONE
#endif

/** Handles reception of ICMP6_REACTIVE_FORWARD_ANT package. */
static void rfa_input(void);
/** Handles reception of ICMP6_REACTIVE_BACKWARD_ANT package. */
static void rba_input(void);
/** Handles reception of ICMP6_PROACTIVE_FORWARD_ANT package. */
static void pfa_input(void);
/** Handles reception of ICMP6_HELLO_MESSAGE package. */
static void hm_input(void);
/** Handles reception of ICMP6_WARNING_MESSAGE package. */
static void wm_input(void);
/** Handles reception of ICMP6_LINK_FAILURE_NOTIFICATION package. */
static void lfn_input(void);

UIP_ICMP6_HANDLER(reactive_forward_or_path_repair_ant_handler, ICMP6_REACTIVE_FORWARD_ANT, UIP_ICMP6_HANDLER_CODE_ANY, rfa_input);
UIP_ICMP6_HANDLER(reactive_backward_ant_handler, ICMP6_REACTIVE_BACKWARD_ANT, UIP_ICMP6_HANDLER_CODE_ANY, rba_input);
UIP_ICMP6_HANDLER(proactive_forward_ant_handler, ICMP6_PROACTIVE_FORWARD_ANT, UIP_ICMP6_HANDLER_CODE_ANY, pfa_input);
UIP_ICMP6_HANDLER(hello_message_handler, ICMP6_HELLO_MESSAGE, UIP_ICMP6_HANDLER_CODE_ANY, hm_input);
UIP_ICMP6_HANDLER(warning_message_handler, ICMP6_WARNING_MESSAGE, UIP_ICMP6_HANDLER_CODE_ANY, wm_input);
UIP_ICMP6_HANDLER(link_failure_notification_handler, ICMP6_LINK_FAILURE_NOTIFICATION, UIP_ICMP6_HANDLER_CODE_ANY, lfn_input);

void anthocnet_icmpv6_register_input_handlers() {
    uip_icmp6_register_input_handler(&reactive_forward_or_path_repair_ant_handler);
    uip_icmp6_register_input_handler(&reactive_backward_ant_handler);
    uip_icmp6_register_input_handler(&proactive_forward_ant_handler);
    uip_icmp6_register_input_handler(&hello_message_handler);
    uip_icmp6_register_input_handler(&warning_message_handler);
    uip_icmp6_register_input_handler(&link_failure_notification_handler);
}

static void rfa_input(void) {
    struct reactive_forward_or_path_repair_ant ant;
    //uint16_t buff_len = uip_len - uip_l3_icmp_hdr_len;
    uint16_t size = sizeof(struct reactive_forward_or_path_repair_ant) - sizeof(uip_ipaddr_t *);

    memcpy(&ant, UIP_ICMP_PAYLOAD, size);
    if (ant.hops == 0) {
        ant.path = NULL;
    } else {
        ant.path = (uip_ipaddr_t *) malloc(ant.hops * sizeof(uip_ipaddr_t));
        memcpy(ant.path, UIP_ICMP_PAYLOAD + size, ant.hops * sizeof(uip_ipaddr_t));
    }

    uipbuf_clear();

    reception_reactive_forward_or_path_repair_ant(ant);
}

static void rba_input(void) {
    LOG_DBG("rba_input\n");

    struct reactive_backward_ant ant;

    uint16_t size = sizeof(struct reactive_backward_ant) - sizeof(uip_ipaddr_t *);

    /*
    if (UIP_ICMP_PAYLOAD == NULL) {
        LOG_ERR("UIP_ICMP_PAYLOAD is NULL\n");
        return;
    }*/

    memcpy(&ant, UIP_ICMP_PAYLOAD, size);
    LOG_DBG("Ant:\n");
    LOG_DBG("\ttype: %d\n", ant.ant_type);
    LOG_DBG("\tant_generation: %d\n", ant.ant_generation);
    LOG_DBG("\tdestination: ");
    LOG_DBG_6ADDR(&ant.destination);
    LOG_DBG_("\n");
    LOG_DBG("\tcurrenthop: %d\n", ant.current_hop);
    LOG_DBG("\ttime_estimate_T_P: %f\n", ant.time_estimate_T_P);
    LOG_DBG("\tlength: %d\n", ant.length);

    if (ant.length == 0) {
        ant.path = NULL;
        // if a backward ant is received that has no path, the ant can be killed, since that cant happen
        // if the path is 0 the destination is a neighbour and thus no reactive forward ant shouldve been sent
        LOG_INFO("RBA path length is 0");
        return;
    } else {
        ant.path = (uip_ipaddr_t *) malloc(ant.length * sizeof(uip_ipaddr_t));
        if (ant.path == NULL) {
            LOG_ERR("Memory allocation for path failed!\n");
            return;
        }
        memcpy(ant.path, UIP_ICMP_PAYLOAD + size, ant.length * sizeof(uip_ipaddr_t));
    }
    LOG_DBG("Path: \n");
    for (int i = 0; i < ant.length; i++) {
        LOG_DBG("[%d]: ", i);
        LOG_DBG_6ADDR(&ant.path[i]);
        LOG_DBG_("\n");
    }

    uip_ipaddr_t host_address = get_host_address();
    // check if processes are running + if host is destination + if ant generation is current generation
    // if so stop the reactive path setup,
    // clear the buffer and call the reception function,
    // in addition, send the buffered messages
    if (processes_running() && get_current_ant_generation() == ant.ant_generation && uip_ipaddr_cmp(&ant.destination, &host_address)) {
        LOG_DBG("Backward ant received!\n");
        LOG_DBG("Backward ant of generation %d received. Host is destination! Stop rps or dtf processes!\n", ant.ant_generation);
        stop_reactive_path_setup_and_data_transmission_failed_process();
        uipbuf_clear();
        reception_reactive_backward_ant(ant);
        send_buffered_data_packages();
    } else {
        LOG_DBG("Backward ant received!\n");
        LOG_DBG("Backward ant of other generation %d than current generation %d received or processes not running (%d) or not destination address (%d)\n", ant.ant_generation, get_current_ant_generation(), processes_running(), uip_ipaddr_cmp(&ant.destination, &host_address));
        uipbuf_clear();
        reception_reactive_backward_ant(ant);
    }
}

static void pfa_input(void) {
    struct proactive_forward_ant ant;
    uint16_t size = sizeof(struct proactive_forward_ant) - sizeof(uip_ipaddr_t *);

    memcpy(&ant, UIP_ICMP_PAYLOAD, size);
    if (ant.hops == 0) {
        ant.path = NULL;
    } else {
        ant.path = (uip_ipaddr_t *) malloc(ant.hops * sizeof(uip_ipaddr_t));
        memcpy(ant.path, UIP_ICMP_PAYLOAD + size, ant.hops * sizeof(uip_ipaddr_t));
    }
    uipbuf_clear();

    reception_proactive_forward_ant(ant);
}

static void hm_input(void) {
    struct hello_message msg;
    memcpy(&msg, UIP_ICMP_PAYLOAD, sizeof(struct hello_message));

    uipbuf_clear();

    reception_hello_message(msg);
}

static void wm_input(void) {
    struct warning_message msg;
    memcpy(&msg, UIP_ICMP_PAYLOAD, sizeof(struct warning_message));

    uipbuf_clear();

    reception_warning(msg);
}

static void lfn_input(void) {
    struct link_failure_notification lfn;
    uint16_t size = sizeof(struct link_failure_notification) - sizeof(link_failure_notification_entry_t *);

    memcpy(&lfn, UIP_ICMP_PAYLOAD, size);

    if (lfn.size_of_list_of_destinations == 0) {
        lfn.entries = NULL;
    } else {
        lfn.entries = (link_failure_notification_entry_t *) malloc(lfn.size_of_list_of_destinations * sizeof(link_failure_notification_entry_t));
        memcpy(lfn.entries, UIP_ICMP_PAYLOAD + size, lfn.size_of_list_of_destinations * sizeof(link_failure_notification_entry_t));
    }

    uipbuf_clear();

    reception_link_failure_notification(lfn);
}