#!/usr/bin/env sh
# The editable Qt Designer file must expose the same controls as the runtime
# dashboard.  This protects against accidentally replacing it with a blank form.
set -eu

ui_file=${1:-"$(dirname "$0")/../widget.ui"}
for object_name in videoLabel ipEdit portEdit rtspEdit startButton stopButton \
    serverValue clientValue totalValue standingValue fallenValue alertBanner logView; do
    if ! grep -q "name=\"${object_name}\"" "$ui_file"; then
        echo "missing Designer control: ${object_name}" >&2
        exit 1
    fi
done
