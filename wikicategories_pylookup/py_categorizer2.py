# py_categorizer2.py
# replaces pl_categorizer.pl

# Daniel Richman, July 2013

import json
from operator import itemgetter
import os
import re # regex
import requests # HTTP and JSON library
import sys # argv, path
from urllib import quote_plus

########################################
# Declare binary search functions
def alphabetic_binary_search(file_ptr, key_to_find, munge_fn):

	# setup min and max
	min_loc = 0
	file_ptr.seek(0, 2) # seek to end
	max_loc = file_ptr.tell()

	current_key = None

	while min_loc < max_loc and current_key != key_to_find and max_loc > 1:
		file_ptr.seek((min_loc + max_loc) // 2, 0)

		current_line_tuple = ()

		try:
			current_line_tuple = get_current_full_line(file_ptr)
		except IOError:
			print >> sys.stderr, "error trying to locate ", key_to_find, " with current location ", file_ptr.tell(), " current line ", file_ptr.readline()
			sys.stderr.flush()

			raise IOError

		current_key = extract_key(current_line_tuple[1])

		#print min_loc, max_loc
		#print file_ptr.tell(), " ", current_key

		if current_key == key_to_find: # yay
			return current_line_tuple[1]

		if current_key < key_to_find:
			min_loc = file_ptr.tell() # end of line, since get_current_full_line leaves the pointer at the end
			continue

		if current_key > key_to_find:
			max_loc = current_line_tuple[0] # beginning of line: don't include a piece of the current (rejected) line in the possible range
			continue

	# If we got here, it means we exited the while loop without returning the found line==>we failed
	return None

# Returns the line from the file that contains the location the file ptr is currently at
def get_current_full_line(file_ptr):

	# find the previous \n in the file (or the beginning of the file)
	while 1:
		current_char = file_ptr.read(1)
		if current_char == '\n' or file_ptr.tell() == 1:
			break

		file_ptr.seek(-2, 1) # seeking -2 seems to move the file pointer 1 char back
		#print "at ", file_ptr.tell()
			

	# reached a \n or the beginning of the file: now read a line
	# because of the order of the tuple, the first item in the tuple is the location of the BEGINNING of the line
	return (file_ptr.tell(), file_ptr.readline())
##############################################################################

# Given a line from the file, extracts the key from the line
def extract_key(string):
	key = string.split(">")[0]
	key = re.sub('[^\w]', "", key)
	key = key.replace("_", "")
	key = key.lower()
	return key

# Given a line from the file, parse the line
def parse_line(string):
	article = {}
	article["name"] = string.split(">")[0]
	article["supercats"] = {}

	supercat_mappings = string.split("> ")[1].split(", ")

	for m in supercat_mappings:
		if len(m) > 5: # a legitimate supercat
			s_name = str(m.split(":")[0])
			s_score = float(m.split(": ")[1])
			article["supercats"][s_name] = s_score

	return article

###############################################
# Begin processing input

query = {} # the dictionary we'll eventually print out

argument_string = " ".join(sys.argv[1:])

query["tweet_id"] = argument_string.split()[0]
query["text"] = " ".join(argument_string.split()[1:])

solr_query_string = query["text"]

re.sub('[^\w\s]', "", solr_query_string) # remove symbols from the query

solr_query_string = quote_plus(solr_query_string)

# Query Solr server
# seine.cs.ucsb.edu
r = requests.get("http://128.111.44.157:8983/solr/collection1/query_clustering?q=" + solr_query_string + "&wt=json&rows=200")
solr_output = r.json() # parse JSON in one line!

responses = solr_output["response"]["docs"]
clusters = solr_output["clusters"]

# Query articles/categories mapping db

# Access ArticlesSupercats.txt using an absolute path so this script can be called anywhere
this_script_path = sys.path[0]
if len(this_script_path) > 0:
	this_script_path = this_script_path + "/"

articles = {}


# Locate all articles returned in the Solr query
with open(this_script_path + "../ArticlesSupercats.txt", "rb") as fi: # open file in binary mode so we can do relative seeks in the binary search

	for r in responses:
		#print r
		find_result = alphabetic_binary_search(fi, extract_key(r["title"]), extract_key)

		if find_result is not None:
			parsed_result = parse_line(find_result)
			parsed_result["id"] = r["id"] # copy the article ID from the Solr response to the ArticlesCategories.txt info block
			parsed_result["score"] = r["score"]

			articles[parsed_result["id"]] = parsed_result # add the article to the list of located articles

# Generate cluster scores
for c in clusters:
	c["num_docs"] = len(c["docs"]) # another possible measure of cluster quality
	c["total_supercat_scores"] = {} # maps supercat names to their relevancy scores for this cluster

	total_article_scores = 0 # sum of Solr scores for all articles in the cluster

	for doc in c["docs"]:
		if doc in articles:
			#print articles[doc]

			for supercat, score in articles[doc]["supercats"].iteritems(): # go through all the supercat scores for this article in the cluster
				article_solr_score = float(articles[doc]["score"])

				if supercat in c["total_supercat_scores"]:
					c["total_supercat_scores"][supercat] += score * article_solr_score
				else:
					c["total_supercat_scores"][supercat] = score * article_solr_score

				total_article_scores += article_solr_score

	for supercat in c["total_supercat_scores"]:
		c["total_supercat_scores"][supercat] /= total_article_scores

total_query_scores = {}

for c in clusters:
	for supercat, score in c["total_supercat_scores"].iteritems():
		
		if supercat in total_query_scores:
			total_query_scores[supercat] += score * c["score"]
		else:
			total_query_scores[supercat] = score * c["score"]

#sorted_total_query_scores = sorted(total_query_scores.iteritems(), key=itemgetter(1))

# save space on disk by removing individual calculated supercat vectors for each cluster
for c in clusters:
	del c["total_supercat_scores"]

query["solr_articles_located"] = articles
query["clusters"] = clusters
query["total_query_scores"] = total_query_scores

print json.dumps(query)
