#!/bin/bash
cd "$(dirname "$0")/build" || exit 1
./mixxx --new-ui --allow-dangerous-data-corruption-risk
echo ""
echo "Mixxx exited with code $?. Press Enter to close this window."
read -r
