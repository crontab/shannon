#!/bin/bash

OBJDIR="release"
EXT="cpp"

for i in *.$EXT ; do
    name=$(echo $i | sed 's/\.\'$EXT'$//')
    lines=$(wc -l "$name.$EXT" | awk '{print $1}')
    objfile="$OBJDIR/$name.o"
    [ -f "$objfile" ] || continue
    bytes=$(wc -c "$objfile" | awk '{print $1}')
    coeff=$((bytes / lines))
    echo "$name.$EXT: $coeff  ($bytes/$lines)"
done
