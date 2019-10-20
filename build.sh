#!/bin/bash

set -e

echo Building...

curDir="$(pwd)"
codeDir="$curDir/src"
buildDir="$curDir/gebouw"

flags="-O0 -g -ggdb -Wall -Werror -pedantic -std=c++11"

exceptions="-Wno-unused-function -Wno-writable-strings -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-missing-braces"

mkdir -p "$buildDir"

pushd "$buildDir" > /dev/null
    clang++ $flags $exceptions "$codeDir/flac_decode.cpp" -o flacdecode -lasound
    clang++ $flags $exceptions "$codeDir/wav_decode.cpp" -o wavdecode -lasound
    clang++ $flags $exceptions "$codeDir/sound.cpp" -o make-sound -lasound
popd > /dev/null

