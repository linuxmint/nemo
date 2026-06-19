#!/usr/bin/env python3
import sys
import subprocess
import time
import os
import signal

def main():
    if len(sys.argv) < 2:
        print("Usage: test-dbus-fallback.py <nemo-binary>")
        sys.exit(1)

    nemo_bin = sys.argv[1]
    print(f"Using nemo binary: {nemo_bin}")

    # Start primary nemo instance in background
    # (Since we are inside dbus-run-session, we have our own isolated session bus)
    print("Starting primary Nemo instance...")
    primary = subprocess.Popen([nemo_bin, "--no-default-window"])

    # Give it some time to register on DBus
    time.sleep(2)

    # Suspend the primary instance (simulate frozen desktop/nemo process)
    print(f"Suspending primary Nemo instance (PID: {primary.pid})...")
    os.kill(primary.pid, signal.SIGSTOP)

    # Now launch the second instance to test the fallback
    print("Launching second Nemo instance (should fallback and exit)...")
    try:
        # We run it with timeout to make sure it doesn't hang if the fallback fails.
        result = subprocess.run(
            [nemo_bin, "--no-default-window"],
            capture_output=True,
            text=True,
            timeout=5
        )
        print("Second instance output (stdout):")
        print(result.stdout)
        print("Second instance output (stderr):")
        print(result.stderr)

        # Resume and clean up primary
        print("Cleaning up primary instance...")
        os.kill(primary.pid, signal.SIGCONT)
        primary.terminate()
        primary.wait()

        # Check stderr for the fallback warning
        expected_warning = "The process holding org.Nemo is frozen or unresponsive"
        if expected_warning not in result.stderr:
            print(f"Error: Expected warning not found in stderr. Stderr was:\n{result.stderr}")
            sys.exit(1)

        print("Test passed: Fallback warning was successfully logged and the application did not hang.")

    except subprocess.TimeoutExpired:
        print("Error: Second instance hung (timed out after 5 seconds)!")
        # Make sure to cleanup primary
        os.kill(primary.pid, signal.SIGCONT)
        primary.terminate()
        primary.wait()
        sys.exit(1)

if __name__ == '__main__':
    main()
