#!/bin/sh

RESEARCH_ROOT_DIR=/afs/ir/users/s/a/saurabh1/research
AFS_ROOT_DIR=/afs/ir/data/saurabh1

PIN_OUTPUT_DIR=${AFS_ROOT_DIR}/pinatrace_out/memcached
PIN_ROOT_DIR=${RESEARCH_ROOT_DIR}/pin

PIN_BINARY_PATH=${PIN_ROOT_DIR}/pin
PIN_TOOL_PATH=${PIN_ROOT_DIR}/source/tools/ManualExamples/obj-intel64/pinatrace.so
MEMCACHED_BINARY_PATH=${RESEARCH_ROOT_DIR}/memcached_log/memcached
MUTILATE_BINARY_PATH=${RESEARCH_ROOT_DIR}/mutilate/mutilate

FILENAME_PREFIX=`date +%Y_%m_%d_%H_%M_%S`

PINATRACE_OUTPUT_FILENAME=${PIN_OUTPUT_DIR}/${FILENAME_PREFIX}_memcached.out
MEMCACHED_ALLOC_FILENAME=${PIN_OUTPUT_DIR}/${FILENAME_PREFIX}_memcached_alloc.out
PINATRACE_PIPE=/tmp/${FILENAME_PREFIX}.pipe

MEMCACHED_PORT=$[ 10000 + $[ RANDOM % 40000 ]]
MEMCACHED_COMMAND="${MEMCACHED_BINARY_PATH} -p ${MEMCACHED_PORT}"

MUTILATE_COMMAND="${MUTILATE_BINARY_PATH} -s localhost:${MEMCACHED_PORT}"

echo "Making named pipe"
echo "mkfifo ${PINATRACE_PIPE}"
mkfifo ${PINATRACE_PIPE}
echo

echo "Starting memcached under pin"
export PINATRACE_PIPE
export PINATRACE_OUTPUT_FILENAME
echo "PINATRACE_PIPE=${PINATRACE_PIPE} PINATRACE_OUTPUT_FILENAME=${PINATRACE_OUTPUT_FILENAME} ${PIN_BINARY_PATH} -injection child -t ${PIN_TOOL_PATH} -- ${MEMCACHED_COMMAND} &"
PINATRACE_PIPE=${PINATRACE_PIPE} PINATRACE_OUTPUT_FILENAME=${PINATRACE_OUTPUT_FILENAME} ${PIN_BINARY_PATH} -injection child -t ${PIN_TOOL_PATH} -- ${MEMCACHED_COMMAND} &
PIN_PORT=$!
echo

echo "Waiting for 5 seconds"
echo "sleep 5"
sleep 5
echo

echo "Starting mutilate in loadonly mode"
echo "${MUTILATE_COMMAND} --loadonly"
${MUTILATE_COMMAND} --loadonly
echo

echo "Waiting for 3 seconds"
echo "sleep 3"
sleep 3
echo

echo "Writing to named pipe"
echo "echo -n A > ${PINATRACE_PIPE}"
echo -n A > ${PINATRACE_PIPE}
echo

echo "Waiting for 2 seconds"
echo "sleep 2"
sleep 2
echo

echo "Starting mutilate in noload mode"
echo "${MUTILATE_COMMAND} --noload"
${MUTILATE_COMMAND} --noload
echo

echo "Waiting for 2 seconds"
echo "sleep 2"
sleep 2
echo

echo "Killing memcached"
MEMCACHED_PORT=`pgrep -P ${PIN_PORT}`
echo "kill ${MEMCACHED_PORT}"
kill ${MEMCACHED_PORT}
echo

echo "Creating symlink"
echo "ln -s ${PINATRACE_OUTPUT_FILENAME} ."
ln -s ${PINATRACE_OUTPUT_FILENAME} .
echo
