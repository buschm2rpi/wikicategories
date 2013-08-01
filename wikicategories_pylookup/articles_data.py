header = ""
articles_supercats_scores = {}

def init_data():
	with open('../source_files/ArticlesSupercats.txt', 'rb') as f:
		header = f.readline().split(";")[1:-1] # store every header column after the first one, Article, but not the final '\n'

		print header

		globals()["header"] = header

		for line in f:
			values = line.split(";") # file is ;-separated
			articles_supercats_scores[values[0]] = values[1:-1] # values[0] is the article title. The final entry in values[], which we throw out, is a '\n

		print len(articles_supercats_scores)

def get_header():
	return header

def get_scores(article):
	return articles_supercats_scores[article]
