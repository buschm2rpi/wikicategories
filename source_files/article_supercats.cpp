/**
	article_supercats.cpp
	
	Daniel Richman, June/July 2013

	Reads CategoriesSupercats.txt and ArticleCategories.txt. 
	For each article a, calculates S(a,t) for every t in topcats. 
	This function is defined mathematically as the S(a,t) in Incarnation 3 of the Google Doc "Categorizing Text by Asking Wikipedia."

	S(a,t) = max(S(n, t) for n in N) / sum(for t0 in T, max S(n, t) for n in N)

EDIT: Now the average score, not the max, for testing. 

	where a = an article, N = set of the article's categories, T = set of all top categories (categories we're ultimately interested in). 

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

	// Read all the S(n, t) for the n (the category) in this line. 
	unordered_map<string, float> *supercats_map = new unordered_map<string, float>; // supercat scores for this cat

	for (string supercat : supercats_list) {
		size_t colon_loc = supercat.find(':');

		if (colon_loc == string::npos)
			continue; // junk supercat without data in it

		string supercat_name = supercat.substr(0, colon_loc);
		
		size_t whitespace = supercat_name.find_first_not_of(' ');

		if (whitespace != string::npos)
			supercat_name = supercat_name.substr(whitespace); // remove any leading whitespace

		// In the file, it's an integer. We need to use a float because we end up normalizing it
		float supercat_value = atof(supercat.substr(colon_loc + 1).c_str());

		(*supercats_map)[supercat_name] = supercat_value;
	}

	// Store the supercats map for this category in the lookup table. 
	lookup_table[category] = supercats_map;
}

// Look at a line representing all the categories to which an article belongs and compute the article's relationship to each top cat
void search_article_line(const string &line, category_map &lookup_table) {
	size_t space_loc = line.find('>'); // find the >, separator between name and values
	string article = line.substr(0, space_loc); // "Anarchy"
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

	// This is a map, not an unordered_map, so that we get the supercats printed in alphabetical order
	map<string, float> *supercats_map = new map<string, float>;

	// Iterate through all categories associated with this article
	for (string cat : categories_list) {

		// Ignore a category if it isn't in the lookup table
		if (lookup_table.find(cat) == lookup_table.end())
			continue;

		// Iterate through each supercat mapping for this category
		for (auto it = lookup_table[cat]->begin(); it != lookup_table[cat]->end(); it++) {

			string supercat_name = it->first;
			float supercat_score = it->second;

			if (supercats_map->find(supercat_name) != supercats_map->end()) { // present already

				// No exception thrown here, so the supercat was already present. Add the current score. 

				(*supercats_map)[supercat_name] += supercat_score;
			}
			
			else {
				// Supercat not previously present. Set it. 
				(*supercats_map)[supercat_name] = supercat_score;
			}
		}
	}


	float total_score = 0; // will contain sum(for t0 in T, max S(n, t) for n in N)

	for (auto it = supercats_map->begin(); it != supercats_map->end(); it++)
		total_score += it->second; // add on each of the preliminary scores for all the supercats for this article. 

	cout << article << "> "; // print article name

	for (auto it = supercats_map->begin(); it != supercats_map->end(); it++) {
		cout << it->first << ": " << (it->second / total_score) << ", "; // print the final score between this article and each topcat
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
		cerr << "\tOpened file, building data structures...";

		while ( infile.good() ) {
			getline(infile,line);
			add_category_line(line, category_supercats);
		}

		infile.close();

		cerr << "done\n";
	}

	else {
		cerr << "Unable to open CategoriesSupercats.txt!\n";
		return 1;
	}


	// Now we've loaded all the categories and their supercats hop lists into memory

	// Perform lookups for each article
	infile.open("ArticleCategories.txt");

	unsigned long lines_read = 0;

	if (infile.is_open()) {
		cerr << "\tOpened ArticleCategories.txt...\n";
		
		while ( infile.good() ) {
			getline(infile,line);
			search_article_line(line, category_supercats);

			lines_read++;

			if (lines_read % 100000 == 0)
				cerr << lines_read << " lines processed\n";
		}

		infile.close();
	}

	else {
		cerr << "Unable to open ArticleCategories.txt!\n";
		return 1;
	}
}

