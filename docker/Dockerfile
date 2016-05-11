FROM ocaml/opam:ubuntu-16.04_ocaml-4.02.3
MAINTAINER Dan Williams <djwillia@us.ibm.com>

USER opam

# install necessary packages for ocaml
RUN sudo apt-get update \
    && sudo apt-get install -y libxen-dev time uuid-dev autoconf libgmp-dev

# add opam-solo5 remote
RUN opam remote add opam-solo5 https://github.com/djwillia/opam-solo5.git#solo5-kernel-ukvm

# install mirage from OPAM
# note: there may be a dependency issue with "logs"
RUN opam install -y logs && opam install -y mirage

# get mirage-skeleton source
RUN git clone https://github.com/mirage/mirage-skeleton.git

COPY solo5-startup.bash /tmp/
ENTRYPOINT ["/bin/bash", "/tmp/solo5-startup.bash"]
