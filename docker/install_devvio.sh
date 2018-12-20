#!/bin/bash -ex

cd /opt/dcrunner/src

ssh-agent bash -c 'ssh-add /opt/dcrunner/.ssh/id_rsa.bitbucket; git clone git@bitbucket.org:leveragerock/devcash-core.git'

# Checkout the branch
cd devcash-core
git checkout $1

# Create build directory
mkdir build
cd build

# Configure and build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=${HOME}/local ../src
make -j4
make test
sudo make install

PATH=${HOME}/local/bin:${PATH}

# Test
set +e # disable errors because "devcash --help" returns 255 (-1)
devv-validator --help
set -e

exit 0
