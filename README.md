# Webserv

The webserv project from 42 School.

This is a simple clone of [nginx](https://nginx.org/en/) webserver, made from scratch in C++ 98.

The goal of this project is to create a program that acts as a HTTP web server, capable of serving static websites, processing CGI requests, displaying local filesystem or acting as a proxy.

## Building instructions

Make sure you have installed all the necessary dependencies:
* On Debian/Ubuntu: `sudo apt install make clang`

After installing dependencies, `cd` into the repository directory and run `make`.

## Working with the project

Running `make` simply builds a release-suitable build with no debug symbols included. To make a debuggable build, run `make debug`. This will produce a binary that you can debug with `gdb` or `lldb`.

`make clean` cleans up the project directory from build artifacts (object files), whereas `make fclean` additionally deletes the resulting binary.

`make redb` and `make re` recompiles the project with and without debugging symbols respectively.

### Development guidelines

When working on this project, try to follow these as much as you can:

0) Keep the header files in the source directory - for better or worse, this is the project structure I've decided to go with with all of my C/C++ projects, as it allows for a simple-ish level of composability amongst them with the makefile I use.

That's it!

## Running the webserver

You can start the webserver by running the `webserver` executable.

```
./webserv /path/to/config/file
```

`webserv` can take an optional path to the configuration file as a single parameter. If the path was not provided - it uses `wsconf.wsv` file located in the current working directory.

## Configuration file

TODO.

## Technical details

TODO.