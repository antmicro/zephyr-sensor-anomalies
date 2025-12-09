#!/bin/sh

# Copyright (c) 2025 Antmicro <www.antmicro.com>
#
# SPDX-License-Identifier: Apache-2.0

set -e

curl https://builds.renode.io/renode-latest.linux-portable-dotnet.tar.gz -o renode-latest.tar.gz
tar -xf renode-latest.tar.gz
rm -f renode-latest.tar.gz
mv -f renode_*-dotnet_portable renode_latest
