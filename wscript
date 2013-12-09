#! /usr/bin/env python
# coding: utf-8

import os
from waflib import Logs

APPNAME = "mpaxos"
VERSION = "0.1"

top = "."
out = "bin"

pargs = ['--cflags', '--libs']

def options(opt):
    opt.load("compiler_c")

def configure(conf):
    conf.load("compiler_c")
    
    _enable_debug(conf)     #debug
    _enable_static(conf)    #static

    conf.env.LIB_PTHREAD = 'pthread'
    conf.check_cfg(atleast_pkgconfig_version='0.0.0') 
    conf.check_cfg(package='apr-1', uselib_store='APR', args=pargs)
    conf.check_cfg(package='apr-util-1', uselib_store='APR-UTIL', args=pargs)
    conf.check_cfg(package='json', uselib_store='JSON', args=pargs)
    conf.check_cfg(package='libprotobuf-c', uselib_store='PROTOBUF', args=pargs)
    conf.check_cfg(package='check', uselib_store='CHECK', args=pargs)

    #c99
    conf.env.append_value("CFLAGS", "-std=c99")


def build(bld):
    bld.stlib(source=bld.path.ant_glob("libmpaxos/rpc/*.c libmpaxos/*.c"), target="mpaxos", includes="libmpaxos include", use="APR APR-UTIL JSON PROTOBUF PTHREAD")
    bld.program(source="test/test_mpaxos.c", target="test_mpaxos.out", includes="include", use="mpaxos APR APR-UTIL")
    bld.program(source="test/test_check.c", target="test_check.out", includes="include libmpaxos", use="mpaxos APR APR-UTIL CHECK")
    bld.program(source="test/bench_rpc.c", target="bench_rpc.out", includes="include libmpaxos", use="mpaxos APR APR-UTIL CHECK")


def _enable_debug(conf):
    if os.getenv("DEBUG") == "1":
        Logs.pprint("PINK", "Debug support enabled")
        conf.env.append_value("CFLAGS", "-Wall -Wno-unused -pthread -O0 -g -rdynamic -fno-omit-frame-pointer".split())
        conf.env.append_value("LINKFLAGS", "-Wall -Wno-unused -O0 -g -rdynamic -fno-omit-frame-pointer".split())
    else:
        conf.env.append_value("CFLAGS", "-Wall -O2 -pthread".split())

    if os.getenv("CLANG") == "1":
        Logs.pprint("PINK", "Use clang as compiler")
        conf.env.append_value("C", "clang")

def _enable_static(conf):
    if os.getenv("STATIC") == "1":
        Logs.pprint("PINK", "statically link")
        conf.env.append_value("CFLAGS", "-static")
        conf.env.append_value("LINKFLAGS", "-static")
        pargs.append('--static')
