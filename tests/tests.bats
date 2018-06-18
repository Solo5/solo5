#!/usr/bin/env bats
# Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
#
# This file is part of Solo5, a unikernel base layer.
#
# Permission to use, copy, modify, and/or distribute this software
# for any purpose with or without fee is hereby granted, provided
# that the above copyright notice and this permission notice appear
# in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

setup() {
  cd ${BATS_TEST_DIRNAME}

  MAKECONF=../Makeconf
  [ ! -f ${MAKECONF} ] && skip "Can't find Makeconf, looked in ${MAKECONF}"
  eval $(grep -E ^BUILD_.+=.+ ${MAKECONF})
  eval $(grep -E ^TEST_TARGET=.+ ${MAKECONF})

  if [ -x "$(command -v timeout)" ]; then
    TIMEOUT=timeout
  elif [ -x "$(command -v gtimeout)" ]; then
    TIMEOUT=gtimeout
  else
    skip "timeout(gtimeout) is required"
  fi

  case "${BATS_TEST_NAME}" in
  *ukvm)
    [ "${BUILD_UKVM}" = "no" ] && skip "ukvm not built"
    case ${TEST_TARGET} in
    *-linux-gnu)
      [ -c /dev/kvm -a -w /dev/kvm ] || skip "no access to /dev/kvm or not present"
      ;;
    x86_64-*-freebsd*)
      # TODO, just try and run the test anyway
      ;;
    amd64-unknown-openbsd*)
      # TODO, just try and run the test anyway
      ;;
    *)
      skip "Don't know how to run ${BATS_TEST_NAME} on ${TEST_TARGET}"
      ;;
    esac
    ;;
  *virtio)
    [ "${BUILD_VIRTIO}" = "no" ] && skip "virtio not built"
    VIRTIO=../tools/run/solo5-run-virtio.sh
    ;;
  esac

  NET=tap100
  NET_IP=10.0.0.2
  DISK=${BATS_TMPDIR}/disk.img
  dd if=/dev/zero of=${DISK} bs=4k count=1024
}

teardown() {
  echo "${output}"
  rm -f ${DISK}
}

@test "hello ukvm" {
  run ${TIMEOUT} --foreground 30s test_hello/ukvm-bin test_hello/test_hello.ukvm Hello_Solo5
  [ "$status" -eq 0 ]
}

@test "hello virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_hello/test_hello.virtio Hello_Solo5
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "quiet ukvm" {
  run ${TIMEOUT} --foreground 30s test_quiet/ukvm-bin test_quiet/test_quiet.ukvm --solo5:quiet
  [ "$status" -eq 0 ]
  [[ "$output" == *"SUCCESS"* ]]
  [[ "$output" != *"Solo5:"* ]]
}

@test "quiet virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_quiet/test_quiet.virtio --solo5:quiet
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  OS="$(uname -s)"
  case ${OS} in
  Linux)
    [[ "$output" == *"SUCCESS"* ]]
    [[ "$output" != *"Solo5:"* ]]
    ;;
  FreeBSD)
    [[ "${lines[3]}" == "**** Solo5 standalone test_verbose ****" ]]
    [[ "${lines[4]}" == "SUCCESS" ]]
    [[ "${lines[5]}" == "Solo5: Halted" ]]
    ;;
  OpenBSD)
    [[ "${lines[3]}" == "**** Solo5 standalone test_verbose ****" ]]
    [[ "${lines[4]}" == "SUCCESS" ]]
    [[ "${lines[5]}" == "Solo5: Halted" ]]
    ;;
  *)
    skip "Don't know how to run on ${OS}"
    ;;
  esac
}

@test "globals ukvm" {
  run ${TIMEOUT} --foreground 30s test_globals/ukvm-bin test_globals/test_globals.ukvm
  [ "$status" -eq 0 ]
}

@test "globals virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_globals/test_globals.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "exception ukvm" {
  run ${TIMEOUT} --foreground 30s test_exception/ukvm-bin test_exception/test_exception.ukvm
  [ "$status" -eq 255 ]
}

@test "exception virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_exception/test_exception.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "fpu ukvm" {
  run ${TIMEOUT} --foreground 30s test_fpu/ukvm-bin test_fpu/test_fpu.ukvm
  [ "$status" -eq 0 ]
}

@test "fpu virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_fpu/test_fpu.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "time ukvm" {
  run ${TIMEOUT} --foreground 30s test_time/ukvm-bin test_time/test_time.ukvm
  [ "$status" -eq 0 ]
}

@test "time virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_time/test_time.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "blk ukvm" {
  run ${TIMEOUT} --foreground 30s test_blk/ukvm-bin --disk=${DISK} test_blk/test_blk.ukvm
  [ "$status" -eq 0 ]
}

@test "blk virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -d ${DISK} -- test_blk/test_blk.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "ping-serve ukvm" {
  UKVM=test_ping_serve/ukvm-bin
  UNIKERNEL=test_ping_serve/test_ping_serve.ukvm

  [ $(id -u) -ne 0 ] && skip "Need root to run this test, for ping -f"

  (
    sleep 1
    ${TIMEOUT} 30s ping -fq -c 100000 ${NET_IP} 
  ) &

  run ${TIMEOUT} --foreground 30s ${UKVM} --net=${NET} -- ${UNIKERNEL} limit
  [ "$status" -eq 0 ]
}

@test "ping-serve virtio" {
  UNIKERNEL=test_ping_serve/test_ping_serve.virtio

  [ $(id -u) -ne 0 ] && skip "Need root to run this test, for ping -f"

  (
    sleep 1
    ${TIMEOUT} 30s ping -fq -c 100000 ${NET_IP} 
  ) &

  run ${TIMEOUT} --foreground 30s ${VIRTIO} -n ${NET} -- $UNIKERNEL limit
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "abort ukvm" {
  case ${TEST_TARGET} in
    x86_64-linux-gnu|x86_64-*-freebsd*)
      ;;
    *)
      skip "not implemented for ${TEST_TARGET}"
      ;;
  esac
  run ${TIMEOUT} --foreground 30s test_abort/ukvm-bin --dumpcore test_abort/test_abort.ukvm
  [ "$status" -eq 255 ]
  CORE=`echo "$output" | grep -o "core\.ukvm\.[0-9]*$"`
  [ -f "$CORE" ]
  [ -f "$CORE" ] && mv "$CORE" "$BATS_TMPDIR"
}

@test "abort virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_abort/test_abort.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"solo5_abort() called"* ]]
}
