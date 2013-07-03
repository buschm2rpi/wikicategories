#!/bin/sh

# Shell script to parse skos_categories_en.nt and generate the tree file used by supercat_lookup. 

# The output is of the form
# subcategory: parent1 parent2 ...

# Daniel Richman, June 2013

# we're only interested in "broader" relationships

cat skos_categories_en.nt | awk '/#broader/ {print $1 $3}' | sed -r "s/<[^<]*Category:([^ <>]+)>/\1 /g" | sort | sed -e "s/ $//" > categories_parsed.txt



# old: used to try to amalgamate the categories
# | perl -e '$lastsubcat = ""; foreach $line (<>) { ($subcat, $supercat) = split(/ /, $line); if ($subcat ne $lastsubcat || $lastsubcat eq "") { print "\n" . $subcat . ": "; $lastsubcat = $subcat; } print $supercat . " "; }' 
