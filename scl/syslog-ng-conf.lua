local a = UnixStreamSource("/tmp/log")
local a2 = UnixStreamSource("/tmp/log2")
Source("s_test", { a, a2 })
local b = FileDestination("/tmp/messages")
Destination("d_test", { b })
Log{
   Source("s_test"),
   Source{ UnixStreamSource("/tmp/log3") },
   Destination("d_test")
}

