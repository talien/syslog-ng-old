import time

def print_user(msg):
    print '    ', time.strftime('%Y-%M-%dT%H:%m:%S'), msg

def print_banner(message, big = True):
    print "\n", 
    if big:
        print("##############################################")
    print("### %s" % message)
    if big:
        print("##############################################")
    print "\n",

def print_big_banner(message):
    print_banner(message, big = True);

def print_small_banner(message):
    print_banner(message, big = False);

def print_start(testcase):
    print_small_banner("Starting testcase: %s" % testcase)    
    print_user("Testcase start")

def print_end(testcase, result):
    print_user("Testcase end")
    if result:
        print_small_banner("PASS: %s" % testcase)
    else:
        print_small_banner("FAIL: %s" % testcase)

