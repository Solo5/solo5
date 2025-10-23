#!/bin/sh
#
# This script is used to generate OPAM metadata for publishing a Solo5 release
# to ocaml/opam-repository. It should be run from the root of a clean, tagged
# Solo5 git repository.
# From Makefile.commmon, the '-Werror' should be removed for the tag of the
# release, to avoid failures with future C compilers (which may add new
# warnings).

if [ ! -f "./solo5.opam" ]; then
    echo "ERROR: missing ./solo5.opam. Run this from the root of a Solo5 repository."
    exit 1
fi

echo $GIT_VERSION
GIT_VERSION=${GIT_VERSION:-$(git -C . describe --tags --always)}

DEV=
if ! echo "$GIT_VERSION" | grep -q -E '^v[0-9]+.[0-9]+.[0-9]+$'; then
    echo "WARNING: Not a clean Git release: $GIT_VERSION"
    echo "WARNING: This is almost certainly not what you want."
    DEV=~dev
fi

WERROR=$(git grep -c ' -Werror' Makefile.common | cut -d ':' -f 2)
if [ -n "$WERROR" ]; then
    echo "ERROR: There are occurences of '-Werror' in 'Makefile.common'."
    echo "ERROR: This is almost certainly not what you want."
    exit 1
fi

TARBALL="solo5-${GIT_VERSION}.tar.gz"
if [ ! -f ${TARBALL} ]; then
    echo "ERROR: Release tarball ${TARBALL} not found."
    echo "ERROR: Did you run 'make distrib'?"
    exit 1
fi
echo "Using tarball: ${TARBALL}"
CHECKSUM="$(sha512sum ${TARBALL} | cut -d' ' -f1)"

OPAM_VERSION=$(echo -n "${GIT_VERSION}" | cut -d- -f1 | tr -d v)
[ -n "${DEV}" ] && OPAM_VERSION="${OPAM_VERSION}${DEV}"
echo "OPAM version: ${OPAM_VERSION}"

OUTPUT_DIR=./release
if [ -d ${OUTPUT_DIR} ]; then
    echo "Deleting existing output directory: ${OUTPUT_DIR}"
    rm -rf ${OUTPUT_DIR}
fi
mkdir -p ${OUTPUT_DIR}/tmp
cat <<EOM >${OUTPUT_DIR}/tmp/url
url {
  src: "https://github.com/Solo5/solo5/releases/download/${GIT_VERSION}/${TARBALL}"
  checksum: "sha512=${CHECKSUM}"
}
EOM
for VARIANT in '' '-cross-aarch64'; do
    PKG_DIR=${OUTPUT_DIR}/packages/solo5${VARIANT}/solo5${VARIANT}.${OPAM_VERSION}
    mkdir -p ${PKG_DIR} || exit 1
    cat solo5${VARIANT}.opam ${OUTPUT_DIR}/tmp/url \
        > ${PKG_DIR}/opam || exit 1
    opam lint ${PKG_DIR}/opam || exit 1
done

echo "Done. Submit ${OUTPUT_DIR}/packages as a PR to opam-repository."
echo "Example: cp -r release/packages path/to/opam-repository"
echo "         cd path/to/opam/repository"
echo "         git checkout -b solo5.${OPAM_VERSION}"
echo "         git add ."
