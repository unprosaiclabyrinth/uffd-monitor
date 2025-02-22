# ![](images/title.png)

# Overview

Uffd-monitor is a userspace code execution monitor that reduces executable code in memory. It does this by copying the code pages of the target executable 

# Get Started

## Prerequisites

Install packages required by [CRIU](https://criu.org/Installation). You can use the following command on Ubuntu 22.04:-
```bash
sudo apt update
sudo apt upgrade -y
sudo apt install -y libprotobuf-dev libprotobuf-c-dev protobuf-c-compiler protobuf-compiler \
    pkg-config libnl-3-dev libnet1-dev libcap-dev libbsd-dev python3-pip cmake
```
Build CRIU v3.19:-
```bash
git submodule update --init --recursive
make -C criu-3.19 -j`nproc`
```

## Build and run

Build the monitor simply by using:-
```bash
make
```
This will create a shared library file called `libuffd.so`, which is the monitor. As aforementioned, the monitor essentially creates a sliding window over the code pages of the target executable. You can set the size of this sliding window using the environment variable `UFFD_MONITOR_SIZE`. The default size is 1 page. Also, optionally, you can choose for a monitor log to be dumped to stderr by setting the environment variable `UFFD_LOG_DUMP` to 1. In order to run an application under the monitor, it must load the monitor (a shared library). To ensure this, you can set the environment variable `LD_PRELOAD` to `./libuffd.so`. Hence, you can run the application under the monitor using:-
```bash
sudo UFFD_MONITOR_SIZE=1 UFFD_LOG_DUMP=1 LD_PRELOAD=./libuffd.so ./application
```
By default, **Linux 5.11+** restricts `userfaultfd` to privileged users unless explicitly allowed. You can check the permission with:-
```bash
cat /proc/sys/vm/unprivileged_userfaultfd
```
If set to 1, unprivileged users can run `userfaultfd` without `sudo`.

## Demo

You can see some simple applications in action using the following `make` targets:-
```bash
make run1 # run a simple I/O application
make run2 # run a simple counter that keeps incrementing
```
You can see a little bigger application, a tiny web epoll server, in action using the `make` target:-
```bash
make run4 # run a single-processed epoll server
```
You can see a real-world application, [lighttpd](https://www.lighttpd.net/), in action using the make target:-
```bash
make run5 # run lighttpd
```
You can configure the `UFFD_MONITOR_SIZE` and the `UFFD_LOG_DUMP` option in the Makefile.

# Performance Evaluation

## Setup

## Results