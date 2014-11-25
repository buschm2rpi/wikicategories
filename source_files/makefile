CPP=g++
CPPFLAGS=-Wall -std=c++11 -march=native -O3

all: categories_supercats_scores article_supercats article2id

categories_supercats_scores: categories_supercats_scores.cpp
	$(CPP) $(CPPFLAGS) -pthread -o categories_supercats_scores categories_supercats_scores.cpp

article_supercats: article_supercats.cpp
	$(CPP) $(CPPFLAGS) -o article_supercats article_supercats.cpp
	
article2id: article2id.cpp
	$(CPP) $(CPPFLAGS) -o article2id article2id.cpp

clean:
	rm -f categories_supercats_scores article_supercats article2id
