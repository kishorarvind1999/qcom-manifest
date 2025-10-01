#!/bin/bash

BOSCH_IMAGE_DIR=bcr-de01.inside.bosch.cloud/st-co/
BOSCH_DOCKER_NAME=st-co_yocto
BOSCH_DOCKER_TAG=scarthgap-5.0.3_lattix-11.5_sconstools-10760_20250114.0

NAME="${USER}_$(cat /dev/urandom | tr -dc 0-9 | head -c 8)_$(date '+%Y%m%d%H%M%S')"


docker run \
    --name $NAME\
    --rm \
    $([ -t 0 ] && echo -ti) \
    -e HOST_UID=$(id -u) \
    -e HOST_GID=$(id -g) \
    -e BOSCH_RELEASE \
    -e BOSCH_REVISION \
    -e USER \
    -v $(pwd):/workdir \
    -v /home/${USER}/work:/home/dockerdev/work \
    -w /workdir \
    --network host \
    -e http_proxy=$http_proxy \
    -e https_proxy=$https_proxy \
    -e no_proxy=$no_proxy \
    `[ "$DISPLAY" ] && echo -v ~/.Xauthority:/home/dockerdev/.Xauthority -e DISPLAY` \
    -v /mnt/clones:/mnt/clones:ro \
    -v ~/.gitconfig:/home/dockerdev/.gitconfig \
    -v ~/.subversion:/home/dockerdev/.subversion \
    -v /etc/ssl/certs:/etc/ssl/certs:ro \
    -v ~/.git-credentials:/home/dockerdev/.git-credentials \
    ${BOSCH_IMAGE_DIR}${BOSCH_DOCKER_NAME}:${BOSCH_DOCKER_TAG} \
    "$@"
