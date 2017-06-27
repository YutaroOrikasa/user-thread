#!/bin/bash

astyle --style=java --pad-oper --align-pointer=type  --pad-header --unpad-paren --add-brackets  --recursive '*.cpp' --recursive '*.hpp' --recursive '*.h' --exclude=googletest "$@"
