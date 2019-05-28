#!/usr/bin/env bats
# Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
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
  # This is subtle; see the comments in configure.sh.
  eval $(grep -E ^CONFIG_ ${MAKECONF})

  if [ -x "$(command -v timeout)" ]; then
    TIMEOUT=timeout
  elif [ -x "$(command -v gtimeout)" ]; then
    TIMEOUT=gtimeout
  else
    skip "timeout(gtimeout) is required"
  fi

  case "${BATS_TEST_NAME}" in
  *hvt)
    [ -z "${CONFIG_HVT}" ] && skip "hvt not configured"
    case "${CONFIG_HOST}" in
    Linux)
      [ -c /dev/kvm -a -w /dev/kvm ] || skip "no access to /dev/kvm or not present"
      ;;
    FreeBSD|OpenBSD)
      # TODO, just try and run the test anyway
      ;;
    *)
      skip "Don't know how to run ${BATS_TEST_NAME} on ${CONFIG_HOST}"
      ;;
    esac
    HVT_TENDER=../tenders/hvt/solo5-hvt
    HVT_TENDER_DEBUG=../tenders/hvt/solo5-hvt-debug
    ;;
  *virtio)
    if [ "${CONFIG_HOST}" = "OpenBSD" ]; then
      skip "virtio tests not run for OpenBSD"
    fi
    [ -z "${CONFIG_VIRTIO}" ] && skip "virtio not configured"
    VIRTIO=../scripts/virtio-run/solo5-virtio-run.sh
    ;;
  *spt)
    [ -z "${CONFIG_SPT}" ] && skip "spt not configured"
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

hvt_run() {
  run ${TIMEOUT} --foreground 60s ${HVT_TENDER} "$@"
}

spt_run() {
  run ${TIMEOUT} --foreground 60s ${SPT_TENDER} "$@"
}

virtio_run() {
  run ${TIMEOUT} --foreground 60s ${VIRTIO} "$@"
}

expect_success() {
  [ "$status" -eq 0 ] && [[ "$output" == *"SUCCESS"* ]]
}

virtio_expect_success() {
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ] && \
    [[ "$output" == *"SUCCESS"* ]]
}

expect_abort() {
  [ "$status" -eq 255 ] && [[ "$output" == *"ABORT"* ]]
}

expect_segfault() {
  [ "$status" -eq 139 ]
}

virtio_expect_abort() {
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ] && \
    [[ "$output" == *"ABORT"* ]]
}

# ------------------------------------------------------------------------------
# Tests start here
# ------------------------------------------------------------------------------

@test "hello hvt" {
  hvt_run test_hello/test_hello.hvt Hello_Solo5
  expect_success
}

@test "hello virtio" {
  virtio_run test_hello/test_hello.virtio Hello_Solo5
  virtio_expect_success
}

@test "hello spt" {
  spt_run test_hello/test_hello.spt Hello_Solo5
  expect_success
}

@test "quiet hvt" {
  hvt_run -- test_quiet/test_quiet.hvt --solo5:quiet
  expect_success
  [[ "$output" != *"Solo5:"* ]]
}

@test "quiet virtio" {
  virtio_run -- test_quiet/test_quiet.virtio --solo5:quiet
  virtio_expect_success
  # XXX "Solo5: Halted" might be printed (see old version), what to test for
  # here?
}

@test "quiet spt" {
  spt_run -- test_quiet/test_quiet.spt --solo5:quiet
  expect_success
  [[ "$output" != *"Solo5:"* ]]
}

@test "globals hvt" {
  hvt_run test_globals/test_globals.hvt
  expect_success
}

@test "globals virtio" {
  virtio_run test_globals/test_globals.virtio
  virtio_expect_success
}

@test "globals spt" {
  spt_run test_globals/test_globals.spt
  expect_success
}

@test "exception hvt" {
  hvt_run test_exception/test_exception.hvt
  expect_abort
}

@test "exception virtio" {
  virtio_run test_exception/test_exception.virtio
  virtio_expect_abort
}

@test "exception spt" {
  spt_run test_exception/test_exception.spt
  [ "$status" -eq 139 ] # SIGSEGV
}

@test "zeropage hvt" {
  hvt_run test_zeropage/test_zeropage.hvt
  expect_abort
}

@test "zeropage virtio" {
  virtio_run test_zeropage/test_zeropage.virtio
  virtio_expect_abort
}

@test "zeropage spt" {
  spt_run test_zeropage/test_zeropage.spt
  [ "$status" -eq 139 ] # SIGSEGV
}

@test "xnow hvt" {
  if [ "${CONFIG_HOST}" != "Linux" ]; then
    skip "not supported on ${CONFIG_HOST}"
  fi
  hvt_run test_xnow/test_xnow.hvt
  [ "$status" -eq 1 ] && [[ "$output" == *"host/guest translation fault"* ]]
}

@test "xnow spt" {
  spt_run test_xnow/test_xnow.spt
  [ "$status" -eq 139 ] # SIGSEGV
}

@test "wnox hvt" {
  skip "not supported on ${CONFIG_HOST}"
  hvt_run test_wnox/test_wnox.hvt
  expect_abort
}

@test "wnox spt" {
  spt_run test_wnox/test_wnox.spt
  [ "$status" -eq 139 ] # SIGSEGV
}

@test "notls hvt" {
  hvt_run test_notls/test_notls.hvt
  expect_abort
}

@test "notls virtio" {
  virtio_run test_notls/test_notls.virtio
  virtio_expect_abort
}

@test "notls spt" {
  if [ "$(uname -m)" = "ppc64le" ]; then
    skip "not supported on ${CONFIG_HOST}"
  fi
  spt_run test_notls/test_notls.spt
  expect_segfault
}

@test "tls hvt" {
  if [ "${CONFIG_HOST}" = "OpenBSD" ]; then
    skip "tls tests not run for OpenBSD"
  fi

  hvt_run test_tls/test_tls.hvt
  expect_success
}

@test "tls virtio" {
  if [ "${CONFIG_HOST}" = "OpenBSD" ]; then
    skip "tls tests not run for OpenBSD"
  fi

  virtio_run test_tls/test_tls.virtio
  virtio_expect_success
}

@test "tls spt" {
  if [ "${CONFIG_HOST}" = "OpenBSD" ]; then
    skip "tls tests not run for OpenBSD"
  fi

  spt_run test_tls/test_tls.spt
  expect_success
}

@test "ssp hvt" {
  hvt_run test_ssp/test_ssp.hvt
  expect_abort
}

@test "ssp virtio" {
  virtio_run test_ssp/test_ssp.virtio
  virtio_expect_abort
}

@test "ssp spt" {
  spt_run test_ssp/test_ssp.spt
  expect_abort
}

@test "fpu hvt" {
  hvt_run test_fpu/test_fpu.hvt
  expect_success
}

@test "fpu virtio" {
  virtio_run test_fpu/test_fpu.virtio
  virtio_expect_success
}

@test "fpu spt" {
  spt_run test_fpu/test_fpu.spt
  expect_success
}

@test "time hvt" {
  hvt_run test_time/test_time.hvt
  expect_success
}

@test "time virtio" {
  virtio_run test_time/test_time.virtio
  virtio_expect_success
}

@test "time spt" {
  spt_run test_time/test_time.spt
  expect_success
}

@test "seccomp spt" {
  spt_run test_seccomp/test_seccomp.spt
  [ "$status" -eq 159 ] # SIGSYS
}

@test "blk hvt" {
  hvt_run --disk=${DISK} -- test_blk/test_blk.hvt
  expect_success
}

@test "blk virtio" {
  virtio_run -d ${DISK} -- test_blk/test_blk.virtio
  virtio_expect_success
}

@test "blk spt" {
  spt_run --disk=${DISK} -- test_blk/test_blk.spt
  expect_success
}

@test "ping-serve hvt" {
  [ $(id -u) -ne 0 ] && skip "Need root to run this test, for ping -f"

  ( sleep 1; ${TIMEOUT} 60s ping -fq -c 100000 ${NET_IP} ) &
  hvt_run --net=${NET} -- test_ping_serve/test_ping_serve.hvt limit
  expect_success
}

@test "ping-serve virtio" {
  [ $(id -u) -ne 0 ] && skip "Need root to run this test, for ping -f"

  ( sleep 3; ${TIMEOUT} 60s ping -fq -c 100000 ${NET_IP} ) &
  virtio_run -n ${NET} -- test_ping_serve/test_ping_serve.virtio limit
  virtio_expect_success
}

@test "ping-serve spt" {
  [ $(id -u) -ne 0 ] && skip "Need root to run this test, for ping -f"

  ( sleep 1; ${TIMEOUT} 60s ping -fq -c 100000 ${NET_IP} ) &
  spt_run --net=${NET} -- test_ping_serve/test_ping_serve.spt limit
  expect_success
}

@test "dumpcore hvt" {
  [ "${CONFIG_ARCH}" = "x86_64" ] || skip "not implemented for ${CONFIG_ARCH}"
  case "${CONFIG_HOST}" in
    Linux|FreeBSD)
      ;;
    *)
      skip "not implemented for ${CONFIG_HOST}"
      ;;
  esac

  run ${TIMEOUT} --foreground 60s ${HVT_TENDER_DEBUG} \
      --dumpcore test_dumpcore/test_dumpcore.hvt
  [ "$status" -eq 255 ]
  CORE=`echo "$output" | grep -o "core\.solo5-hvt\.[0-9]*$"`
  [ -f "$CORE" ]
  [ -f "$CORE" ] && mv "$CORE" "$BATS_TMPDIR"
}
