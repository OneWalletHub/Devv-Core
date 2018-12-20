#!/bin/bash -ex

cd ${HOME}
mkdir -p src
cd src

git clone https://github.com/google/protobuf.git

cd protobuf
git checkout v3.6.1

git submodule update --init --recursive
./autogen.sh
./configure --prefix=/usr/local
make -j4
make -j4 check
sudo make install
sudo ldconfig

cd python
python3 setup.py build
python3 setup.py test
sudo python3 setup.py install
