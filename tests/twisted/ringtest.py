
"""
Infrastructure code for testing Ring
"""

import os
import sys
import dbus
import servicetest
import time
from servicetest import (unwrap, Event)
from twisted.internet import reactor

def install_colourer():
    def red(s):
        return '\x1b[31m%s\x1b[0m' % s

    def green(s):
        return '\x1b[32m%s\x1b[0m' % s

    patterns = {
        'handled': green,
        'not handled': red,
        }

    class Colourer:
        def __init__(self, fh, patterns):
            self.fh = fh
            self.patterns = patterns

        def write(self, s):
            f = self.patterns.get(s, lambda x: x)
            self.fh.write(f(s))

    sys.stdout = Colourer(sys.stdout, patterns)
    return sys.stdout

class Simulator(object):
    def __init__(self):
        self.bus = dbus.SystemBus()

        try:
            manager = dbus.Interface(self.bus.get_object('org.ofono', '/'),
                dbus_interface='org.ofono.Manager')
        except dbus.exceptions.DBusException:
              print "  Ofono needs to be running to execute tests"
              os._exit(1)

def make_connection(bus, event_func, params=None):
    default_params = {
        'modem': dbus.ObjectPath('/phonesim'),
        }

    if params:
        default_params.update(params)

    return servicetest.make_connection(bus, event_func, 'ring', 'tel',
        default_params)

def exec_test_deferred (funs, params, protocol=None, timeout=None):
    colourer = None

    if sys.stdout.isatty():
        colourer = install_colourer()

    queue = servicetest.IteratingEventQueue(timeout)
    queue.verbose = (
        os.environ.get('CHECK_TWISTED_VERBOSE', '') != ''
        or '-v' in sys.argv)

    bus = dbus.SessionBus()

    sim = Simulator()

    bus.add_signal_receiver(
        lambda *args, **kw:
            queue.append(
                Event('dbus-signal',
                    path=unwrap(kw['path']),
                    signal=kw['member'], args=map(unwrap, args),
                    interface=kw['interface'])),
        None,       # signal name
        None,       # interface
        None,
        path_keyword='path',
        member_keyword='member',
        interface_keyword='interface',
        byte_arrays=True
        )

    try:
        for f in funs:
            conn = make_connection(bus, queue.append, params)
            f(queue, bus, conn)
    except Exception:
        import traceback
        traceback.print_exc()

    try:
        if colourer:
          sys.stdout = colourer.fh
        reactor.crash()

        # force Disconnect in case the test crashed and didn't disconnect
        # properly.  We need to call this async because the BaseIRCServer
        # class must do something in response to the Disconnect call and if we
        # call it synchronously, we're blocking ourself from responding to the
        # quit method.
        servicetest.call_async(queue, conn, 'Disconnect')

        if 'RING_TEST_REFDBG' in os.environ:
            # we have to wait for the timeout so the process is properly
            # exited and refdbg can generate its report
            time.sleep(5.5)

    except dbus.DBusException:
        pass

def exec_tests(funs, params=None, protocol=None, timeout=None):
  reactor.callWhenRunning (exec_test_deferred, funs, params, protocol, timeout)
  reactor.run()

def exec_test(fun, params=None, protocol=None, timeout=None):
  exec_tests([fun], params, protocol, timeout)

