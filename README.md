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
build with TEST_SIGSEGV
```sh
make TEST_SIGSEGV=1
```

## run
```sh
mkdir data
unzip test_data/data.zip -d data
mkdir data/out
bin/main $(find data/ -maxdepth 1 -type f) data/out/ a --mode=sequential
```
## info
using `ts_queue` from https://github.com/kroften4/cpong
