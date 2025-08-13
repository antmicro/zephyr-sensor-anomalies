#!/bin/sh

export PYRENODE_BIN=$(realpath renode_latest/renode)
export PYRENODE_RUNTIME=coreclr

export PATH=$PATH:$(realpath renode_latest/)
