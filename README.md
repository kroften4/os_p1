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

## run
- cli
  ```sh
  bin/main <src_file> <dst_file> <key> 
  ```
- unzip test data
  ```sh
  mkdir data
  unzip test_data/data.zip -d data
  ```
- test encrypting and decrypting back
  ```sh
  scripts/encrypt_decrypt.sh ./data/hello.txt <key> 
  ```

## info
using `ts_queue` from https://github.com/kroften4/cpong
