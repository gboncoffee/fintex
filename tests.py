import me.python.me as me
from multiprocessing import Process

engineContext = me.Context(me.ME_DEFAULT_CACHE_SIZE, me.ME_DEFAULT_SECURITIES_NUMBER)
engineProcess = Process(target=engineContext.run)
engineProcess.start()

clientContext = me.ClientContext()

clientContext.sendMessage(3, me.SetMarketPriceMessage(30))
print(clientContext.getMessage())

clientContext.sendMessage(3, me.PanicMessage())
print(clientContext.getMessage())

engineProcess.join()
