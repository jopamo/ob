#!/bin/sh
# Simple Xvfb wrapper script similar to xvfb-run

set -e

# Default values
SERVER_NUM=99
SCREEN="1280x800x24"
WAIT_TIME=5
DISPLAY=":$SERVER_NUM"
AUTO_SERVER_NUM=false
XVFB_ARGS=""

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --auto-servernum)
            AUTO_SERVER_NUM=true
            shift
            ;;
        --server-num)
            SERVER_NUM="$2"
            DISPLAY=":$SERVER_NUM"
            shift 2
            ;;
        --screen)
            SCREEN="$2"
            shift 2
            ;;
        --wait)
            WAIT_TIME="$2"
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *)
            # Unknown option, probably the command
            break
            ;;
    esac
done

if [ $# -eq 0 ]; then
    echo "Error: No command specified"
    exit 1
fi

# Find Xvfb
XVFB_PATH=$(command -v Xvfb)
if [ -z "$XVFB_PATH" ]; then
    echo "Error: Xvfb not found in PATH"
    exit 1
fi

# Find xdpyinfo for display verification
XDPYINFO_PATH=$(command -v xdpyinfo)

# Auto find server number if requested
if [ "$AUTO_SERVER_NUM" = true ]; then
    SERVER_NUM=99
    while [ $SERVER_NUM -lt 200 ]; do
        LOCKFILE="/tmp/.X$SERVER_NUM-lock"
        if [ ! -f "$LOCKFILE" ]; then
            DISPLAY=":$SERVER_NUM"
            break
        fi
        SERVER_NUM=$((SERVER_NUM + 1))
    done
    if [ $SERVER_NUM -ge 200 ]; then
        echo "Error: Could not find free server number (tried 99-199)"
        exit 1
    fi
fi

# Start Xvfb
echo "Starting Xvfb on display $DISPLAY with screen $SCREEN..." >&2
"$XVFB_PATH" "$DISPLAY" -screen 0 "$SCREEN" -nolisten tcp &
XVFB_PID=$!

# Wait for Xvfb to be ready
READY=false
END_TIME=$(( $(date +%s) + WAIT_TIME ))
while [ $(date +%s) -lt $END_TIME ]; do
    if [ -n "$XDPYINFO_PATH" ]; then
        if "$XDPYINFO_PATH" -display "$DISPLAY" >/dev/null 2>&1; then
            READY=true
            break
        fi
    else
        # Check if lock file exists (crude but works)
        LOCKFILE="/tmp/.X${SERVER_NUM}-lock"
        if [ -f "$LOCKFILE" ]; then
            sleep 0.1
            READY=true
            break
        fi
    fi
    sleep 0.1
done

if [ "$READY" = false ]; then
    echo "Error: Xvfb failed to start on display $DISPLAY within ${WAIT_TIME}s" >&2
    kill $XVFB_PID 2>/dev/null || true
    wait $XVFB_PID 2>/dev/null || true
    exit 1
fi

echo "Xvfb ready on display $DISPLAY" >&2

# Run the command
export DISPLAY
RET=0
"$@" || RET=$?

# Clean up Xvfb
echo "Shutting down Xvfb..." >&2
kill $XVFB_PID 2>/dev/null || true
wait $XVFB_PID 2>/dev/null || true

exit $RET