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
    && opam switch 4.01.0 \
    && echo "eval \`opam config env\`" >> ~/.profile \
    && eval `opam config env` \
    && opam install -y oasis depext

# Get Solo5/Mirage 
RUN cd /home/solo5/solo5 \
    && git pull && git checkout mirage

# Get other repositories
RUN mkdir /home/solo5/solo5-mirage \
    && cd /home/solo5/solo5-mirage \
    && for d in mirage mirage-block-solo5 mirage-console mirage-net-solo5 mirage-platform mirage-skeleton mirage-www; do \
        git clone --branch=solo5 https://github.com/djwillia/$d.git; \
    done

COPY solo5-startup.bash /tmp/
ENTRYPOINT ["/bin/bash", "/tmp/solo5-startup.bash"]
