#!/bin/bash

SESSION="distributed"
PROGRAM="/home/mdb326/cse376/376DHS/server"

HOSTS=(
    "mdb326@io.cse.lehigh.edu"
    "mdb326@mars.cse.lehigh.edu"
)

screen -dmS $SESSION

INDEX=0
for HOST in "${HOSTS[@]}"; do
    WIN="server_$INDEX"

    echo "Starting $PROGRAM on $HOST in screen window $WIN"

    # Add a new window to the screen session and run ssh
    screen -S "$SESSION" -X screen -t "$WIN_NAME" bash -c "ssh $HOST; exec bash"

    INDEX=$((INDEX + 1))
done

# Attach to session
screen -r $SESSION