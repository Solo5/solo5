#!/bin/bash

cd /home/solo5/solo5
echo "PATH=/home/solo5/opt/cross/bin:\$PATH" >> ~/.profile
source ~/.profile

# pull latest source
git pull

# configure virtual network for solo5 to be on 10.0.0.2
./config.bash

# run whatever is given in CMD
$@

# move to bash for the user
echo "solo5 environment ready!"
exec /bin/bash -l

# sudo docker run -d --privileged --name test -t solo5
# sudo docker exec -it test /bin/bash -l
