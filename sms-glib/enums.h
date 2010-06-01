/*
 * sms-glib/enums.h -
 *
 * Copyright (C) 2008-2010 Nokia Corporation
 *   @author Pekka Pessi <first.surname@nokia.com>
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

#ifndef SMS_GLIB_ENUMS_H
#define SMS_GLIB_ENUMS_H

G_BEGIN_DECLS

typedef enum {
  sms_port_vcard = 0xe2,
  sms_port_vcalendar = 0xe4,
  sms_port_wap_vcard = 0x23f4,
  sms_port_wap_vcalendar = 0x23f5,
} SMSGPortNumber;

enum {
  SMS_G_SUBMIT_MIN_LEN = 7,
  SMS_G_DELIVER_MIN_LEN = 13,
  SMS_G_STATUS_REPORT_MIN_LEN = 19,
};

/* Mask in first byte */
enum {
  SMS_G_TP_MTI_MASK = 0x03,
  SMS_G_TP_MMS_MASK = 0x04,
  SMS_G_TP_VPF_MASK = 0x18,	/* in SMS-SUBMIT */
  SMS_G_TP_SRI_MASK = 0x20,	/* in SMS-DELIVER */
  SMS_G_TP_SRR_MASK = 0x20,	/* in SMS-SUBMIT */
  SMS_G_TP_SRQ_MASK = 0x20,	/* in SMS-STATUS-REPORT */
  SMS_G_TP_UDHI_MASK = 0x40,
  SMS_G_TP_RP_MASK = 0x80		/* in SMS-DELIVER/DELIVER */
};


enum {
  SMS_G_TP_MMS = 0x04,
  SMS_G_TP_SRI = 0x20,	/* in SMS-DELIVER */
  SMS_G_TP_SRR = 0x20,	/* in SMS-SUBMIT */
  SMS_G_TP_SRQ = 0x20,	/* in SMS-STATUS-REPORT */
  SMS_G_TP_UDHI = 0x40,
  SMS_G_TP_RP = 0x80     /* in SMS-DELIVER/DELIVER */
};

enum {
  SMS_G_TP_MTI_DELIVER = 0,
  SMS_G_TP_MTI_DELIVER_REPORT = 0,
  SMS_G_TP_MTI_SUBMIT = 1,
  SMS_G_TP_MTI_SUBMIT_REPORT = 1,
  SMS_G_TP_MTI_STATUS_REPORT = 2,
  SMS_G_TP_MTI_COMMAND = 2
};

enum {
  SMS_G_TP_FCS_UNSUPPORTED_INTERWORKING = 0x80,
  SMS_G_TP_FCS_UNSUPPORTED_TYPE_0 = 0x81,
  SMS_G_TP_FCS_UNSUPPORTED_REPLACE = 0x82,
  SMS_G_TP_FCS_PID_ERROR = 0x8f,
  SMS_G_TP_FCS_UNSUPPORTED_ALPHABET = 0x90,
  SMS_G_TP_FCS_UNSUPPORTED_CLASS = 0x91,
  SMS_G_TP_FCS_DCS_ERROR = 0x9f,
  SMS_G_TP_FCS_NO_COMMAND_ACTION = 0xa0,
  SMS_G_TP_FCS_UNSUPPORTED_COMMAND = 0xa1,
  SMS_G_TP_FCS_COMMAND_ERROR = 0xaf,

  SMS_G_TP_FCS_SC_BUSY = 0xc0,
  SMS_G_TP_FCS_NO_SC_SUBSCRIPTION = 0xc1,
  SMS_G_TP_FCS_INVALID_DESTINATION = 0xc2,
  SMS_G_TP_FCS_BARRED_DESTINATION = 0xc3,
  SMS_G_TP_FCS_REJECTED_DUPLICATE = 0xc4,
  SMS_G_TP_FCS_UNSUPPORTED_VPF = 0xc5,
  SMS_G_TP_FCS_UNSUPPORTED_VALIDITY = 0xc6,

  SMS_G_TP_FCS_SIM_STORAGE_FULL = 0xd0,
  SMS_G_TP_FCS_NO_SIM_STORAGE = 0xd1,
  SMS_G_TP_FCS_MS_ERROR = 0xd2,
  SMS_G_TP_FCS_MEMORY_EXCEEDED = 0xd3,
  SMS_G_TP_FCS_SIM_ATK_BUSY = 0xd4,
  SMS_G_TP_FCS_SIM_DOWNLOAD_ERROR = 0xD5,

  SMS_G_TP_FCS_UNSPECIFIED = 0xff,
};

G_END_DECLS

#endif /* SMS_GLIB_ENUMS_H */
