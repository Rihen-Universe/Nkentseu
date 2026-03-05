#!/bin/bash

COMMAND=$1
shift # Retire le premier argument

case $COMMAND in
    "install")
        ./Tools/install.sh "$@"
        ;;
    "gen")
        ./Tools/gen.sh "$@"
        ;;
    "build")
        ./Tools/build.sh "$@"
        ;;
    "clear")
        ./Tools/clear.sh "$@"
        ;;
    "debug")
        ./Tools/debug.sh "$@"
        ;;
    "run")
        ./Tools/run.sh "$@"
        ;;
    *)
        echo "Usage: ./nken.sh [gen|build|clear|run] [options]"
        echo "Exemples:"
        echo "  ./nken.sh gen --arch=avx2"
        echo "  ./nken.sh build debug"
        echo "  ./nken.sh run --test"
        exit 1
        ;;
esac