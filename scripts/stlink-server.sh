#!/bin/sh
# Start (or restart) the ST-LINK GDB server for CubeIDE debug sessions.
#
#   ./scripts/stlink-server.sh          start / restart
#   ./scripts/stlink-server.sh status   is it running, is a probe attached
#   ./scripts/stlink-server.sh stop     kill it
#   ./scripts/stlink-server.sh log      tail the server log
#
# CubeIDE's "GDB Hardware Debugging" launch expects a server already listening
# on port 61234 -- it does not start one itself. Run this before hitting Debug.
#
# The -k flag (connect under reset) is required: without it the server often
# reports "Target no device found" on this board.

PORT=61234
LOG=/tmp/stlink_gdbserver.log
PLUGINS=/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins

GDBSRV=$(find "$PLUGINS" -maxdepth 4 -type f -name 'ST-LINK_gdbserver' 2>/dev/null | head -1)
CUBEPROG=$(find "$PLUGINS" -maxdepth 4 -type d -path '*cubeprogrammer*/tools/bin' 2>/dev/null | head -1)

if [ -z "$GDBSRV" ] || [ -z "$CUBEPROG" ]; then
  echo "error: could not locate ST-LINK GDB server or STM32CubeProgrammer under"
  echo "       $PLUGINS"
  echo "       Is STM32CubeIDE installed in /Applications?"
  exit 1
fi

kill_server() {
  pkill -f 'ST-LINK_gdbserver' 2>/dev/null
  # a wedged gdb client holds the socket open and blocks the next launch
  pkill -f 'arm-none-eabi-gdb --interpreter' 2>/dev/null
  sleep 2
  pkill -9 -f 'ST-LINK_gdbserver' 2>/dev/null
  sleep 1
}

case "$1" in
  stop)
    kill_server
    echo "stopped."
    exit 0
    ;;
  log)
    tail -f "$LOG"
    exit 0
    ;;
  status)
    if lsof -i :"$PORT" >/dev/null 2>&1; then
      echo "server: LISTENING on $PORT"
    else
      echo "server: not running"
    fi
    printf 'probe:  '
    cd "$(dirname "$GDBSRV")" || exit 1
    ./ST-LINK_gdbserver -q -cp "$CUBEPROG" 2>/dev/null | grep ST-LINK || echo "NO PROBE DETECTED (check USB)"
    exit 0
    ;;
esac

kill_server

cd "$(dirname "$GDBSRV")" || exit 1

if ! ./ST-LINK_gdbserver -q -cp "$CUBEPROG" 2>/dev/null | grep -q ST-LINK; then
  echo "error: no ST-LINK detected over USB."
  echo "       Unplug and replug the probe, then run this again."
  exit 1
fi

# SWD attach can fail on the first try; a couple of retries is normal.
i=1
while [ "$i" -le 3 ]; do
  nohup ./ST-LINK_gdbserver -p "$PORT" -d -e -k -cp "$CUBEPROG" -v > "$LOG" 2>&1 &
  sleep 4
  if lsof -i :"$PORT" >/dev/null 2>&1; then
    echo "ST-LINK GDB server listening on $PORT  (log: $LOG)"
    echo "You can hit Debug in STM32CubeIDE now."
    exit 0
  fi
  echo "attempt $i failed, retrying..."
  i=$((i + 1))
  sleep 1
done

echo "error: server did not come up. Last lines of $LOG:"
tail -15 "$LOG"
exit 1
