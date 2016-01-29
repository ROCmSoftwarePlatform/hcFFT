#!/bin/bash
#This script is invoked to test all generators of the hcfft library
#Preliminary version

# CURRENT_WORK_DIRECTORY
current_work_dir=$PWD

red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`

# Move to gtest bin
working_dir1="$current_work_dir/../../build/test/unit/gtest/bin/"
cd $working_dir1

#Gtest functions
unittest="$working_dir1/unittest"

runcmd1="$unittest >> gtestlog.txt"
eval $runcmd1

Log_file="$working_dir1/gtestlog.txt"
if grep -q FAILED "$Log_file";
then
    echo "${red}GTEST               ----- [ FAILED ]${reset}"
else
    echo "${green}GTEST             ----- [ PASSED ]${reset}"
    rm $working_dir1/gtestlog.txt
fi
