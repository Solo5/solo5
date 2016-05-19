FROM ocaml/opam:ubuntu-16.04_ocaml-4.02.3
MAINTAINER Dan Williams <djwillia@us.ibm.com>

USER opam

# install necessary packages for tests
RUN sudo apt-get update \
    && sudo apt-get install -y libxen-dev time uuid-dev autoconf libgmp-dev qemu-kvm xorriso

# add opam-solo5 remote
RUN opam remote add solo5 https://github.com/djwillia/opam-solo5.git

# install mirage from OPAM
RUN opam install -y mirage

# get mirage-skeleton source
RUN git clone https://github.com/mirage/mirage-skeleton.git

# prepare a sample disk.img
RUN dd if=/dev/zero of=disk.img count=100000

COPY solo5-startup.bash /tmp/
ENTRYPOINT ["/bin/bash", "/tmp/solo5-startup.bash"]
