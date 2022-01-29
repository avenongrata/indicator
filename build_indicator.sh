#!/bin/bash

# This script is used for building the test
# program and driver for the scheduler driver


# VARIABLES
#----------------------------------------------------
DRV_NAME="indicator_driver.ko";
DRV_PATH="driver"
API_DIRECTORY="indicator_api";
PROG_NAME="indicator";

THIS_PATH="$(dirname "$(readlink -f "${0}")")";
SCRIPT_NAME=`basename "$0"`;
TMP_DIRECTORY="tmp_build";
IP_ADDR="192.168.11.109";
#----------------------------------------------------

# FUNCTIONS
#----------------------------------------------------

# help function
function helpFunction()
{
   echo ""
   echo "Usage: ${SCRIPT_NAME} [options] ..."
   echo "Options:"
   echo "-ip [0-252]            Specify block Ip-address"
   echo "-d                     Build driver"
   echo ""
   echo ""
}

# check if directory exist
function check_directory()
{
    if [ -d ${THIS_PATH}/${TMP_DIRECTORY} ]; then return 1;
    else return 0;
    fi
}

# create directory if it doesn't exist
function create_directory()
{
    check_directory;
    if [ $? -eq 0 ]; then
    mkdir ${THIS_PATH}/${TMP_DIRECTORY};
    fi
}

# build function
function build()
{
    create_directory;
    
    cd ${THIS_PATH}/$1;
    make distclean;
    make;
}

function build_driver()
{
    create_directory;
    
    cd ${THIS_PATH}/${DRV_PATH};
    make;
    cp ${DRV_NAME} ${THIS_PATH}/${TMP_DIRECTORY};
}

# function to change block ip-address
function change_ip()
{
    IP_ADDR="192.168.11.$1";
}
#----------------------------------------------------


# call help function
helpFunction;

# check function parameters
while [ -n "$1" ]
do
case "$1" in
"-d")
    build_driver;;
"-ip")
    shift;
    change_ip $1;;
*) 
    echo "$1 there is no such parameter";
    exit 1;;
esac
shift
done 

build "api-library";
cp ${THIS_PATH}/libs/"libapi.so" ${THIS_PATH}/${TMP_DIRECTORY};

build "device-library";
cp ${THIS_PATH}/libs/"libdevice.so" ${THIS_PATH}/${TMP_DIRECTORY};

build ${API_DIRECTORY};
cp ${PROG_NAME} ${THIS_PATH}/${TMP_DIRECTORY};

scp ${THIS_PATH}/${TMP_DIRECTORY}/* root@${IP_ADDR}:/tmp;

# delete temporary directory
rm -r ${THIS_PATH}/${TMP_DIRECTORY};

