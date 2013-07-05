/**
	article_supercats.cpp
	
	Daniel Richman, June 2013

	Reads CategoriesSupercats.txt and ArticleCategories.txt. 
	For each article a, calculates S(a,t) for every t in topcats. 
	This function is defined mathematically as the S(a,t) in Incarnation 2 of the Google Doc "Categorizing Text by Asking Wikipedia."

	S(a,t) = 1 - sum(for c in C, d(c, t)) / sum(for t0 in T, sum(for c in C, d(c, t0)))

	where A = set of all articles, C = set of all categories, T = set of all top categories (categories we're ultimately interested in. 

*/


#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

typedef unordered_map<string, unordered_map<string, float> * > category_map;

// Split a string
vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

// Parse a line of CategorySupercats.txt
void add_category_line(const string &line, category_map &lookup_table) {

	size_t space_loc = line.find('>'); // find the >, separator between name and valus
	string category = line.substr(0, space_loc); // "Anarchism"
	string supercats = line.substr(space_loc + 1);

	/* supercats will look like this:
Agriculture: 15, Arts: 8, Belief: 4, Business: 6, Chronology: 5, Culture: 2, Education: 6, Environment: 13, Geography: 7, Health: 7, History: 5, Humanities: 4, Language: 8, Law: 7, Life: 4, Mathematics: 7, Nature: 5, People: 9, Politics: 2, Science: 4, Society: 3, Technology: 6,
*/

	vector<string> supercats_list;

	try {
		split(supercats, ',', supercats_list);
	} catch (out_of_range &) {
		// couldn't split, so this CategorySupercats line has nothing interesting on it

		return;
	}

	unordered_map<string, float> *supercats_map = new unordered_map<string, float>;

	for (string supercat : supercats_list) {
		size_t colon_loc = supercat.find(':');

		if (colon_loc == string::npos)
			continue; // junk supercat without data in it

		string supercat_name = supercat.substr(0, colon_loc);
		
		size_t whitespace = supercat_name.find_first_not_of(' ');

		if (whitespace != string::npos)
			supercat_name = supercat_name.substr(whitespace); // remove any peading whitespace

		// In the file, it's an integer. We need to use a flot because we end up normalizing it
		float supercat_value = (float) atoi(supercat.substr(colon_loc + 1).c_str());

		(*supercats_map)[supercat_name] = supercat_value;
	}

	// Normalize all the distances. 
	int distances_sum = 0;
	for (auto it = supercats_map->begin(); it != supercats_map->end(); it++)
		distances_sum += it->second;

	for (auto it = supercats_map->begin(); it != supercats_map->end(); it++)
		it->second = (it->second / (float) distances_sum);

	lookup_table[category] = supercats_map;
}

// Look at a line representing all the categories to which an article belongs and compute the article's relationship to each top cat
void search_article_line(const string &line, category_map &lookup_table) {
	size_t space_loc = line.find('>'); // find the >, separator between name and valus
	string category = line.substr(0, space_loc); // "Anarchism"
	string categories = line.substr(space_loc + 1);

	/* categories will look like this:
Anarchism Political_culture Political_ideologies Social_theories Anti-fascism Greek_loanwords
*/

	vector<string> categories_list;

	try {
		split(categories, ' ', categories_list);
	} catch (out_of_range &) {
		// couldn't split, so this line has nothing interesting on it

		return;
	}

	// This is a map, not an unordered_map, so the supercats are printed in alphabetical order
	map<string, float> *supercats_map = new map<string, float>;

	float total_score = 0; // will contain sum(for t0 in T, sum(for c in C, d(c, t0)))

	// Iterate through all categories associated with this article
	for (string cat : categories_list) {

		// Ignore a category if it isn't in the lookup table
		try {
			lookup_table.at(cat);
		}
		catch (out_of_range &) {
			continue;
		}

		// Iterate through each supercat mapping for this category
		for (auto it = lookup_table[cat]->begin(); it != lookup_table[cat]->end(); it++) {

			string supercat_name = it->first;
			float supercat_weight = it->second;

			total_score += supercat_weight; 

			try {
				supercats_map->at(supercat_name); // throws an exception if not present

				// No exception thrown here, so the supercat was already present. Add the current weight to it
				(*supercats_map)[supercat_name] += supercat_weight;
			}
			
			catch (out_of_range &) {
				// Supercat not previously present. Set it. 
				(*supercats_map)[supercat_name] = supercat_weight;
			}
		}
	}

	cout << category << "> ";

	for (auto it = supercats_map->begin(); it != supercats_map->end(); it++) {
		cout << it->first << ": " << 1 - (it->second / total_score) << ", ";
	}

	cout << endl;

	delete supercats_map;
}

int main(int argc, char ** argv) {
	
	//cout << "article_supercats: generate supercat mapping for wikipedia articles\n";
	//cout << "\tReading from CategoriesSupercats.txt\n";

	category_map category_supercats;

	ifstream infile;
	infile.open("CategoriesSupercats.txt");
	string line;

	if (infile.is_open()) {
		//cout << "\tOpened file, building data structures...";
		//cout.flush();
		
		while ( infile.good() ) {
			getline(infile,line);
			add_category_line(line, category_supercats);
		}

		infile.close();

		//cout << "done\n";
	}

	else {
		cout << "Unable to open CategoriesSupercats.txt!\n";
		return 1;
	}


	// Now we've loaded all the categories and their supercats hop lists into memory

	// Perform lookups for each article
	infile.open("ArticleCategories.txt");

	if (infile.is_open()) {
		//cout << "\tOpened ArticleCategories.txt...\n";
		//cout.flush();
		
		while ( infile.good() ) {
			getline(infile,line);
			search_article_line(line, category_supercats);
		}

		infile.close();
	}

	else {
		cout << "Unable to open ArticleCategories.txt!\n";
		return 1;
	}
}

