/*
 * modem/error.h - Ofono errors
 *
 * Copyright (C) 2008-2010 Nokia Corporation
 *   @author Pekka Pessi <first.surname@nokia.com>
 *   @author Lassi Syrjala <first.surname@nokia.com>
 *
 * This work is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _MODEM_ERRORS_
#define _MODEM_ERRORS_

#include <glib-object.h>

G_BEGIN_DECLS

/* ---------------------------------------------------------------------- */
/* This is what oFono provides now */

/** The error domain for oFono */
#define MODEM_OFONO_ERRORS modem_ofono_errors_quark ()
GQuark modem_ofono_errors_quark (void);

/** The error type for oFono */
#define MODEM_TYPE_OFONO_ERROR modem_ofono_error_get_type ()
GType modem_ofono_error_get_type (void);

/** The error prefix for oFono */
#define MODEM_OFONO_ERROR_PREFIX "org.ofono.Error"

typedef enum _ModemoFonoError
{
  MODEM_OFONO_ERROR_FAILED = 0,
  MODEM_OFONO_ERROR_INVALID_ARGUMENTS,
  MODEM_OFONO_ERROR_INVALID_FORMAT,
  MODEM_OFONO_ERROR_NOT_IMPLEMENTED,
  MODEM_OFONO_ERROR_NOT_SUPPORTED,
  MODEM_OFONO_ERROR_IN_PROGRESS,
  MODEM_OFONO_ERROR_NOT_FOUND,
  MODEM_OFONO_ERROR_NOT_ACTIVE,
  MODEM_OFONO_ERROR_TIMED_OUT,
  MODEM_OFONO_ERROR_SIM_NOT_READY,
  MODEM_OFONO_ERROR_IN_USE,
  MODEM_OFONO_ERROR_NOT_ATTACHED,
  MODEM_OFONO_ERROR_ATTACH_IN_PROGRESS
} ModemoFonoError;

/* ---------------------------------------------------------------------- */
/* And this is what we'd like oFono to provide one day */

/** The error domain for the call service. */
#define MODEM_CALL_ERRORS modem_call_errors_quark ()
GQuark modem_call_errors_quark (void);

/** The error type for the call service. */
#define MODEM_TYPE_CALL_ERROR modem_call_error_get_type ()
GType modem_call_error_get_type (void);

#define MODEM_CALL_ERROR_PREFIX "org.ofono.Bogus.Call"

typedef enum _ModemCallError
{
  MODEM_CALL_ERROR_NO_ERROR = 0,

  MODEM_CALL_ERROR_NO_CALL,
  MODEM_CALL_ERROR_RELEASE_BY_USER,
  MODEM_CALL_ERROR_BUSY_USER_REQUEST,
  MODEM_CALL_ERROR_ERROR_REQUEST,
  MODEM_CALL_ERROR_CALL_ACTIVE,
  MODEM_CALL_ERROR_NO_CALL_ACTIVE,
  MODEM_CALL_ERROR_INVALID_CALL_MODE,
  MODEM_CALL_ERROR_TOO_LONG_ADDRESS,
  MODEM_CALL_ERROR_INVALID_ADDRESS,
  MODEM_CALL_ERROR_EMERGENCY,
  MODEM_CALL_ERROR_NO_SERVICE,
  MODEM_CALL_ERROR_NO_COVERAGE,
  MODEM_CALL_ERROR_CODE_REQUIRED,
  MODEM_CALL_ERROR_NOT_ALLOWED,
  MODEM_CALL_ERROR_DTMF_ERROR,
  MODEM_CALL_ERROR_CHANNEL_LOSS,
  MODEM_CALL_ERROR_FDN_NOT_OK,
  MODEM_CALL_ERROR_BLACKLIST_BLOCKED,
  MODEM_CALL_ERROR_BLACKLIST_DELAYED,
  MODEM_CALL_ERROR_EMERGENCY_FAILURE,
  MODEM_CALL_ERROR_NO_SIM,
  MODEM_CALL_ERROR_DTMF_SEND_ONGOING,
  MODEM_CALL_ERROR_CS_INACTIVE,
  MODEM_CALL_ERROR_NOT_READY,
  MODEM_CALL_ERROR_INCOMPATIBLE_DEST,

  MODEM_CALL_ERROR_GENERIC

} ModemCallError;

/* ---------------------------------------------------------------------- */

GQuark modem_call_net_errors_quark (void);
GType modem_call_net_error_get_type (void);

/** The error domain for errors from GSM network.
 *
 * See 3GPP 24.008 and Q.931 for detailed explanation.
 */
#define MODEM_CALL_NET_ERRORS modem_call_net_errors_quark ()

/** The error type for errors from GSM network */
#define MODEM_TYPE_CALL_NET_ERROR modem_call_net_error_get_type ()

#define MODEM_CALL_NET_ERROR_PREFIX "org.ofono.Bogus.Call.Network"

typedef enum _ModemNetError
{
  MODEM_CALL_NET_ERROR_UNASSIGNED_NUMBER        = 0x01, /* UnassignedNumber */
  MODEM_CALL_NET_ERROR_NO_ROUTE                 = 0x03, /* NoRouteToDestination */
  MODEM_CALL_NET_ERROR_CH_UNACCEPTABLE          = 0x06, /* ChannelUnacceptable */
  MODEM_CALL_NET_ERROR_OPER_BARRING             = 0x08, /* OperatorDeterminedBarring */
  MODEM_CALL_NET_ERROR_NORMAL                   = 0x10, /* NormalCallClearing */
  MODEM_CALL_NET_ERROR_USER_BUSY                = 0x11, /* UserBusy */
  MODEM_CALL_NET_ERROR_NO_USER_RESPONSE         = 0x12, /* NoUserResponse */
  MODEM_CALL_NET_ERROR_ALERT_NO_ANSWER          = 0x13, /* AlertNoAnswer */
  MODEM_CALL_NET_ERROR_CALL_REJECTED            = 0x15, /* CallRejected */
  MODEM_CALL_NET_ERROR_NUMBER_CHANGED           = 0x16, /* NumberChanged */
  MODEM_CALL_NET_ERROR_NON_SELECT_CLEAR         = 0x1A, /* NonSelectedClearing */
  MODEM_CALL_NET_ERROR_DEST_OUT_OF_ORDER        = 0x1B, /* DestinationOutOfOrder */
  MODEM_CALL_NET_ERROR_INVALID_NUMBER           = 0x1C, /* InvalidNumber */
  MODEM_CALL_NET_ERROR_FACILITY_REJECTED        = 0x1D, /* FacilityRejected */
  MODEM_CALL_NET_ERROR_RESP_TO_STATUS           = 0x1E, /* ResponseToStatus */
  MODEM_CALL_NET_ERROR_NORMAL_UNSPECIFIED       = 0x1F, /* UnspecifiedNormal */
  MODEM_CALL_NET_ERROR_NO_CHANNEL               = 0x22, /* NoChannelAvailable */
  MODEM_CALL_NET_ERROR_NETW_OUT_OF_ORDER        = 0x26, /* NetworkOutOfOrder */
  MODEM_CALL_NET_ERROR_TEMPORARY_FAILURE        = 0x29, /* TemporaryFailure */
  MODEM_CALL_NET_ERROR_CONGESTION               = 0x2A, /* Congestion */
  MODEM_CALL_NET_ERROR_ACCESS_INFO_DISC         = 0x2B, /* AccessInformationDiscarded */
  MODEM_CALL_NET_ERROR_CHANNEL_NA               = 0x2C, /* ChannelNotAvailable */
  MODEM_CALL_NET_ERROR_RESOURCES_NA             = 0x2F, /* ResourcesNotAvailable */
  MODEM_CALL_NET_ERROR_QOS_NA                   = 0x31, /* QoSNotAvailable */
  MODEM_CALL_NET_ERROR_FACILITY_UNSUBS          = 0x32, /* RequestedFacilityNotSubscribed */
  MODEM_CALL_NET_ERROR_COMING_BARRED_CUG        = 0x37, /* IncomingCallsBarredWithinCUG */
  MODEM_CALL_NET_ERROR_BC_UNAUTHORIZED          = 0x39, /* BearerCapabilityUnauthorized */
  MODEM_CALL_NET_ERROR_BC_NA                    = 0x3A, /* BearerCapabilityNotAvailable */
  MODEM_CALL_NET_ERROR_SERVICE_NA               = 0x3F, /* ServiceNotAvailable */
  MODEM_CALL_NET_ERROR_BEARER_NOT_IMPL          = 0x41, /* BearerNotImplemented */
  MODEM_CALL_NET_ERROR_ACM_MAX                  = 0x44, /* ACMMax */
  MODEM_CALL_NET_ERROR_FACILITY_NOT_IMPL        = 0x45, /* FacilityNotImplemented */
  MODEM_CALL_NET_ERROR_ONLY_RDI_BC              = 0x46, /* OnlyRestrictedDIBearerCapability */
  MODEM_CALL_NET_ERROR_SERVICE_NOT_IMPL         = 0x4F, /* ServiceNotImplemented */
  MODEM_CALL_NET_ERROR_INVALID_TI               = 0x51, /* InvalidTransactionIdentifier */
  MODEM_CALL_NET_ERROR_NOT_IN_CUG               = 0x57, /* NotInCUG */
  MODEM_CALL_NET_ERROR_INCOMPATIBLE_DEST        = 0x58, /* IncompatibleDestination */
  MODEM_CALL_NET_ERROR_INV_TRANS_NET_SEL        = 0x5B, /* InvalidTransitNetSelected */
  MODEM_CALL_NET_ERROR_SEMANTICAL_ERR           = 0x5F, /* SemanticalError */
  MODEM_CALL_NET_ERROR_INVALID_MANDATORY        = 0x60, /* InvalidMandatoryInformation */
  MODEM_CALL_NET_ERROR_MSG_TYPE_INEXIST         = 0x61, /* MessageTypeNonExistent */
  MODEM_CALL_NET_ERROR_MSG_TYPE_INCOMPAT        = 0x62, /* MessageTypeIncompatible */
  MODEM_CALL_NET_ERROR_IE_NON_EXISTENT          = 0x63, /* InformationElementNonExistent */
  MODEM_CALL_NET_ERROR_COND_IE_ERROR            = 0x64, /* ConditionalInformationElementError */
  MODEM_CALL_NET_ERROR_MSG_INCOMPATIBLE         = 0x65, /* IncompatibleMessage */
  MODEM_CALL_NET_ERROR_TIMER_EXPIRY             = 0x66, /* TimerExpiry */
  MODEM_CALL_NET_ERROR_PROTOCOL_ERROR           = 0x6F, /* ProtocolError */
  MODEM_CALL_NET_ERROR_INTERWORKING             = 0x7F, /* Generic */
  MODEM_CALL_NET_ERROR_GENERIC = MODEM_CALL_NET_ERROR_INTERWORKING
} ModemNetError;

/* ---------------------------------------------------------------------- */

/** The error domain for the GSM cause. */
#define MODEM_SMS_NET_ERRORS modem_sms_net_errors_quark ()

GQuark modem_sms_net_errors_quark (void);

/** The error type for the SMS GSM cause. */
#define MODEM_TYPE_SMS_NET_ERROR modem_sms_net_error_get_type ()

GType modem_sms_net_error_get_type (void);

#define MODEM_SMS_NET_ERROR_PREFIX "org.ofono.Bogus.SMS.Network"

enum
{
  /* Generic ISDN errors */
  MODEM_SMS_NET_ERROR_SUCCESS                              = 0x00,
  MODEM_SMS_NET_ERROR_UNASSIGNED_NUMBER                    = 0x01,
  MODEM_SMS_NET_ERROR_OPER_DETERMINED_BARR                 = 0x08,
  MODEM_SMS_NET_ERROR_CALL_BARRED                          = 0x0A,
  MODEM_SMS_NET_ERROR_RESERVED                             = 0x0B,
  MODEM_SMS_NET_ERROR_MSG_TRANSFER_REJ                     = 0x15,
  MODEM_SMS_NET_ERROR_MEMORY_CAPACITY_EXC                  = 0x16,
  MODEM_SMS_NET_ERROR_DEST_OUT_OF_ORDER                    = 0x1B,
  MODEM_SMS_NET_ERROR_UNDEFINED_SUBSCRIBER                 = 0x1C,
  MODEM_SMS_NET_ERROR_FACILITY_REJECTED                    = 0x1D,
  MODEM_SMS_NET_ERROR_UNKNOWN_SUBSCRIBER                   = 0x1E,
  MODEM_SMS_NET_ERROR_NETWORK_OUT_OF_ORDER                 = 0x26,
  MODEM_SMS_NET_ERROR_TEMPORARY_FAILURE                    = 0x29,
  MODEM_SMS_NET_ERROR_CONGESTION                           = 0x2A,
  MODEM_SMS_NET_ERROR_RESOURCE_UNAVAILABLE                 = 0x2F,
  MODEM_SMS_NET_ERROR_REQ_FACILITY_NOT_SUB                 = 0x32,
  MODEM_SMS_NET_ERROR_REQ_FACILITY_NOT_IMP                 = 0x45,
  MODEM_SMS_NET_ERROR_INVALID_REFERENCE                    = 0x51,
  MODEM_SMS_NET_ERROR_INVALID_MSG                          = 0x5F,
  MODEM_SMS_NET_ERROR_INVALID_MAND_IE                      = 0x60,
  MODEM_SMS_NET_ERROR_INVALID_MSG_TYPE                     = 0x61,
  MODEM_SMS_NET_ERROR_INCOMPATIBLE_MSG_TYPE                = 0x62,
  MODEM_SMS_NET_ERROR_INVALID_IE_TYPE                      = 0x63,
  MODEM_SMS_NET_ERROR_PROTOCOL_ERROR                       = 0x6F,
  MODEM_SMS_NET_ERROR_INTERWORKING                         = 0x7F,

  /* GSM-specifc errors (see 24.008 annexes G, H) */
  MODEM_SMS_NET_ERROR_LOW_LAYER_NO_CAUSE                   = 0x80,
  MODEM_SMS_NET_ERROR_IMSI_UNKNOWN_HLR                     = 0x82,
  MODEM_SMS_NET_ERROR_ILLEGAL_MS                           = 0x83,
  MODEM_SMS_NET_ERROR_IMSI_UNKNOWN_VLR                     = 0x84,
  MODEM_SMS_NET_ERROR_IMEI_NOT_ACCEPTED                    = 0x85,
  MODEM_SMS_NET_ERROR_ILLEGAL_ME                           = 0x86,
  MODEM_SMS_NET_ERROR_PLMN_NOT_ALLOWED                     = 0x8B,
  MODEM_SMS_NET_ERROR_LA_NOT_ALLOWED                       = 0x8C,
  MODEM_SMS_NET_ERROR_ROAM_NOT_ALLOWED_LA                  = 0x8D,
  MODEM_SMS_NET_ERROR_NO_SUITABLE_CELLS_LA                 = 0x8F,
  MODEM_SMS_NET_ERROR_NETWORK_FAILURE                      = 0x91,
  MODEM_SMS_NET_ERROR_MAC_FAILURE                          = 0x94,
  MODEM_SMS_NET_ERROR_SYNC_FAILURE                         = 0x95,
  MODEM_SMS_NET_ERROR_LOW_LAYER_CONGESTION                 = 0x96,
  MODEM_SMS_NET_ERROR_AUTH_UNACCEPTABLE                    = 0x97,
  MODEM_SMS_NET_ERROR_SERV_OPT_NOT_SUPPORTED               = 0xA0,
  MODEM_SMS_NET_ERROR_SERV_OPT_NOT_SUBSCRIBED              = 0xA1,
  MODEM_SMS_NET_ERROR_SERV_OPT_TEMP_OUT_OF_ORDER           = 0xA2,
  MODEM_SMS_NET_ERROR_CALL_CANNOT_BE_IDENTIFIED            = 0xA6,
  MODEM_SMS_NET_ERROR_LOW_LAYER_INVALID_MSG                = 0xDF,
  MODEM_SMS_NET_ERROR_LOW_LAYER_INVALID_MAND_IE            = 0xE0,
  MODEM_SMS_NET_ERROR_LOW_LAYER_INVALID_MSG_TYPE           = 0xE1,
  MODEM_SMS_NET_ERROR_LOW_LAYER_INCOMPATIBLE_MSG_TYPE      = 0xE2,
  MODEM_SMS_NET_ERROR_LOW_LAYER_INVALID_IE_TYPE            = 0xE3,
  MODEM_SMS_NET_ERROR_LOW_LAYER_INVALID_IE                 = 0xE4,
  MODEM_SMS_NET_ERROR_LOW_LAYER_INCOMPATIBLE_MSG           = 0xE5,
  MODEM_SMS_NET_ERROR_CS_BARRED                            = 0xE8,
  MODEM_SMS_NET_ERROR_LOW_LAYER_PROTOCOL_ERROR             = 0xEF,
};

/* ---------------------------------------------------------------------- */

/** The error domain for the SMS errors. */
#define MODEM_SMS_ERRORS modem_sms_errors_quark ()

GQuark modem_sms_errors_quark (void);

/** The error type for the SMS errors. */
#define MODEM_TYPE_SMS_ERROR modem_sms_error_get_type ()

GType modem_sms_error_get_type (void);

#define MODEM_SMS_ERROR_PREFIX "org.ofono.Bogus.SMS"

enum
{
  MODEM_SMS_ERROR_ROUTING_RELEASED         = 0x01,
  MODEM_SMS_ERROR_INVALID_PARAMETER        = 0x02,
  MODEM_SMS_ERROR_DEVICE_FAILURE           = 0x03,
  MODEM_SMS_ERROR_PP_RESERVED              = 0x04,
  MODEM_SMS_ERROR_ROUTE_NOT_AVAILABLE      = 0x05,
  MODEM_SMS_ERROR_ROUTE_NOT_ALLOWED        = 0x06,
  MODEM_SMS_ERROR_SERVICE_RESERVED         = 0x07,
  MODEM_SMS_ERROR_INVALID_LOCATION         = 0x08,
  MODEM_SMS_ERROR_NO_NETW_RESPONSE         = 0x0B,
  MODEM_SMS_ERROR_DEST_ADDR_FDN_RESTRICTED = 0x0C,
  MODEM_SMS_ERROR_SMSC_ADDR_FDN_RESTRICTED = 0x0D,
  MODEM_SMS_ERROR_RESEND_ALREADY_DONE      = 0x0E,
  MODEM_SMS_ERROR_SMSC_ADDR_NOT_AVAILABLE  = 0x0F,
  MODEM_SMS_ERROR_ROUTING_FAILED           = 0x10,
  MODEM_SMS_ERROR_CS_INACTIVE              = 0x11,
  MODEM_SMS_ERROR_SENDING_ONGOING          = 0x15,
  MODEM_SMS_ERROR_SERVER_NOT_READY         = 0x16,
  MODEM_SMS_ERROR_NO_TRANSACTION           = 0x17,
  MODEM_SMS_ERROR_INVALID_SUBSCRIPTION_NR  = 0x19,
  MODEM_SMS_ERROR_RECEPTION_FAILED         = 0x1A,
  MODEM_SMS_ERROR_RC_REJECTED              = 0x1B,
  MODEM_SMS_ERROR_ALL_SUBSCRIPTIONS_ALLOCATED = 0x1C,
  MODEM_SMS_ERROR_SUBJECT_COUNT_OVERFLOW   = 0x1D,
  MODEM_SMS_ERROR_DCS_COUNT_OVERFLOW       = 0x1E,
};

/* ---------------------------------------------------------------------- */

void modem_error_register_mapping (GQuark domain, char const *prefix, GType);
char const *modem_error_domain_prefix (GQuark);
char const *modem_error_name (GError const *error, void *buffer, guint len);
void modem_error_fix (GError **error);

char *modem_error_fqn (GError const *error);

G_END_DECLS

#endif
