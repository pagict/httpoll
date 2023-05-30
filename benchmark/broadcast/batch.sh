#!/bin/bash

nconns=(1 4 8 16 64 128 256 2048 8192)
nbytes=(1024 10k 50k 80k 100k 200k 500k 1m)
nconns=(2048)
nbytes=(100k)

echo "=========uname========" > result.txt
uname -a >> result.txt
echo "=========free========" >> result.txt
free -h >> result.txt
echo "=========lscpu========" >> result.txt
lscpu >> result.txt

echo "=========begin========" >> result.txt
for nconn in ${nconns[@]}; do
    for nbyte in ${nbytes[@]}; do
        echo "nconn: $nconn, nbyte: $nbyte"
        ./broadcast $nconn  $nbyte result.txt err.txt
    done
done
