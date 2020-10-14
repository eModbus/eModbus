#!/bin/bash

pip install -U platformio
platformio update
platformio lib -g install AsyncTCP
platformio lib -g install https://github.com/maxgerhardt/Ethernet.git  # Ethernet

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

lines=$(find ./examples/ -maxdepth 1 -mindepth 1 -type d)
retval=0
while read line; do
  echo -e "========================== BUILDING $line =========================="
  if [[ -e "$line/platformio.ini" ]]
  then
    output=$(platformio ci --lib="." --project-conf="$line/platformio.ini" $line 2>&1)
  else
    output=$(platformio ci --lib="." --project-conf="scripts/platformio.ini" $line 2>&1)
  fi
  if [ $? -ne 0 ]; then
    echo "$output"
    echo -e "Building $line ${RED}FAILED${NC}"
    retval=1
  else
    echo -e "${GREEN}SUCCESS${NC}"
  fi
done <<< "$lines"

# cleanup
platformio lib -g uninstall AsyncTCP
platformio lib -g uninstall https://github.com/maxgerhardt/Ethernet.git

exit "$retval"
