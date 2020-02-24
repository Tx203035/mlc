#ifndef PYTHON_PLUGIN_H
#define PYTHON_PLUGIN_H

#include "mlc/mlc.h"

#if PY_MAJOR_VERSION < 3
#define PyInit_MLC initmlc_py
#endif

#define PyCycle_Check(o) (Py_TYPE(o) == &CycleObject_Type)

typedef struct
{
    PyObject_HEAD
    cycle_t *cycle;
    PyObject *on_connect;
    PyObject *on_recv;
    PyObject *on_status;
} CycleObject;

typedef struct
{
    PyObject_HEAD
    session_t *session;
    PyObject *on_recv;
    PyObject *on_status;
} SessionObject;

PyMODINIT_FUNC PyInit_MLC(void);

#endif