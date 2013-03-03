from log import *
from control import *
import sys, traceback

class SyslogNGTestCase(object):
    def __init__(self, verbose):
        self.verbose = verbose

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def run_test_function(self, test_case_name):
        try:
            print_start(test_case_name) 
            self.setUp()
            success = getattr(self, test_case_name)()
            self.tearDown()
            print_end(test_case_name, success)
        except Exception, e:
            print_end(test_case_name, False)
            print traceback.print_exc()
            return False
        return success

    def is_name_of_a_test_function(self, name):
        return name[0:4] == "test"

    def run(self):
        if (not self.checkRunnable()):
            return (0,0)
        self.initialize()
        succeeded = 0
        failed = 0
        for key in dir(self):
            if self.is_name_of_a_test_function(key):
                if self.run_test_function(key):
                   succeeded = succeeded + 1
                else:
                   failed = failed + 1
        self.deinitialize()
        return (succeeded, failed)
    
    def initialize(self):
        pass
  
    def deinitialize(self):
        pass

    def checkRunnable(self):
        return True


class SyslogNGOldTest(SyslogNGTestCase):
    def setUp(self):
        if not start_syslogng(self.config, self.verbose):
            sys.exit(1)

    def tearDown(self):
        if not stop_syslogng():
            sys.exit(1)


        
