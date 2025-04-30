#!/bin/sh

find . -name *.c -exec gindent -kr -nut -br -i4 -l120 -npcs -npsl -nprs -ts4 -sob -ncs -bli0 {} \;
find . -name *.h -exec gindent -kr -nut -br -i4 -l120 -npcs -npsl -nprs -ts4 -sob -ncs -bli0 {} \;
