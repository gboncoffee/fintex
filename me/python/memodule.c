#define PY_SSIZE_T_CLEAN
#include <python3.12/Python.h>

#include "../me.h"

#define DOCS "This module implements bindings to the FinTEx matching engine."

/*
 * Error handling.
 */
static PyObject *meErrorPosixQueue;
static PyObject *meErrorOpenPosixQueue;

/*
 * Message type(s). They all inherit from Message.
 */

typedef struct {
  PyObject_HEAD int64_t security_id;
} MePyMessage;

static PyTypeObject mePyMessageType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "me.Message",
    .tp_doc = PyDoc_STR("Base message type."),
    .tp_basicsize = sizeof(MePyMessage),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
};

/* Order. */
typedef struct {
  MePyMessage message_obj;
  MeOrder order;
} MePyOrderMessage;

static void mePyOrderMessage_dealloc(MePyOrderMessage *self) {
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *mePyOrderMessage_new(PyTypeObject *type, PyObject *args,
                                      PyObject *kwds) {
  MePyOrderMessage *self;
  self = (MePyOrderMessage *)type->tp_alloc(type, 0);
  if (self == NULL) return NULL;

  static char *kwlist[] = {"side", "quantity",  "ord_type", "price",
                           "id",   "timestamp", NULL};
  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "llllll", kwlist, &self->order.side,
          &self->order.quantity, &self->order.ord_type, &self->order.price,
          &self->order.order_id, &self->order.timestamp))
    return NULL;

  return (PyObject *)self;
}

static PyTypeObject mePyOrderMessageType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "me.OrderMessage",
    .tp_doc = PyDoc_STR("Order message, to submit new orders."),
    .tp_basicsize = sizeof(MePyOrderMessage),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = mePyOrderMessage_new,
    .tp_dealloc = (destructor)mePyOrderMessage_dealloc,
};

/* Order executed. Literally equal to order. */
typedef struct {
  MePyMessage message_obj;
  MeOrder order;
} MePyOrderExecutedMessage;

static void mePyOrderExecutedMessage_dealloc(MePyOrderExecutedMessage *self) {
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *mePyOrderExecutedMessage_new(PyTypeObject *type,
                                              PyObject *args, PyObject *kwds) {
  MePyOrderExecutedMessage *self;
  self = (MePyOrderExecutedMessage *)type->tp_alloc(type, 0);
  if (self == NULL) return NULL;

  static char *kwlist[] = {"side", "quantity",  "ord_type", "price",
                           "id",   "timestamp", NULL};
  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "llllll", kwlist, &self->order.side,
          &self->order.quantity, &self->order.ord_type, &self->order.price,
          &self->order.order_id, &self->order.timestamp))
    return NULL;

  return (PyObject *)self;
}

static PyTypeObject mePyOrderExecutedMessageType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name =
        "me.OrderExecutedMessage",
    .tp_doc =
        PyDoc_STR("Order executed message, informs an order totally executed."),
    .tp_basicsize = sizeof(MePyOrderExecutedMessage),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = mePyOrderExecutedMessage_new,
    .tp_dealloc = (destructor)mePyOrderExecutedMessage_dealloc,
};

/* Set Market Price. */
typedef struct {
  MePyMessage message_obj;
  int64_t set_market_price;
} MePySetMarketPriceMessage;

static void mePySetMarketPriceMessage_dealloc(MePySetMarketPriceMessage *self) {
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *mePySetMarketPriceMessage_new(PyTypeObject *type,
                                               PyObject *args, PyObject *kwds) {
  MePySetMarketPriceMessage *self;
  self = (MePySetMarketPriceMessage *)type->tp_alloc(type, 0);
  if (self == NULL) return NULL;

  static char *kwlist[] = {"price", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "l", kwlist,
                                   &self->set_market_price))
    return NULL;

  return (PyObject *)self;
}

static PyTypeObject mePySetMarketPriceMessageType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name =
        "me.SetMarketPriceMessage",
    .tp_doc = PyDoc_STR("Set market price message."),
    .tp_basicsize = sizeof(MePySetMarketPriceMessage),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = mePySetMarketPriceMessage_new,
    .tp_dealloc = (destructor)mePySetMarketPriceMessage_dealloc,
};

/* Trade. */
typedef struct {
  MePyMessage message_obj;
  MeOrderID matched_id;
  MeOrder order;
} MePyTradeMessage;

static void mePyTradeMessage_dealloc(MePyTradeMessage *self) {
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *mePyTradeMessage_new(PyTypeObject *type, PyObject *args,
                                      PyObject *kwds) {
  MePyTradeMessage *self;
  self = (MePyTradeMessage *)type->tp_alloc(type, 0);
  if (self == NULL) return NULL;

  static char *kwlist[] = {"side", "quantity",  "ord_type",   "price",
                           "id",   "timestamp", "matched_id", NULL};
  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "lllllll", kwlist, &self->order.side,
          &self->order.quantity, &self->order.ord_type, &self->order.price,
          &self->order.order_id, &self->order.timestamp, &self->matched_id))
    return NULL;

  return (PyObject *)self;
}

static PyTypeObject mePyTradeMessageType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "me.TradeMessage",
    .tp_doc = PyDoc_STR("Trade message, informs trade events."),
    .tp_basicsize = sizeof(MePyTradeMessage),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = mePyTradeMessage_new,
    .tp_dealloc = (destructor)mePyTradeMessage_dealloc,
};

/* Cancel. */
typedef struct {
  MePyMessage message_obj;
  MeOrderID id;
} MePyCancelOrderMessage;

static void mePyCancelOrderMessage_dealloc(MePyCancelOrderMessage *self) {
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *mePyCancelOrderMessage_new(PyTypeObject *type, PyObject *args,
                                            PyObject *kwds) {
  MePyCancelOrderMessage *self;
  self = (MePyCancelOrderMessage *)type->tp_alloc(type, 0);
  if (self == NULL) return NULL;

  static char *kwlist[] = {"id", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "l", kwlist, &self->id))
    return NULL;

  return (PyObject *)self;
}

static PyTypeObject mePyCancelOrderMessageType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "me.CancelOrderMessage",
    .tp_doc = PyDoc_STR("Cancel order message."),
    .tp_basicsize = sizeof(MePyCancelOrderMessage),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = mePyCancelOrderMessage_new,
    .tp_dealloc = (destructor)mePyCancelOrderMessage_dealloc,
};

/* Panic. */
typedef struct {
  MePyMessage message_obj;
} MePyPanicMessage;

static void mePyPanicMessage_dealloc(MePyPanicMessage *self) {
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *mePyPanicMessage_new(PyTypeObject *type, PyObject *args,
                                      PyObject *kwds) {
  (void)args;
  (void)kwds;
  MePyPanicMessage *self;
  self = (MePyPanicMessage *)type->tp_alloc(type, 0);
  return (PyObject *)self;
}

static PyTypeObject mePyPanicMessageType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "me.PanicMessage",
    .tp_doc = PyDoc_STR("Panic (engine shutdown) message."),
    .tp_basicsize = sizeof(MePyPanicMessage),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = mePyPanicMessage_new,
    .tp_dealloc = (destructor)mePyPanicMessage_dealloc,
};

/*
 * Client context type.
 */

typedef struct {
  PyObject_HEAD MeClientContext context;
} MePyClientContext;

static void mePyClientContext_dealloc(MePyClientContext *self) {
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *mePyClientContext_new(PyTypeObject *type, PyObject *args,
                                       PyObject *kwds) {
  (void)args;
  (void)kwds;
  MePyClientContext *self;
  self = (MePyClientContext *)type->tp_alloc(type, 0);
  if (self == NULL) return NULL;
  if (me_client_init_context(&self->context)) {
    PyErr_SetString(meErrorOpenPosixQueue,
                    "Couldn't open POSIX queues. Is the engine running?");
    return NULL;
  }
  return (PyObject *)self;
}

static PyObject *mePyClientContext_sendmsg(MePyClientContext *self,
                                           PyObject *args) {
  MePyMessage *message;
  MeMessage to_send;
  uint64_t security_id;

  if (!PyArg_ParseTuple(args, "lO", &security_id, (PyObject *)&message))
    return NULL;

  to_send.security_id = security_id;
  PyTypeObject *type = Py_TYPE(message);
  if (type == &mePyOrderMessageType) {
    to_send.message.order = ((MePyOrderMessage *)message)->order;
    to_send.msg_type = ME_MESSAGE_NEW_ORDER;
  } else if (type == &mePySetMarketPriceMessageType) {
    to_send.message.set_market_price =
        ((MePySetMarketPriceMessage *)message)->set_market_price;
    to_send.msg_type = ME_MESSAGE_SET_MARKET_PRICE;
  } else if (type == &mePyTradeMessageType) {
    to_send.message.trade.aggressor = ((MePyTradeMessage *)message)->order;
    to_send.message.trade.matched_id =
        ((MePyTradeMessage *)message)->matched_id;
    to_send.msg_type = ME_MESSAGE_TRADE;
  } else if (type == &mePyCancelOrderMessageType) {
    to_send.message.to_cancel = ((MePyCancelOrderMessage *)message)->id;
    to_send.msg_type = ME_MESSAGE_CANCEL_ORDER;
  }

  if (me_client_send_message(&self->context, &to_send)) {
    PyErr_SetString(meErrorPosixQueue,
                    "Writing to POSIX message queue failed.");
    return NULL;
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *mePyClientContext_getmsg(MePyClientContext *self,
                                          PyObject *Py_UNUSED(ignored)) {
  MeMessage cmsg;
  PyObject *message = NULL;

  if (me_client_get_message(&self->context, &cmsg)) {
    PyErr_SetString(meErrorPosixQueue,
                    "Reading from POSIX message queue failed.");
    return NULL;
  }

  switch (cmsg.msg_type) {
    case ME_MESSAGE_NEW_ORDER:
      message =
          (PyObject *)mePyOrderMessageType.tp_alloc(&mePyOrderMessageType, 0);
      if (message == NULL) return NULL;
      ((MePyOrderMessage *)message)->order = cmsg.message.order;
      break;
    case ME_MESSAGE_CANCEL_ORDER:
      message = (PyObject *)mePyCancelOrderMessageType.tp_alloc(
          &mePyCancelOrderMessageType, 0);
      if (message == NULL) return NULL;
      ((MePyCancelOrderMessage *)message)->id = cmsg.message.to_cancel;
      break;
    case ME_MESSAGE_SET_MARKET_PRICE:
      message = (PyObject *)mePySetMarketPriceMessageType.tp_alloc(
          &mePySetMarketPriceMessageType, 0);
      if (message == NULL) return NULL;
      ((MePySetMarketPriceMessage *)message)->set_market_price =
          cmsg.message.set_market_price;
      break;
    case ME_MESSAGE_TRADE:
      message =
          (PyObject *)mePyTradeMessageType.tp_alloc(&mePyTradeMessageType, 0);
      if (message == NULL) return NULL;
      ((MePyTradeMessage *)message)->order = cmsg.message.trade.aggressor;
      ((MePyTradeMessage *)message)->matched_id = cmsg.message.trade.matched_id;
      break;
    case ME_MESSAGE_ORDER_EXECUTED:
      message = (PyObject *)mePyOrderExecutedMessageType.tp_alloc(
          &mePyOrderExecutedMessageType, 0);
      if (message == NULL) return NULL;
      ((MePyOrderExecutedMessage *)message)->order = cmsg.message.order;
      break;
    case ME_MESSAGE_PANIC:
      message =
          (PyObject *)mePyPanicMessageType.tp_alloc(&mePyPanicMessageType, 0);
      if (message == NULL) return NULL;
  }

  ((MePyMessage *)message)->security_id = cmsg.security_id;

  return message;
}

static PyMethodDef mePyClientContextMethods[] = {
    {"sendMessage", (PyCFunction)mePyClientContext_sendmsg,
     METH_VARARGS | METH_KEYWORDS, "Sends a message to the engine."},
    {"getMessage", (PyCFunction)mePyClientContext_getmsg, METH_NOARGS,
     "Gets a message from the engine."},
    {NULL} /* Sentinel */
};

static PyTypeObject mePyClientContextType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "me.ClientContext",
    .tp_doc = PyDoc_STR("Client context."),
    .tp_basicsize = sizeof(MePyClientContext),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = mePyClientContext_new,
    .tp_dealloc = (destructor)mePyClientContext_dealloc,
    .tp_methods = mePyClientContextMethods,
};

/*
 * Module.
 */

static struct PyModuleDef memodule = {
    PyModuleDef_HEAD_INIT,
    "me",
    DOCS,
    -1,
};

PyMODINIT_FUNC PyInit_me(void) {
  PyObject *m;
  /* Types testing. */
  if (PyType_Ready(&mePyClientContextType) < 0) return NULL;
  if (PyType_Ready(&mePyMessageType) < 0) return NULL;
  if (PyType_Ready(&mePyOrderMessageType) < 0) return NULL;
  if (PyType_Ready(&mePyOrderExecutedMessageType) < 0) return NULL;
  if (PyType_Ready(&mePySetMarketPriceMessageType) < 0) return NULL;
  if (PyType_Ready(&mePyTradeMessageType) < 0) return NULL;
  if (PyType_Ready(&mePyCancelOrderMessageType) < 0) return NULL;
  if (PyType_Ready(&mePyPanicMessageType) < 0) return NULL;

  /* Module creation. */
  m = PyModule_Create(&memodule);
  if (m == NULL) return NULL;

  /* Add classes. */
  Py_INCREF(&mePyClientContextType);
  if (PyModule_AddObject(m, "ClientContext",
                         (PyObject *)&mePyClientContextType) < 0) {
    Py_DECREF(&mePyClientContextType);
    Py_DECREF(m);
    return NULL;
  }
  Py_INCREF(&mePyMessageType);
  if (PyModule_AddObject(m, "Message", (PyObject *)&mePyMessageType) < 0) {
    Py_DECREF(&mePyMessageType);
    Py_DECREF(m);
    return NULL;
  }
  Py_INCREF(&mePyOrderMessageType);
  if (PyModule_AddObject(m, "OrderMessage", (PyObject *)&mePyOrderMessageType) <
      0) {
    Py_DECREF(&mePyOrderMessageType);
    Py_DECREF(m);
    return NULL;
  }
  Py_INCREF(&mePyOrderExecutedMessageType);
  if (PyModule_AddObject(m, "OrderExecutedMessage",
                         (PyObject *)&mePyOrderExecutedMessageType) < 0) {
    Py_DECREF(&mePyOrderExecutedMessageType);
    Py_DECREF(m);
    return NULL;
  }
  Py_INCREF(&mePySetMarketPriceMessageType);
  if (PyModule_AddObject(m, "SetMarketPriceMessage",
                         (PyObject *)&mePySetMarketPriceMessageType) < 0) {
    Py_DECREF(&mePySetMarketPriceMessageType);
    Py_DECREF(m);
    return NULL;
  }
  Py_INCREF(&mePyTradeMessageType);
  if (PyModule_AddObject(m, "TradeMessage", (PyObject *)&mePyTradeMessageType) <
      0) {
    Py_DECREF(&mePyTradeMessageType);
    Py_DECREF(m);
    return NULL;
  }
  Py_INCREF(&mePyCancelOrderMessageType);
  if (PyModule_AddObject(m, "CancelOrderMessage",
                         (PyObject *)&mePyCancelOrderMessageType) < 0) {
    Py_DECREF(&mePyCancelOrderMessageType);
    Py_DECREF(m);
    return NULL;
  }
  Py_INCREF(&mePyPanicMessageType);
  if (PyModule_AddObject(m, "PanicMessage", (PyObject *)&mePyPanicMessageType) <
      0) {
    Py_DECREF(&mePyPanicMessageType);
    Py_DECREF(m);
    return NULL;
  }

  /* Add error types. */
  meErrorPosixQueue = PyErr_NewException("me.posix_queue_error", NULL, NULL);
  if (PyModule_AddObject(m, "posix_queue_error", meErrorPosixQueue) < 0) {
    Py_XDECREF(meErrorPosixQueue);
    Py_CLEAR(meErrorPosixQueue);
    Py_DECREF(m);
    return NULL;
  }
  meErrorOpenPosixQueue =
      PyErr_NewException("me.open_posix_queue_error", NULL, NULL);
  if (PyModule_AddObject(m, "open_posix_queue_error", meErrorOpenPosixQueue) <
      0) {
    Py_XDECREF(meErrorOpenPosixQueue);
    Py_CLEAR(meErrorOpenPosixQueue);
    Py_DECREF(m);
    return NULL;
  }

  /* Order types. */
  PyModule_AddIntConstant(m, "ME_ORDER_MARKET", ME_ORDER_MARKET);
  PyModule_AddIntConstant(m, "ME_ORDER_LIMIT", ME_ORDER_LIMIT);

  /* Sides. */
  PyModule_AddIntConstant(m, "ME_SIDE_BUY", ME_SIDE_BUY);
  PyModule_AddIntConstant(m, "ME_SIDE_SELL", ME_SIDE_SELL);

  /* Message types. */
  PyModule_AddIntConstant(m, "ME_MESSAGE_NEW_ORDER", ME_MESSAGE_NEW_ORDER);
  PyModule_AddIntConstant(m, "ME_MESSAGE_CANCEL_ORDER",
                          ME_MESSAGE_CANCEL_ORDER);
  PyModule_AddIntConstant(m, "ME_MESSAGE_SET_MARKET_PRICE",
                          ME_MESSAGE_SET_MARKET_PRICE);
  PyModule_AddIntConstant(m, "ME_MESSAGE_TRADE", ME_MESSAGE_TRADE);
  PyModule_AddIntConstant(m, "ME_MESSAGE_ORDER_EXECUTED",
                          ME_MESSAGE_ORDER_EXECUTED);
  PyModule_AddIntConstant(m, "ME_MESSAGE_PANIC", ME_MESSAGE_PANIC);

  return m;
}
