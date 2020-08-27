#!/bin/bash

if [ -z "$LAVA_HOME" ]
then
    # $LAVA_HOME is not set
    export LAVA_HOME=/opt/lava
fi

export LD_LIBRARY_PATH=/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:$LAVA_HOME/lib:$LD_LIBRARY_PATH

# source virtualenv
source /home/max/dev/Falcor/venv/bin/activate
