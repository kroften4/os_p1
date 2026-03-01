# Usage
## build
build
```sh
make
```

clean
```sh
make clean
```

## run cli
`main` will look for `libcaesar.so` in the same directory
```sh
bin/main <src_file> <dst_file> <key> 
```
test encrypting and decrypting back
```sh
scripts/encrypt_decrypts.sh ./data/hello.txt <key> 
```
