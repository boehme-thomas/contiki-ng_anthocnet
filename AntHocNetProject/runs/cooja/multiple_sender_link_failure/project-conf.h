/**
* Project configuration file for AntHocNet Algo implementation.
 */

#ifndef IEEE_802_15_4_ANTNET_PROJECT_CONF_H
#define IEEE_802_15_4_ANTNET_PROJECT_CONF_H

/*---Netstack---*/
#define NETSTACK_CONF_ROUTING anthocnet_driver
#define NETSTACK_CONF_WITH_IPV6 1


/*---UIP---*/
// to disable uip-ds6-routes
#define UIP_CONF_MAX_ROUTES 0
// to disable uip-ds6 default routes
#define UIP_CONF_DS6_DEFRT_NBU 0

#define UIP_CONF_ICMP6 1
#define UIP_CONF_ROUTER 1

// to disable the neighbour routes and neighbour solicitation / advertisement
#define UIP_CONF_ND6_SEND_NA 0
#define UIP_CONF_ND6_SEND_NS 0
#define UIP_CONF_ND6_SEND_RA 0

#if MAC_CONF_WITH_TSCH
/*---TSCH---*/
#define TSCH_CONF_CCA_ENABLED 1
#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL 1

#endif /*MAKE_MAC == MAKE_MAC_TSCH*/

//---Log---*/
#define LOG_ALL 0

// if all dbg messages are logged, some error occurs
#ifdef LOG_ALL
#if LOG_ALL
#define LOG_CONF_LEVEL_6LOWPAN LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_6TOP LOG_LEVEL_DBG
//#define LOG_CONF_LEVEL_SYS LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_MAIN LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_DBG
#endif
#endif

/*---Project-log---*/
#define LOG_LEVEL_ANTHOCNET LOG_LEVEL_INFO

#define LOG_CONF_LEVEL_ANTHOCNET_ICMPV6 LOG_LEVEL_ANTHOCNET
#define LOG_CONF_LEVEL_ANTHOCNET_PHEROMONE LOG_LEVEL_ANTHOCNET
#define LOG_CONF_LEVEL_ANTHOCNET_MAIN LOG_LEVEL_ANTHOCNET

/*---Energest---*/
#define ENERGEST_CONF_ON 1

/*---AntHocNet---*/
#define ANT_HOC_NET_CONF_T_HELLO_SEC 3
#define ANT_HOC_NET_CONF_ALLOWED_HELLO_LOSS 4
#define ANT_HOC_NET_CONF_ACC_FACTOR_A2 2
#define ANT_HOC_NET_CONF_MAX_HOPS 200
#define ANT_HOC_NET_CONF_RESTART_PATH_SETUP_SECS 2
#define ANT_HOC_NET_CONF_BETA_FORWARD 1
#endif //IEEE_802_15_4_ANTNET_PROJECT_CONF_H

