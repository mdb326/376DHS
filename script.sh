#!/bin/bash

# Clean and build the program
make clean
make hw
# for i in {1..10}; do
#     ./test 65536
# done
# ./test


ssh mdb326@nereid.cse.lehigh.edu

cd cse376/376DHS
ssh mdb326@io.cse.lehigh.edu

cd cse376/376DHS
ssh mdb326@mars.cse.lehigh.edu

cd cse376/376DHS
ssh mdb326@saturn.cse.lehigh.edu

cd cse376/376DHS
ssh mdb326@triton.cse.lehigh.edu

cd cse376/376DHS