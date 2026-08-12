"""Fail-closed atomic no-replace publication for sibling directories."""

import ctypes
import errno
import os
import sys
from pathlib import Path


class PublishError(Exception):
    """An atomic directory publication could not complete."""


class PublishCollision(PublishError):
    """The destination was claimed before the staging directory could publish."""


_AT_FDCWD = -2 if sys.platform == "darwin" else -100
_RENAME_NO_REPLACE = 0x00000004 if sys.platform == "darwin" else 0x00000001


def _rename_no_replace(stage: Path, destination: Path) -> None:
    libc = ctypes.CDLL(None, use_errno=True)
    if sys.platform == "darwin":
        try:
            rename = libc.renameatx_np
        except AttributeError as error:
            raise PublishError("atomic no-replace directory publication is unavailable") from error
    elif sys.platform.startswith("linux"):
        try:
            rename = libc.renameat2
        except AttributeError as error:
            raise PublishError("atomic no-replace directory publication is unavailable") from error
    else:
        raise PublishError("atomic no-replace directory publication is unavailable")

    rename.argtypes = (ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint)
    rename.restype = ctypes.c_int
    ctypes.set_errno(0)
    result = rename(
        _AT_FDCWD,
        os.fsencode(stage),
        _AT_FDCWD,
        os.fsencode(destination),
        _RENAME_NO_REPLACE,
    )
    if result == 0:
        return

    error_number = ctypes.get_errno()
    if error_number in (errno.EEXIST, errno.ENOTEMPTY):
        raise PublishCollision(f"refusing to overwrite existing output: {destination}")
    raise PublishError(f"atomic no-replace directory publication failed for {destination}: {os.strerror(error_number)}")


def publish_directory_no_replace(stage: Path, destination: Path) -> None:
    """Atomically publish a sibling stage without replacing a destination race winner."""
    if stage.parent != destination.parent:
        raise PublishError(f"staging and destination must be siblings: {stage} / {destination}")
    _rename_no_replace(stage, destination)
