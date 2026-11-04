import shutil
import subprocess
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[2]
BUILD = REPO / "target" / "build"


@pytest.fixture(scope="session")
def built_target():
    """Build the Linux-stub agent + mock_rm once; skip if no C toolchain."""
    if not (shutil.which("make") and shutil.which("gcc")):
        pytest.skip("make/gcc not available")
    r = subprocess.run(["make", "-C", str(REPO / "target"), "linux"], capture_output=True, text=True)
    if r.returncode != 0:
        pytest.fail("target build failed:\n" + r.stdout + r.stderr)
    return BUILD
