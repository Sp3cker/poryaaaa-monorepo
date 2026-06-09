#!/bin/sh
set -eu

venv_dir="scripts/.venv"
python="$venv_dir/bin/python"

if [ ! -x "$python" ]; then
    python3 -m venv "$venv_dir"
fi

if "$python" -c 'import py2max' >/dev/null 2>&1; then
    exit 0
fi

"$python" -m pip install -r scripts/requirements.txt
