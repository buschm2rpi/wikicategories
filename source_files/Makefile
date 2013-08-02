CPP=g++
CPPFLAGS=-Wall -std=c++11 -march=native -O2

all: categories_supercats_scores article_supercats

categories_supercats_scores: categories_supercats_scores.cpp
	$(CPP) $(CPPFLAGS) -pthread -o categories_supercats_scores categories_supercats_scores.cpp

article_supercats: article_supercats.cpp
	$(CPP) $(CPPFLAGS) -o article_supercats article_supercats.cpp

clean:
	rm -f categories_supercats_scores article_supercats
