#!/bin/sh

# Sort ArticlesSupercats.txt so that its order is as expected by the binary search algorithm in the perl script. 

sort -d -f -t ">" -k 1,1 ArticlesSupercats.txt > new
mv new ArticlesSupercats.txt

