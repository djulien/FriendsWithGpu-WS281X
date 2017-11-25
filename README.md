# What it does

# How it works

# How to install it

# How to use it

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
