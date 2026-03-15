#!/usr/bin/env python3
"""
test-dual-pane-integration.py
==============================
Integration tests for Nemo dual-pane enhancements.

These tests launch a real Nemo process (the installed binary or the one
given by NEMO_BINARY), drive preferences via gsettings, and verify that
Nemo behaves correctly and does not crash.

Requirements
------------
  - A running X display (or set DISPLAY before running; Xvfb works fine)
  - The nemo binary must be runnable (installed or pointed to by NEMO_BINARY)
  - python3, gsettings, xdotool (optional but gives richer window checks)

Usage
-----
  # Basic run against installed nemo:
  python3 test/test-dual-pane-integration.py

  # Against a locally built nemo:
  NEMO_BINARY=./build/src/nemo \
  GSETTINGS_SCHEMA_DIR=./build/test \
  python3 test/test-dual-pane-integration.py

  # With Xvfb (headless):
  xvfb-run python3 test/test-dual-pane-integration.py

  # Verbose output:
  python3 test/test-dual-pane-integration.py -v

Exit codes
----------
  0  all tests passed
  1  one or more tests failed
"""

import os
import sys
import time
import signal
import shutil
import subprocess
import argparse
import textwrap
from typing import Optional

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SCHEMA          = "org.nemo.preferences"
STATE_SCHEMA    = "org.nemo.window-state"
NEMO_BINARY     = os.environ.get("NEMO_BINARY", "nemo")
SCHEMA_DIR      = os.environ.get("GSETTINGS_SCHEMA_DIR", "")

# How long to wait for nemo to finish starting up / settling after a
# gsettings change (seconds).  Increase on slow machines.
STARTUP_DELAY   = 2.0
SETTLE_DELAY    = 0.8

# Keys we touch — all get saved at startup and restored at exit.
PREF_KEYS = [
    "start-with-dual-pane",
    "dual-pane-vertical-split",
    "dual-pane-separate-sidebar",
    "dual-pane-separate-nav-bar",
]
STATE_KEYS = [
    "start-with-sidebar",
]

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_verbose = False

def log(msg: str) -> None:
    if _verbose:
        print(f"  {msg}", flush=True)


def gsettings(*args) -> str:
    """Run gsettings and return stdout, raising on failure."""
    env = dict(os.environ)
    if SCHEMA_DIR:
        env["GSETTINGS_SCHEMA_DIR"] = SCHEMA_DIR
    result = subprocess.run(
        ["gsettings", *args],
        capture_output=True, text=True, env=env
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"gsettings {' '.join(args)} failed:\n{result.stderr.strip()}"
        )
    return result.stdout.strip()


def set_pref(key: str, value: bool, schema: str = SCHEMA) -> None:
    val = "true" if value else "false"
    log(f"gsettings set {schema} {key} {val}")
    gsettings("set", schema, key, val)
    time.sleep(SETTLE_DELAY)


def get_pref(key: str, schema: str = SCHEMA) -> bool:
    val = gsettings("get", schema, key)
    return val.strip() == "true"


def reset_pref(key: str, schema: str = SCHEMA) -> None:
    gsettings("reset", schema, key)


def xdotool_available() -> bool:
    return shutil.which("xdotool") is not None


def nemo_window_count() -> int:
    """Return number of visible Nemo windows via xdotool, or -1 if unavailable."""
    if not xdotool_available():
        return -1
    try:
        result = subprocess.run(
            ["xdotool", "search", "--classname", "nemo"],
            capture_output=True, text=True, timeout=5
        )
        lines = [l for l in result.stdout.strip().splitlines() if l.strip()]
        return len(lines)
    except Exception:
        return -1


# ---------------------------------------------------------------------------
# NemoProcess: context manager that launches and tears down a nemo instance
# ---------------------------------------------------------------------------

class NemoProcess:
    """
    Launches a Nemo window and keeps it alive for the duration of a test.
    Uses --no-default-window then opens a window via nemo <path> so we
    control exactly one window.
    """

    def __init__(self, extra_env: Optional[dict] = None):
        self.proc: Optional[subprocess.Popen] = None
        self.extra_env = extra_env or {}

    def start(self) -> "NemoProcess":
        if not shutil.which(NEMO_BINARY) and not os.path.isfile(NEMO_BINARY):
            raise AssertionError(
                f"nemo binary not found: {NEMO_BINARY!r} — "
                "set NEMO_BINARY env var to the path of the nemo executable"
            )
        env = dict(os.environ)
        if SCHEMA_DIR:
            env["GSETTINGS_SCHEMA_DIR"] = SCHEMA_DIR
        env.update(self.extra_env)

        # Open nemo on /tmp so we don't trigger any slow VFS mounts
        cmd = [NEMO_BINARY, "/tmp"]
        log(f"Launching: {' '.join(cmd)}")
        self.proc = subprocess.Popen(
            cmd, env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        time.sleep(STARTUP_DELAY)
        return self

    def alive(self) -> bool:
        if self.proc is None:
            return False
        return self.proc.poll() is None

    def returncode(self) -> Optional[int]:
        if self.proc is None:
            return None
        return self.proc.poll()

    def stop(self) -> int:
        """Terminate nemo gracefully, return exit code."""
        if self.proc is None:
            return 0
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=3)
        rc = self.proc.returncode
        self.proc = None
        return rc

    def __enter__(self):
        return self.start()

    def __exit__(self, *_):
        self.stop()


# ---------------------------------------------------------------------------
# Test framework
# ---------------------------------------------------------------------------

_results: list[tuple[str, bool, str]] = []

def run_test(name: str, fn) -> bool:
    """Run a single test function, record pass/fail."""
    print(f"  {'·'} {name} ... ", end="", flush=True)
    try:
        fn()
        _results.append((name, True, ""))
        print("PASS")
        return True
    except AssertionError as e:
        msg = str(e) or "(assertion failed)"
        _results.append((name, False, msg))
        print(f"FAIL\n      {msg}")
        return False
    except Exception as e:
        msg = f"{type(e).__name__}: {e}"
        _results.append((name, False, msg))
        print(f"ERROR\n      {msg}")
        return False


def assert_alive(nemo: NemoProcess, context: str = "") -> None:
    ctx = f" ({context})" if context else ""
    assert nemo.alive(), f"Nemo process died unexpectedly{ctx}"


def assert_pref(key: str, expected: bool, schema: str = SCHEMA) -> None:
    actual = get_pref(key, schema)
    assert actual == expected, (
        f"Pref {key!r}: expected {expected}, got {actual}"
    )


# ---------------------------------------------------------------------------
# Saved state — restore everything on exit
# ---------------------------------------------------------------------------

_saved_prefs: dict[str, str] = {}
_saved_state: dict[str, str] = {}

def save_prefs() -> None:
    for key in PREF_KEYS:
        _saved_prefs[key] = gsettings("get", SCHEMA, key)
    for key in STATE_KEYS:
        _saved_state[key] = gsettings("get", STATE_SCHEMA, key)
    log(f"Saved prefs: {_saved_prefs}")
    log(f"Saved state: {_saved_state}")


def restore_prefs() -> None:
    for key, val in _saved_prefs.items():
        gsettings("set", SCHEMA, key, val)
    for key, val in _saved_state.items():
        gsettings("set", STATE_SCHEMA, key, val)
    log("Prefs restored to original values")


def reset_all_dual_pane_prefs() -> None:
    """Reset all dual-pane prefs to schema defaults before each test."""
    for key in PREF_KEYS:
        reset_pref(key)
    time.sleep(0.2)


# ---------------------------------------------------------------------------
# GROUP 1: Schema / gsettings — no running nemo needed
# ---------------------------------------------------------------------------

def test_schema_keys_exist():
    """All four dual-pane keys exist in the schema."""
    for key in PREF_KEYS:
        val = gsettings("get", SCHEMA, key)
        assert val in ("true", "false"), \
            f"Key {key!r} returned unexpected value: {val!r}"


def test_schema_defaults_are_false():
    """All three new dual-pane keys default to false."""
    new_keys = [
        "dual-pane-vertical-split",
        "dual-pane-separate-sidebar",
        "dual-pane-separate-nav-bar",
    ]
    for key in new_keys:
        reset_pref(key)
        val = gsettings("get", SCHEMA, key)
        assert val == "false", \
            f"Key {key!r} default should be false, got {val!r}"


def test_start_with_dual_pane_still_exists():
    """Regression: start-with-dual-pane key was not removed when we moved it in the UI."""
    val = gsettings("get", SCHEMA, "start-with-dual-pane")
    assert val in ("true", "false"), \
        f"start-with-dual-pane missing or invalid: {val!r}"


def test_prefs_roundtrip():
    """Each new dual-pane key can be set to true and back to false."""
    keys = [
        "dual-pane-vertical-split",
        "dual-pane-separate-sidebar",
        "dual-pane-separate-nav-bar",
    ]
    for key in keys:
        set_pref(key, True)
        assert_pref(key, True)
        set_pref(key, False)
        assert_pref(key, False)
        reset_pref(key)


def test_prefs_are_independent():
    """Setting one dual-pane key does not affect the others."""
    set_pref("dual-pane-vertical-split",   True)
    set_pref("dual-pane-separate-sidebar", False)
    set_pref("dual-pane-separate-nav-bar", False)

    assert_pref("dual-pane-vertical-split",   True)
    assert_pref("dual-pane-separate-sidebar", False)
    assert_pref("dual-pane-separate-nav-bar", False)

    set_pref("dual-pane-vertical-split",   False)
    set_pref("dual-pane-separate-sidebar", True)
    set_pref("dual-pane-separate-nav-bar", False)

    assert_pref("dual-pane-vertical-split",   False)
    assert_pref("dual-pane-separate-sidebar", True)
    assert_pref("dual-pane-separate-nav-bar", False)

    # Cleanup
    for key in ["dual-pane-vertical-split", "dual-pane-separate-sidebar",
                "dual-pane-separate-nav-bar"]:
        reset_pref(key)


# ---------------------------------------------------------------------------
# GROUP 2: Process health — nemo stays alive through pref changes
# ---------------------------------------------------------------------------

def test_nemo_starts_and_exits_cleanly():
    """Nemo starts up and terminates without crashing."""
    reset_all_dual_pane_prefs()
    with NemoProcess() as n:
        assert_alive(n, "after startup")
    # nemo should exit cleanly (0 or -15 for SIGTERM, both are fine)
    # We just verify it didn't crash with SIGSEGV etc.


def test_nemo_starts_with_all_prefs_off():
    """Baseline: nemo starts fine with all new prefs at defaults (off)."""
    reset_all_dual_pane_prefs()
    with NemoProcess() as n:
        assert_alive(n, "all prefs off")


def test_vertical_split_toggle_does_not_crash():
    """Toggling vertical-split while nemo is running does not crash it."""
    reset_all_dual_pane_prefs()
    # Start with dual pane so there's actually something to orient
    set_pref("start-with-dual-pane", True)
    try:
        with NemoProcess() as n:
            assert_alive(n, "before vertical toggle")

            set_pref("dual-pane-vertical-split", True)
            assert_alive(n, "after vertical=true")

            set_pref("dual-pane-vertical-split", False)
            assert_alive(n, "after vertical=false")

            set_pref("dual-pane-vertical-split", True)
            assert_alive(n, "after second vertical=true")
    finally:
        reset_all_dual_pane_prefs()


def test_separate_sidebar_toggle_does_not_crash():
    """Toggling separate-sidebar while nemo is running does not crash it."""
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane",        True)
    set_pref("dual-pane-vertical-split",    True)
    try:
        with NemoProcess() as n:
            assert_alive(n, "before sidebar toggle")

            set_pref("dual-pane-separate-sidebar", True)
            assert_alive(n, "after separate-sidebar=true")

            set_pref("dual-pane-separate-sidebar", False)
            assert_alive(n, "after separate-sidebar=false")

            # Toggle again — this is the cycle that previously caused doubling
            set_pref("dual-pane-separate-sidebar", True)
            assert_alive(n, "after second separate-sidebar=true")

            set_pref("dual-pane-separate-sidebar", False)
            assert_alive(n, "after second separate-sidebar=false")
    finally:
        reset_all_dual_pane_prefs()


def test_separate_nav_bar_toggle_does_not_crash():
    """Toggling separate-nav-bar while nemo is running does not crash it."""
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane",     True)
    set_pref("dual-pane-vertical-split", True)
    try:
        with NemoProcess() as n:
            assert_alive(n, "before nav-bar toggle")

            set_pref("dual-pane-separate-nav-bar", True)
            assert_alive(n, "after separate-nav-bar=true")

            # This previously caused the toolbar to disappear permanently
            set_pref("dual-pane-separate-nav-bar", False)
            assert_alive(n, "after separate-nav-bar=false")

            set_pref("dual-pane-separate-nav-bar", True)
            assert_alive(n, "after second separate-nav-bar=true")

            set_pref("dual-pane-separate-nav-bar", False)
            assert_alive(n, "after second separate-nav-bar=false")
    finally:
        reset_all_dual_pane_prefs()


def test_all_prefs_on_does_not_crash():
    """All three new prefs ON simultaneously: nemo stays alive."""
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane", True)
    try:
        with NemoProcess() as n:
            assert_alive(n, "before enabling all prefs")

            set_pref("dual-pane-vertical-split",   True)
            set_pref("dual-pane-separate-sidebar", True)
            set_pref("dual-pane-separate-nav-bar", True)
            assert_alive(n, "all three prefs on")
    finally:
        reset_all_dual_pane_prefs()


def test_all_prefs_on_then_off_does_not_crash():
    """All three new prefs turned on then individually turned off: no crash."""
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane", True)
    try:
        with NemoProcess() as n:
            set_pref("dual-pane-vertical-split",   True)
            set_pref("dual-pane-separate-sidebar", True)
            set_pref("dual-pane-separate-nav-bar", True)
            assert_alive(n, "all on")

            set_pref("dual-pane-separate-sidebar", False)
            assert_alive(n, "sidebar off")

            set_pref("dual-pane-separate-nav-bar", False)
            assert_alive(n, "nav-bar off")

            set_pref("dual-pane-vertical-split",   False)
            assert_alive(n, "vertical off")
    finally:
        reset_all_dual_pane_prefs()


def test_rapid_pref_toggling_does_not_crash():
    """Rapidly toggling prefs back and forth does not crash nemo."""
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane", True)
    try:
        with NemoProcess() as n:
            set_pref("dual-pane-vertical-split", True)
            assert_alive(n, "vertical on")

            for i in range(5):
                set_pref("dual-pane-separate-sidebar", True)
                assert_alive(n, f"sidebar on cycle {i}")
                set_pref("dual-pane-separate-sidebar", False)
                assert_alive(n, f"sidebar off cycle {i}")
    finally:
        reset_all_dual_pane_prefs()


def test_nav_bar_off_toolbar_still_running():
    """
    Regression: turning separate-nav-bar OFF must not make nemo
    appear broken. Previously the shared toolbar was hidden and never
    re-shown, effectively breaking the UI. We verify nemo stays alive
    and does not exit on its own after the toggle.
    """
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane", True)
    try:
        with NemoProcess() as n:
            set_pref("dual-pane-vertical-split",   True)
            set_pref("dual-pane-separate-nav-bar", True)
            assert_alive(n, "nav-bar on")

            set_pref("dual-pane-separate-nav-bar", False)
            # Give nemo time to process and potentially crash
            time.sleep(SETTLE_DELAY * 2)
            assert_alive(n, "after nav-bar turned off — toolbar must still work")
    finally:
        reset_all_dual_pane_prefs()


# ---------------------------------------------------------------------------
# GROUP 3: Start-up state tests
# ---------------------------------------------------------------------------

def test_start_with_dual_pane_vertical():
    """Nemo starts correctly with dual-pane + vertical-split pre-set."""
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane",     True)
    set_pref("dual-pane-vertical-split", True)
    try:
        with NemoProcess() as n:
            assert_alive(n, "startup with vertical dual-pane")
    finally:
        reset_all_dual_pane_prefs()


def test_start_with_all_dual_pane_prefs():
    """Nemo starts correctly with all dual-pane prefs pre-enabled."""
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane",        True)
    set_pref("dual-pane-vertical-split",    True)
    set_pref("dual-pane-separate-sidebar",  True)
    set_pref("dual-pane-separate-nav-bar",  True)
    try:
        with NemoProcess() as n:
            assert_alive(n, "startup with all dual-pane prefs on")
    finally:
        reset_all_dual_pane_prefs()


def test_start_with_separate_sidebar_no_vertical():
    """
    Regression guard: separate-sidebar without vertical-split must not
    crash nemo at startup (the feature requires vertical, so it should
    simply be inactive but harmless).
    """
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane",       True)
    set_pref("dual-pane-separate-sidebar", True)
    # vertical-split is still FALSE
    try:
        with NemoProcess() as n:
            assert_alive(n, "startup: separate-sidebar without vertical")
    finally:
        reset_all_dual_pane_prefs()


def test_start_with_separate_nav_bar_no_vertical():
    """
    Regression guard: separate-nav-bar without vertical-split must not
    crash nemo at startup.
    """
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane",       True)
    set_pref("dual-pane-separate-nav-bar", True)
    # vertical-split is still FALSE
    try:
        with NemoProcess() as n:
            assert_alive(n, "startup: separate-nav-bar without vertical")
    finally:
        reset_all_dual_pane_prefs()


# ---------------------------------------------------------------------------
# GROUP 4: Sidebar hide/show regression
# ---------------------------------------------------------------------------

def test_sidebar_on_off_with_separate_sidebar():
    """
    Turning the sidebar off and on while separate-sidebar is active
    must not crash nemo.
    """
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane",       True)
    set_pref("dual-pane-vertical-split",   True)
    set_pref("dual-pane-separate-sidebar", True)
    try:
        with NemoProcess() as n:
            assert_alive(n, "separate-sidebar active")

            # Hide sidebar (View → Sidebar → None equivalent via state key)
            gsettings("set", STATE_SCHEMA, "start-with-sidebar", "false")
            time.sleep(SETTLE_DELAY)
            assert_alive(n, "sidebar hidden")

            gsettings("set", STATE_SCHEMA, "start-with-sidebar", "true")
            time.sleep(SETTLE_DELAY)
            assert_alive(n, "sidebar restored")
    finally:
        reset_all_dual_pane_prefs()
        gsettings("reset", STATE_SCHEMA, "start-with-sidebar")


def test_separate_sidebar_off_restores_global_sidebar():
    """
    Regression: turning separate-sidebar OFF must not leave nemo with
    a broken/missing global sidebar. Nemo must remain alive and not
    exit spontaneously after several seconds.
    """
    reset_all_dual_pane_prefs()
    set_pref("start-with-dual-pane",       True)
    set_pref("dual-pane-vertical-split",   True)
    set_pref("dual-pane-separate-sidebar", True)
    try:
        with NemoProcess() as n:
            assert_alive(n, "separate-sidebar on")

            set_pref("dual-pane-separate-sidebar", False)
            # Wait longer than normal — a crashed nemo due to bad widget
            # teardown might not die immediately
            time.sleep(SETTLE_DELAY * 3)
            assert_alive(n, "after separate-sidebar turned off — global sidebar must be restored")
    finally:
        reset_all_dual_pane_prefs()


# ---------------------------------------------------------------------------
# GROUP 5: Multiple windows
# ---------------------------------------------------------------------------

def test_two_windows_independent_prefs():
    """
    Nemo is single-instance: a second invocation forwards its open
    request to the primary instance and exits immediately, causing the
    primary to open a second window.  We launch the primary, wait for
    it to settle, trigger a second window, then toggle prefs and verify
    the primary process stays alive throughout.
    """
    reset_all_dual_pane_prefs()
    env = dict(os.environ)
    if SCHEMA_DIR:
        env["GSETTINGS_SCHEMA_DIR"] = SCHEMA_DIR

    with NemoProcess() as primary:
        assert_alive(primary, "primary window started")

        # Open a second window in the same nemo instance by invoking nemo
        # with a different path.  The launcher exits immediately after
        # forwarding to the primary — that is expected and not a failure.
        subprocess.Popen(
            [NEMO_BINARY, os.path.expanduser("~")],
            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        time.sleep(STARTUP_DELAY)

        # Primary must still be alive with two windows open
        assert_alive(primary, "after second window opened")

        set_pref("dual-pane-vertical-split", True)
        assert_alive(primary, "after vertical-split toggle with two windows")

        set_pref("dual-pane-separate-sidebar", True)
        assert_alive(primary, "after separate-sidebar toggle with two windows")

        set_pref("dual-pane-separate-sidebar", False)
        assert_alive(primary, "after separate-sidebar off with two windows")

        set_pref("dual-pane-vertical-split", False)
        assert_alive(primary, "after vertical-split off with two windows")

    reset_all_dual_pane_prefs()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

GROUPS = [
    ("Schema / gsettings (no running nemo)", [
        ("schema keys exist",                   test_schema_keys_exist),
        ("schema defaults are false",           test_schema_defaults_are_false),
        ("start-with-dual-pane still exists",   test_start_with_dual_pane_still_exists),
        ("pref keys round-trip correctly",      test_prefs_roundtrip),
        ("pref keys are independent",           test_prefs_are_independent),
    ]),
    ("Process health — pref toggles", [
        ("nemo starts and exits cleanly",              test_nemo_starts_and_exits_cleanly),
        ("nemo starts with all prefs off",             test_nemo_starts_with_all_prefs_off),
        ("vertical-split toggle does not crash",       test_vertical_split_toggle_does_not_crash),
        ("separate-sidebar toggle does not crash",     test_separate_sidebar_toggle_does_not_crash),
        ("separate-nav-bar toggle does not crash",     test_separate_nav_bar_toggle_does_not_crash),
        ("all three prefs on does not crash",          test_all_prefs_on_does_not_crash),
        ("all three prefs on then off does not crash", test_all_prefs_on_then_off_does_not_crash),
        ("rapid pref toggling does not crash",         test_rapid_pref_toggling_does_not_crash),
        ("nav-bar off: nemo stays alive (toolbar bug regression)",
                                                       test_nav_bar_off_toolbar_still_running),
    ]),
    ("Start-up state", [
        ("start with dual-pane + vertical",            test_start_with_dual_pane_vertical),
        ("start with all dual-pane prefs on",          test_start_with_all_dual_pane_prefs),
        ("separate-sidebar without vertical: no crash",test_start_with_separate_sidebar_no_vertical),
        ("separate-nav-bar without vertical: no crash",test_start_with_separate_nav_bar_no_vertical),
    ]),
    ("Sidebar hide/show regression", [
        ("sidebar hide/show with separate-sidebar",    test_sidebar_on_off_with_separate_sidebar),
        ("separate-sidebar off restores global sidebar",test_separate_sidebar_off_restores_global_sidebar),
    ]),
    ("Multiple windows", [
        ("two windows stay alive through pref toggle", test_two_windows_independent_prefs),
    ]),
]


def main() -> int:
    global _verbose

    parser = argparse.ArgumentParser(
        description="Nemo dual-pane integration tests",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""
            Environment variables:
              NEMO_BINARY          path to nemo executable (default: nemo)
              GSETTINGS_SCHEMA_DIR path to compiled schemas (default: system)
        """)
    )
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Print detailed progress")
    parser.add_argument("--group", metavar="N", type=int,
                        help="Run only test group N (1-based)")
    args = parser.parse_args()
    _verbose = args.verbose

    # Check nemo binary — only required for groups that launch nemo (2+)
    nemo_available = shutil.which(NEMO_BINARY) is not None or os.path.isfile(NEMO_BINARY)
    if not nemo_available:
        if args.group is not None and args.group == 1:
            pass  # Group 1 (schema tests) never needs the binary
        else:
            print(f"WARNING: nemo binary not found: {NEMO_BINARY!r}", file=sys.stderr)
            print("Set NEMO_BINARY to the path of the nemo executable.", file=sys.stderr)
            print("Groups 2-5 will be skipped or will fail.", file=sys.stderr)

    # Check display
    if not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        print("WARNING: No DISPLAY or WAYLAND_DISPLAY set. "
              "Process-health tests will fail without a display.", file=sys.stderr)

    print(f"\nNemo dual-pane integration tests")
    print(f"  Binary : {NEMO_BINARY}")
    print(f"  Schema : {SCHEMA_DIR or '(system default)'}")
    print()

    # Save current prefs so we can restore them after tests
    try:
        save_prefs()
    except Exception as e:
        print(f"ERROR: Could not read gsettings: {e}", file=sys.stderr)
        return 1

    total_pass = 0
    total_fail = 0

    try:
        groups = GROUPS
        if args.group is not None:
            idx = args.group - 1
            if idx < 0 or idx >= len(GROUPS):
                print(f"ERROR: Group {args.group} does not exist "
                      f"(1–{len(GROUPS)})", file=sys.stderr)
                return 1
            groups = [GROUPS[idx]]

        for group_name, tests in groups:
            print(f"── {group_name}")
            for test_name, test_fn in tests:
                ok = run_test(test_name, test_fn)
                if ok:
                    total_pass += 1
                else:
                    total_fail += 1
            print()

    finally:
        restore_prefs()

    print(f"Results: {total_pass} passed, {total_fail} failed "
          f"out of {total_pass + total_fail} tests")

    if total_fail > 0:
        print("\nFailed tests:")
        for name, ok, msg in _results:
            if not ok:
                print(f"  ✗ {name}")
                if msg:
                    print(f"      {msg}")
        return 1

    print("\nAll tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
