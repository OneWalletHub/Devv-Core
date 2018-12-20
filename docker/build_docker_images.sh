#!/bin/bash -ex

ver=$1
git_branch=$2

registry="682078287735.dkr.ecr.us-east-2.amazonaws.com"
image="devvio-x86_64-ubuntu16.04"

## Build dev and prod
declare -a image_types=("dev" "prod")

## now loop through the types and build
for image_type in "${image_types[@]}"
do
    docker build \
	   --build-arg version=${ver} \
	   --build-arg branch=${git_branch} \
	   --build-arg CACHEBUST=$(date +%s) \
	   -t ${image}-${image_type}:${ver} \
	   -f Dockerfile-x86_64-ubuntu16.04-${image_type} .

    docker tag ${image}-${image_type}:${ver} ${registry}/${image}-${image_type}:${ver}
    docker push ${registry}/${image}-${image_type}:${ver}
done
