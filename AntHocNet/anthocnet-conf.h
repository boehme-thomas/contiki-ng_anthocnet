/**
 * \file
 *      Definitions of different parameters
 */
#ifndef IEEE_802_15_4_ANTNET_ANTHOCNET_CONF_H
#define IEEE_802_15_4_ANTNET_ANTHOCNET_CONF_H

#include <math.h>

#ifdef ANT_HOC_NET_CONF_BETA_FORWARD
#define ANT_HOC_NET_BETA_FORWARD    ANT_HOC_NET_CONF_BETA_FORWARD
#else
/* defines Beta for P_nd for forward ant; has to be >= 1; needed in equation (1) of AntNetHoc Paper */
#define ANT_HOC_NET_BETA_FORWARD    1
#endif

#ifdef ANT_HOC_NET_CONF_BETA_STOCHASTIC
#define ANT_HOC_NET_BETA_STOCHASTIC    ANT_HOC_NET_CONF_BETA_STOCHASTIC
#else
/* defines Beta for P_nd for stochastic routing; has to be >= 1; needed in equation (1) of AntNetHoc Paper */
#define ANT_HOC_NET_BETA_STOCHASTIC    2
#endif

#ifdef ANT_HOC_NET_CONF_ALPHA
#define ANT_HOC_NET_ALPHA    ANT_HOC_NET_CONF_ALPHA
#else
/* defines Alpha; has to be in [0, 1]; needed in equation (4) of AntNetHoc Paper */
#define ANT_HOC_NET_ALPHA    0.7
#endif

#ifdef ANT_HOC_NET_CONF_GAMMA
#define ANT_HOC_NET_GAMMA    ANT_HOC_NET_CONF_GAMMA
#else
/* defines Gamma; has to be in [0, 1]; needed in equation (6) of AntNetHoc Paper */
#define ANT_HOC_NET_GAMMA    0.7
#endif

#ifdef ANT_HOC_NET_CONF_T_HOP
#define ANT_HOC_NET_T_HOP    ANT_HOC_NET_CONF_T_HOP
#else
/* defines time to take one hop in unloaded conditions; T_hop; needed in equation (5) of AntNetHoc Paper  */
#define ANT_HOC_NET_T_HOP    (3.0 * pow(10, -3))
#endif

#ifdef ANT_HOC_NET_CONF_RESTART_PATH_SETUP_SECS
#define ANT_HOC_NET_RESTART_PATH_SETUP_SECS    ANT_HOC_NET_CONF_RESTART_PATH_SETUP_SECS
#else
/* defines the amount of time after which the path setup process is restarted, in sec. */
#define ANT_HOC_NET_RESTART_PATH_SETUP_SECS    2
#endif

#ifdef ANT_HOC_NET_CONF_MAX_TRIES_PATH_SETUP
#define ANT_HOC_NET_MAX_TRIES_PATH_SETUP    ANT_HOC_NET_CONF_MAX_TRIES_PATH_SETUP
#else
/* defines maximum tries of path setups */
#define ANT_HOC_NET_MAX_TRIES_PATH_SETUP    3
#endif

#ifdef ANT_HOC_NET_CONF_ACC_FACTOR_A1
#define ANT_HOC_NET_ACC_FACTOR_A1    ANT_HOC_NET_CONF_ACC_FACTOR_A1
#else
/* defines acceptance factor a1 */
#define ANT_HOC_NET_ACC_FACTOR_A1    0.9
#endif

#ifdef ANT_HOC_NET_CONF_ACC_FACTOR_A2
#define ANT_HOC_NET_ACC_FACTOR_A2    ANT_HOC_NET_CONF_ACC_FACTOR_A2
#else
/* defines acceptance factor a2 */
#define ANT_HOC_NET_ACC_FACTOR_A2    2
#endif

#ifdef ANT_HOC_NET_CONF_PFA_SENDING_RATE_N
#define ANT_HOC_NET_PFA_SENDING_RATE_N    ANT_HOC_NET_CONF_PFA_SENDING_RATE_N
#else
/* define data sending rate n of proactive forward ant */
#define ANT_HOC_NET_PFA_SENDING_RATE_N    5
#endif

#ifdef ANT_HOC_NET_CONF_PFA_TIME_THRESHOLD
#define ANT_HOC_NET_PFA_TIME_THRESHOLD  ANT_HOC_NET_CONF_PFA_TIME_THRESHOLD
#else
/* define data sending rate m of proactive forward ant */
#define ANT_HOC_NET_PFA_TIME_THRESHOLD    0.5
#endif

#ifdef ANT_HOC_NET_CONF_PFA_BROADCAST_PROBABILITY
#define ANT_HOC_NET_PFA_BROADCAST_PROBABILITY    ANT_HOC_NET_CONF_PFA_BROADCAST_PROBABILITY
#else
/* defines broadcast probability of proactive forward ant */
#define ANT_HOC_NET_PFA_BROADCAST_PROBABILITY    0.1
#endif

#ifdef ANT_HOC_NET_CONF_MAX_NUMBER_BROADCASTS_PFA
#define ANT_HOC_NET_MAX_NUMBER_BROADCASTS_PFA   ANT_HOC_NET_CONF_MAX_NUMBER_BROADCASTS_PFA
#else
/* max number of broadcasts the proactive ant is allowed to do before getting killed */
#define ANT_HOC_NET_MAX_NUMBER_BROADCASTS_PFA    2
#endif

#ifdef ANT_HOC_NET_CONF_T_HELLO_SEC
#define ANT_HOC_NET_T_HELLO_SEC    ANT_HOC_NET_CONF_T_HELLO_SEC
#else
/* define in which interval the hello messages are broadcasted, in sec */
#define ANT_HOC_NET_T_HELLO_SEC    1
#endif

#ifdef ANT_HOC_NET_CONF_ALLOWED_HELLO_LOSS
#define ANT_HOC_NET_ALLOWED_HELLO_LOSS    ANT_HOC_NET_CONF_ALLOWED_HELLO_LOSS
#else
/* defines how often a hello message can be lost, before the neighbour is assumed to be lost */
#define ANT_HOC_NET_ALLOWED_HELLO_LOSS    2
#endif

#ifdef ANT_HOC_NET_CONF_MAX_NUMBER_BROADCASTS_PATH_REPAIR_A
#define ANT_HOC_NET_MAX_NUMBER_BROADCASTS_PATH_REPAIR_A    ANT_HOC_NET_CONF_MAX_NUMBER_BROADCASTS_PATH_REPAIR_A
#else
/* defines the allowed number of broadcast for path repair ant */
#define ANT_HOC_NET_MAX_NUMBER_BROADCASTS_PATH_REPAIR_A    2
#endif

#ifdef ANT_HOC_NET_CONF_FACTOR_OF_WAITING_TIME_BRA
#define ANT_HOC_NET_FACTOR_OF_WAITING_TIME_BRA ANT_HOC_NET_CONF_FACTOR_OF_WAITING_TIME_BRA
#else
/* the factor, how long it is waited for a backward repair ant to be received
 (is multiplied with the estimated time of that neighbour)
 */
#define ANT_HOC_NET_FACTOR_OF_WAITING_TIME_BRA 5
#endif

#ifdef ANT_HOC_NET_CONF_MAX_HOPS
#define ANT_HOC_NET_MAX_HOPS    ANT_HOC_NET_CONF_MAX_HOPS
#else
/* define max number of hops a reactive forward ant or path repair ant is allowed to take */
#define ANT_HOC_NET_MAX_HOPS    100
#endif

#endif //IEEE_802_15_4_ANTNET_ANTHOCNET_CONF_H