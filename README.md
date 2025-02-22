# ![](images/title.png)

# Overview

# Get Started

Install packages required by [CRIU](https://criu.org/Installation). You can use the following command on Ubuntu 22.04:-
```
sudo apt update
sudo apt upgrade -y
sudo apt install -y libprotobuf-dev libprotobuf-c-dev protobuf-c-compiler protobuf-compiler \
    pkg-config libnl-3-dev libnet1-dev libcap-dev libbsd-dev python3-pip cmake
```
Build CRIU v3.19:-
```
git submodule update --init --recursive
make -C criu-3.19 -j`nproc`
```

```sh
make run1 # run test1: simple I/O
make run2 # run test2: counter
make run3 # run test3: epoll server
make run4 # run test4: tiny web server with 2 child processes
```

# Performance Evaluation

## Setup

## Results