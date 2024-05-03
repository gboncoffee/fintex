from multiprocessing import Process

import me.python.melow as melow

engineContext = melow.Context(melow.ME_DEFAULT_CACHE_SIZE, melow.ME_DEFAULT_SECURITIES_NUMBER)
engineProcess = Process(target=engineContext.run)
engineProcess.start()

clientContext = melow.ClientContext()

clientContext.sendMessage(melow.ME_MESSAGE_SET_MARKET_PRICE, (3, 30))
print(clientContext.getMessage())

clientContext.sendMessage(melow.ME_MESSAGE_PANIC, ())
print(clientContext.getMessage())

engineProcess.join()
