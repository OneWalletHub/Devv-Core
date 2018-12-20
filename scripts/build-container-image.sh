#!/bin/bash -ex

# Usage:
# From the docker directory of devcash-core or devvio-inn
#
# ../../devcash-core/scripts/build-container-image.sh ${ver} ${repo} ${rel}
#
# ../../devcash-core/scripts/build-container-image.sh v005 devvio-inn dev
#

ver=$1
repo=$2
rel=$3

if [ "${repo}X" = "devvio-coreX" ]; then
    echo "repo: ${repo}"
elif [ "${repo}X" = "devvio-innX" ]; then
    echo "repo: ${repo}"
else
    echo "Unsupported repository"
    exit -1
fi

if [ "${rel}X" = "prodX" ]; then
    echo "rel: ${rel}"
elif [ "${rel}X" = "devX" ]; then
    echo "rel: ${rel}"
else
    echo "Unsupported release type (prod or dev)"
    exit -1
fi

arch=`uname -m`
image_name="${repo}-$arch-ubuntu16.04-${rel}"
build="docker build -t ${image_name}:$ver -f Dockerfile-$arch-ubuntu16.04-${rel} ."
dtoch="ch-docker2tar ${image_name}:${ver} /z/c-cloud/tars/"
move="mv /z/c-cloud/tars/${image_name}:${ver}.tar.gz /z/c-cloud/tars/${image_name}-${ver}.tar.gz"
chtodir="ch-tar2dir /z/c-cloud/tars/${image_name}-${ver}.tar.gz /z/c-cloud/dirs/"

echo $build
echo $dtoch
echo $move
echo $chtodir

read -p "Are you sure? " -n 1 -r
echo    # (optional) move to a new line
if [[ $REPLY =~ ^[Yy]$ ]]
then
    $build
    $dtoch
    $move
    $chtodir
fi
