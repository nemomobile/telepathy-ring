
"""
Test connecting to a server.
"""

from ringtest import exec_test
from servicetest import EventPattern, call_async
import constants as cs

def test(q, bus, conn):
    conn.Connect()
    q.expect('dbus-signal', signal='StatusChanged', args=[cs.CONN_STATUS_CONNECTING, cs.CSR_REQUESTED])
    q.expect('dbus-signal', signal='StatusChanged', args=[cs.CONN_STATUS_CONNECTED, cs.CSR_REQUESTED])

    call_async(q, conn, 'Disconnect')
    q.expect_many(
            EventPattern('dbus-signal', signal='StatusChanged', args=[2, cs.CSR_REQUESTED]),
            EventPattern('dbus-return', method='Disconnect'))

if __name__ == '__main__':
    exec_test(test)

