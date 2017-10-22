SMUX Stream MUltiplexer library
===============================

The purpose of this library is to multiplex multiple virtual communication channels
over a single "physical" channel. See file `include/smux.h` for detailed interface
documentation. For convenience, a c++ wrapper is available at `include/smux.hpp`.

Dependencies and Compiling
--------------------------

The library itself has no dependencies beyond standard c/c++ lib. For compiling, go
into directory `lib` and type `make`.

However, in order to run the unit tests, Boost.Test is required (lib boost\_unit\_test\_framework).
To build and run the tests, go into directory `lib` and type `make test`.

SMUX Host Application
=====================

The host application is intended to be run on a Unix-like system and provides means to connect
various applications to the individual channels. In order to allow the largest possible flexibility,
it integrates with the great `socat` tool that offers all the tool you need to connect to whatever
type of sink or source you like to connect to.

Compiling
---------

The host application can be built from the root directory by running `make`. The application will
be compiled as bin/main/smux/smux.

Usage
-----

Run `smux -h` for a brief explanation of the usage of the program.

Examples
--------

Connect the master side to a console at /dev/pts/16 and open terminals for the channels 0 and 42:

    smux -m file:/dev/pts/16 -c 0=socat:pty,rawer -c 42=socat:pty,rawer

Read master side from stdin, write master to a file/pipe called "out" and connect channel 12 to
stdin/stdout of a "program".

    smux -m stdio%file:out 12=exec:program

