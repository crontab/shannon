#!/bin/bash

SRCDIR="."
OBJDIR="release"
EXT="cpp"

for i in "$SRCDIR"/*.$EXT ; do
    name=$(echo "$i" | sed 's/\.\'$EXT'$//')
    lines=$(wc -l "$i" | awk '{print $1}')
    objfile="$OBJDIR/$name.o"
    [ -f "$objfile" ] || continue
    bytes=$(wc -c "$objfile" | awk '{print $1}')
    coeff=$((bytes / lines))
    echo "$coeff  -  $name ($bytes/$lines)"
done
