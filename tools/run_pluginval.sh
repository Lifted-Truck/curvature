#!/bin/zsh
# pluginval strictness 10 against the built VST3 (Phase 1+ gate).
# Downloads the pluginval release binary on first run.
set -e
cd "$(dirname "$0")/.."

PLUGINVAL=build/pluginval.app/Contents/MacOS/pluginval
VST3=build/curvsynth_plugin_artefacts/Release/VST3/curvsynth.vst3

if [[ ! -x $PLUGINVAL ]]; then
    echo "fetching pluginval..."
    curl -fsSL -o build/pluginval.zip \
        https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip
    unzip -oq build/pluginval.zip -d build
fi

[[ -d $VST3 ]] || { echo "VST3 not built at $VST3"; exit 1; }

"$PLUGINVAL" --strictness-level 10 --validate-in-process --timeout-ms 120000 \
    --output-dir build/pluginval-logs --validate "$VST3"
