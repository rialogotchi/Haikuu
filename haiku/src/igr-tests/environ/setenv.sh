#!/bin/sh

cd ..

case "$1" in
    setenv1)
        if [ "$FOO" = "foo" ] && [ "$BAR" = "bar" ] && [ "$BAZ" = "baz" ]; then
            echo 1 >> "$OUTPUT"
        fi
        ;;
    setenv2)
        if [ "$FOO" = "foo" -a -z "$BAR" ]; then
            echo 2 >> "$OUTPUT"
            export BAR=bar
            "$HAIKUCTL" -p "$SOCKET" setenv BAR BAZ=baz
        fi
        ;;
    setenv3)
        "$HAIKUCTL" -p "$SOCKET" setenv FOO=foo
        "$HAIKUCTL" -p "$SOCKET" unsetenv BAR
        echo 3 >> "$OUTPUT"
        ;;
    *) ;;
esac
