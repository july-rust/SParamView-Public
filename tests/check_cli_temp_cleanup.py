"""Verify CLI-owned temporary cache cleanup without touching caller-managed files."""
import os
import pathlib
import subprocess
import sys
import tempfile

cli = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="SParamView-cli-test-") as base:
    root = pathlib.Path(base)
    source = root / "original.s2p"
    source.write_text(
        "# Hz S RI R 50\n1 .1 0 .8 0 .6 0 .2 0\n2 .1 0 .8 0 .6 0 .2 0\n",
        encoding="ascii",
    )
    env = os.environ.copy()
    env.update({"TMPDIR": str(root), "TMP": str(root), "TEMP": str(root)})

    success = subprocess.run(
        [str(cli), "inspect", str(source)], env=env,
        capture_output=True, text=True, timeout=30,
    )
    assert success.returncode == 0, success.stderr
    assert "cache_reused=" in success.stdout
    assert not list(root.glob("SParamView-cli-session-*")), (
        "Default CLI cache must be deleted on success"
    )

    failure = subprocess.run(
        [str(cli), "inspect", str(root / "absent.s2p")], env=env,
        capture_output=True, text=True, timeout=30,
    )
    assert failure.returncode != 0
    assert not list(root.glob("SParamView-cli-session-*")), (
        "Default CLI cache must be deleted on failure"
    )

    user_dir = root / "explicit-user-cache"
    user_dir.mkdir()
    marker = user_dir / "keep.txt"
    marker.write_text("keep", encoding="ascii")
    explicit = subprocess.run(
        [str(cli), "inspect", str(source), str(user_dir)], env=env,
        capture_output=True, text=True, timeout=30,
    )
    assert explicit.returncode == 0, explicit.stderr
    assert marker.read_text(encoding="ascii") == "keep"
    assert list(user_dir.glob("*.sicache")), (
        "Caller-supplied cache must not be automatically removed"
    )
    assert source.is_file()
    print("CLI cache cleanup and user-managed cache preservation: PASS")
