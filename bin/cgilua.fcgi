#!/bin/sh

LUAROCKS_SYSCONFDIR='/etc/luarocks' exec '/usr/bin/lua5.4' -e 'package.path="/home/ygorko/Roma42/webserv-0/./share/lua/5.4/?.lua;/home/ygorko/Roma42/webserv-0/./share/lua/5.4/?/init.lua;"..package.path;package.cpath="/home/ygorko/Roma42/webserv-0/./lib/lua/5.4/?.so;"..package.cpath;local k,l,_=pcall(require,"luarocks.loader") _=k and l.add_context("cgilua","6.0.2-0")' '/home/ygorko/Roma42/webserv-0/./lib/luarocks/rocks-5.4/cgilua/6.0.2-0/bin/cgilua.fcgi' "$@"
