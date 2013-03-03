#!/usr/bin/python
from func_test_case import SyslogNGTestCase
import sys,os, re
from log import *

sys.path += ['.']

def discover_test_modules(path):
    return [ i.split(".")[0] for i in os.listdir(path) if re.match("test_.*\.py$", i) ]

def discover_test_cases_from_module(module_name):
    modul = __import__(module_name)
    return [ getattr(modul,i) for i in dir(modul) if isinstance(getattr(modul,i), type) and issubclass(getattr(modul,i),SyslogNGTestCase)]

def run_tests_from_test_case_class_list(tests):
    succeeded = 0
    failed = 0
    for test_case in tests:
        test = test_case(False)
        print_big_banner("Running tests from test_case: %s" % test_case.__name__)
        (test_succeeded, test_failed) =  test.run()
        succeeded = succeeded + test_succeeded
        failed = failed + test_failed

    print_big_banner("Results: all testcases:%d, success:%d failure:%d " % (succeeded + failed, succeeded, failed))
    return failed
       
def run_tests():
    test_modules = discover_test_modules('./')
    tests = set()
    for module in test_modules:
        tests |= set(discover_test_cases_from_module(module))
    return run_tests_from_test_case_class_list(list(tests))

def seed_rnd():
    # Some platforms lack kernel supported random numbers, on those we have to explicitly seed the RNG.
    # using a fixed RND input does not matter as we are not protecting real data in this test program.
    try:
        import base64
        import socket
        rnd = base64.decodestring("""k/zFvqjGWdhStmhfOeTNtTs89P8soknF1J9kSQrz8hKdrjIutqTXMfIqCNUb7DXrMykMW+wKd1Pg
DwaUwxKmlaU1ItOek+jNUWVw9ZOSI1EmsXVgu+Hu7URgmeyY0A3WsDmMzR0Z2wTcRFSuINgBP8LC
8SG27gJZVOoVv09pfY9WyjvUYwg1jBdTfEM+qcDQKOACx4DH+SzO0bOOJMfMbR2iFaq18b/TCeQN
kRy9Lz2WvBsByQoXw/afxiu5xzn0MHoxTMCZCTjIyhGXzO/R2yj3eBVc5vxc5oxG3/EdjGnhmn/L
HEFVgc6EJ5OF1ye8hDIHFdZFehCAPbso3LL/3r8oJn+Axmc2sOrvhzMLpCV2KWdy8haiv3WZ9hZP
iJkC0pGBR+vhrdYE7qh2RzcdYoHRCvX5tpaOG+SE8+Az2WPHPOD7j4j+4KbhW+YbzYfUAsmwOQb4
R3vIyGO6Og4fg+AmKcycpv9Bo17zidbQe9zdZO6X4Df+ychK+yRSCMHLUyrEry7IFt60ivLAjc8b
Ladp70v7icWO+J9P/3pXIzKuQuaeT7J1q2xlCc/srV2pNV4+EhKJ8qSe+hG4LzpXWRGKo72h6BvL
Wgcp3S/wy1e6ov8XsVKjkWeQH8Nh3xOMWGYh6/yXSN44BLIBUdqk4I3TCKy1DawJQd1ivytEF0Xh
BstGzSoyeR92mN/K5/gy9wKFZffTbE7DmysKflRAN85ht7IVqxrTNXQ+UPWvlGZDAQ0NY45rQI3K
S1q0ahQivgGUZmEhuZkAUlYGWAqtCeDz3rZvrp5r2WJFGTZ+9yJZC5T2L+erDGBYxmcwxDzz8AvS
Ybp4aIBEtTK71cx9DtFTtSaQ6aW9rI5nc/owo0gv4Ddm5BYjMAd7kcc9TWnb1DZ9AJQAnSxgcuJM
acbYkFcqtMnafh/VnGekZ8yM0ZZqQPKBhysx+u2UBXib7Vvb6x6Y/xglNcqBWGFmzruKt6hx0pR2
x9IunzeIwdlcwIhLEKfIiy9ULwi7RTjVSeqgwucWoC0lw0BTotGeLDcFxTlQsE3T/UneLa8H6iSH
VW5iRZrvI0sdxt5Ud0TjNqXRGxuVczSuwpQwwxBn0ogr9DoRnp375PwGGh1/yqimW/+OStwP3cRR
yXEg6Zq1CvuYF/E6el4h9GylxkU7wEM2Ti9QJY4n3YsHyesalERqdd9xx5t7ADRodpMpZXoZGbrS
vccp3zMzS/aEZRuxky1/qjrAEh8OVA58e82jQqTdY8OQ/kKOu/gUgKBnHAvLkB/020p0CNbq6HjY
l625DLckaYmOPTh0ECFKzhaPF+/LNmzD36ToOAeuNjfbUjiUVGfntr2mc4E8mUFyo+TskrkSfw==
""")
        socket._ssl.RAND_add(rnd, 1024)
        if not socket._ssl.RAND_status():
            raise "PRNG not seeded"
    except ImportError:
        return

seed_rnd()

sys.exit(run_tests())
