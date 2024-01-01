#!/bin/bash

case "$1" in
    start)
        echo "Starting aesdsocket"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
    ;;
    stop)
        echo "Stoping aesdsocket"
        start-stop-daemon -K -n aesdsocket --signal SIGTERM
    ;;
    *)
        echo "Usage: $0 {start|stop}"
    ;;
esac
