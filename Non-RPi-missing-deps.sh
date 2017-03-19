#install missing dependencies on non-RPi machine:
sudo  apt-get  install  libxrandr-dev  libxinerama-dev  libxcursor-dev  libfreeimage-dev  libxi-dev
sudo  apt-get  install  build-essential  libxmu-dev  libxi-dev  libgl-dev  libosmesa-dev

#GLEW:
wget  https://sourceforge.net/projects/glew/files/glew/2.0.0/glew-2.0.0.tgz
tar  -xf  glew*.tgz
cd  glew-2.0.0
make
sudo  make  install
sudo  mv  /usr/lib64/*  /usr/lib
cd  ..

#GLFW:
wget  https://github.com/glfw/glfw/releases/download/3.2.1/glfw-3.2.1.zip
unzip  glfw*.zip
cd  glfw-3.2.1
cmake  .
make
sudo make install
cd  ..

#AntTweakBar:
wget  https://sourceforge.net/projects/anttweakbar/files/latest/download?source=dlp
mv *dlp  anttweakbar.zip
unzip  ant*.zip
cd  AntTweakBar
cd  src
make
sudo  cp  ../include/*  /usr/local/include
sudo  cp  ../lib/libAntTweakBar.*  /usr/local/lib
sudo  ldconfig
cd  ..

#eof
