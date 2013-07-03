# perl_categorizer.pl: queries Wikipedia Solr db for its entire argument string. 
# Parses the result and categorizes the query string based on the top Solr result. 

# query string modified for dbtest_3

# TERMS used in comments:
#	"query" = a string of text to be looked for
#	"article" = a Wikipedia article returned by Solr as a result for the query
#	"supercat/supercat value" = for a particular article, a number giving the distance from that article to a Wikipedia top-level category
#			[the distance formula used is subject to change]. 

# Written June 2013 by Daniel Richman. 

use Data::Dumper;
use File::SortedSeek ':all';
use HTTP::Lite;
use JSON::XS;
use Time::HiRes qw / time /;
use URI::Escape;

# Prepare HTTP subsystem
$http = HTTP::Lite->new;

# Define munge for file lookup
# Transformer we apply to each line considered in the binary search. 
# Eliminates spaces (represented by _) and punctuation and transforms to lowercase. 
sub munge {
	local $_ = shift;
	
	$_ = (split(">", $_))[0];

	s/[^\w>]//g;
	s/_//g;

	return lc $_;  # makes comparison case insensistive
}

# Set up timer
$start = time;

# Prepare query string
$query = join(" ", @ARGV);
$query =~ s/[^\w\s]//;
$query = uri_escape($query);

# Make Solr request
$req = $http->request("http://128.111.44.157:8983/solr/collection1/query_clustering?q=$query&wt=json")
    or die "Unable to get document: $!";
die "Request failed ($req): ".$http->status_message()
  if $req ne "200";
# @headers = $http->headers_array();
$body = $http->body();

$json_body_hashref = decode_json $body;
print "***\n";
$response_ref = $$json_body_hashref{response};
%response = %$response_ref;
$docs_list_ref = $response{docs};
@docs_list = @$docs_list_ref;
#$title1 = ${$docs_list[0]}{title};

for $doc (@docs_list) {
	print Dumper($doc);
}

#print Dumper($docs_list_ref);
print "***\n";

# Query supercat db based on top article match (need to make this more sophisticated?)
@article_matches_raw = split('\n', $body);
shift(@article_matches_raw); # Eliminate the top CSV row, which contains only the title

@article_matches_parsed = ();

# Separate the CSV columns of each match
for $article_match (@article_matches_raw) {
	push(@article_matches_parsed, [ split(",", $article_match) ]);
}


# Lookup each match in ArticlesSupercats.txt
# This step tells us what top-level categories each matching article is related to. 
open LOOKUP_FILE, "/home/ddrichman/Desktop/wikicategories/ArticlesSupercats.txt" or die $!;
File::SortedSeek::set_cuddle();

my %query_supercats;

for $article_match (@article_matches_parsed) {

#	print "\tLooking for $article_match->[0]\n";

	# Perform fast binary search of ArticlesSupercats.txt. 
	$search = $article_match->[0]; # title of the article

	# Print all the articles used
	print "m: $article_match->[0] $article_match->[1]\n";

	# Transform the search string so search is case-insensitive and has no special characters. 
	$search = lc $search;

	$search =~ s/[^\w]//g;
	$search =~ s/ //g; # Remove spaces

	File::SortedSeek::alphabetic(*LOOKUP_FILE, $search, \&munge);
	$lookup_result = <LOOKUP_FILE>;

	# If for some reason the binary search returned the wrong line, we ignore it. 
	# The given article probably wasn't in the file at all. 
	$entry_found = munge((split(">", $lookup_result))[0]);

	if (munge($entry_found) ne $search) {
#		print "SortedSeek gave bad match: looked for $search, found $entry_found\n";
		next;
	}
	
	# Success: Split each supercategory in the result line. 
	$article_supercats = (split("> ", $lookup_result))[1]; # the part that contains the supercats, not the article name
	@article_supercats = split(", ", $article_supercats);

	foreach $category_result (@article_supercats) {
		$supercat_name = (split(": ", $category_result))[0];
		$supercat_val = (split(": ", $category_result))[1];

		if ($supercat_name ne "\n") { # If the given supercat is valid

			# Add the supercat values for this article to the supercat values for the entire query. 
			if (exists $query_supercats{$supercat_name}) {
				$query_supercats{$supercat_name} += $supercat_val;
			} else {
				$query_supercats{$supercat_name} = $supercat_val;
			}
		}
	}
}

close LOOKUP_FILE;

# Sort %query_supercats by the value (higher value means that supercat is more related to the article and thus to the query text)

# Note that the $b <=> $a order makes this sort a reverse, so higher values are printed first. 
foreach $key ( sort { $query_supercats{$b} <=> $query_supercats{$a} } keys %query_supercats ) {
	print "$key $query_supercats{$key}\n";
}

#printf "Time elapsed: %.5f\n", time - $start;
