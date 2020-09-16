#!/bin/sh
#install Netpbm first
pngtopnm $1 | ppmquant 31 | ppmtobmp -bpp 8 > $2
#jpegtopnm $1 | ppmquant 31 | ppmtobmp -bpp 8 > $2
