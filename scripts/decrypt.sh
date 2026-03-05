#!/usr/bin/env bash

SRC=data/out.txt
DEST=data/out2.txt

bin/main $SRC $DEST $1
echo "decrypted into $DEST"
