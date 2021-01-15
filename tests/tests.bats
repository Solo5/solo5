#!/usr/bin/env bats
# Copyright (c) 2015-2020 Contributors as noted in the AUTHORS file
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

fatal()
{
  echo "ERROR: $@" 1>&2
  exit 1
}

setup() {
  cd ${BATS_TEST_DIRNAME}

  MAKECONF=../Makeconf.sh
  [ ! -f ${MAKECONF} ] && fatal "Can't find Makeconf.sh, looked in ${MAKECONF}"
  source ${MAKECONF}

  if [ -x "$(command -v timeout)" ]; then
    TIMEOUT=timeout
  elif [ -x "$(command -v gtimeout)" ]; then
    TIMEOUT=gtimeout
  else
    fatal "timeout(gtimeout) is required"
  fi
  if [ -x "$(command -v seq)" ]; then
    SEQ=seq
  elif [ -x "$(command -v gseq)" ]; then
    SEQ=gseq
  else
    fatal "seq(gseq) is required"
  fi

  case "${BATS_TEST_NAME}" in
  *hvt)
    [ -z "${CONFIG_HVT}" ] && skip
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
    [ -z "${CONFIG_VIRTIO}" ] && skip
    [ "${CONFIG_HOST}" = "OpenBSD" ] && skip
    VIRTIO=../scripts/virtio-run/solo5-virtio-run.sh
    ;;
  *spt)
    [ -z "${CONFIG_SPT}" ] && skip
    SPT_TENDER=../tenders/spt/solo5-spt
    ;;
  *xen)
    [ -z "${CONFIG_XEN}" ] && skip
    [ ! -x "$(command -v xl)" ] && skip "xl not available"
    [ $(id -u) -ne 0 ] && skip "need root to run Xen tests"
  esac

  NET0=tap100
  NET0_IP=10.0.0.2
  NET1=tap101
  NET1_IP=10.1.0.2
}

teardown() {
  echo "${output}"
  rm -f ${BATS_TMPDIR}/storage*.img
}

setup_block() {
  BLOCK=${BATS_TMPDIR}/storage.img
  dd if=/dev/zero of=${BLOCK} bs=4k count=1024 status=none
}

hvt_run() {
  run ${TIMEOUT} --foreground 60s ${HVT_TENDER} --mem=2 "$@"
}

spt_run() {
  run ${TIMEOUT} --foreground 60s ${SPT_TENDER} --mem=2 "$@"
}

virtio_run() {
  run ${TIMEOUT} --foreground 60s ${VIRTIO} "$@"
}

xen_run() {
  # "xl create" is really amenable to automation, oh yeah!
  local TEST_DIR=$(dirname "$1")
  local TEST_NAME=$(basename "$1" .xen)
  shift
  run ${TIMEOUT} --foreground 60s \
    xl create -F -c /dev/null \
      name=\"${TEST_NAME}\" kernel=\"${TEST_DIR}/${TEST_NAME}.xen\" \
      type=\"pvh\" memory=32 cmdline=\"$@\"
}

# Given a list of arguments in the format HOST[_RELEASE], returns true iff CONFIG_HOST
# matches at least one HOST. If a RELEASE is specified, additionally checks that the
# running host kernel release (uname -r) matches RELEASE *exactly*.
match_hostrelease() {
  local arg=
  local host=
  local release=
  local found=1 # false
  for arg in "$@"; do
    release="${arg#*_}"
    if [ "${release}" = "${arg}" ]; then
      host="${arg}" # just host
      release=      # don't care about release
    else
      host="${arg%_*}" # just host
                       # release got set by the initial ${arg#*_}
    fi

    if [ "${CONFIG_HOST}" = "${host}" ]; then
      if [ -n "${release}" ]; then
        [ "$(uname -r)" = "${release}" ] && found=0 && break
      else
        found=0 && break
      fi
    fi
  done
  return ${found}
}

skip_if_host_is() {
  match_hostrelease "$@" && skip "not supported on ${CONFIG_HOST}"
}

skip_unless_host_is() {
  match_hostrelease "$@" || skip "not supported on ${CONFIG_HOST}"
}

skip_unless_root() {
  [ $(id -u) -ne 0 ] && skip "need root to run this test"
  return 0
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

xen_expect_abort() {
  [ "$status" -eq 0 ] && [[ "$output" == *"ABORT"* ]]
}

# ------------------------------------------------------------------------------
# Tests start here
# ------------------------------------------------------------------------------

@test "noexec host" {
  # Here be dragons.
  run test_hello/test_hello.hvt
  case "${CONFIG_HOST}" in
  Linux)
    [ "$status" -eq 127 ] && [[ "$output" == *"No such file or directory"* ]]
    ;;
  FreeBSD)
    # XXX: imgact_elf.c:load_interp() outputs the "ELF interpreter ... not
    # found" using uprintf() here, so we can't test for it. Boo.
    [ "$status" -eq 134 ]
    ;;
  OpenBSD)
    # XXX: Unclear why the "Abort trap" is not showing up in the output.
    [ "$status" -eq 134 ]
    ;;
  *)
    skip "not implemented for ${CONFIG_HOST}"
    ;;
  esac
}

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

@test "hello xen" {
  xen_run test_hello/test_hello.xen Hello_Solo5
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

# Don't run this for now, as we have a message that is always output in
# console.c.
# @test "quiet xen" {
#   xen_run test_quiet/test_quiet.xen --solo5:quiet
#   expect_success
#   [[ "$output" != *"Solo5:"* ]]
# }

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

@test "globals xen" {
  xen_run test_globals/test_globals.xen
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

@test "exception xen" {
  xen_run test_exception/test_exception.xen
  xen_expect_abort
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

@test "zeropage xen" {
  xen_run test_zeropage/test_zeropage.xen
  xen_expect_abort
}

@test "xnow hvt" {
  skip_unless_host_is Linux OpenBSD_6.7

  hvt_run test_xnow/test_xnow.hvt
  [ "$status" -eq 1 ] && [[ "$output" == *"host/guest translation fault"* ]]
}

@test "xnow spt" {
  spt_run test_xnow/test_xnow.spt
  [ "$status" -eq 139 ] # SIGSEGV
}

@test "xnow xen" {
  xen_run test_xnow/test_xnow.xen
  xen_expect_abort
}

@test "wnox hvt" {
  skip_unless_host_is OpenBSD_6.7

  hvt_run test_wnox/test_wnox.hvt
  [ "$status" -eq 1 ] && [[ "$output" == *"host/guest translation fault"* ]]
}

@test "wnox spt" {
  spt_run test_wnox/test_wnox.spt
  [ "$status" -eq 139 ] # SIGSEGV
}

@test "wnox xen" {
  xen_run test_wnox/test_wnox.xen
  xen_expect_abort
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
  spt_run test_notls/test_notls.spt
  expect_segfault
}

@test "notls xen" {
  xen_run test_notls/test_notls.xen
  xen_expect_abort
}

@test "tls hvt" {
  skip_unless_host_is Linux

  hvt_run test_tls/test_tls.hvt
  expect_success
}

@test "tls virtio" {
  skip_unless_host_is Linux # XXX is this necessary for virtio?

  virtio_run test_tls/test_tls.virtio
  virtio_expect_success
}

@test "tls spt" {
  spt_run test_tls/test_tls.spt
  expect_success
}

@test "tls xen" {
  xen_run test_tls/test_tls.xen
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

@test "ssp xen" {
  xen_run test_ssp/test_ssp.xen
  xen_expect_abort
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

@test "fpu xen" {
  xen_run test_fpu/test_fpu.xen
  expect_success
}

@test "time hvt" {
  hvt_run test_time/test_time.hvt
  # XXX:
  # On Debian 10 CI nodes, this test is flaky and fails too often with
  # "slept too little". Ignore that (and only that) case.
  if [ "${CONFIG_HOST}" = "Linux" ]; then
    if gcc --version | grep -q Debian; then
      if [ "$status" -eq 1 ] && [[ "$output" == *"slept too little"* ]]; then
        skip "flaky, ignored"
      fi
    fi
  fi
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

@test "time xen" {
  xen_run test_time/test_time.xen
  expect_success
}

@test "seccomp spt" {
  spt_run test_seccomp/test_seccomp.spt
  [ "$status" -eq 159 ] # SIGSYS
}

@test "blk hvt" {
  setup_block
  hvt_run --block:storage=${BLOCK} -- test_blk/test_blk.hvt
  expect_success
}

@test "blk virtio" {
  setup_block
  virtio_run -d ${BLOCK} -- test_blk/test_blk.virtio
  virtio_expect_success
}

@test "blk spt" {
  setup_block
  spt_run --block:storage=${BLOCK} -- test_blk/test_blk.spt
  expect_success
}

@test "net hvt" {
  skip_unless_root

  ( sleep 1; ${TIMEOUT} 60s ping -fq -c 100000 ${NET0_IP} ) &
  hvt_run --net:service0=${NET0} -- test_net/test_net.hvt limit
  expect_success
}

@test "net virtio" {
  skip_unless_root

  ( sleep 3; ${TIMEOUT} 60s ping -fq -c 100000 ${NET0_IP} ) &
  virtio_run -n ${NET0} -- test_net/test_net.virtio limit
  virtio_expect_success
}

@test "net spt" {
  skip_unless_root

  ( sleep 1; ${TIMEOUT} 60s ping -fq -c 100000 ${NET0_IP} ) &
  spt_run --net:service0=${NET0} -- test_net/test_net.spt limit
  expect_success
}

@test "net_2if hvt" {
  skip_unless_root
  [ "${CONFIG_HOST}" = "OpenBSD" ] && skip "breaks on OpenBSD due to #374"

  ( sleep 1; ${TIMEOUT} 60s ping -fq -c 50000 ${NET0_IP} ) &
  ( sleep 1; ${TIMEOUT} 60s ping -fq -c 50000 ${NET1_IP} ) &
  hvt_run --net:service0=${NET0} --net:service1=${NET1} -- \
      test_net_2if/test_net_2if.hvt limit
  expect_success
}

@test "net_2if spt" {
  skip_unless_root

  ( sleep 1; ${TIMEOUT} 60s ping -fq -c 50000 ${NET0_IP} ) &
  ( sleep 1; ${TIMEOUT} 60s ping -fq -c 50000 ${NET1_IP} ) &
  spt_run --net:service0=${NET0} --net:service1=${NET1} -- \
      test_net_2if/test_net_2if.spt limit
  expect_success
}

@test "dumpcore hvt" {
  [ "${CONFIG_ARCH}" = "x86_64" ] || skip "not implemented for ${CONFIG_ARCH}"
  skip_unless_host_is Linux FreeBSD

  run ${TIMEOUT} --foreground 60s ${HVT_TENDER_DEBUG} \
     --dumpcore="$BATS_TMPDIR" test_dumpcore/test_dumpcore.hvt
  [ "$status" -eq 255 ]
  CORE=`echo "$output" | grep -o "core\.solo5-hvt\.[0-9]*$"`
  [ -f "$BATS_TMPDIR"/"$CORE" ]
}

@test "mft_maxdevices hvt" {
  for num in $(${SEQ} 0 62); do
      dd if=/dev/zero of=${BATS_TMPDIR}/storage${num}.img \
          bs=4k count=1 status=none
  done

  DEVS=$(
  for num in $(${SEQ} 0 62); do
      echo -n "--block:storage${num}=${BATS_TMPDIR}/storage${num}.img "
  done
  )

  hvt_run ${DEVS} -- test_mft_maxdevices/test_mft_maxdevices.hvt
  expect_success
}

@test "mft_maxdevices spt" {
  for num in $(${SEQ} 0 62); do
      dd if=/dev/zero of=${BATS_TMPDIR}/storage${num}.img \
          bs=4k count=1 status=none
  done

  DEVS=$(
  for num in $(${SEQ} 0 62); do
      echo -n "--block:storage${num}=${BATS_TMPDIR}/storage${num}.img "
  done
  )

  spt_run ${DEVS} -- test_mft_maxdevices/test_mft_maxdevices.spt
  expect_success
}
