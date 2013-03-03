import func_test_case
from control import start_syslogng
from messagegen import *
from messagecheck import *

class TestLuaConfig(func_test_case.SyslogNGTestCase):
    config = """ 

Options{
  threaded = True,
  ts_format = "iso",
  keep_hostname = False,
}   
Source("s_test", { 
    UnixStreamSource("test-lua-log", { host_override = "bzorp", } )
})
Destination("d_test", { FileDestination("test-lua.log") })
Log {
   Source("s_test"),
   Destination("d_test")
}
"""
    def setUp(self):
        if not start_syslogng(self.config, self.verbose, config_type = 'lua'):
            sys.exit(1)

    def test_lua_config(self):
        sender = SocketSender(AF_UNIX, "test-lua-log", dgram = 0, repeat = 10)
        expected = []
        expected.extend(sender.sendMessages("kakukk"))

        return check_file_expected('test-lua', expected)

    def tearDown(self):
        if not stop_syslogng():
            sys.exit(1)
