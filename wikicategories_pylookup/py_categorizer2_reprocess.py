# py_categorizer2_reprocess.py

# Reevaluate a tweet without querying Solr. We accept Solr's article and cluster results, but rescore everything afterward. 

# Daniel Richman, August 2013

import json
import sys

print sys.argv[1]

# Load data
with open(sys.argv[1]) as f: 

	data = json.loads(f.readline())

clusters = data["clusters"]
articles = data["solr_articles_located"]


# Recompute cluster scores
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

	c["average_article_score"] = total_article_scores / len(c["docs"])

total_query_scores = {}

for c in clusters:
	for supercat, score in c["total_supercat_scores"].iteritems():
		
		if supercat in total_query_scores:
			total_query_scores[supercat] += score * c["score"]
		else:
			total_query_scores[supercat] = score * c["score"]

# save space on disk by removing individual calculated supercat vectors for each cluster
for c in clusters:
	del c["total_supercat_scores"]

data["total_query_scores"] = total_query_scores

with open(sys.argv[1], "w") as f:
	print >> f, json.dumps(data)
