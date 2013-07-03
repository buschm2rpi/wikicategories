#!/usr/bin/perl -w

use File::SortedSeek ':all';

# Transformer we apply to each line considered in the binary search. 
# Eliminates spaces (represented by _) and punctuation and transforms to lowercase. 
sub munge {
	local $_ = shift;
	
	$_ = substr $_, 0, index($_, ">") + 1;

	s/[^\w>]//g;
	s/_//g;

	return lc $_;  # makes comparison case insensistive
}

# Transform the search string so search is case-insensitive and has no special characters. 
$search = lc $ARGV[0];
$search =~ s/[^\w]//g;
$search =~ s/_//g;

$search = "$search>";

open LOOKUP_FILE, "ArticlesSupercats.txt" or die $!;

File::SortedSeek::set_cuddle();

File::SortedSeek::alphabetic(*LOOKUP_FILE, $search, \&munge);
$line = <LOOKUP_FILE>;

print $line;

