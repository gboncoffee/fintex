from multiprocessing import Process
import time
import sys
import os

sys.path.append(f"{os.getcwd()}/me/python")

import me


GLOBAL_ID_PLS_DONT_TOUCH = 0


def id():
    global GLOBAL_ID_PLS_DONT_TOUCH
    GLOBAL_ID_PLS_DONT_TOUCH = GLOBAL_ID_PLS_DONT_TOUCH + 1
    return GLOBAL_ID_PLS_DONT_TOUCH


def runAsciiLogger():
    os.system("./me/me-ascii-logger")


def testSimpleTrade():
    order1 = me.MessageNewOrder(me.Order(1, me.SIDE_BUY, 30, 40, timestamp=int(time.time()), id=id()))
    order2 = me.MessageNewOrder(me.Order(1, me.SIDE_SELL, 30, 40, timestamp=int(time.time()), id=id()))
    client.send(order1)
    client.send(order2)


def testCancellation():
    order1id = id()
    order1 = me.MessageNewOrder(me.Order(1, me.SIDE_BUY, 30, 40, timestamp=int(time.time()), id=order1id))
    order2 = me.MessageNewOrder(me.Order(1, me.SIDE_BUY, 30, 40, timestamp=int(time.time()), id=id()))
    order3 = me.MessageNewOrder(me.Order(1, me.SIDE_SELL, 30, 40, timestamp=int(time.time()), id=id()))
    cancelOrder1 = me.MessageCancelOrder(1, order1id)
    client.send(order1)
    client.send(order2)
    client.send(cancelOrder1)
    client.send(order3)


# Even single-threaded all this is still kinda non-deterministic.
os.environ["OMP_NUM_THREADS"] = "1"

engine = me.Engine()
engineProcess = Process(target=engine.run)
engineProcess.start()

asciiLoggerProcess = Process(target=runAsciiLogger)
asciiLoggerProcess.start()

client = me.Client()
testSimpleTrade()
testCancellation()

client.send(me.MessagePanic())
engineProcess.join()
