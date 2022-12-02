#!/usr/bin/bash


systemctl --user start docker.service
export SCRIPT_DIR=`pwd`
export BUILD_DIR=`pwd`/../gem5/build/X86

export CONFIG_DIR=`pwd`/../gem5/configs
export EXE_DIR=`pwd`/../attack
