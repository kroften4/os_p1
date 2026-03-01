#!/usr/bin/env bash

DEST_ENC=data/out.txt
DEST_DEC=data/out2.txt

bin/main $1 $DEST_ENC $2
echo "encrypted into $DEST_ENC"
bin/main $DEST_ENC $DEST_DEC $2
echo "decrypted into $DEST_DEC"
