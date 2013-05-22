from globals import *
from log import *
from messagegen import *
from messagecheck import *
import func_test_case
import socket, time

context = locals()

BUFFER_SIZE = 4096

def receive_all(sock):
    buf = sock.recvfrom(BUFFER_SIZE)
    result = buf[0]
    while (len(buf[0]) == BUFFER_SIZE):
        buf = sock.recvfrom(BUFFER_SIZE)
        result += buf[0]
    return result


class TestRss(func_test_case.SyslogNGOldTest):

    config = """@version: 3.4

    options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); threaded(yes); };

    source s_int { internal(); };
    source s_test { unix-stream("log-stream" flags(expect-hostname)); };

    destination d_rss { rss( port(8081) feed_title("syslog-ng") entry_title("${MESSAGE}") entry_description("${MESSAGE}") ); };

    log { source(s_test); destination(d_rss); };

    """ % context

    simple_expected = 'HTTP/1.1 200 OK\nContent-Type:application/atom+xml\n\n<?xml version="1.0"?>\n<feed xmlns="http://www.w3.org/2005/Atom">\n<title>syslog-ng</title><link>localhost:8081</link><entry>\n <title>kakukk 001/00001 unix-stream(log-stream) xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx</title>\n <description>kakukk 001/00001 unix-stream(log-stream) xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx</description>\n <id>0</id>\n</entry>\n</feed>\n'
    
    def test_rss_simple(self):
        # FIXME: Geeee, this is way tooo hackish!!!
        # The correct solution should be to write a normal send message, to be able to assert to the exact message in the rss feed.
        # This test is too crude, what is should check, is the HTTP header well formedness and XML/Atom well formedness, and the contents of specific elements
        global session_counter
        old_session_counter = session_counter
        session_counter = 1
        s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=1)
        s.sendMessages("kakukk")
        session_counter = old_session_counter
        time.sleep(1)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
        sock.connect(("localhost", 8081),)
        sock.send("GET / HTTP/1.1\n\n");
        result = receive_all(sock)
        #print result
        if (self.simple_expected == result):
            return True
        else:
            print "expected='%r' actual='%r'" % (self.simple_expected, result)
            return False
        sock.close()
           

    def test_rss_more_than_backlog(self):
        s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=102)
        s.sendMessages("kakukk")
        time.sleep(1)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
        sock.connect(("localhost", 8081),)
        sock.send("GET / HTTP/1.1\n\n");
        result = receive_all(sock)
        print result
        if (result.find("<id>101</id>") != -1) and (result.find("<id>0</id>") == -1):
           return True
        else:
           return False
        sock.close()

