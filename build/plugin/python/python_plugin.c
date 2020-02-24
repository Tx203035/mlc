#include <Python.h>
#include "python_plugin.h"
#include "mlc/cycle.h"
#include "mlc/session.h"

static int py_on_recv(session_t *s, const char *data, uint32_t len);
static int py_on_status(session_t *s);
static int py_on_connect(session_t *s, void *data);


static PyObject *py_cycle_step(CycleObject *self, PyObject *args)
{
    if (self->cycle == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "cycle is null");
        return NULL;
    }

    if (self->cycle->is_server > 0 && self->on_connect == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "server cycle must set handler before step");
        return NULL;
    }

    int ret = -1;
    int wait_ms = 1;
    if (!PyArg_ParseTuple(args, "i", &wait_ms))
    {
        PyErr_SetString(PyExc_RuntimeError, "cycle_step parse para error");
        return NULL;
    }

    ret = cycle_step(self->cycle, wait_ms);

    return PyLong_FromLong((long)ret);
}

static PyObject *py_cycle_process(CycleObject *self, PyObject *args)
{
    if (self->cycle == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "cycle is null");
        return NULL;
    }

    if (self->cycle->is_server > 0 && self->on_connect == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "server cycle must set handler before process");
        return NULL;
    }

    int ret = -1;
    int wait_ms = 1;
    if (!PyArg_ParseTuple(args, "i", &wait_ms))
    {
        PyErr_SetString(PyExc_RuntimeError, "cycle_step parse para error");
        return NULL;
    }

    ret = cycle_process(self->cycle, wait_ms);

    return PyLong_FromLong((long)ret);
}

static PyObject *py_cycle_set_handler(CycleObject *self, PyObject *args, PyObject *kwargs)
{
    if (self->cycle == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "cycle is null");
        return NULL;
    }

    if (self->cycle->is_server <= 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "only server cycle can set handler");
        return NULL;
    }

    static char *kwlist[] = {"on_connect", "on_recv", "on_status", NULL};
    PyObject *on_connect = NULL;
    PyObject *on_recv = NULL;
    PyObject *on_status = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOO", kwlist, &on_connect, &on_recv, &on_status))
    {
        printf("ERROR");
        return NULL;
    }

    if (!PyCallable_Check(on_connect) || !PyCallable_Check(on_recv) || !PyCallable_Check(on_status) )
    {
        PyErr_SetString(PyExc_RuntimeError, "set handler para must be function");
        return NULL;
    }

    Py_INCREF(on_connect);
    Py_INCREF(on_recv);
    Py_INCREF(on_status);

    self->on_connect = on_connect;
    self->on_recv = on_recv;
    self->on_status = on_status;

    session_listener_set_handler(self->cycle->sl, self, py_on_connect, py_on_recv, py_on_status);

    Py_INCREF(self);
    return (PyObject *)self;
}

static PyMethodDef Cycle_methods[] =
{
    {"step", (PyCFunction)py_cycle_step, METH_VARARGS, "cycle step"},
    {"process", (PyCFunction)py_cycle_process, METH_VARARGS, "cycle process"},
    {"set_handler", (PyCFunction)py_cycle_set_handler, METH_VARARGS | METH_KEYWORDS, "client cycle set handler"},
    {NULL, NULL}
};

static void CycleObject_dealloc(CycleObject *self)
{
    if (self->cycle != NULL)
    {
        //TODEL
        self->cycle = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PyTypeObject CycleObject_Type =
{
    PyVarObject_HEAD_INIT(NULL, 0) "mlc_py.cycle", /*tp_name*/
    sizeof(CycleObject),                           /*tp_basicsize*/
    0,                                             /*tp_itemsize*/
    (destructor)CycleObject_dealloc,               /*tp_dealloc*/
    0,                                             /*tp_print*/
    0,                                             /*tp_getattr*/
    0,                                             /*tp_setattr*/
    0,                                             /*tp_compare*/
    0,                                             /*tp_repr*/
    0,                                             /*tp_as_number*/
    0,                                             /*tp_as_sequence*/
    0,                                             /*tp_as_mapping*/
    0,                                             /*tp_hash*/
    0,                                             /*tp_call*/
    0,                                             /*tp_str*/
    0,                                             /*tp_getattro*/
    0,                                             /*tp_setattro*/
    0,                                             /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,      /*tp_flags*/
    "cycle object",                                /*tp_doc*/
    0,                                             /*tp_traverse*/
    0,                                             /*tp_clear*/
    0,                                             /*tp_richcompare*/
    0,                                             /*tp_weaklistoffset*/
    0,                                             /*tp_iter*/
    0,                                             /*tp_iternext*/
    Cycle_methods,                                 /*tp_methods*/
    0,                                             /*tp_members*/
    0,                                             /*tp_getset*/
    0,                                             /*tp_base*/
    0,                                             /*tp_dict*/
    0,                                             /*tp_descr_get*/
    0,                                             /*tp_descr_set*/
    0,                                             /*tp_dictoffset*/
    0,                                             /*tp_init*/
    PyType_GenericAlloc,                           /*tp_alloc*/
    PyType_GenericNew,                             /*tp_new*/
    PyObject_Del,                                  /*tp_free*/
    0,                                             /*tp_is_gc*/
};

static void SessionObject_dealloc(SessionObject *self)
{
    if (self->session != NULL)
    {
        //TODEL
        self->session = NULL;
    }

    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *py_session_send(SessionObject *self, PyObject *args)
{
    if (self->session == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "session is null");
        return NULL;
    }

    PyObject *data = PyTuple_GetItem(args, 0);

    int ret = -1;
    if (PyUnicode_Check(data) || PyBytes_Check(data))
    {
        PyObject *bstr = PyUnicode_AsEncodedString(data, "utf-8", NULL);
        PyErr_Clear();

        Py_ssize_t len;
        char *s;
        PyBytes_AsStringAndSize(bstr ? bstr : data, &s, &len);
        if (bstr)
        {
            Py_DECREF(bstr);
        }

        ret = session_data_send(self->session, s, len);
    }
    else
    {
        PyErr_SetString(PyExc_RuntimeError, "send data must be bytes or str");
        return NULL;
    }

    return PyLong_FromLong((long)ret);
}

static PyObject *py_session_connect(SessionObject *self)
{
    if (self->session == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "session is null");
        return NULL;
    }

    int ret = session_connect(self->session);
    return PyLong_FromLong((long)ret);
}

static PyObject *py_session_set_handler(SessionObject *self, PyObject *args, PyObject *kwargs)
{
    if (self->session == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "session is null");
        return NULL;
    }

    if (self->session->cycle->is_server > 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "only client session can set handler");
        return NULL;
    }

    static char *kwlist[] = {"on_recv", "on_status", NULL};
    PyObject *on_recv = NULL;
    PyObject *on_status = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kwlist, &on_recv, &on_status))
    {
        printf("ERROR");
        return NULL;
    }

    if (!PyCallable_Check(on_recv) || !PyCallable_Check(on_status) )
    {
        PyErr_SetString(PyExc_RuntimeError, "set handler para must be function");
        return NULL;
    }

    Py_INCREF(on_recv);
    Py_INCREF(on_status);

    self->on_recv = on_recv;
    self->on_status = on_status;

    session_set_receive_function(self->session, py_on_recv);
    session_set_status_change_function(self->session, py_on_status);

    Py_INCREF(self);
    return (PyObject *)self;
}

static PyMethodDef Session_methods[] =
{
    {"send", (PyCFunction)py_session_send, METH_VARARGS, "session send data"},
    {"connect", (PyCFunction)py_session_connect, METH_NOARGS, "client session connect"},
    {"set_handler", (PyCFunction)py_session_set_handler, METH_VARARGS | METH_KEYWORDS, "client session set_handler"},
    {NULL, NULL}
};

PyTypeObject SessionObject_Type =
{
    PyVarObject_HEAD_INIT(NULL, 0) "mlc_py.session", /*tp_name*/
    sizeof(SessionObject),                           /*tp_basicsize*/
    0,                                               /*tp_itemsize*/
    (destructor)SessionObject_dealloc,               /*tp_dealloc*/
    0,                                               /*tp_print*/
    0,                                               /*tp_getattr*/
    0,                                               /*tp_setattr*/
    0,                                               /*tp_compare*/
    0,                                               /*tp_repr*/
    0,                                               /*tp_as_number*/
    0,                                               /*tp_as_sequence*/
    0,                                               /*tp_as_mapping*/
    0,                                               /*tp_hash*/
    0,                                               /*tp_call*/
    0,                                               /*tp_str*/
    0,                                               /*tp_getattro*/
    0,                                               /*tp_setattro*/
    0,                                               /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "session object",                                /*tp_doc*/
    0,                                               /*tp_traverse*/
    0,                                               /*tp_clear*/
    0,                                               /*tp_richcompare*/
    0,                                               /*tp_weaklistoffset*/
    0,                                               /*tp_iter*/
    0,                                               /*tp_iternext*/
    Session_methods,                                 /*tp_methods*/
    0,                                               /*tp_members*/
    0,                                               /*tp_getset*/
    0,                                               /*tp_base*/
    0,                                               /*tp_dict*/
    0,                                               /*tp_descr_get*/
    0,                                               /*tp_descr_set*/
    0,                                               /*tp_dictoffset*/
    0,                                               /*tp_init*/
    PyType_GenericAlloc,                             /*tp_alloc*/
    PyType_GenericNew,                               /*tp_new*/
    PyObject_Del,                                    /*tp_free*/
    0,                                               /*tp_is_gc*/
};

static PyObject *py_cycle_create_client(PyObject * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = {"port", "max_connection", NULL};

    cycle_conf_t conf = {0,};
    conf.connection_n = 10;
    conf.addr.port = 8888;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ii", kwlist, &conf.port, &conf.connection_n))
    {
        printf("ERROR");
        return NULL;
    }
    cycle_t *cycle = cycle_create(&conf, 0);
    assert(cycle);
    CycleObject *obj = PyObject_New(CycleObject, &CycleObject_Type);
    obj->cycle = cycle;
    obj->on_connect = NULL;
    obj->on_recv = NULL;
    obj->on_status = NULL;
    return (PyObject *)obj;
}

static PyObject *py_cycle_create_server(PyObject * self, PyObject * args, PyObject * kwargs)
{
    static char *kwlist[] = {"port", "max_connection", NULL};

    cycle_conf_t conf = {0,};
    conf.connection_n = 10;
    conf.addr.port = 8888;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ii", kwlist, &conf.port, &conf.connection_n))
    {
        printf("ERROR");
        return NULL;
    }

    cycle_t *cycle = cycle_create(&conf, 1);
    assert(cycle);

    CycleObject *obj = PyObject_New(CycleObject, &CycleObject_Type);
    obj->cycle = cycle;
    obj->on_connect = NULL;
    obj->on_recv = NULL;
    obj->on_status = NULL;
    cycle->sl->data = obj;

    return (PyObject *)obj;
}

static PyObject *py_session_create_client(PyObject *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"cycle", "ip_addr", "port", NULL};
    mlc_addr_conf_t conf = {0,};
    PyObject *obj = NULL;
    PyObject *ip_addr = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOi", kwlist, &obj, &ip_addr, &conf.port))
    {
        printf("ERROR");
        return NULL;
    }

    if (PyUnicode_Check(ip_addr) || PyBytes_Check(ip_addr))
    {
        PyObject *bstr = PyUnicode_AsEncodedString(ip_addr, "utf-8", NULL);
        PyErr_Clear();

        Py_ssize_t len;
        char *s;
        PyBytes_AsStringAndSize(bstr ? bstr : ip_addr, &s, &len);
        if (bstr)
        {
            Py_DECREF(bstr);
        }

        if (len > 64)
        {
            PyErr_SetString(PyExc_RuntimeError, "ip addr len must less than 16");
            return NULL;
        }

        memcpy(conf.addr, s, len);
    }
    else
    {
        PyErr_SetString(PyExc_RuntimeError, "ip addr must be str");
        return NULL;
    }

    if (!PyCycle_Check(obj))
    {
        PyErr_SetString(PyExc_RuntimeError, "cycle para must be a cycle object");
        return NULL;
    }

    CycleObject *cycle_obj = (CycleObject *)obj;
    if (cycle_obj->cycle == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "cycle is NULL must be a cycle object");
        return NULL;
    }
    //Py_INCREF(obj);

    session_t *session = session_create_client(cycle_obj->cycle, &conf);

    SessionObject *session_obj = PyObject_New(SessionObject, &SessionObject_Type);
    session_obj->session = session;
    session_obj->on_recv = NULL;
    session_obj->on_status = NULL;
    session->data = session_obj;

    return (PyObject *)session_obj;
}

static PyMethodDef mlc_methods[] =
{
    {"cycle_create_client", (PyCFunction)py_cycle_create_client, METH_VARARGS | METH_KEYWORDS, "create new cycle for client"},
    {"cycle_create_server", (PyCFunction)py_cycle_create_server, METH_VARARGS | METH_KEYWORDS, "create new cycle for server"},
    {"session_create_client", (PyCFunction)py_session_create_client, METH_VARARGS | METH_KEYWORDS, "create new session for client"},
    {NULL, NULL}
};

static int py_on_recv(session_t *s, const char *data, uint32_t len)
{
    SessionObject * obj = (SessionObject *)s->data;
    if (obj== NULL || obj->session != s)
    {
        printf("session[%p] on recv, but no session obj\n", s);
        return -1;
    }
    
    int nRet = -1;
    if (obj->on_recv && PyCallable_Check(obj->on_recv))
    {
        PyObject *pRet = NULL;
        PyObject *pArgs = Py_BuildValue("Os#", obj, data, len);
        pRet = PyEval_CallObject(obj->on_recv, pArgs);
        if (pRet)
        {
            nRet = PyLong_AsLong(pRet);
        }
    }

    return nRet;
}

static int py_on_status(session_t *s,uint8_t state)
{
    SessionObject * obj = (SessionObject *)s->data;
    if (obj== NULL || obj->session != s)
    {
        printf("session[%p] on status, but no session obj\n", s);
        return -1;
    }
    
    int nRet = -1;
    if (obj->on_status && PyCallable_Check(obj->on_status))
    {
        PyObject *pRet = NULL;
        PyObject *pArgs = Py_BuildValue("Oi", obj, s->state);
        pRet = PyEval_CallObject(obj->on_status, pArgs);
        if (pRet)
        {
            nRet = PyLong_AsLong(pRet);
        }
    }

    return nRet;
}

static int py_on_connect(session_t *s, void *data)
{
    CycleObject *cycle_obj = (CycleObject *)data;

    int nRet = -1;
    if (cycle_obj->cycle != NULL && cycle_obj->on_connect != NULL)
    {
        SessionObject *session_obj = PyObject_New(SessionObject, &SessionObject_Type);
        session_obj->session = s;
        Py_INCREF(cycle_obj->on_recv);
        Py_INCREF(cycle_obj->on_status);
        session_obj->on_recv = cycle_obj->on_recv;
        session_obj->on_status = cycle_obj->on_status;
        s->data = session_obj;

        PyObject *pRet = NULL;
        PyObject *pArgs = Py_BuildValue("O", session_obj);
        pRet = PyEval_CallObject(cycle_obj->on_connect, pArgs);
        if (pRet)
        {
            nRet = PyLong_AsLong(pRet);
        }
    }

    return nRet;
}

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef mlc_module =
    {
        PyModuleDef_HEAD_INIT,
        "mlc_py",
        "MLC network library",
        -1,
        mlc_methods};
#endif

PyMODINIT_FUNC PyInit_MLC(void)
{
    PyObject *m;
    if (PyType_Ready(&CycleObject_Type) < 0 ||
        PyType_Ready(&SessionObject_Type) < 0 ||
#if PY_MAJOR_VERSION >= 3
        (m = PyModule_Create(&mlc_module)) == NULL)
        return NULL;
#else
        (m = Py_InitModule3("mlc_py", mlc_methods, "MLC network library")) == NULL)
        return;
#endif

    Py_INCREF(&SessionObject_Type);
    PyModule_AddObject(m, "session", (PyObject *)&SessionObject_Type);

    Py_INCREF(&CycleObject_Type);
    PyModule_AddObject(m, "cycle", (PyObject *)&CycleObject_Type);

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
