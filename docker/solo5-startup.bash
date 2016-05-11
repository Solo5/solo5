#!/bin/bash

# get latest sample code
(cd mirage-skeleton && git pull)

# configure virtual network tap device
sudo ip tuntap add tap100 mode tap
sudo ip link set tap100 up
sudo ip addr add 10.0.0.1/24 dev tap100


# run whatever is given in CMD
$@

# move to bash for the user
echo "solo5 environment ready!"
exec /bin/bash -l

# sudo docker run -d --privileged --name test -t solo5-mirage
# sudo docker exec -it test /bin/bash -l
