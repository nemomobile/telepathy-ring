/*
 * modem/tones.h - Call signaling tones handling
 *
 * Copyright (C) 2007 Nokia Corporation
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

#ifndef _MODEM_TONES_H_
#define _MODEM_TONES_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ModemTones ModemTones;
typedef struct _ModemTonesClass ModemTonesClass;
typedef struct _ModemTonesPrivate ModemTonesPrivate;

struct _ModemTonesClass {
  GObjectClass parent_class;
};

struct _ModemTones {
  GObject parent;
  ModemTonesPrivate *priv;
};

GType modem_tones_get_type(void);

/* TYPE MACROS */
#define MODEM_TYPE_TONES                        \
  (modem_tones_get_type())
#define MODEM_TONES(obj)                                                \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MODEM_TYPE_TONES, ModemTones))
#define MODEM_TONES_CLASS(klass)                                        \
  (G_TYPE_CHECK_CLASS_CAST((klass), MODEM_TYPE_TONES, ModemTonesClass))
#define MODEM_IS_TONES(obj)                             \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MODEM_TYPE_TONES))
#define MODEM_IS_TONES_CLASS(klass)                     \
  (G_TYPE_CHECK_CLASS_TYPE((klass), MODEM_TYPE_TONES))
#define MODEM_TONES_GET_CLASS(obj)                                      \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MODEM_TYPE_TONES, ModemTonesClass))

enum {
  TONES_STOP = -2,
  TONES_NONE = -1,
  TONES_EVENT_DTMF_0 = 0,
  TONES_EVENT_DTMF_1 = 1,
  TONES_EVENT_DTMF_2 = 2,
  TONES_EVENT_DTMF_3 = 3,
  TONES_EVENT_DTMF_4 = 4,
  TONES_EVENT_DTMF_5 = 5,
  TONES_EVENT_DTMF_6 = 6,
  TONES_EVENT_DTMF_7 = 7,
  TONES_EVENT_DTMF_8 = 8,
  TONES_EVENT_DTMF_9 = 9,
  TONES_EVENT_DTMF_ASTERISK = 10,
  TONES_EVENT_DTMF_HASH = 11,
  TONES_EVENT_DTMF_A = 12,
  TONES_EVENT_DTMF_B = 13,
  TONES_EVENT_DTMF_C = 14,
  TONES_EVENT_DTMF_D = 15,

  TONES_EVENT_DIAL = 66,
  TONES_EVENT_RINGING = 70,
  TONES_EVENT_BUSY = 72,
  TONES_EVENT_CONGESTION = 73,
  TONES_EVENT_SPECIAL_INFORMATION = 74,
  TONES_EVENT_CALL_WAITING = 79,
  TONES_EVENT_RADIO_PATH_ACK = 256,
  TONES_EVENT_RADIO_PATH_UNAVAILABLE = 257,

  TONES_EVENT_DROPPED = 257,
};

guint modem_tones_start(ModemTones *, int event, unsigned duration);

typedef void
ModemTonesStoppedNotify(ModemTones *, guint event, gpointer data);

guint modem_tones_start_full(ModemTones *,
  int event, int volume, unsigned duration,
  ModemTonesStoppedNotify *notify, gpointer data);

guint modem_tones_is_playing(ModemTones const *self, guint source);

int modem_tones_playing_event(ModemTones const *self, guint playing);

void modem_tones_stop(ModemTones *, guint source);

G_END_DECLS

#endif /* #ifndef _MODEM_TONES_H_ */
