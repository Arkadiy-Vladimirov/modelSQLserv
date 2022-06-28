#! /bin/bash

prog=$1
test_dir=$2
set -e

for tst in ${test_dir}/*.dat; do
    test_num=$(basename $tst .dat)
    ans=${test_dir}/${test_num}.ans
    echo TEST ${test_num}
    echo INPUT:
    cat $tst
    echo YOUR PROGRAM OUTPUT:
    ${prog} < $tst
    echo
done
