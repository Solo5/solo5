#!/usr/bin/env bats
# Copyright (c) 2015-2018 Contributors as noted in the AUTHORS file
#
# This file is part of Solo5, a sandboxed execution environment.
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
  eval $(grep -E ^C_HOST=.+ ${MAKECONF})
  eval $(grep -E ^C_ARCH=.+ ${MAKECONF})

  if [ -x "$(command -v timeout)" ]; then
    TIMEOUT=timeout
  elif [ -x "$(command -v gtimeout)" ]; then
    TIMEOUT=gtimeout
  else
    skip "timeout(gtimeout) is required"
  fi

  case "${BATS_TEST_NAME}" in
  *hvt)
    [ "${BUILD_HVT}" = "no" ] && skip "hvt not built"
    case ${C_HOST} in
    Linux)
      [ -c /dev/kvm -a -w /dev/kvm ] || skip "no access to /dev/kvm or not present"
      ;;
    FreeBSD|OpenBSD)
      # TODO, just try and run the test anyway
      ;;
    *)
      skip "Don't know how to run ${BATS_TEST_NAME} on ${C_HOST}"
      ;;
    esac
    HVT_TENDER=../tenders/hvt/solo5-hvt
    HVT_TENDER_DEBUG=../tenders/hvt/solo5-hvt-debug
    ;;
  *virtio)
    if [ "$(uname -s)" = "OpenBSD" ]; then
      skip "virtio tests not run for OpenBSD"
    fi
    [ "${BUILD_VIRTIO}" = "no" ] && skip "virtio not built"
    VIRTIO=../scripts/virtio-run/solo5-virtio-run.sh
    ;;
  *spt)
    [ "${BUILD_SPT}" = "no" ] && skip "spt not built"
    SPT_TENDER=../tenders/spt/solo5-spt
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

@test "hello hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} test_hello/test_hello.hvt Hello_Solo5
  [ "$status" -eq 0 ]
}

@test "hello virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_hello/test_hello.virtio Hello_Solo5
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "hello spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_hello/test_hello.spt Hello_Solo5
  [ "$status" -eq 0 ]
}

@test "quiet hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} test_quiet/test_quiet.hvt --solo5:quiet
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

@test "quiet spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_quiet/test_quiet.spt --solo5:quiet
  [ "$status" -eq 0 ]
  [[ "$output" == *"SUCCESS"* ]]
  [[ "$output" != *"Solo5:"* ]]
}

@test "globals hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} test_globals/test_globals.hvt
  [ "$status" -eq 0 ]
}

@test "globals virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_globals/test_globals.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "globals spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_globals/test_globals.spt
  [ "$status" -eq 0 ]
}

@test "exception hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} test_exception/test_exception.hvt
  [ "$status" -eq 255 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "exception virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_exception/test_exception.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "exception spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_exception/test_exception.spt
  [ "$status" -eq 139 ] # SIGSEGV
}

@test "zeropage hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} test_zeropage/test_zeropage.hvt
  [ "$status" -eq 255 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "zeropage virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_zeropage/test_zeropage.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "zeropage spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_zeropage/test_zeropage.spt
  [ "$status" -eq 139 ] # SIGSEGV
}

@test "notls hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} test_notls/test_notls.hvt
  [ "$status" -eq 255 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "notls virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_notls/test_notls.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "notls spt" {
  skip "not supported on spt yet"
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_notls/test_notls.spt
  [ "$status" -eq 139 ]
}

@test "ssp hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} test_ssp/test_ssp.hvt
  [ "$status" -eq 255 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "ssp virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_ssp/test_ssp.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "ssp spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_ssp/test_ssp.spt
  [ "$status" -eq 255 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "fpu hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} test_fpu/test_fpu.hvt
  [ "$status" -eq 0 ]
}

@test "fpu virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_fpu/test_fpu.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "fpu spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_fpu/test_fpu.spt
  [ "$status" -eq 0 ]
}

@test "time hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} test_time/test_time.hvt
  [ "$status" -eq 0 ]
}

@test "time virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -- test_time/test_time.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "time spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_time/test_time.spt
  [ "$status" -eq 0 ]
}

@test "seccomp spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} test_seccomp/test_seccomp.spt
  [ "$status" -eq 159 ] # SIGSYS
}

@test "blk hvt" {
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER} --disk=${DISK} test_blk/test_blk.hvt
  [ "$status" -eq 0 ]
}

@test "blk virtio" {
  run ${TIMEOUT} --foreground 30s ${VIRTIO} -d ${DISK} -- test_blk/test_blk.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "blk spt" {
  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} --disk=${DISK} test_blk/test_blk.spt
  [ "$status" -eq 0 ]
}

@test "ping-serve hvt" {
  TENDER=${HVT_TENDER}
  UNIKERNEL=test_ping_serve/test_ping_serve.hvt

  [ $(id -u) -ne 0 ] && skip "Need root to run this test, for ping -f"

  (
    sleep 1
    ${TIMEOUT} 30s ping -fq -c 100000 ${NET_IP} 
  ) &

  run ${TIMEOUT} --foreground 30s ${TENDER} --net=${NET} -- ${UNIKERNEL} limit
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

@test "ping-serve spt" {
  UNIKERNEL=test_ping_serve/test_ping_serve.spt

  [ $(id -u) -ne 0 ] && skip "Need root to run this test, for ping -f"

  (
    sleep 1
    ${TIMEOUT} 30s ping -fq -c 100000 ${NET_IP} 
  ) &

  run ${TIMEOUT} --foreground 30s ${SPT_TENDER} --net=${NET} -- ${UNIKERNEL} limit
  [ "$status" -eq 0 ]
}

@test "abort hvt" {
  [ "${C_ARCH}" = "x86_64" ] || skip "not implemented for ${C_ARCH}"
  case ${C_HOST} in
    Linux|FreeBSD)
      ;;
    *)
      skip "not implemented for ${C_HOST}"
      ;;
  esac
  run ${TIMEOUT} --foreground 30s ${HVT_TENDER_DEBUG} --dumpcore test_abort/test_abort.hvt
  [ "$status" -eq 255 ]
  CORE=`echo "$output" | grep -o "core\.solo5-hvt\.[0-9]*$"`
  [ -f "$CORE" ]
  [ -f "$CORE" ] && mv "$CORE" "$BATS_TMPDIR"
}
