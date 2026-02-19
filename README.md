# Usage
## build
build
```sh
make <all/lib/cli>
```

clean
```sh
make clean
```

change library function names for cli
```sh
make XOR_KEY_FN=xor_key XOR_ENC_FN=xor_encrypt
```

## run cli
```sh
./build/main ./build/libcaesar.so <key> ./data/test.txt ./data/out.txt
```
