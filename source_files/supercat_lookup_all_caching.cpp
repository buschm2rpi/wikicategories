/**************** supercat_lookup_all_caching.cpp
	Reads a file listing wikipedia categories and their parents.
	Then reads a second file listing all Wikipedia articles and their immediate categories. 
	Generates a file representing the strength of the relationship between each article and each of the 25 or so supercategories. 
	
	Data structure: an unordered_map maps category names to a node *. Each node contains the category name and
	the name of the node's parents. 

	Written June 2013 by Daniel Richman. 

	Note: This program requires C++11 support. Compile with -std=c++11. 
***************/


#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// Used in the BFS
const string root_category = "Main_topic_classifications"; // category from which we perform a BFS to visit *all* articles

// the supercats we're interested in. 
const string top_categories[] {"Mathematics", "Language", "Chronology", "Belief", "Environment",
"Education", "Law", "Geography", "History", "Health", "People", "Nature", "Science", "Technology",
"Sports", "Business", "Arts", "Life", "Politics"};

enum class visit_status : char { WHITE, GRAY, BLACK };

// Represents a single category
struct node {
	string name;
	vector<node *> parents;
	vector<node *> children;

	visit_status vs = visit_status::WHITE; // used in the BFS
	map<string, unsigned short> depths; // used in the BFS--depth of this node from each named category
};


// Split a string
vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

// Find a node in the lookup table if it's there or make and add a new one
node *get_or_add_node(const string &name, unordered_map<string, node *> &lookup_table) {

	// Locate the node if it already exists; otherwise make a new one
	auto node_it = lookup_table.find(name);

	node *n;

	if (node_it != lookup_table.end()) // found it
		n = node_it->second;
	
	else {
		// subcat not in the lookup table: make a new node
		n = new node;
		n->name = name;

		lookup_table[name] = n;
	}

	return n;
}


// Parse a line of categories_parsed.txt
void addline(const string &line, unordered_map<string, node *> &lookup_table) {

	size_t space_loc = line.find(' '); // find the space
	string subcategoryname = line.substr(0, space_loc);
	string supercategoryname = line.substr(space_loc + 1);

	node *supercat = get_or_add_node(supercategoryname, lookup_table);
	node *subcat = get_or_add_node(subcategoryname, lookup_table);

	supercat->children.push_back(subcat);
	subcat->parents.push_back(supercat);
}

// Annotate each reachable node in the tree with the distance to each of the specified top nodes. 
void tree_annotate(const string *topnodenames, const size_t number_of_topnodes, unordered_map<string, node *> &lookup_table) {

	// For each of the children [these are categories like Arts, Culture, Computing, etc.], perform a breadth-first search from that child annotating *all* children as we go
	
	for (size_t node_num = 0; node_num < number_of_topnodes; node_num++) {

		string main_category_name = *(topnodenames + node_num);
		node *main_category;

		try {
			main_category = lookup_table[main_category_name];
		}

		catch (out_of_range &) {
			cerr << "Could not look up category " << main_category_name << " in table, skipping\n";			
			continue;
		}

		cerr << "\tAnnotating for category " << main_category_name << endl;

		queue<node *> tovisit;
		queue<node *> allvisited; // keeps track of all visited nodes so we can reset their colors for the next BFS

		// Tell each child of the top node that its distance to itself is zero	
		main_category->depths[main_category_name] = 0;
		main_category->vs = visit_status::GRAY;

		tovisit.push(main_category);
		allvisited.push(main_category);

		// Perform BFS. 
		while (!tovisit.empty()) {
			node *nextnode = tovisit.front();
			tovisit.pop();

			for (node *child : nextnode->children) {

				if (child->vs == visit_status::WHITE) {
					child->vs = visit_status::GRAY;
					child->depths[main_category_name] = nextnode->depths[main_category_name] + 1;
			
					tovisit.push(child);
				}
			}

			nextnode->vs = visit_status::BLACK;
			allvisited.push(nextnode);
		}

		// Reset all colors from BFS. Depths do not need to be reset. 
		while (!allvisited.empty()) {
			node *toreset = allvisited.front();
			allvisited.pop();

			toreset->vs = visit_status::WHITE;
		}
	}
}


// Writes all tree information to stdout. 
void tree_dump_annotations(const string &topnodename, unordered_map<string, node *> &lookup_table) {

	// Find the top node
	node *top;

	try {
		top = lookup_table.at(topnodename);
	}

	catch (out_of_range&) { return; }

	// Perform a BFS writing all node information to stdout. 
	queue<node *> tovisit;
	queue<node *> allvisited; // keeps track of all visited nodes so we can reset their colors for the next BFS

	// We don't update distances in this search, just print them
	top->vs = visit_status::GRAY;

	tovisit.push(top);
	allvisited.push(top);

	// Perform BFS. 
	while (!tovisit.empty()) {
		node *nextnode = tovisit.front();
		tovisit.pop();

		for (node *child : nextnode->children) {

			if (child->vs == visit_status::WHITE) {
				child->vs = visit_status::GRAY;
		
				tovisit.push(child);
			}
		}

		nextnode->vs = visit_status::BLACK;

		cout << nextnode->name << "> ";

		for (auto it = nextnode->depths.cbegin(); it != nextnode->depths.cend(); it++)
			cout << it->first << ": " << it->second << ", ";

		cout << endl;

		allvisited.push(nextnode);
	}

	// Reset all colors from BFS. Depths do not need to be reset. 
	while (!allvisited.empty()) {
		node *toreset = allvisited.front();
		allvisited.pop();

		toreset->vs = visit_status::WHITE;
	}
}

/*
// Each parameter is a map that maps a name to a value. 
// If a key is present in both maps, summaps sets v1[key] to be the sum of both maps' value for that key. 
// Keys present in only one map are copied to v1. 
void summaps(map<string, unsigned short> &v1, map<string, unsigned short> &v2) {
	
	// Iterate through keys in v1 and look for matching keys in v2. 
	for (auto &currpair : v1) {
		string name = currpair.first;

		try {
			// Add v2's value to v1's value, if v2 has this value. Then delete the key in v2. 
			currpair.second += v2[name];
			v2.erase(name);
		}

		// Ignore errors. 
		catch (out_of_range&) {}
	}

	// Move all remaining keys in v2 to v1 since they weren't present in v1. 
	v1.insert(v2.cbegin(), v2.cend());
}
*/

int main(int argc, char ** argv) {
	
	cerr << "categories_supercats_relationship_mapper: generate scores mapping each category to each top category\n";
	cerr << "\tReading from categories_parsed.txt generated by skos_parser.sh\n";

	unordered_map<string, node *> lookup_table;

	ifstream infile;
	infile.open("categories_parsed.txt");
	string line;

	if (infile.is_open()) {
		cerr << "\tOpened file, building data structures...";
		
		while ( infile.good() ) {
			getline(infile,line);
			addline(line, lookup_table);
		}

		infile.close();

		cerr << "done\n";
	}

	else {
		cerr << "Unable to open file!";
		return 1;
	}


	// Data loaded, perform BFS annotation
	cerr << "Performing tree annotation for all categories...\n";
	tree_annotate(top_categories, sizeof top_categories / sizeof(string), lookup_table);

	// Output all categories and their annotated values
	tree_dump_annotations(root_category, lookup_table);
}

