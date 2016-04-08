FROM djwillia/solo5
MAINTAINER Dan Williams <djwillia@us.ibm.com>

USER solo5

# install necessary packages for ocaml
RUN sudo apt-get update \
    && sudo apt-get install -y software-properties-common \
    && sudo add-apt-repository -y ppa:avsm/ppa \
    && sudo apt-get update \
    && sudo apt-get install -y ocaml ocaml-native-compilers camlp4-extra opam m4 pkg-config libpcre3-dev libxen-dev ncurses-dev time iptables

# install ocaml and opam packages for mirage
RUN opam init -a \
    && opam switch 4.02.3 \
    && echo "eval \`opam config env\`" >> ~/.profile \
    && eval `opam config env` \
    && opam install -y oasis depext

# Get Solo5/Mirage 
RUN cd /home/solo5/solo5 \
    && git pull && git checkout mirage-update

# Get other repositories
RUN mkdir /home/solo5/solo5-mirage \
    && cd /home/solo5/solo5-mirage \
    && for d in mirage mirage-block-solo5 mirage-console mirage-net-solo5 mirage-platform mirage-skeleton mirage-www; do \
        git clone --branch=solo5-update https://github.com/djwillia/$d.git; \
    done \
    && for d in mirage-entropy mirage-bootvar-solo5 ocaml-nocrypto; do \
        git clone --branch=solo5 https://github.com/djwillia/$d.git; \
    done

# Pin OPAM packages
RUN cd /home/solo5/solo5-mirage \
    && eval `opam config env` \
    && opam pin add -n mirage-solo5 mirage-platform \
    && opam pin add -n mirage mirage \
    && opam pin add -n mirage-block-solo5 mirage-block-solo5 \
    && opam pin add -n mirage-net-solo5 mirage-net-solo5 \
    && opam pin add -n mirage-console mirage-console \
    && opam pin add -n mirage-bootvar-solo5 mirage-bootvar-solo5 \
    && opam pin add -n mirage-entropy-xen mirage-entropy \
    && opam pin add -n nocrypto ocaml-nocrypto

COPY solo5-startup.bash /tmp/
ENTRYPOINT ["/bin/bash", "/tmp/solo5-startup.bash"]
