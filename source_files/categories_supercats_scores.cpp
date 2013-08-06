/**************** categories_supercats_scores.cpp
	Reads a file listing wikipedia categories and their parents.
	Then reads a second file listing all Wikipedia articles and their immediate categories. 
	Generates a file representing the strength of the relationship between each article and each of the 25 or so supercategories. 

	Algorithm: random walk with restart. The heavy time and memory requirements of this algorithm mean that multiple threads are used. 

	Data structure: an unordered_map maps category names to a node *. Each node contains the category name and
	the name of the node's parents. 

	Written June-August 2013 by Daniel Richman. 

	Note: This program requires C++11 language and library support. Compile with -std=c++11. On Macs (clang++) also use -stdlib=libc++. 
***************/

#include <algorithm>
#include <chrono>
#include <functional>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;


// Represents a single category
struct node {
	string name;
	vector<node *> parents;
	vector<node *> children;
};


// Used in the BFS
const string root_category = "Main_topic_classifications"; // category from which we perform a BFS to visit *all* articles

ifstream *infile_global;

// the supercats we're interested in. 
const string top_categories[] {"Mathematics", "Language", "Chronology", "Belief", "Environment",
"Education", "Law", "Geography", "History", "Health", "People", "Nature", "Science", "Technology",
"Sports", "Business", "Arts", "Life", "Politics"};


// Synchronizers! 
mutex cout_lock; // locks writing to cout
mutex task_reassign_lock; // locks managing the queue of random walks to be performed

unsigned threads_working = 0;
const unsigned MAX_THREADS = 8;


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

// Prototype
void print_random_walk_output(const string &category, const unordered_map<string, float> &scores);
void reassign_annotation_tasks(unordered_map<string, node *> &lookup_table);

// Perform Random Walk with Restart, beginning at the specified node in the tree. 
// Note: start_node_name should not be declared as a reference because of the
// loop in which it is created. 
void random_walk(const string start_node_name, // node at which to begin
				unordered_map<string, node *> &lookup_table)
{

	float alpha = 0.01; // probability of returning to the top node at any given step)
	float walk_iterations = 10; // number of random walk iterations performed

	node *start_node;

	// Check that the start category is valid
	try {
		start_node = lookup_table.at(start_node_name);
	}

	catch (out_of_range &) {
		cerr << "Fatal error: could not look up category " << start_node_name << " (probably a bad name)\n";			
		return;
	}

	// Allocate the unordered_map for this node
	unordered_map<string, float> *rw_probabilities_last, *rw_probabilities_current;

	rw_probabilities_last = new unordered_map<string, float>;
	rw_probabilities_current = new unordered_map<string, float>;

	(*rw_probabilities_last)[start_node_name] = 1; // At t = 0, there's 100% probability we're at the starting node. If a node is not in the unordered_map at time t, there is 0% probability the random walker is there at time t. 

	// Perform, walk_iterations times,
	// a breadth-first-search step followed by a random walker recomputation step
	for (int j = 0; j < walk_iterations; j++) {

		
		// Perform random walk calculations on all nodes. 
		for (auto it = lookup_table.begin(); it != lookup_table.end(); it++) {

			node *n = it->second; // current node we're considering in relation to the start_node

			float probability = 0; // probability the random walker is at this node at the current timestep

			// Iterate through all child nodes of n to determine the probability that we walked from c to n in this timestep. 
			for (node *c : n->children) {

				// Make sure this child has a nonzero score from last time
				auto child_mapping = rw_probabilities_last->find(c->name);
				if (child_mapping == rw_probabilities_last->end())
					continue;
				
				// child was in rw_probabilities_last, so we might have been at the child last time, so we might be at the parent this time
				float child_last_prob = child_mapping->second; // probability we were at the child last timestep
				
				probability += child_last_prob / c->parents.size(); // a walker at the child could have walked randomly to any of the child's parent nodes
			}

			// Check whether probability is nonzero. If it's zero, we can ignore this node as long as it's not the start node (to which there is always alpha probability of returning).
			if (probability == 0 && n != start_node)
				continue;

			// Make final adjustments to probability for n, given that we return randomly to the starting node. 
			probability *= (1 - alpha);
			
			if (n == start_node) {
				probability += alpha;
			}
			
			(*rw_probabilities_current)[n->name] = probability;
		}

		// Perform sanity check: allowing for floating-point error, rw_probabilities_current should add up to 1 since there's a 100% probability the walker will be in the graph
		float sum = 0;
		
		for (auto it = rw_probabilities_current->begin(); it != rw_probabilities_current->end(); it++)
			sum += it->second;

		// If things are off, it's because there's a sinkhole (the Contents category) at the top of the graph. Renormalize all scores. 
		if (fabs(sum - 1) > 0.001) {

			float renormalization_factor = 1.0 / sum;

			for (auto it = rw_probabilities_current->begin(); it != rw_probabilities_current->end(); it++)
				it->second *= renormalization_factor;
		}

		// Next cycle: rw_probabilities_current will become rw_probabilities_last and the old rw_probabilities_last will go away. 
		unordered_map<string, float> * tmp2 = rw_probabilities_last;
		tmp2->clear(); // now empty and suitable for reuse
		
		rw_probabilities_last = rw_probabilities_current; // save current state
		rw_probabilities_current = tmp2; // empty
	}
	
	
	// Reached the end of the random walk iterations. Save the data we're interested in. 
	unordered_map<string, float> random_walk_connections;
	
	// Iterate through all the categories we're interested in and print out their 
	for (string cat : top_categories) {
		try {
			random_walk_connections[cat] = rw_probabilities_last->at(cat);
		} catch (out_of_range &) {
			// specified supercat not reached in the random walk
			random_walk_connections[cat] = 0;
		}
	}

	// Print data
	print_random_walk_output(start_node_name, random_walk_connections);

	// Inform dispatcher another thread can be created
	reassign_annotation_tasks(lookup_table);

	// Clean up. 
	delete rw_probabilities_last;
	delete rw_probabilities_current;
}


// Print out the supercat scores vector for this category
void print_random_walk_output(const string &category, const unordered_map<string, float> &scores) {

	lock_guard<mutex> lock(cout_lock);

	cout << category << "> ";

	// Print out each score
	for (auto it = scores.cbegin(); it != scores.cend(); it++) {

		cout << it->first << ": " << it->second << ", ";
	}

	cout << endl;
}


// Called by a random walker task right before it finishes to suggest that another thread be allocated. 
// This makes a poor man's thread pool. 
void reassign_annotation_tasks(unordered_map<string, node *> &lookup_table) {
	lock_guard<mutex> lock(task_reassign_lock); // mutex will be unlocked when this function exits

	threads_working--; // we were called by a current thread about to stop	

	// Perform BFS to visit all articles accessible from the rootcat.
	while (infile_global->good() && threads_working < MAX_THREADS) {
		string next_category;
		getline(*infile_global, next_category);

		threads_working++;
		thread t(random_walk, next_category, ref(lookup_table));
		t.detach();
	}
}


// main
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

	cerr << "Building node list...";


	// Data loaded, perform random walks
	cerr << "Performing random walks for all categories...\n";
	
	infile.open("Categories_BFSOrder.txt");
	
	if (infile.good()) {
		threads_working++;
		infile_global = &infile;
		reassign_annotation_tasks(lookup_table);
	}
	
	
	while (true) {
		
		task_reassign_lock.lock();
		int tw = threads_working; // copy shared data
		task_reassign_lock.unlock();		

		if (tw != 0) {
			this_thread::sleep_for(chrono::milliseconds(50));
		}
		else
			break; // done random-walking
	}
}

