import web
import articles_data

urls = (
    '/(.*)', 'index'
)

global articles_supercat_scores, header

def get_header():
	global header
	return header

def get_scores(art_name):
	global articles_supercat_scores
	return articles_supercat_scores[art_name]

class index:
    def GET(self, article):
        # Return the requested article's supercat scores (the article param is passed by web.py from the urls regex (.*))

		if not article:
			return "NoArticleNameInQueryString"

		try:
			response = ""

			header = articles_data.get_header()
			scores = articles_data.get_scores(article)

			for i, h in enumerate(header):
				response += h + ":" + scores[i] + ","

			response += "\n\n"

			return response

		except KeyError, ValueError:
			return "InvalidArticleName"


if __name__ == "__main__":
	articles_data.init_data()

	app = web.application(urls, globals())
	app.run()
