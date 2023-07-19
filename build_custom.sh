#!/usr/bin/env bash
set -e

ROOT=`dirname "$0"`
ROOT=`cd "$ROOT"; pwd`

export STARROCKS_HOME=${ROOT}

cd $STARROCKS_HOME

echo 'build fe/be...'
./build.sh

cd ./fs_brokers/apache_hdfs_broker

echo 'build broker...'
./build.sh

cd $STARROCKS_HOME
mv ./fs_brokers/apache_hdfs_broker/output/* ./output

echo 'build complete'
