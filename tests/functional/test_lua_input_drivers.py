import test_input_drivers
from control import *
import sys
from globals import *

context = locals()

class TestLuaInputDrivers(test_input_drivers.TestInputDrivers):
    config = """
Options{ ts_format = "iso", chain_hostnames = false, keep_hostname = true, threaded = true }

Source( "s_int", { Internal() })
Source( "s_unix", { UnixStreamSource("log-stream", { flags = { "expect-hostname" } }), UnixDgramSource("log-dgram", { flags = { "expect-hostname" } }) })
Source( "s_inet", { TcpSource{ port = %(port_number)d }, UdpSource{ port = %(port_number)d, so_recvbuf = 131072 } })
Source( "s_inetssl", { TcpSource{ port = %(ssl_port_number)d, tls = { peer_verify = "none", cert_file = "%(src_dir)s/ssl.crt", key_file = "%(src_dir)s/ssl.key" } } })
Source( "s_pipe", { PipeSource("log-pipe", { flags = { "expect-hostname"} }), PipeSource("log-padded-pipe", { flags = { "expect-hostname"}, pad_size = 2048 }) } )
Source( "s_file", { FileSource("log-file") })
Source( "s_network", { NetworkSource{ transport = "udp", port = %(port_number_network)s}, NetworkSource{ transport = "tcp", port = %(port_number_network)s } })
Source( "s_catchall", { UnixStreamSource("log-stream-catchall", { flags = { "expect-hostname"} }) })

Source( "s_syslog", { SyslogSource{ port = %(port_number_syslog)d, transport = "tcp",  so_rcvbuf = 131072}, SyslogSource{ port = %(port_number_syslog)d, transport = "udp",  so_rcvbuf = 131072}})

Filter( "f_input1", MessageFilter("input_drivers"))

Destination( "d_input1", { FileDestination("test-input1.log") } )
Destination( "d_input1_new", { FileDestination("test-input1_new.log", { flags = {"syslog-protocol"} } ) } )

Log {
  Source("s_int"), Source("s_unix"), Source("s_inet"), Source("s_inetssl"), Source("s_pipe"), Source("s_network"), Source("s_file"),
  EmbeddedLog{
     Filter("f_input1"),
     Destination("d_input1")
  }
}

Log {
  Source("s_syslog"),
  EmbeddedLog{
     Filter("f_input1"),
     Destination("d_input1_new")
  }
}

Destination( "d_indep1", { FileDestination("test-indep1.log") } )
Destination( "d_indep2", { FileDestination("test-indep2.log") } )

Filter( "f_indep", MessageFilter("indep_pipes"))

Log {
  Source("s_int"), Source("s_unix"), Source("s_inet"), Source("s_inetssl"), Filter("f_indep"),
  EmbeddedLog{
     Destination("d_indep1")
  },
  EmbeddedLog{
     Destination("d_indep2")
  }
}

Filter( "f_final", MessageFilter("final"))

Filter( "f_final1", MessageFilter("final1"))
Filter( "f_final2", MessageFilter("final2"))
Filter( "f_final3", MessageFilter("final3"))

Destination( "d_final1", { FileDestination("test-final1.log") } )
Destination( "d_final2", { FileDestination("test-final2.log") } )
Destination( "d_final3", { FileDestination("test-final3.log") } )
Destination( "d_final4", { FileDestination("test-final4.log") } )

Log {
  Source("s_int"), Source("s_unix"), Source("s_inet"), Source("s_inetssl"), Filter("f_final"),
  EmbeddedLog{
     Filter("f_final1"),
     Destination("d_final1"),
     flags = {"final"}
  },
  EmbeddedLog{
     Filter("f_final2"),
     Destination("d_final2"),
     flags = {"final"}
  },
  EmbeddedLog{
     Filter("f_final3"),
     Destination("d_final3"),
     flags = {"final"}
  },
  EmbeddedLog{
     Destination("d_final4")
  }
}

Filter( "f_fb", MessageFilter("fallback"))
Filter( "f_fb1", MessageFilter("fallback1"))
Filter( "f_fb2", MessageFilter("fallback2"))
Filter( "f_fb3", MessageFilter("fallback3"))
--for i = 1,3 do Filter( "f_fb"..i, MessageFilter("fallback"..i)) end

Destination( "d_fb1", { FileDestination("test-fb1.log") } )
Destination( "d_fb2", { FileDestination("test-fb2.log") } )
Destination( "d_fb3", { FileDestination("test-fb3.log") } )
Destination( "d_fb4", { FileDestination("test-fb4.log") } )

Log {
  Source("s_int"), Source("s_unix"), Source("s_inet"), Source("s_inetssl"), Filter("f_fb"),
  EmbeddedLog{ Destination("d_fb4"), flags = { "fallback"} },
  EmbeddedLog{ Filter( "f_fb1"), Destination("d_fb1") },
  EmbeddedLog{ Filter( "f_fb2"), Destination("d_fb2") },
  EmbeddedLog{ Filter( "f_fb3"), Destination("d_fb3") }
}

Filter( "f_catchall", MessageFilter("catchall"))

Destination( "d_catchall", { FileDestination("test-catchall.log") } )

Log { Filter( "f_catchall"), Destination( "d_catchall"), flags = { "catch-all" } }

""" % context
    def setUp(self):
        if not start_syslogng(self.config, self.verbose, config_type = 'lua'):
            sys.exit(1)

    def tearDown(self):
        if not stop_syslogng():
            sys.exit(1)

