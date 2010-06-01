/*
 * telepathy-ring.c - Telepathy-ring connection manager
 *
 * Copyright (C) 2007-2010 Nokia Corporation
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

#include "config.h"

#define DEBUG_FLAG RING_DEBUG_CONNECTION

#include "ring-debug.h"
#include "ring-connection-manager.h"

#include <telepathy-glib/run.h>

#include <stdlib.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#if HAVE_LIBMLOCKNICE
#include <mlocknice.h>
#else
#warning "The mlocknice library is not available"
#endif

static TpBaseConnectionManager *
telepathy_ring_connection_manager_new(void)
{
  return (TpBaseConnectionManager *)
    g_object_new(RING_TYPE_CONNECTION_MANAGER, NULL);
}

char *
readfile(char *file)
{
  int fd = open(file, O_RDONLY);
  char buffer[1024];
  ssize_t n;
  size_t ws, nws;

  if (fd == -1)
    return NULL;

  n = read(fd, buffer, (sizeof buffer) - 1);
  close(fd);
  if (n < 0)
    return NULL;

  buffer[n] = '\0';
  ws = strspn(buffer, " \t\v\r\n");
  nws = strcspn(buffer + ws, " \t\v\r\n");
  buffer[ws + nws] = '\0';

  return g_strdup(buffer + ws);
}

#define STATEDIR "/var/lib/telepathy-ring/"

int
main (int argc, char** argv)
{
  ring_debug_set_flags_from_env();

  char const *ring_realtime = getenv("RING_REALTIME");

  if (!ring_realtime)
    ring_realtime = readfile(STATEDIR "realtime");

  if (ring_realtime) {
    struct sched_param sp = {
      .sched_priority = strtoul(ring_realtime, NULL, 0)
    };

    int priority_min = sched_get_priority_min(SCHED_FIFO);
    if (sp.sched_priority < priority_min)
      sp.sched_priority = priority_min;

    int priority_max = sched_get_priority_max(SCHED_FIFO);
    if (sp.sched_priority > priority_max)
      sp.sched_priority = priority_max;

    if (sched_setscheduler(0, SCHED_FIFO, &sp) < 0)
      DEBUG("sched_setscheduler(RING_REALTIME=%s): %s",
        ring_realtime, strerror(errno));
  }

  char const *ring_memlock = getenv("RING_MEMLOCK");

  if (!ring_memlock)
    ring_memlock = readfile(STATEDIR "memlock");

  if (ring_memlock) {
    struct rlimit rl[1];
    char *rest = NULL;
    rlim_t rlim = strtoul(ring_memlock, &rest, 0);

    switch (*rest) {
      case 'g': case 'G': rlim *= 1024;
        /* FALLTHROUGH */
      case 'm': case 'M': rlim *= 1024;
        /* FALLTHROUGH */
      case 'k': case 'K': rlim *= 1024;
    }

    if (rlim == 0)
      rlim = 1024 * 1024 * 256;

    rl->rlim_cur = rlim;
    rl->rlim_max = rlim;

    if (setrlimit(RLIMIT_MEMLOCK, rl) < 0) {
      DEBUG("setrlimit(): %s", strerror(errno));
      ring_memlock = NULL;
    }
  }

  /* Drop privileges */
#if HAVE_GETRESUID
  {
    uid_t ruid, euid, suid;

    if (getresuid(&ruid, &euid, &suid) == -1)
      DEBUG("%s(): %s", "getresuid", strerror(errno));
    else if (ruid == euid && ruid == suid)
      ;
    else if (setresuid(ruid, ruid, ruid) == -1) {
      DEBUG("%s(): %s", "setresuid", strerror(errno));
      exit(2);
    }
  }
#else
  {
    uid_t ruid = getuid();
    if (ruid != geteuid())
      if (setreuid(ruid, ruid) == -1) {
        DEBUG("%s(): %s", "setreuid", strerror(errno));
        exit(2);
      }
  }
#endif
  if (ring_memlock) {
    if (mlockall(MCL_FUTURE) < 0)
      DEBUG("mlockall(): %s", strerror(errno));
#if HAVE_LIBMLOCKNICE
    else if (mln_lock_data() < 0)
      DEBUG("mln_lock_data(): %s", strerror(errno));
#endif
  }

  return tp_run_connection_manager(
    "telepathy-ring", PACKAGE_VERSION,
    telepathy_ring_connection_manager_new,
    argc, argv);
}
