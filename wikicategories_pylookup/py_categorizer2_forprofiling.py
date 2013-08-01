# py_categorizer2.py FOR PROFILING USE ONLY: does not print actual data, only timing information
# replaces pl_categorizer.pl
# see README in parent directory

# Daniel Richman, July 2013

import json
from operator import itemgetter
import re # regex
import requests # HTTP and JSON library
import sys # argv, path
from urllib import quote_plus
from time import time

# Parse output from the Python article score query system
def parse_supercats(string):

	if "," not in string:
		return None # invalid string

	supercats = {}

	supercat_mappings = string.split(",")

	for m in supercat_mappings:

		if len(m) > 5: # a legitimate supercat
			s_name = str(m.split(":")[0])
			s_score = float(m.split(":")[1])
			supercats[s_name] = s_score

	return supercats

###############################################
# Begin processing input

starttime = time()

query = {} # the dictionary we'll eventually print out

argument_string = " ".join(sys.argv[1:])

query["tweet_id"] = argument_string.split()[0]
query["text"] = " ".join(argument_string.split()[1:])

solr_query_string = query["text"]

re.sub('[^\w\s]', "", solr_query_string) # remove symbols from the query

solr_query_string = quote_plus(solr_query_string)

query_prep_time = time()

# Query Solr server
# seine.cs.ucsb.edu
r = requests.get("http://128.111.44.157:8983/solr/collection1/query_clustering?q=" + solr_query_string + "&wt=json&rows=200")

solr_time = time()

solr_output = r.json() # parse JSON in one line!

responses = solr_output["response"]["docs"]
clusters = solr_output["clusters"]

# Query articles/categories mapping db

# Access ArticlesSupercats.txt using an absolute path so this script can be called anywhere
this_script_path = sys.path[0]
if len(this_script_path) > 0:
	this_script_path = this_script_path + "/"

articles = {}

json_parse_time = time()


# Locate all articles returned in the Solr query
for r in responses:

	art = {}
	supercats_text = requests.get("http://0.0.0.0:8080/" + r["title"].replace(" ", "_")).text

	art["supercats"] = parse_supercats(supercats_text)

	if art["supercats"] is None:
		print "Couldn't query " + r["title"].encode('ascii', 'ignore')
		continue # invalid--skip this one

	art["id"] = r["id"] # copy the article ID from the Solr response to the ArticlesCategories.txt info block
	art["title"] = r["title"]	
	art["score"] = r["score"]

	articles[r["id"]] = art


file_lookup_time = time()

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

score_calculation_time = time()

print json.dumps({"1query_prep" : query_prep_time - starttime, "2solr" : solr_time - query_prep_time, "3json_parse" : json_parse_time - solr_time, "4file_lookup" : file_lookup_time - json_parse_time, "5score_calculation" : score_calculation_time - file_lookup_time})
