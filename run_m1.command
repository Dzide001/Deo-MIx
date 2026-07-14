#!/bin/bash
# build-stemsep/, not build/: this is the only build directory configured
# with -DAI_STEM_SEPARATION=ON, needed for the "Prepare Stems" pad.
cd "$(dirname "$0")/build-stemsep" || exit 1
./mixxx --new-ui --allow-dangerous-data-corruption-risk
echo ""
echo "Mixxx exited with code $?. Press Enter to close this window."
read -r
