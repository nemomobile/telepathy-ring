#! /bin/sh

USER=user

# obtain session bus address for Ring
test -r /tmp/session_bus_address.$USER &&
. /tmp/session_bus_address.$USER

test -x /usr/bin/waitdbus &&
/usr/bin/waitdbus session

export RING_REALTIME=10
export RING_MEMLOCK=32M

# Uncomment to obtain debug output
# RING_DEBUG=all
# CALL_DEBUG=all
# SMS_DEBUG=all
# MODEM_DEBUG=all

# Make ring wait for Connection requests
export RING_PERSIST=1

exec ${RING_VALGRIND} /usr/lib/telepathy/telepathy-ring
