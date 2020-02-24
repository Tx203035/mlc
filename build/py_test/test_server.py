#!/usr/bin/python
#coding=utf-8

import mlc_py

def on_connect(s):
    print s, " on connect"

def on_status(s, status):
    print s, " on status=", status

def on_recv(s, data):
    print s, " python recv data, ", data

c = mlc_py.cycle_create_server(port=8888, max_connection=10)
c.set_handler(on_connect=on_connect, on_recv=on_recv, on_status=on_status)

i = 5000
while (i > 0):
    c.step(100)
    i -= 1
