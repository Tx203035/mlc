#!/usr/bin/python
#coding=utf-8

import mlc_py

def on_connect(s):
    print s, " on connect"

def on_status(s, status):
    print s, " on status=", status

def on_recv(s, data):
    print s, " recv data, ", data

c = mlc_py.cycle_create_client(port=9999, max_connection=10)
s = mlc_py.session_create_client(c, "127.0.0.1", 8888)
s.set_handler(on_recv=on_recv, on_status=on_status)
s.connect()

i = 5000
while (i > 0):
    c.step(100)
    s.send("123")
    i -= 1
