#!/bin/bash

SRCDIR="."
OBJDIR="release"
EXT="cpp"

make release || exit 1

for i in "$SRCDIR"/*.$EXT ; do
    name=$(echo "$i" | sed 's/\.\'$EXT'$//;s|^./||')
    lines=$(wc -l "$i" | awk '{print $1}')
    objfile="$OBJDIR/$name.o"
    [ -f "$objfile" ] || continue
    bytes=$(wc -c "$objfile" | awk '{print $1}')
    coeff=$((bytes / lines))
    echo "$coeff  -  $name ($bytes/$lines)"
    cpp_names="$cpp_names $name.cpp"
done

lines=$(wc -l $cpp_names *.h | tail -1 | awk '{print $1}')
objfile="shn"
bytes=$(wc -c "$objfile" | awk '{print $1}')
coeff=$((bytes / lines))
echo "$coeff  -  *.cpp *.h ($bytes/$lines)"
