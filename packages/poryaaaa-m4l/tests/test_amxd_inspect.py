import contextlib
import importlib.util
import io
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "amxd_inspect",
    ROOT / "scripts" / "amxd_inspect.py",
)
amxd_inspect = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(amxd_inspect)


class AmxdInspectValidateTests(unittest.TestCase):
    def test_validate_reports_nested_patchline_errors_with_path(self) -> None:
        device = {
            "patcher": {
                "boxes": [
                    {
                        "box": {
                            "id": "obj-parent",
                            "maxclass": "newobj",
                            "text": "p nested",
                            "numinlets": 1,
                            "numoutlets": 1,
                            "patcher": {
                                "boxes": [
                                    {
                                        "box": {
                                            "id": "obj-inner",
                                            "maxclass": "button",
                                            "numinlets": 1,
                                            "numoutlets": 1,
                                        }
                                    }
                                ],
                                "lines": [
                                    {
                                        "patchline": {
                                            "source": ["obj-missing", 0],
                                            "destination": ["obj-inner", 0],
                                        }
                                    }
                                ],
                            },
                        }
                    }
                ],
                "lines": [],
            }
        }

        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            amxd_inspect.cmd_validate(device)

        self.assertEqual(
            output.getvalue().strip(),
            "root > obj-parent (newobj / p nested): "
            "DANGLING source: obj-missing -> obj-inner",
        )

    def test_validate_keeps_root_success_message(self) -> None:
        device = {
            "patcher": {
                "boxes": [
                    {
                        "box": {
                            "id": "obj-1",
                            "maxclass": "button",
                            "numinlets": 1,
                            "numoutlets": 1,
                        }
                    }
                ],
                "lines": [],
            }
        }

        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            amxd_inspect.cmd_validate(device)

        self.assertEqual(output.getvalue(), "ok: all patchlines resolve\n")


if __name__ == "__main__":
    unittest.main()
