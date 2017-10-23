# FriendsWithGpu-WS281X
Test/sample programs using RPi GPU to control WS281X pixels

# Dependencies:
on RPi, libpng-dev <- libslang2-dev <- libcaca-dev <- libsdl1.2-dev

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
