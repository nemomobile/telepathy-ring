"""
Test Ring's o.fd.T.Protocol implementation
"""

import dbus
from servicetest import unwrap, tp_path_prefix, assertEquals, assertContains
from ringtest import exec_test
import constants as cs

def test(q, bus, conn):
    cm = bus.get_object(cs.CM + '.ring',
        tp_path_prefix + '/ConnectionManager/ring')
    cm_iface = dbus.Interface(cm, cs.CM)
    cm_prop_iface = dbus.Interface(cm, cs.PROPERTIES_IFACE)

    protocols = unwrap(cm_prop_iface.Get(cs.CM, 'Protocols'))
    assertEquals(set(['tel']), set(protocols.keys()))

    protocol_names = unwrap(cm_iface.ListProtocols())
    assertEquals(set(['tel']), set(protocol_names))

    cm_params = cm_iface.GetParameters('tel')
    local_props = protocols['tel']
    local_params = local_props[cs.PROTOCOL + '.Parameters']
    assertEquals(cm_params, local_params)

    proto = bus.get_object(cm.bus_name, cm.object_path + '/tel')
    proto_prop_iface = dbus.Interface(proto, cs.PROPERTIES_IFACE)
    proto_props = unwrap(proto_prop_iface.GetAll(cs.PROTOCOL))

    for key in ['Parameters', 'Interfaces', 'ConnectionInterfaces',
      'RequestableChannelClasses', u'VCardField', u'EnglishName', u'Icon']:
        a = local_props[cs.PROTOCOL + '.' + key]
        b = proto_props[key]
        assertEquals(a, b)

    assertEquals('tel', proto_props['VCardField'])
    assertEquals('Mobile Telephony', proto_props['EnglishName'])
    assertEquals('im-tel', proto_props['Icon'])

    assertContains(cs.CONN_IFACE_REQUESTS, proto_props['ConnectionInterfaces'])

if __name__ == '__main__':
    exec_test(test)
