#!/bin/bash

# Modify dcrunner to have UID and GID of user
# Either use the LOCAL_USER_ID if passed in at runtime or
# fallback

H_USER_ID=${USER_ID:-1002}
H_GROUP_ID=${GROUP_ID:-1002}

echo "Changing ownership of /opt/dcrunner"
chown_cmd="chown -R $H_USER_ID:$H_GROUP_ID /opt/dcrunner"
usermod_cmd="usermod -u $H_USER_ID dcrunner"

echo "sudo ${usermod_cmd}"
sudo ${usermod_cmd}

echo "sudo ${chown_cmd}"
sudo ${chown_cmd}
echo "Starting with UID:GID : $H_USER_ID:$H_GROUP_ID"

if [ "$#" -eq 0 ]; then
    echo "su dcrunner"
    su dcrunner
else
    echo "sudo -H -u dcrunner -g dcrunner \"$@\""
    sudo -H -u dcrunner -g dcrunner "$@"
fi
