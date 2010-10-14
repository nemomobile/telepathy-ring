/*
 * modem/error.c - Ofono errors
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

#include "config.h"

#define MODEM_DEBUG_FLAG MODEM_SERVICE_DBUS
#include <modem/debug.h>

#include <modem/errors.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-protocol.h>
#include <stdio.h>
#include <string.h>

GQuark
modem_ofono_errors_quark (void)
{
  static gsize quark = 0;

  if (g_once_init_enter (&quark))
    {
      GQuark domain = g_quark_from_static_string (MODEM_OFONO_ERROR_PREFIX);
      g_once_init_leave (&quark, domain);
    }

  return (GQuark)quark;
}

/** Get GType for  errors */
GType
modem_ofono_error_get_type (void)
{
  static const GEnumValue values[] = {
#define _(x, t) { MODEM_OFONO_ERROR_ ## x, "MODEM_OFONO_ERROR_" #x, #t }
    _(FAILED, Failed),
    _(INVALID_ARGUMENTS, InvalidArguments),
    _(INVALID_FORMAT, InvalidFormat),
    _(NOT_IMPLEMENTED, NotImplemented),
    _(NOT_SUPPORTED, NotSupported),
    _(IN_PROGRESS, InProgress),
    _(NOT_FOUND, NotFound),
    _(NOT_ACTIVE, NotActive),
    _(TIMED_OUT, Timedout), /* XXX: TimedOut? */
    _(SIM_NOT_READY, SimNotReady),
    _(IN_USE, InUse),
    _(NOT_ATTACHED, NotAttached),
    _(ATTACH_IN_PROGRESS, AttachInProgress),
#undef _
    { 0, NULL, NULL },
  };

  static gsize etype = 0;

  if (g_once_init_enter (&etype))
    {
      GType type = g_enum_register_static ("ModemOfonoError", values);
      g_once_init_leave (&etype, type);
    }

  return (GType)etype;
}

GQuark
modem_call_errors_quark (void)
{
  static gsize quark = 0;

  if (g_once_init_enter (&quark))
    {
      GQuark domain = g_quark_from_static_string (MODEM_CALL_ERROR_PREFIX);
      g_once_init_leave (&quark, domain);
    }

  return (GQuark)quark;
}

/** Get GType for  errors */
GType
modem_call_error_get_type (void)
{
  static const GEnumValue values[] = {
#define _(x, t) { MODEM_CALL_ERROR_ ## x, "MODEM_CALL_ERROR_" #x, #t }
    _(GENERIC, Generic),

    _(NO_ERROR, None),
    _(NO_CALL, NoCall),
    _(RELEASE_BY_USER, ReleaseByUser),
    _(BUSY_USER_REQUEST, BusyUserRequest),
    _(ERROR_REQUEST, RequestError),
    _(CALL_ACTIVE, CallActive),
    _(NO_CALL_ACTIVE, NoCallActive),
    _(INVALID_CALL_MODE, InvalidCallMode),
    _(TOO_LONG_ADDRESS, TooLongAddress),
    _(INVALID_ADDRESS, InvalidAddress),
    _(EMERGENCY, Emergency),
    _(NO_SERVICE, NoService),
    _(NO_COVERAGE, NoCoverage),
    _(CODE_REQUIRED, CodeRequired),
    _(NOT_ALLOWED, NotAllowed),
    _(DTMF_ERROR, DTMFError),
    _(CHANNEL_LOSS, ChannelLoss),
    _(FDN_NOT_OK, FDNNotOk),
    _(BLACKLIST_BLOCKED, BlacklistBlocked),
    _(BLACKLIST_DELAYED, BlacklistDelayed),
    _(EMERGENCY_FAILURE, EmergencyFailure),
    _(NO_SIM, NoSIM),
    _(DTMF_SEND_ONGOING, DTMFSendOngoing),
    _(CS_INACTIVE, CSInactive),
    _(NOT_READY, NotReady),
    _(INCOMPATIBLE_DEST, IncompatibleDest),
#undef _
    { 0, NULL, NULL },
  };

  static gsize etype = 0;

  if (g_once_init_enter (&etype))
    {
      GType type = g_enum_register_static ("ModemCallError", values);
      g_once_init_leave (&etype, type);
    }

  return (GType)etype;
}

GQuark
modem_call_net_errors_quark (void)
{
  static gsize quark = 0;
  if (g_once_init_enter (&quark))
    {
      GQuark domain = g_quark_from_static_string (MODEM_CALL_NET_ERROR_PREFIX);
      g_once_init_leave (&quark, domain);
    }
  return (GQuark)quark;
}

/** Get GType for network errors */
GType
modem_call_net_error_get_type (void)
{
  static const GEnumValue values[] = {
#define _(x, t) { MODEM_CALL_NET_ERROR_ ## x, "MODEM_CALL_NET_ERROR_" #x, #t }
    _(UNASSIGNED_NUMBER, UnassignedNumber),
    _(NO_ROUTE, NoRouteToDestination),
    _(CH_UNACCEPTABLE, ChannelUnacceptable),
    _(OPER_BARRING, OperatorDeterminedBarring),
    _(NORMAL, NormalCallClearing),
    _(USER_BUSY, UserBusy),
    _(NO_USER_RESPONSE, NoUserResponse),
    _(ALERT_NO_ANSWER, AlertNoAnswer),
    _(CALL_REJECTED, CallRejected),
    _(NUMBER_CHANGED, NumberChanged),
    _(NON_SELECT_CLEAR, NonSelectedClearing),
    _(DEST_OUT_OF_ORDER, DestinationOutOfOrder),
    _(INVALID_NUMBER, InvalidNumber),
    _(FACILITY_REJECTED, FacilityRejected),
    _(RESP_TO_STATUS, ResponseToStatus),
    _(NORMAL_UNSPECIFIED, UnspecifiedNormal),
    _(NO_CHANNEL, NoChannelAvailable),
    _(NETW_OUT_OF_ORDER, NetworkOutOfOrder),
    _(TEMPORARY_FAILURE, TemporaryFailure),
    _(CONGESTION, Congestion),
    _(ACCESS_INFO_DISC, AccessInformationDiscarded),
    _(CHANNEL_NA, ChannelNotAvailable),
    _(RESOURCES_NA, ResourcesNotAvailable),
    _(QOS_NA, QoSNotAvailable),
    _(FACILITY_UNSUBS, RequestedFacilityNotSubscribed),
    _(COMING_BARRED_CUG, IncomingCallsBarredWithinCUG),
    _(BC_UNAUTHORIZED, BearerCapabilityUnauthorized),
    _(BC_NA, BearerCapabilityNotAvailable),
    _(SERVICE_NA, ServiceNotAvailable),
    _(BEARER_NOT_IMPL, BearerNotImplemented),
    _(ACM_MAX, ACMMax),
    _(FACILITY_NOT_IMPL, FacilityNotImplemented),
    _(ONLY_RDI_BC, OnlyRestrictedDIBearerCapability),
    _(SERVICE_NOT_IMPL, ServiceNotImplemented),
    _(INVALID_TI, InvalidTransactionIdentifier),
    _(NOT_IN_CUG, NotInCUG),
    _(INCOMPATIBLE_DEST, IncompatibleDestination),
    _(INV_TRANS_NET_SEL, InvalidTransitNetSelected),
    _(SEMANTICAL_ERR, SemanticalError),
    _(INVALID_MANDATORY, InvalidMandatoryInformation),
    _(MSG_TYPE_INEXIST, MessageTypeNonExistent),
    _(MSG_TYPE_INCOMPAT, MessageTypeIncompatible),
    _(IE_NON_EXISTENT, InformationElementNonExistent),
    _(COND_IE_ERROR, ConditionalInformationElementError),
    _(MSG_INCOMPATIBLE, IncompatibleMessage),
    _(TIMER_EXPIRY, TimerExpiry),
    _(PROTOCOL_ERROR, ProtocolError),
    _(INTERWORKING, Generic),
#undef _
    { 0, NULL, NULL },
  };

  static gsize etype = 0;

  if (g_once_init_enter (&etype))
    {
      GType type = g_enum_register_static ("ModemCallNetError", values);
      g_once_init_leave (&etype, type);
    }

  return (GType)etype;
}

/* ---------------------------------------------------------------------- */

GQuark
modem_sms_net_errors_quark (void)
{
  static gsize quark = 0;

  if (g_once_init_enter (&quark))
    {
      GQuark domain = g_quark_from_static_string (MODEM_SMS_NET_ERROR_PREFIX);
      g_once_init_leave (&quark, domain);
    }

  return (GQuark)quark;
}

/** Get GType for GSM errors */
GType
modem_sms_net_error_get_type (void)
{
  static const GEnumValue values[] = {
#define _(x, t) { MODEM_SMS_NET_ERROR_ ## x, "MODEM_SMS_NET_ERROR_" #x, #t }
    _(SUCCESS,                          Success),
    _(UNASSIGNED_NUMBER,                UnassignedNumber),
    _(OPER_DETERMINED_BARR,             OperatorDeterminedBarring),
    _(CALL_BARRED,                      CallBarred),
    _(RESERVED,                         Reserved),
    _(MSG_TRANSFER_REJ,                 MessageTransferRejected),
    _(MEMORY_CAPACITY_EXC,              MemoryCapacityExceeded),
    _(DEST_OUT_OF_ORDER,                DestinationOutOfOrder),
    _(UNDEFINED_SUBSCRIBER,             UndefinedSubscriber),
    _(FACILITY_REJECTED,                FacilityRejected),
    _(UNKNOWN_SUBSCRIBER,               UnknownSubscriber),
    _(NETWORK_OUT_OF_ORDER,             NetworkOutOfOrder),
    _(TEMPORARY_FAILURE,                TemporaryFailure),
    _(CONGESTION,                       Congestion),
    _(RESOURCE_UNAVAILABLE,             ResourceUnavailable),
    _(REQ_FACILITY_NOT_SUB,             RequiredFacilityNotSubscribed),
    _(REQ_FACILITY_NOT_IMP,             RequiredFacilityNotImplemented),
    _(INVALID_REFERENCE,                InvalidReferenceValue),
    _(INVALID_MSG,                      InvalidMessage),
    _(INVALID_MAND_IE,                  InvalidMandatoryInfoElement),
    _(INVALID_MSG_TYPE,                 InvalidMessageType),
    _(INCOMPATIBLE_MSG_TYPE,            IncompatibleMessageType),
    _(INVALID_IE_TYPE,                  InvalidInfoElementType),
    _(PROTOCOL_ERROR,                   ProtocolError),
    _(INTERWORKING,                     InterworkingError),
    _(LOW_LAYER_NO_CAUSE,               LowLayerUnknown),
    _(IMSI_UNKNOWN_HLR,                 IMSIUnknownInHLR),
    _(ILLEGAL_MS,                       IllegalMS),
    _(IMSI_UNKNOWN_VLR,                 IMSIUnknownInVLR),
    _(IMEI_NOT_ACCEPTED,                IMEINotAccepted),
    _(ILLEGAL_ME,                       IllegalME),
    _(PLMN_NOT_ALLOWED,                 PLMNNotAllowed),
    _(LA_NOT_ALLOWED,                   LocationAreaNotAllowed),
    _(ROAM_NOT_ALLOWED_LA,              RoamingNotAllowedInThisLocationArea),
    _(NO_SUITABLE_CELLS_LA,             NoSuitableCellsInThisLocationArea),
    _(NETWORK_FAILURE,                  NetworkFailure),
    _(MAC_FAILURE,                      MacFailure),
    _(SYNC_FAILURE,                     SyncFailure),
    _(LOW_LAYER_CONGESTION,             LowLayerCongestion),
    _(AUTH_UNACCEPTABLE,                AuthUnacceptable),
    _(SERV_OPT_NOT_SUPPORTED,           ServiceOptionNotSupported),
    _(SERV_OPT_NOT_SUBSCRIBED,          ServiceOptionNotSubscribed),
    _(SERV_OPT_TEMP_OUT_OF_ORDER,       ServiceOptionTemporarilyOutOfOrder),
    _(CALL_CANNOT_BE_IDENTIFIED,        CallCannotBeIdentified),
    _(LOW_LAYER_INVALID_MSG,            LowLayerInvalidMessage),
    _(LOW_LAYER_INVALID_MAND_IE,        LowLayerInvalidMandatoryInfoElement),
    _(LOW_LAYER_INVALID_MSG_TYPE,       LowLayerInvalidMessageType),
    _(LOW_LAYER_INCOMPATIBLE_MSG_TYPE,  LowLayerIncompatibleMessageType),
    _(LOW_LAYER_INVALID_IE_TYPE,        LowLayerInvalidIEType),
    _(LOW_LAYER_INVALID_IE,             LowLayerInvalidIE),
    _(LOW_LAYER_INCOMPATIBLE_MSG,       LowLayerIncompatibleMessage),
    _(CS_BARRED,                        CSBarred),
    _(LOW_LAYER_PROTOCOL_ERROR,         LowLayerProtocolError),
#undef _
    { 0, NULL, NULL },
  };

  static gsize etype = 0;

  if (g_once_init_enter (&etype))
    {
      GType type = g_enum_register_static ("ModemSmsNetError", values);
      g_once_init_leave (&etype, type);
    }

  return (GType)etype;
}

/* ---------------------------------------------------------------------- */

GQuark
modem_sms_errors_quark (void)
{
  static gsize quark = 0;

  if (g_once_init_enter (&quark))
    {
      GQuark domain = g_quark_from_static_string (MODEM_SMS_ERROR_PREFIX);
      g_once_init_leave (&quark, domain);
    }

  return (GQuark)quark;
}

/** GType for sms errors */
GType
modem_sms_error_get_type (void)
{
  static const GEnumValue values[] = {
#define _(x, t) { MODEM_SMS_ERROR_ ## x, "MODEM_SMS_ERROR_" #x, #t }
    _(ROUTING_RELEASED,                 RoutingReleased),
    _(INVALID_PARAMETER,                InvalidParameter),
    _(DEVICE_FAILURE,                   DeviceFailure),
    _(PP_RESERVED,                      PointToPointReserved),
    _(ROUTE_NOT_AVAILABLE,              RouteNotAvailable),
    _(ROUTE_NOT_ALLOWED,                RouteNotAllowed),
    _(SERVICE_RESERVED,                 ServiceReserved),
    _(INVALID_LOCATION,                 InvalidLocation),
    _(NO_NETW_RESPONSE,                 NoNetworkResponse),
    _(DEST_ADDR_FDN_RESTRICTED,         DestinationAddressFDNRestricted),
    _(SMSC_ADDR_FDN_RESTRICTED,         SMSCAddressFDNRestricted),
    _(RESEND_ALREADY_DONE,              ResendAlreadyDone),
    _(SMSC_ADDR_NOT_AVAILABLE,          SMSCAddressNotAvailable),
    _(ROUTING_FAILED,                   RoutingFailed),
    _(CS_INACTIVE,                      CSInactive),
    _(SENDING_ONGOING,                  SendingOngoing),
    _(SERVER_NOT_READY,                 ServerNotReady),
    _(NO_TRANSACTION,                   NoTransaction),
    _(INVALID_SUBSCRIPTION_NR,          InvalidSubscriptionNumber),
    _(RECEPTION_FAILED,                 ReceptionFailed),
    _(RC_REJECTED,                      RCRejected),
    _(ALL_SUBSCRIPTIONS_ALLOCATED,      AllSubscriptionsAllocated),
    _(SUBJECT_COUNT_OVERFLOW,           SubjectCountOverflow),
    _(DCS_COUNT_OVERFLOW,               DCSCountOverflow),
#undef _
    { 0, NULL, NULL },
  };

  static gsize etype = 0;

  if (g_once_init_enter (&etype))
    {
      GType type = g_enum_register_static ("ModemSmsError", values);
      g_once_init_leave (&etype, type);
    }

  return (GType)etype;
}

/* ---------------------------------------------------------------------- */

typedef struct _ModemErrorMapping ModemErrorMapping;

struct {
  GStaticRWLock lock;
  struct _ModemErrorMapping {
    ModemErrorMapping *next;
    GQuark domain;
    GType  type;
    char const *prefix;
    gsize prefixlen;
  } *list;
} modem_registered_errors =
  {
    G_STATIC_RW_LOCK_INIT,
    NULL,
  };

static ModemErrorMapping **
modem_error_append_mapping (ModemErrorMapping **list,
                            GQuark domain,
                            char const *prefix,
                            GType type)
{
  *list = g_new0 (ModemErrorMapping, 1);

  (*list)->domain = domain;
  (*list)->type = type;
  (*list)->prefix = prefix;
  (*list)->prefixlen = strlen (prefix);

  g_type_class_unref (g_type_class_ref (type));

  dbus_g_error_domain_register (domain, prefix, type);

  return &(*list)->next;
}

static ModemErrorMapping **
modem_registered_errors_writer_lock (void)
{
  ModemErrorMapping **list;

  g_static_rw_lock_writer_lock (&modem_registered_errors.lock);

  list = &modem_registered_errors.list;

  if (*list == NULL)
    {
#define _(n) list = modem_error_append_mapping (list,   \
          MODEM_## n ##_ERRORS,                         \
          MODEM_## n ##_ERROR_PREFIX,                   \
          MODEM_TYPE_## n ##_ERROR)
      _(OFONO);
      _(CALL);
      _(CALL_NET);
      _(SMS);
      _(SMS_NET);
#undef _
    }

  return &modem_registered_errors.list;
}

static void
modem_registered_errors_writer_unlock (void)
{
  g_static_rw_lock_writer_unlock (&modem_registered_errors.lock);
}

static ModemErrorMapping const *
modem_registered_errors_reader_lock (void)
{
  if (modem_registered_errors.list == NULL)
    {
      modem_registered_errors_writer_lock ();
      modem_registered_errors_writer_unlock ();
    }

  g_static_rw_lock_reader_lock (&modem_registered_errors.lock);

  return modem_registered_errors.list;
}

static void
modem_registered_errors_reader_unlock (void)
{
  g_static_rw_lock_reader_unlock (&modem_registered_errors.lock);
}

void
modem_error_register_mapping (GQuark domain,
                              char const *prefix,
                              GType type)
{
  DEBUG ("enter");

  ModemErrorMapping **list =  modem_registered_errors_writer_lock ();

  for (; *list; list = &(*list)->next)
    if ((*list)->domain == domain)
      {
        modem_registered_errors_writer_unlock ();
        return;
      }

  modem_error_append_mapping (list, domain, prefix, type);

  modem_registered_errors_writer_unlock ();
}

char const *
modem_error_domain_prefix (GQuark error_domain)
{
  ModemErrorMapping const *map;

  map = modem_registered_errors_reader_lock ();

  for (; map; map = map->next)
    if (map->domain == error_domain)
      break;

  modem_registered_errors_reader_unlock ();

  if (map)
    return map->prefix;
  else
    return "";
}


char const *
modem_error_name (GError const *error, void *buffer, guint len)
{
  GType type = G_TYPE_INVALID;

  if (error)
    {
      ModemErrorMapping const *map;

      map = modem_registered_errors_reader_lock ();

      for (; map; map = map->next)
        {
          if (map->domain == error->domain)
            {
              type = map->type;
              break;
            }
        }

      modem_registered_errors_reader_unlock ();
    }

  if (type)
    {
      GEnumClass *gec = g_type_class_peek (type);
      GEnumValue *ev = g_enum_get_value (gec, error->code);

      if (ev)
        return ev->value_nick;
    }

  if (error)
    g_snprintf (buffer, len, "Code%u", error->code);
  else
    g_snprintf (buffer, len, "NullError");

  return buffer;
}

static char *
dbus_gerror_fqn (GError const *error)
{
  char const *fqn;

  switch (error->code)
    {
    case DBUS_GERROR_FAILED:
      fqn = DBUS_ERROR_FAILED;
      break;
    case DBUS_GERROR_NO_MEMORY:
      fqn = DBUS_ERROR_NO_MEMORY;
      break;
    case DBUS_GERROR_SERVICE_UNKNOWN:
      fqn = DBUS_ERROR_SERVICE_UNKNOWN;
      break;
    case DBUS_GERROR_NAME_HAS_NO_OWNER:
      fqn = DBUS_ERROR_NAME_HAS_NO_OWNER;
      break;
    case DBUS_GERROR_NO_REPLY:
      fqn = DBUS_ERROR_NO_REPLY;
      break;
    case DBUS_GERROR_IO_ERROR:
      fqn = DBUS_ERROR_IO_ERROR;
      break;
    case DBUS_GERROR_BAD_ADDRESS:
      fqn = DBUS_ERROR_BAD_ADDRESS;
      break;
    case DBUS_GERROR_NOT_SUPPORTED:
      fqn = DBUS_ERROR_NOT_SUPPORTED;
      break;
    case DBUS_GERROR_LIMITS_EXCEEDED:
      fqn = DBUS_ERROR_LIMITS_EXCEEDED;
      break;
    case DBUS_GERROR_ACCESS_DENIED:
      fqn = DBUS_ERROR_ACCESS_DENIED;
      break;
    case DBUS_GERROR_AUTH_FAILED:
      fqn = DBUS_ERROR_AUTH_FAILED;
      break;
    case DBUS_GERROR_NO_SERVER:
      fqn = DBUS_ERROR_NO_SERVER;
      break;
    case DBUS_GERROR_TIMEOUT:
      fqn = DBUS_ERROR_TIMEOUT;
      break;
    case DBUS_GERROR_NO_NETWORK:
      fqn = DBUS_ERROR_NO_NETWORK;
      break;
    case DBUS_GERROR_ADDRESS_IN_USE:
      fqn = DBUS_ERROR_ADDRESS_IN_USE;
      break;
    case DBUS_GERROR_DISCONNECTED:
      fqn = DBUS_ERROR_DISCONNECTED;
      break;
    case DBUS_GERROR_INVALID_ARGS:
      fqn = DBUS_ERROR_INVALID_ARGS;
      break;
    case DBUS_GERROR_FILE_NOT_FOUND:
      fqn = DBUS_ERROR_FILE_NOT_FOUND;
      break;
    case DBUS_GERROR_FILE_EXISTS:
      fqn = DBUS_ERROR_FILE_EXISTS;
      break;
    case DBUS_GERROR_UNKNOWN_METHOD:
      fqn = DBUS_ERROR_UNKNOWN_METHOD;
      break;
    case DBUS_GERROR_TIMED_OUT:
      fqn = DBUS_ERROR_TIMED_OUT;
      break;
    case DBUS_GERROR_MATCH_RULE_NOT_FOUND:
      fqn = DBUS_ERROR_MATCH_RULE_NOT_FOUND;
      break;
    case DBUS_GERROR_MATCH_RULE_INVALID:
      fqn = DBUS_ERROR_MATCH_RULE_INVALID;
      break;
    case DBUS_GERROR_SPAWN_EXEC_FAILED:
      fqn = DBUS_ERROR_SPAWN_EXEC_FAILED;
      break;
    case DBUS_GERROR_SPAWN_FORK_FAILED:
      fqn = DBUS_ERROR_SPAWN_FORK_FAILED;
      break;
    case DBUS_GERROR_SPAWN_CHILD_EXITED:
      fqn = DBUS_ERROR_SPAWN_CHILD_EXITED;
      break;
    case DBUS_GERROR_SPAWN_CHILD_SIGNALED:
      fqn = DBUS_ERROR_SPAWN_CHILD_SIGNALED;
      break;
    case DBUS_GERROR_SPAWN_FAILED:
      fqn = DBUS_ERROR_SPAWN_FAILED;
      break;
    case DBUS_GERROR_UNIX_PROCESS_ID_UNKNOWN:
      fqn = DBUS_ERROR_UNIX_PROCESS_ID_UNKNOWN;
      break;
    case DBUS_GERROR_INVALID_SIGNATURE:
      fqn = DBUS_ERROR_INVALID_SIGNATURE;
      break;
    case DBUS_GERROR_INVALID_FILE_CONTENT:
      fqn = DBUS_ERROR_INVALID_FILE_CONTENT;
      break;
    case DBUS_GERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN:
      fqn = DBUS_ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN;
      break;
    case DBUS_GERROR_REMOTE_EXCEPTION:
      fqn = dbus_g_error_get_name ((GError *)error);
      break;
    default:
      return NULL;
    }

  return g_strdup (fqn);
}

char *
modem_error_fqn (GError const *error)
{
  char const *domain, *name;

  g_return_val_if_fail (error, NULL);

  if (error->domain == DBUS_GERROR)
    return dbus_gerror_fqn (error);

  domain = modem_error_domain_prefix (error->domain);
  if (domain == NULL)
    return NULL;
  name = modem_error_name (error, NULL, 0);
  if (name == NULL)
    return NULL;

  return g_strdup_printf ("%s.%s", domain, name);
}

void
modem_error_fix (GError **error)
{
  if (*error == NULL ||
      (*error)->domain != DBUS_GERROR ||
      (*error)->code != DBUS_GERROR_REMOTE_EXCEPTION)
    return;

  ModemErrorMapping const *map;
  GEnumClass *gec;
  GEnumValue *ev;
  GError *fixed;

  char const *fqe = (*error)->message + strlen ((*error)->message) + 1;

  map = modem_registered_errors_reader_lock ();

  for (; map; map = map->next)
    if (strncmp (fqe, map->prefix, map->prefixlen) == 0 &&
        fqe[map->prefixlen] == '.' &&
        strchr (fqe + map->prefixlen + 1, '.') == 0)
      break;

  modem_registered_errors_reader_unlock ();

  if (!map)
    {
      DEBUG ("no match for %s", fqe);
      return;
    }

  gec = g_type_class_peek (map->type);
  ev = g_enum_get_value_by_nick (gec, fqe + map->prefixlen + 1);

  if (ev == NULL)
    {
      DEBUG ("no code point for %s", fqe);
      return;
    }

  fixed = g_error_new_literal (map->domain, ev->value, (*error)->message);

  g_clear_error (error);

  *error = fixed;
}
