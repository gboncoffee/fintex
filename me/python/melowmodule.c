#define PY_SSIZE_T_CLEAN
#include <python3.12/Python.h>
#include <stdlib.h>

#include "../me.h"

#define DOCS                                                                  \
  "This module implements low level bindings to the FinTEx matching engine. " \
  "Contexts uses tuples to easily interact with the POSIX message queues. "   \
  "For an easier object-oriented API written in Python, see `me.py`"

/*
 * Error handling.
 */
static PyObject *meErrorPosixQueue;
static PyObject *meErrorOpenPosixQueue;

/*
 * Client context type.
 */

typedef struct {
  PyObject_HEAD MeClientContext context;
} MePyClientContext;

static void mePyClientContext_dealloc(MePyClientContext *self) {
  Py_TYPE(self)->tp_free((PyObject *)self);
}

/* We must use (void) instead of the unused macro as the macro changes the
 * function signature. */
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
  MeMessage to_send;
  PyObject *item;
  int dumb_bool;

  /* Parse first argument of the tuple to get message type. Uses dumb_bool to
   * just ignore the value of the tuple passed as the second argument. */
  if (!PyArg_ParseTuple(args, "Ip", &to_send.msg_type, &dumb_bool)) {
    PyErr_SetString(PyExc_TypeError, "Unknown message type.");
    return NULL;
  }

  /* Parse the entire tuple. */
  switch (to_send.msg_type) {
    case ME_MESSAGE_PANIC:
      break;
    case ME_MESSAGE_CANCEL_ORDER:
      if (!PyArg_ParseTuple(args, "I(lL)", &to_send.msg_type,
                            &to_send.security_id, &to_send.message.to_cancel)) {
        PyErr_SetString(PyExc_AttributeError,
                        "Cannot parse arguments as cancel message.");
        return NULL;
      }
      break;
    case ME_MESSAGE_SET_MARKET_PRICE:
      if (!PyArg_ParseTuple(args, "I(ll)", &to_send.msg_type,
                            &to_send.security_id,
                            &to_send.message.set_market_price)) {
        PyErr_SetString(PyExc_TypeError,
                        "Cannot parse arguments as set market price message.");
        return NULL;
      }
      break;
    case ME_MESSAGE_TRADE:
      if (!PyArg_ParseTuple(args, "I(lIlIlLLL)", &to_send.msg_type,
                            &to_send.security_id,
                            &to_send.message.trade.aggressor.side,
                            &to_send.message.trade.aggressor.quantity,
                            &to_send.message.trade.aggressor.ord_type,
                            &to_send.message.trade.aggressor.price,
                            &to_send.message.trade.aggressor.order_id,
                            &to_send.message.trade.aggressor.timestamp,
                            &to_send.message.trade.matched_id)) {
        PyErr_SetString(PyExc_AttributeError,
                        "Cannot parse Arguments as trade message.");
        return NULL;
      }
      break;
    case ME_MESSAGE_NEW_ORDER:
    case ME_MESSAGE_ORDER_EXECUTED:
      if (!PyArg_ParseTuple(
              args, "I(lIlIlLL)", &to_send.msg_type, &to_send.security_id,
              &to_send.message.order.side, &to_send.message.order.quantity,
              &to_send.message.order.ord_type, &to_send.message.order.price,
              &to_send.message.order.order_id,
              &to_send.message.order.timestamp)) {
        PyErr_SetString(PyExc_AttributeError,
                        "Cannot parse Arguments as order message.");
        return NULL;
      }
      break;
    default:
      PyErr_SetString(PyExc_AttributeError, "Unknown message type.");
      return NULL;
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
  MeMessage msg;
  PyObject *tuple;

  if (me_client_get_message(&self->context, &msg)) {
    PyErr_SetString(meErrorPosixQueue,
                    "Reading from POSIX message queue failed.");
    return NULL;
  }

  switch (msg.msg_type) {
    case ME_MESSAGE_PANIC:
      tuple =
          Py_BuildValue("(I())", msg.msg_type, msg.security_id, msg.msg_type);
      break;
    case ME_MESSAGE_ORDER_EXECUTED:
    case ME_MESSAGE_NEW_ORDER:
      tuple = Py_BuildValue("(I(lIlIlLL))", msg.msg_type, msg.security_id,
                            msg.message.order.side, msg.message.order.quantity,
                            msg.message.order.ord_type, msg.message.order.price,
                            msg.message.order.order_id,
                            msg.message.order.timestamp);
      break;
    case ME_MESSAGE_TRADE:
      tuple = Py_BuildValue("(I(lIlIlLLL))", msg.msg_type, msg.security_id,
                            msg.message.trade.aggressor.side,
                            msg.message.trade.aggressor.quantity,
                            msg.message.trade.aggressor.ord_type,
                            msg.message.trade.aggressor.price,
                            msg.message.trade.aggressor.order_id,
                            msg.message.trade.aggressor.timestamp,
                            msg.message.trade.matched_id);
      break;
    case ME_MESSAGE_CANCEL_ORDER:
      tuple = Py_BuildValue("I(lL)", msg.msg_type, msg.security_id,
                            msg.message.to_cancel);
      break;
    case ME_MESSAGE_SET_MARKET_PRICE:
      tuple = Py_BuildValue("I(ll)", msg.msg_type, msg.security_id,
                            msg.message.set_market_price);
      break;
    default:
      PyErr_SetString(PyExc_ValueError,
                      "Received unknown message type from the engine.");
      return NULL;
  }

  return tuple;
}

static PyMethodDef mePyClientContextMethods[] = {
    {"sendMessage", (PyCFunction)mePyClientContext_sendmsg, METH_VARARGS,
     "Sends a message to the engine."},
    {"getMessage", (PyCFunction)mePyClientContext_getmsg, METH_NOARGS,
     "Gets a message from the engine."},
    {NULL} /* Sentinel */
};

static PyTypeObject mePyClientContextType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "melow.ClientContext",
    .tp_doc = PyDoc_STR("Client context."),
    .tp_basicsize = sizeof(MePyClientContext),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = mePyClientContext_new,
    .tp_dealloc = (destructor)mePyClientContext_dealloc,
    .tp_methods = mePyClientContextMethods,
};

/*
 * Engine context type.
 */

typedef struct {
  PyObject_HEAD MeContext *context;
} MePyContext;

static void mePyContext_dealloc(MePyContext *self) {
  me_dealloc_context(self->context, free);
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *mePyContext_new(PyTypeObject *type, PyObject *args,
                                 PyObject *kwds) {
  MePyContext *self;
  self = (MePyContext *)type->tp_alloc(type, 0);
  if (self == NULL) return NULL;

  size_t l2size;
  size_t securities;

  static char *kwlist[] = {"l2size", "securities", NULL};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "ll", kwlist, &l2size,
                                   &securities))
    return NULL;

  self->context = me_alloc_context(l2size, securities, malloc);

  return (PyObject *)self;
}

static PyObject *mePyContext_run(MePyContext *self, PyObject *args) {
  (void)args;

  me_run(self->context, NULL, NULL);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef mePyContextMethods[] = {
    {"run", (PyCFunction)mePyContext_run, METH_NOARGS, "Runs the engine."},
    {NULL},
};

static PyTypeObject mePyContextType = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0).tp_name = "melow.Context",
    .tp_doc = PyDoc_STR("Engine context."),
    .tp_basicsize = sizeof(MePyContext),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = mePyContext_new,
    .tp_dealloc = (destructor)mePyContext_dealloc,
    .tp_methods = mePyContextMethods,
};

/*
 * Module.
 */

static struct PyModuleDef memodule = {
    PyModuleDef_HEAD_INIT,
    "melow",
    DOCS,
    -1,
};

PyMODINIT_FUNC PyInit_melow(void) {
  PyObject *m;
  /* Types testing. */
  if (PyType_Ready(&mePyClientContextType) < 0) return NULL;
  if (PyType_Ready(&mePyContextType) < 0) return NULL;

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
  Py_INCREF(&mePyContextType);
  if (PyModule_AddObject(m, "Context", (PyObject *)&mePyContextType) < 0) {
    Py_DECREF(&mePyContextType);
    Py_DECREF(m);
    return NULL;
  }

  /* Add error types. */
  meErrorPosixQueue = PyErr_NewException("melow.posix_queue_error", NULL, NULL);
  if (PyModule_AddObject(m, "posix_queue_error", meErrorPosixQueue) < 0) {
    Py_XDECREF(meErrorPosixQueue);
    Py_CLEAR(meErrorPosixQueue);
    Py_DECREF(m);
    return NULL;
  }
  meErrorOpenPosixQueue =
      PyErr_NewException("melow.open_posix_queue_error", NULL, NULL);
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

  /* Usefull constants. */
  PyModule_AddIntConstant(m, "ME_DEFAULT_CACHE_SIZE", 1610612736);
  PyModule_AddIntConstant(m, "ME_DEFAULT_SECURITIES_NUMBER", 400);

  return m;
}
