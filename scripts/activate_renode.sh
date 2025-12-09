#!/bin/sh

# Copyright (c) 2025 Antmicro <www.antmicro.com>
#
# SPDX-License-Identifier: Apache-2.0

export PYRENODE_BIN=$(realpath renode_latest/renode)
export PYRENODE_RUNTIME=coreclr

export PATH=$(realpath renode_latest/):$PATH
