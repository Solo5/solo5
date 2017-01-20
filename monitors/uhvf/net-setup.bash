#!/bin/bash

BRIDGE=`ifconfig -l |grep -o bridge[0-9]* |tail -n 1`
IF=`ifconfig -l |grep -o en[0-9]* |tail -n 1`
sudo ifconfig $BRIDGE 10.0.0.1/24 -hostfilter $IF

