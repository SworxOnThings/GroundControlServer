#!/bin/bash

# Check for the correct number of arguments
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <number_of_plane>"
    exit 1
fi

num_planes=$1

send_in_air() {
    read response
    if [ "$response" == "TAKEOFF\n" ]; then
       echo "INAIR"
    fi
}


for ((i=1; i<=$num_planes; i++)); do
    {
        nc localhost 8080 <<EOF
REG $(tr -dc 'a-zA-Z0-9' < /dev/urandom | head -c 10)
REQTAXI
REQPOS
REQAHEAD
$(send_in_air)
EOF
    } &
done

wait

echo "All planes have completed communication."