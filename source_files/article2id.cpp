/*
 * article2id.cpp
 *
 *  Created on: Nov 24, 2014
 *      Author: mbusch
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

typedef unordered_map<string, string> id_map;

// Split a string
vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

// Replace whitespaces with underscores
string &replacer(string &s, char subout, char subin){

	vector<string> newstring_list;
	string newstring = "";

	try {

			split(s, subout, newstring_list);

		} catch (out_of_range &) {
			// couldn't split, so this line has nothing interesting on it
			cout << s << endl;
			return s;
		}
		for(string item : newstring_list){
						newstring += subin +item; //adds leading character, always
					}
					newstring = newstring.substr(1,string::npos); //removes leading character
					return s= newstring;
}

// Parse a line of id2article.txt
void add_id_line(const string &line, id_map &lookup_table) {

	//Each line of the mapping text file looks like this
	//Key: 12: Value: Anarchism

	vector<string> tokens;
	try {
		split(line, ':', tokens);
	} catch (out_of_range &) {
		// couldn't split, so this CategorySupercats line has nothing interesting on it
		cout << "caught exception" << endl;
		return;
	}

	size_t whitespace;
	string id = tokens.at(1);
	//cout << id << endl;
	while(id.find(' ') == 0){
		whitespace = id.find(' ');
		id = id.substr(whitespace+1,string::npos); // trim leading whitespace
		//cout << id << endl;
	}

	string article = tokens.at(3);
	//cout << article << endl;
	while(article.find(' ') == 0){
		whitespace = article.find(' ');
		article = article.substr(whitespace+1,string::npos); // trim leading whitespace
		//cout << article << endl;
	}

	replacer(article, ' ', '_');

	// add article2id to the mapper
	lookup_table[article] = id;
	//cout << "here" << endl;
	return;

}

int main(int argc, char ** argv) {

	//cout << "article_supercats: generate supercat mapping for wikipedia articles\n";
	//cout << "\tReading from CategoriesSupercats.txt\n";

	//category_map category_supercats;
	id_map article2id;

	ifstream infile;
	string line;

	// Load the article to id mapping into memory
	infile.open("id2article.txt");
		//string line ("Key: 307: Value: Abraham Lincoln");
		//add_id_line(line, article2id);

		if (infile.is_open()) {
			cerr << "\tOpened file, building data structures...";
			while ( infile.good() ) {
				getline(infile,line);
				if(line.length()==0){break;} // doesn't always catch the end of file
				add_id_line(line, article2id);
			}

			infile.close();

			cerr << "done\n";
		}

		else {
			cerr << "Unable to open id2article.txt!\n";
			return 1;
		}

	// Now we've loaded all the categories and their supercats hop lists into memory

	// Perform lookups for each article
	infile.open("/home/mbusch/Documents/ddrichman_seine/ArticlesSupercats.txt");

	unsigned long lines_read = 0;

	if (infile.is_open()) {
		cerr << "\tOpened ArticlesSupercats.txt...\n";

		while ( infile.good() ) {
			getline(infile,line);
			size_t space_loc = line.find('>'); // find the >, separator between name and values
			string article = line.substr(0, space_loc); // "Anarchy"
			string categories = line.substr(space_loc + 1);
			//cout << article << endl;

			try{
				string id=article2id.at(article);
				cout << id + ">" + categories << endl;
			} catch(out_of_range &){
				//cout << article + " not found" << endl;
				continue;
			}

			//return 1;

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



