#!/bin/bash
#fix up RPi arp

sudo  arp  -s 192.168.1.5  74:da:38:7e:1c:74
arp  -n
