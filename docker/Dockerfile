FROM ubuntu:15.04
MAINTAINER Dan Williams <djwillia@us.ibm.com>

# install necessary packages for:
#   git
#   building cross compilers
#   building solo5 image
#   setting up virtual network
#   running solo5
#   pinging solo5

RUN apt-get update && apt-get install -y \
    sudo \
    git \
    build-essential \
    xorriso \
    bridge-utils \
    kvm \
    iputils-ping

# set up solo5 user
RUN useradd -m -k /root/ -s /bin/bash solo5 \
    && echo "solo5:solo5" | chpasswd solo5 \
    && echo "solo5    ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/solo5 \
    && chmod 440 /etc/sudoers.d/solo5

# install pacakges to build cross compilers
RUN apt-get install -y \
    libgmp3-dev \
    libmpfr-dev \
    libisl-dev \
    libcloog-isl-dev \
    libmpc-dev \
    texinfo \
    wget

USER solo5

# Run script to build cross compilers
COPY build_xcompile.bash /tmp/
RUN /bin/bash /tmp/build_xcompile.bash

# Get Solo5 
RUN cd /home/solo5 \
    && git clone https://github.com/djwillia/solo5.git

COPY solo5-startup.bash /tmp/
ENTRYPOINT ["/bin/bash", "/tmp/solo5-startup.bash"]
