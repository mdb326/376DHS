#!/bin/bash

USER="mdb326"
HOSTS=("mars.cse.lehigh.edu" "io.cse.lehigh.edu")

SESSION="mysession"
DIR="/home/mdb326/cse376/376DHS"
PROG1="./client"
PROG2="./server"

for HOST in "${HOSTS[@]}"; do
  echo "Starting programs on $HOST..."

  ssh -t "$USER@$HOST" bash <<EOF &
screen -S $SESSION -X quit 2>/dev/null

screen -dmS $SESSION

screen -S $SESSION -X screen bash -c "cd $DIR && $PROG1; exec bash"
screen -S $SESSION -X screen bash -c "cd $DIR && $PROG2; exec bash"

screen -r $SESSION
EOF

done

wait
