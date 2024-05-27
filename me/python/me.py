"""
This module implements a friendly API to the FinTEx matching engine.
"""

import melow # type: ignore


# Don't change this.
ORDER_TYPE_LIMIT = melow.ME_ORDER_LIMIT
ORDER_TYPE_MARKET = melow.ME_ORDER_MARKET
SIDE_BUY = melow.ME_SIDE_BUY
SIDE_SELL = melow.ME_SIDE_SELL


class Order:
    def __init__(self, security_id: int, side: int, price: int, quantity: int, timestamp: int, type=ORDER_TYPE_LIMIT, id=0):
        self.security_id = security_id
        self.side = side
        self.price = price
        self.quantity = quantity
        self.timestamp = timestamp
        self.type = type
        self.order_id = id


    def getSecurityID(self) -> int:
        return self.security_id


    def getID(self) -> int:
        return self.order_id


    def getPrice(self) -> int:
        return self.price


    def getQuantity(self) -> int:
        return self.quantity


    def getTimestamp(self) -> int:
        return self.timestamp


    def isGreaterThan(self, other) -> bool:
        """This function throws if the orders have different security ID or different sides."""
        if self.side != other.side or self.security_id != other.security_id:
            raise AttributeError

        if self.side == SIDE_BUY:
            return self.getPrice() > other.getPrice() or self.getTimestamp() > other.getTimestamp()
        else:
            return self.getPrice() < other.getPrice() or self.getTimestamp() > other.getTimestamp()


    def isLimit(self) -> bool:
        return self.type == ORDER_TYPE_LIMIT


    def isMarket(self) -> bool:
        return self.type == ORDER_TYPE_MARKET


    def isBuy(self) -> bool:
        return self.side == SIDE_BUY


    def isSell(self) -> bool:
        return self.side == SIDE_SELL


class Message:
    def toTuple(self):
        pass


    def fromTuple(t):
        # The melow module is build against Python 3.12 so we don't mind using
        # such a new feature.
        ot = t[1]
        match t[0]:
            case melow.ME_MESSAGE_PANIC:
                return MessagePanic()
            case melow.ME_MESSAGE_NEW_ORDER:
                return MessageNewOrder(Order(security_id=ot[0], side=ot[1], quantity=ot[2], type=ot[3], price=ot[4], id=ot[5], timestamp=ot[6]))
            case melow.ME_MESSAGE_ORDER_EXECUTED:
                return MessageOrderExecuted(Order(security_id=ot[0], side=ot[1], quantity=ot[2], type=ot[3], price=ot[4], id=ot[5], timestamp=ot[6]))
            case melow.ME_MESSAGE_CANCEL_ORDER:
                return MessageCancelOrder(ot[0], ot[1])
            case melow.ME_MESSAGE_TRADE:
                return MessageTrade(Order(security_id=ot[0], side=ot[1], quantity=ot[2], type=ot[3], price=ot[4], id=ot[5], timestamp=ot[6]), ot[7])
            case melow.ME_MESSAGE_SET_MARKET_PRICE:
                return MessageSetMarketPrice(ot[0], ot[1])


class MessagePanic(Message):
    def toTuple(self):
        return (melow.ME_MESSAGE_PANIC, ())


    def __init__(self):
        pass


class MessageNewOrder(Message):
    def toTuple(self):
        return (melow.ME_MESSAGE_NEW_ORDER, (self.order.security_id, self.order.side, self.order.quantity, self.order.type, self.order.price, self.order.order_id, self.order.timestamp))


    def __init__(self, order):
        self.order = order


class MessageOrderExecuted(Message):
    def toTuple(self):
        return (melow.ME_MESSAGE_ORDER_EXECUTED, (self.order.security_id, self.order.side, self.order.quantity, self.order.type, self.order.price, self.order.order_id, self.order.timestamp))


    def __init__(self, order):
        self.order = order


class MessageTrade(Message):
    def toTuple(self):
        return (melow.ME_MESSAGE_ORDER_EXECUTED, (self.order.security_id, self.order.side, self.order.quantity, self.order.type, self.order.price, self.order.order_id, self.order.timestamp, self.matched_id))


    def __init__(self, order, matched_id):
        self.order = order
        self.matched_id = matched_id


class MessageCancelOrder(Message):
    def toTuple(self):
        return (melow.ME_MESSAGE_CANCEL_ORDER, (self.security_id, self.order_id))


    def __init__(self, security_id, order_id):
        self.order_id = order_id
        self.security_id = security_id


class MessageSetMarketPrice(Message):
    def toTuple(self):
        return (melow.ME_MESSAGE_SET_MARKET_PRICE, (self.security_id, self.price))


    def __init__(self, security_id, price):
        self.security_id = security_id
        self.price = price


class Engine:
    def __init__(self, cache=melow.ME_DEFAULT_CACHE_SIZE, secs=melow.ME_DEFAULT_SECURITIES_NUMBER):
        self.secs = secs
        self.context = melow.Context(cache, secs)


    def run(self) -> None:
        self.context.run()


class Client:
    def __init__(self):
        self.context = melow.ClientContext()


    def send(self, message: Message) -> None:
        self.context.sendMessage(*message.toTuple())


    def get(self) -> Message:
        return Message.fromTuple(self.context.getMessage())
