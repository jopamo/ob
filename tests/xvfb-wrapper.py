#!/usr/bin/env python3
"""
xvfb-wrapper.py - A simple Xvfb wrapper similar to xvfb-run
"""

import os
import sys
import subprocess
import signal
import time
import argparse
import tempfile
import shutil

def find_xvfb():
    """Find Xvfb executable."""
    xvfb_path = shutil.which('Xvfb')
    if not xvfb_path:
        # Try common paths
        for path in ['/usr/bin/Xvfb', '/usr/local/bin/Xvfb']:
            if os.path.exists(path) and os.access(path, os.X_OK):
                xvfb_path = path
                break
    return xvfb_path

def find_xdpyinfo():
    """Find xdpyinfo for display verification."""
    return shutil.which('xdpyinfo')

def main():
    parser = argparse.ArgumentParser(
        description='Run a command in a virtual X server (Xvfb)',
        epilog='Example: xvfb-wrapper.py -- python -c "import os; print(os.environ[\"DISPLAY\"])"'
    )
    parser.add_argument('-a', '--auto-servernum', action='store_true',
                       help='Automatically choose a free server number')
    parser.add_argument('-s', '--server-num', type=int, default=99,
                       help='Server number to use (default: 99)')
    parser.add_argument('-f', '--fbdir', help='Framebuffer directory')
    parser.add_argument('-p', '--port', type=int, help='TCP port to listen on')
    parser.add_argument('-n', '--nolisten', default='tcp',
                       help='Protocol to disable (default: tcp)')
    parser.add_argument('-d', '--display', help='Display to use (overrides server-num)')
    parser.add_argument('-w', '--wait', type=float, default=5.0,
                       help='Seconds to wait for Xvfb to start (default: 5)')
    parser.add_argument('-e', '--error-file', help='File to redirect Xvfb stderr')
    parser.add_argument('--screen', default='1280x800x24',
                       help='Screen specification (default: 1280x800x24)')
    parser.add_argument('command', nargs=argparse.REMAINDER,
                       help='Command to run under Xvfb')

    args = parser.parse_args()

    if not args.command:
        parser.error('No command specified')

    # Determine display
    if args.display:
        display = args.display
        server_num = int(display.split(':')[1].split('.')[0])
    else:
        if args.auto_servernum:
            # Find a free server number
            server_num = 99
            for i in range(99, 200):
                lockfile = f'/tmp/.X{i}-lock'
                if not os.path.exists(lockfile):
                    server_num = i
                    break
        else:
            server_num = args.server_num
        display = f':{server_num}'

    # Build Xvfb command
    xvfb_cmd = [find_xvfb()]
    if not xvfb_cmd[0]:
        print(f"ERROR: Xvfb not found in PATH", file=sys.stderr)
        sys.exit(1)

    xvfb_cmd.append(display)

    # Add screen argument
    if args.screen:
        xvfb_cmd.extend(['-screen', '0', args.screen])

    # Add other options
    if args.fbdir:
        xvfb_cmd.extend(['-fbdir', args.fbdir])
    if args.port:
        xvfb_cmd.extend(['-port', str(args.port)])
    if args.nolisten:
        xvfb_cmd.extend(['-nolisten', args.nolisten])

    # Set up error redirection
    stderr_dest = subprocess.DEVNULL
    if args.error_file:
        stderr_dest = open(args.error_file, 'w')

    # Start Xvfb
    print(f"Starting Xvfb on display {display}...", file=sys.stderr)
    xvfb_proc = subprocess.Popen(xvfb_cmd, stderr=stderr_dest)

    # Wait for Xvfb to be ready
    xdpyinfo = find_xdpyinfo()
    start_time = time.time()
    ready = False

    while time.time() - start_time < args.wait:
        try:
            # Check if display is available
            if xdpyinfo:
                subprocess.run([xdpyinfo, '-display', display],
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL,
                              timeout=1)
            else:
                # Simple check: see if the lock file exists
                lockfile = f'/tmp/.X{server_num}-lock'
                if os.path.exists(lockfile):
                    # Additional small delay for X server initialization
                    time.sleep(0.1)
            ready = True
            break
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError, FileNotFoundError):
            time.sleep(0.1)

    if not ready:
        print(f"ERROR: Xvfb failed to start on display {display} within {args.wait} seconds", file=sys.stderr)
        xvfb_proc.terminate()
        xvfb_proc.wait(timeout=5)
        sys.exit(1)

    print(f"Xvfb ready on display {display}", file=sys.stderr)

    # Run the command
    env = os.environ.copy()
    env['DISPLAY'] = display

    try:
        cmd_result = subprocess.run(args.command, env=env)
        return_code = cmd_result.returncode
    finally:
        # Clean up Xvfb
        print(f"Shutting down Xvfb...", file=sys.stderr)
        xvfb_proc.terminate()
        try:
            xvfb_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            xvfb_proc.kill()
            xvfb_proc.wait()

        if args.error_file and stderr_dest != subprocess.DEVNULL:
            stderr_dest.close()

    sys.exit(return_code)

if __name__ == '__main__':
    main()