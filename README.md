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
