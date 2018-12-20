#/bin/bash

WORKON_HOME=/opt/dcrunner/py_virtual_env
VIRTUALENVWRAPPER_PYTHON=`which python3`

mkdir -p ${WORKON_HOME}
source /usr/local/bin/virtualenvwrapper.sh
mkvirtualenv --system-site-packages dc-dev
pip install --upgrade ipython

# Bug in pip/scipy, numpy must be installed first
pip install --upgrade numpy

pip install --upgrade \
    nose \
    nose2 \
    parameterized \
    grpcio \
    matplotlib \
    pillow \
    portpicker \
    scipy \
    xmlrunner\
    zmq
