#!/bin/bash

tc qdisc del dev lo root netem delay 100ms 
