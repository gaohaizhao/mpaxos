
#! /usr/bin/env python
# coding: utf-8


import os
from waflib import Logs

APPNAME = "mpaxos"
VERSION = "0.1"

top = "."
out = "bin"


def options(opt):
    opt.load("compiler_c")

def configure(conf):
    conf.load("compiler_c")

    conf.check_cfg(atleast_pkgconfig_version='0.0.0') 
    conf.check_cfg(package='apr-1', uselib_store='APR', args=['--cflags', '--libs'])
    conf.check_cfg(package='apr-util-1', uselib_store='APR-UTIL', args=['--cflags', '--libs'])
    conf.check_cfg(package='apr-util-1', uselib_store='APR-UTIL', args=['--cflags', '--libs'])
    conf.check_cfg(package='json', uselib_store='JSON', args=['--cflags', '--libs'])
    conf.check_cfg(package='libprotobuf-c', uselib_store='PROTOBUF', args=['--cflags', '--libs'])

    #c99
    conf.env.append_value("CFLAGS", "-std=c99")

    #debug
    _enable_debug(conf)

def build(bld):
    bld.stlib(source=bld.path.ant_glob("libmpaxos/*.c"), target="libmpaxos", includes="libmpaxos include", use="APR APR-UTIL JSON PROTOBUF")
    bld.program(source="test/test_mpaxos.c", target="test_mpaxos.out", includes="include", use="libmpaxos APR APR-UTIL")


def _enable_debug(conf):
    if os.getenv("DEBUG") == "1":
        Logs.pprint("PINK", "Debug support enabled")
        conf.env.append_value("CFLAGS", "-Wall -Wno-unused -O0 -g ".split())
    else:
        conf.env.append_value("CFLAGS", "-Wall -O2".split())
