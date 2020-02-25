# Pintos
Labs for undergraduate OS class (EN.601.318/418) at Johns Hopkins CS. [Pintos](http://pintos-os.org) 
is a teaching operating system for x86, challenging but not overwhelming, small
but realistic enough to understand OS in depth (it can run x86 machine and simulators 
including QEMU, Bochs and VMWare Player!). The main source code, documentation and assignments 
are developed by Ben Pfaff and others from Stanford (refer to its [LICENSE](./LICENSE)).

## Changes
The course instructor ([Ryan Huang](huang@cs.jhu.edu)) made some changes to the original
Pintos labs to tailor for his class. They include:
* Tweaks to Makefiles for tool chain configuration and some new targets
* Make qemu image and run it in non-test mode.
* Tweaks and patches to get Pintos run and test on Mac OS
* Color testing result
* Bochs build fix
* Various bug fixes
* Lab 0
* Misc enhancement

**This is the upstream branch to track the changes. For students in the class, please clone
or download the release version from [here](https://github.com/jhu-cs318/pintos.git)
to work on the projects.**

## Setup
The instructor wrote a detailed [guide](https://cs.jhu.edu/~huang/cs318/fall17/project/setup.html) on 
how to setup the toolchain and development environment to work on Pintos projects.

## Instructions

### Debugging

```bash
cd src/threads
make
pintos --gdb -- run mytest

# in another window
cd build
pintos-gdb kernel.o
target remote localhost:1234
```

Running: 

```
make qemu
```

### Testing

To run unit tests run:

```
cd src/unit_tests
make test
```

> Note: runtime for tests is userland. Useful for testing pure functions 