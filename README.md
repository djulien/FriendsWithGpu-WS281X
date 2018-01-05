# UNDER CONSTRUCTION
# UNDER CONSTRUCTION
# UNDER CONSTRUCTION


install/repair:
update nvm (CAUTION: removes node + npm)
 sudo rm -rf ~/.nvm
curl https://raw.githubusercontent.com/creationix/nvm/v0.25.0/install.sh | bash
nvm --version
nvm ls-remote
nvm install v9.2.1
node -v
npm -v
npm install
sudo ln -s "$(which node)" /usr/local/bin/node

Pixel clock:
** https://github.com/raspberrypi/firmware/issues/734
  vcgencmd hdmi_timings 506 1 8 44 52 264 1 6 10 6 0 0 0 60 0 4000000 1
  hdmi_timings=506 1 8 44 52 264 1 6 10 6 0 0 0 60 0 4000000 1
  tvservice  -e "CEA 4" ; sleep 1; vcgencmd measure_clock pixel ; tvservice  -e "DMT 87"; sleep 1; vcgencmd measure_clock pixel
According to spec PLLH must be between 600MHz and 2400MHz (but we support up to 3000MHz when overclocking)
There is a fixed divide by 10 from the PLL, plus an 8-bit divisor.
So we should be able to divide PLL by between 10 and 2550.
So highest pixel clock is 240MHz (300MHz with overclock)
And lowest pixel clock is 0.235MHz

http://www.techmind.org/lcd/phasexplan.html

Node.js tuning:
https://blog.jayway.com/2015/04/13/600k-concurrent-websocket-connections-on-aws-using-node-js/
- use Websockets/ws instead of socket.io
- –nouse-idle-notification
- –expose-gc + call gc every ~ 30 sec


TODO:
- fix npm install (quits, nan absent, requires 2x)
https://www.npmjs.com/package/tiny-worker
https://wiki.libsdl.org/SDL_GetWindowBordersSize?highlight=%28%5CbCategoryAPI%5Cb%29%7C%28SDLFunctionTemplate%29
https://github.com/projectM-visualizer/projectm
https://www.perl.com/pub/2011/01/visualizing-music-with-sdl-and-perl.html
https://www.cg.tuwien.ac.at/courses/Seminar/WS2010/processing.pdf
https://nodeaddons.com/streaming-data-from-c-to-node-js/
good expl of pre-reqs (g++): https://github.com/chrisemunt/tcp-netx
maybe use Docker on a stack of (say, 4) RPis or RPi-Zeros? https://blog.alexellis.io/getting-started-with-docker-on-raspberry-pi/
  16 cores * 0.1 Intel ~= 1.5 Intel CPUs
password-less ssh:
  ssh-keygen
  cat ~/.ssh/id_rsa.pub | ssh pi@raspberrypi "mkdir ~/.ssh; cat >> ~/.ssh/authorized_keys"
  https://blog.codecentric.de/en/2013/03/home-automation-with-angularjs-and-node-js-on-a-raspberry-pi/
RPi cluster? https://makezine.com/projects/build-a-compact-4-node-raspberry-pi-cluster/
LCD front panel: https://www.raspberrypi-spy.co.uk/2012/07/16x2-lcd-module-control-using-python/#prettyPhoto
RPi 2/3 SAMBA + cross-over: http://thisdavej.com/connecting-a-raspberry-pi-using-an-ethernet-crossover-cable-and-internet-connection-sharing/
https://raspberrypi.stackexchange.com/questions/37554/local-network-between-two-rpis/37596#37596
http://amzn.to/2chSQ4N
http://amzn.to/2cqvjn5
Video core info:
https://github.com/hermanhermitage/videocoreiv/wiki/VideoCore-IV-Programmers-Manual

PERF notes:
** https://medium.com/the-node-js-collection/get-ready-a-new-v8-is-coming-node-js-performance-is-changing-46a63d6da4de
- func calls were slow within try/catch
- avoid delete and instead set properties to undefined 
- fat arrow functions which do not have arguments objects
- less code != faster code
- expose arguments object to another function instead of converting to array
- bind was slower than closure
- comments used to slow down funcs
- put long numeric ID’s in strings
- use Object.keys for loop, Object.values directly is slower
- monomorphic func args are faster
- remove "debugger" stmts
the soc is a GPU with a CPU bolted on as a feature
to check version# of components:  npm version   -OR-    node -e 'console.log(process.versions);'
32-bit vs. 64-bit:
- 32 bit more mature?
- 64-bit code is sometimes larger, fewer instructions fit in cache
- 64-bit ptrs take more RAM, cache than 32-bit ptrs; most data they point to isn't, #pointers is a lot lees than overall data. So the memory hit is fairly insignificant.
https://groups.google.com/forum/#!topic/nodejs/rAvPzqZ-YY4
- Instructions with immediate values are larger but position independent code tends to be smaller
and more efficient due to RIP-relative addressing.
- **more registers so data gets spilled to the stack less often.
- 32-bit system call performance suffers on 64 bit hosts because every system call goes through a thunk that
switches modes and converts arguments and the return value.
- Javascript uses 64 bits for its numbers exclusively. 
- reuse functions (esp lambas) so optimized versions are used
arm7 == 32-bit
armv8 = 32/64-bit
cat /proc/cpuinfo

node -p "process.config"
node -p "process.arch"

flamegraphs
node --trace_opt --trace_inlining --trace_deopt file.js


for scripts, remember to edit getcfg.js

  sudo apt-get dist-upgrade
  sudo apt-get update
  sudo apt-get upgrade
RPi firmware:  (4.9.69-v7+ as of 12/17/17)
  sudo rpi-update
offline update:
https://www.element14.com/community/thread/25357/l/updating-raspberry-pi-without-connecting-to-the-internet?displayFullThread=true
  https://github.com/raspberrypi/firmware    download + extract, move to /boot

find disk usage:
https://askubuntu.com/questions/432836/how-can-i-check-disk-space-used-in-a-partition-using-the-terminal-in-ubuntu-12-0/432842
  cd /; sudo du -sh ./*

wifi diag:
** https://www.raspberrypi.org/forums/viewtopic.php?t=44044
  lsusb
  lsmod        8192cu 
  try to load it manually:   modprobe 8192cu
  ifconfig -a      wlx...
  apt-get install <firmware-package>
sudo vi /etc/NetworkManager/system-connections/SSID-file
copy an existing ssid file to correct ssid in /etc/NetworkManager/system-connections; change UUID (must be unique)
uuidgen
sudo systemctl stop NetworkManager
sudo systemctl start NetworkManager
sudo ifconfig wlan0 down && sudo ifconfig wlan0 up
journalctl -u NetworkManager
#https://askubuntu.com/questions/117065/how-do-i-find-out-the-name-of-the-ssid-im-connected-to-from-the-command-line
nmcli -t -f active,ssid dev wifi
iwgetid
iwgetid -r
nmcli con show
ifconfig
------------------------------------
sudo apt-get dist-upgrade
sudo apt-get update
sudo apt-get upgrade
------------------------------------------------------
sudo /etc/init.d/network-manager start



list largest Ubuntu packages:
http://www.commandlinefu.com/commands/view/3842/list-your-largest-installed-packages-on-debianubuntu
  dpkg-query -Wf '${Installed-Size}\t${Package}\n' | sort -n
remove packages:
https://www.howtogeek.com/229699/how-to-uninstall-software-using-the-command-line-in-linux/
  sudo apt-get --purge remove   xyz

RPi /boot/cmdline.txt:
dwc_otg.lpm_enable=0 console=serial0,115200 console=tty1 root=/dev/sda2 rootfstype=ext4 elevator=deadline fsck.repair=yes rootwait

[![Build Status](https://travis-ci.org/djulien/friends-gpu-ws281x.svg?branch=master)](https://travis-ci.org/djulien/friends-gpu-ws281x) 
[![Build status](https://ci.appveyor.com/api/projects/status/tbd/branch/master?svg=true)](https://ci.appveyor.com/project/djulien/friends-gp4-ws281x/branch/master) 
[![Coverage Status](https://coveralls.io/repos/djulien/friends-gpu-ws281x/badge.svg?branch=master&service=github)](https://coveralls.io/github/djulien/friends-gpu-ws281x?branch=master) 
[![npm version](https://badge.fury.io/js/tbd.svg)](http://badge.fury.io/js/tbd)

friends-gpu-ws281x
========

# Table of Contents
* [What it does](#what-it-does)
* [How it works](#how-it-works)
* [How to install it](#how-to-install-it)
* [How to use it](#how-to-use-it)
* [More info](#more-info)

What it does
============

# What it does

How it works
============

# How it works

How to install it
=================

# How to install it

How to use it
=============

# How to use it

More info
=========

# More info

# UNDER CONSTRUCTION
# UNDER CONSTRUCTION
# UNDER CONSTRUCTION

NOTE: if "npm install" fails to install devDependencies, check your Node.js production flag or delete package-lock.json and try again.
For more info see https://stackoverflow.com/questions/34700610/npm-install-wont-install-devdependencies

git clone + npm install for dev install
npm run rebuild to recompile

npm run postinstall for demos

INITIAL VERSION: not using OpenGL

# RPi VGA Pin-out
VGA signals can be redirected to up to 26 GPIO pins as follows:

//"universe" mapping on RPi pins:
// [0] = R7 = GPIO27 (GEN2)
// [1] = R6 = GPIO26 (absent on GoWhoops board)
// [2] = R5 = GPIO25 (GEN6)
// [3] = R4 = GPIO24 (GEN5)
// [4] = R3 = GPIO23 (GEN4)
// [5] = R2 = GPIO22 (GEN3)
// [6] = R1 = GPIO21
// [7] = R0 = GPIO20
// [8] = G7 = GPIO19 (PWM)
// [9] = G6 = GPIO18 (GEN1)
// [10] = G5 = GPIO17 (GEN0)
// [11] = G4 = GPIO16
// [12] = G3 = GPIO15 (RXD0)
// [13] = G2 = GPIO14 (TXD0)
// [14] = G1 = GPIO13 (PWM)
// [15] = G0 = GPIO12 (PWM)
// [16] = B7 = GPIO11 (SPI_CLK)
// [17] = B6 = GPIO10 (SPI_MOSI)
// [18] = B5 = GPIO09 (SPI_MISO)
// [19] = B4 = GPIO08 (SPI_CE0_N)
// [20] = B3 = GPIO07 (SPI_CE1_N)
// [21] = B2 = GPIO06
// [22] = B1 = GPIO05
// [23] = B0 = GPIO04 (GPIO_GCLK)
//---------------------------------
//    H SYNC = GPIO03 (SCL1, I2C)
//    V SYNC = GPIO02 (SDA1, I2C)
//        DE = ID_SD (I2C ID EEPROM)
//     PXCLK = ID_SC (I2C ID EEPROM)

There are existing device tree overlays to redirect 16, 18, or 24 VGA signals to the RPi GPIO pins:
//dpi24 or vga565/666 send video to GPIO pins
    var dpi24 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/dpi24*") || [];
    if (dpi24.length) return toGPIO.cached = 8+8+8;
    var vga666 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/vga666*") || [];
    if (vga666.length) return toGPIO.cached = 6+6+6;
    var vga565 = glob.sync("/sys/firmware/devicetree/base/soc/gpio*/vga565*") || [];
    if (vga565.length) return toGPIO.cached = 5+6+5;

# FriendsWithGpu-WS281X
Test/sample programs using RPi GPU to control WS281X pixels
tree:

# Dependencies:
on RPi, libpng-dev <- libslang2-dev <- libcaca-dev <- libsdl1.2-dev
TODO: see Makefile at https://github.com/mattgodbolt/compiler-explorer/blob/master/Makefile
for devDependencies:
npm install --dev

# other info
NOTE: if you get a message like:
"was compiled against a different Node.js version using
NODE_MODULE_VERSION ##. This version of Node.js requires
NODE_MODULE_VERSION ##. ", then
delete node_modules and do npm install again to rebuild.

2 use cases implemented:
1. dev PC (Ubuntu) with X Windows
2. prod RPi (Ubuntu) without X Windows

2 operation modes implemented:
1. CPU-generated graphics, GPU only adds WS281X timing
2. CPU provides timing sync only, GPU generates graphics effects

3 display modes:
1. dev: show pixels on screen in matrix
2. prod: generate real WS281X timing and control signals
3. debug: prod mode + timing debug on screen

GLX extension not found:
SOMETHING BELOW HELPED:
cat /var/log/Xorg.0.log | grep glx
glxinfo | grep "OpenGL renderer"
sudo apt-get update
sudo apt-get 
sudo apt-get  check
sudo add-apt-repository ppa:ubuntu-x-swat/updates
sudo apt update && sudo apt dist-upgrade
sudo add-apt-repository multiverse
sudo apt-get install steam
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install wget gdebi libgl1-mesa-dri:i386 libgl1-mesa-glx:i386 libc6:i386
steam
sudo apt-get install mesa-utils
sudo apt autoremove


https://unix.stackexchange.com/questions/254377/xlib-extension-glx-missing-with-an-nvidia-card-and-on-board-graphics
cat /var/log/Xorg.0.log | grep glx
glxinfo | grep "OpenGL renderer"
https://forum.manjaro.org/t/solved-opengl-glx-extension-not-supported-by-display/18615/2
lspci -nnnk | grep "VGA\|'Kern'\|3D\|Display" -A2
inxi -F

http://www.omgubuntu.co.uk/2017/03/easy-way-install-mesa-17-0-2-ubuntu-16-04-lts
sudo add-apt-repository ppa:ubuntu-x-swat/updates
sudo apt update && sudo apt dist-upgrade
#sudo ppa-purge ppa:ubuntu-x-swat/updates

https://linuxconfig.org/how-to-install-steam-on-ubuntu-16-04-xenial-xerus

SDL2 on Ubuntu 16.04:
https://stackoverflow.com/questions/44758276/build-error-for-sdl2-2-0-5-in-ubuntu-16-04
 ./configure --enable-mir-shared=no
 make all  #or make -j4
 sudo make install
puts them at /usr/local/include/SDL2 and /usr/lib
