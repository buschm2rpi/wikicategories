#!/bin/sh

# Transform article_categories_en.nt into ArticleCategories.txt, a much more nicely processed form

head -n 400 article_categories_en.nt | awk '{print $1 $3}' | sed -r "s,<[^>]*?/([^>]*?)>,\1,g" | sed "s/Category:/ /" | perl -e '$lastsubcat = ""; 
foreach $line 
(<>) 
{
($subcat, $supercat) = split(/ /, $line); if ($subcat ne $lastsubcat || $lastsubcat eq "") { print "\n" . $subcat . "> "; $lastsubcat = $subcat; } 
chomp $supercat;
print $supercat . " "; }' | sort -d -f -t ">" -k 1,1 > ArticleCategories2.txt
