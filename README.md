# MemoryAccessor

MemoryAccessor is a console tool that makes easier performing reading/writing on /proc/PID/mem files, which represent virtual memory of processes. Such operations can be done by running specific commands.

## Usage

### Commands

If program starts without additional arguments, it awaits command. By typing "help" list of available commands is shown.

First it is necessary to choose PID to operate with. It can be done with command 

    pid PID

or 

    name process_name [PID number]

The PID is be checked if it exists, and the /proc/PID/maps are parsed to get information about memory segments. This info is shown with the command "maps".

Now reading/writing operations can be performed. Data can be read by command

    read address amount

or written by commands

    write address amount string

or

    write address amount -f file

The program can wait for some process by command "await":

    await process_name

or

    await -p pid

Finally, command "diff" is available, which creates dump of process memory and starts constantly updating it, trying to find differences of exact length and possibly replace it to string:

    diff length [replacement]


### Start arguments

Start program with "--help" to see info about arguments.

One command can be run by typing "--command COMMAND".

Commands from file can be executed line-by-line by specifying file: "--file PATH"

## Build

### Dependencies

To build MemoryAccessor, you need:

| Software                                 | Debian/Ubuntu package | RHEL/CentOS package|
|------------------------------------------|-----------------------|--------------------|
|CMake                                     | cmake                 | cmake              |
|A C++20 compiler (e.g. gcc 8+)            | g++                   | gcc-c++            |
|GNU ReadLine library                      | libreadline-dev       | libreadline-devel  |
|(optional, for testing) Doctest framework | doctest-dev           | doctest-devel      |

### Building

To build main binary, run:

    mkdir build
    cd build
    cmake ..
    cmake --build . --target MemoryAccessor -j

To build tests, run:

    cmake --build . --target project_test -j

### Generating documentation

Doxygen is used for documentation. To generate docs, run:

    rm -rf doc
    cd src
    doxygen Doxyfile
    cd ../testing
    rm -rf doc
    doxygen Doxyfile

## License

[![license](https://img.shields.io/badge/License-GNU%20GPL-blue)](https://github.com/zloymish/memac/blob/develop/LICENSE)

This project is licensed under the terms of the [GPL-3.0 license](/LICENSE).
