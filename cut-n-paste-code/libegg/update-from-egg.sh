#!/bin/sh

function die() {
  echo $*
  exit 1
}

if test -z "$EGGDIR"; then
   echo "Must set EGGDIR"
   exit 1
fi

if test -z "$EGGFILES"; then
   echo "Must set EGGFILES"
   exit 1
fi

for FILE in $EGGFILES; do
  if cmp -s $EGGDIR/$FILE $FILE; then
     echo "File $FILE is unchanged"
  else
     cp $EGGDIR/$FILE $FILE || die "Could not move $EGGDIR/$FILE to $FILE"
     echo "Updated $FILE"
  fi
done
