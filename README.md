# Webserv

The webserv project from 42 School.

This is a simple clone of [nginx](https://nginx.org/en/) webserver, made from scratch in C++ 98.

The goal of this project is to create a program that acts as a HTTP web server, capable of serving static websites, processing CGI requests, displaying local filesystem or acting as a proxy.

## Building instructions

Make sure you have installed all the necessary dependencies:
* On Debian/Ubuntu: `sudo apt install make clang`

After installing dependencies, `cd` into the repository directory and invoke `make`.

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