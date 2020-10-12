#!/bin/bash

if [ -z "$LAVA_HOME" ]
then
    # $LAVA_HOME is not set
    export LAVA_HOME=/opt/lava
fi

cd /opt/houdini17.5
source ./houdini_setup

export LD_LIBRARY_PATH=/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:$LAVA_HOME/lib:$LD_LIBRARY_PATH
export PATH=/opt/houdini17.5/bin:$PATH

# source virtualenv
source /home/max/dev/Falcor/venv/bin/activate
