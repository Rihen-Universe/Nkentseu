#!/bin/bash

COMMAND=$1
shift # Retire le premier argument

case $COMMAND in
    "install")
        ./tools/install.sh "$@"
        ;;
    "gen")
        ./tools/gen.sh "$@"
        ;;
    "build")
        ./tools/build.sh "$@"
        ;;
    "clear")
        ./tools/clear.sh "$@"
        ;;
    "run")
        ./tools/run.sh "$@"
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