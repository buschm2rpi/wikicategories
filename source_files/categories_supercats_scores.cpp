/**************** categories_supercats_scores.cpp
	Reads a file listing wikipedia categories and their parents.
	Then reads a second file listing all Wikipedia articles and their immediate categories. 
	Generates a file representing the strength of the relationship between each article and each of the 25 or so supercategories. 

	Algorithm: random walk with restart. The heavy time and memory requirements of this algorithm mean that multiple threads are used. 



	IMPLEMENTATION DETAILS:
		1) Parse data in addline(). At this stage, we build a network of nodes, each with pointers to parent and child nodes. We also build a hash table (unordered_map)
			that allows us to quickly find a node object, given the name of the category it represents. 
		2) Build graph matrix in populate_graph_matrix. At this stage we convert the node matrix into an alternative representation as a sparse matrix. 
			// Graph structure: G(i,j) represents the probability of travelling from i to j in any given step. 
			// Since this graph is unweighted, G(i,j) is simply:
			//			(1 - alpha) / len(neighbors(i)) if i and j are neighbors
			//			0 					  if i and j are not neighbors

			alpha is the probability (random walk with **restart**) that the random walker returns to the starting node. This is typically ~0.01-0.03. 

			The graph matrix we construct at this stage does not care which node is the starting node. The random walk with restart algo simply has to
			add alpha to the probability of being at its starting node at each stage of its evolution. 

		3) Execute RWR algorithm for each category in Wikipedia. This is executed in a threaded way for speed. 


	Data structure: an unordered_map maps category names to a node *. Each node contains the category name and
	the name of the node's parents. 

	Written June-August 2013 by Daniel Richman. 

	Note: This program requires C++11 language and library support. Compile with -std=c++11. On Macs with clang++ also use -stdlib=libc++.
			Tested with g++ 4.7.2 on linux x86_64 and clang++ 3.3 on OS X. 
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

#define ARMA_64BIT_WORD
#include <armadillo>

using namespace std;
using namespace arma;


// Represents a single category
struct node {
	string name;
	vector<node *> parents;
	vector<node *> children;

	unsigned long category_id; // each category has a number assigned by populate_graph_matrix to enable lookups in the sp_fmat category_matrix. 
};


const unsigned long ID_UNASSIGNED = 0;

// Used in the BFS
const string root_category = "Main_topic_classifications"; // category from which we perform a BFS to visit *all* articles

ifstream *infile_global;

// the supercats we're interested in. 
const string top_categories[] {"Mathematics", "Language", "Chronology", "Belief", "Environment",
"Education", "Law", "Geography", "History", "Health", "People", "Nature", "Science", "Technology",
"Sports", "Business", "Arts", "Life", "Politics"};


// alpha for the random walk with restart
const float RWR_ALPHA = 0.01;


// Synchronizers! 
mutex cout_lock; // locks writing to cout
mutex task_reassign_lock; // locks managing the queue of random walks to be performed

unsigned threads_working = 0;
const unsigned MAX_THREADS = 8;


// Count # of neighbors of a given node
inline int count_neighbors_of(node *n) {
	return n->parents.size() + n->children.size();
}

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
		n->category_id = ID_UNASSIGNED;

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

// Build graph matrix: M(i,j) = probability of moving along the edge from i to j in a single step. 
// alpha = probability the random walk with restart returns to the start at any step. 
void populate_graph_matrix(unordered_map<string, node*> &lookup_table, sp_fmat *category_matrix, float alpha) {

	unsigned long current_category_id = 1;

	unsigned long categories_processed = 0; // just for debugging


	for (auto it = lookup_table.cbegin(); it != lookup_table.cend(); it++) {

		node *n = it->second;
		int n_neighbors = count_neighbors_of(n);
		
		if (n->category_id == ID_UNASSIGNED) {
			n->category_id = current_category_id; // assign the category id
			current_category_id++;
		}

		// add info about neighbors to graph
		for (node *p : n->parents) {
			if (p->category_id == ID_UNASSIGNED) {
				p->category_id = current_category_id; // assign the category id
				current_category_id++;
			}

			(*category_matrix)(n->category_id, p->category_id) = (1.f - alpha) / n_neighbors; // probability of moving from n to p in a single step is 1/count_neighbors_of(n)
		}

		for (node *c : n->children) {
			if (c->category_id == ID_UNASSIGNED) {
				c->category_id = current_category_id; // assign the category id
				current_category_id++;
			}

			(*category_matrix)(n->category_id, c->category_id) = (1.f - alpha) / n_neighbors; // probability of moving from n to c in a single step is 1/count_neighbors_of(n)
		}


		if (++categories_processed % 25000 == 0) // passed a multiple of 25,000 in this iteration
			cerr << "\tFinished processing category " << categories_processed << endl;

	}
}

// Prototype
void print_random_walk_output(const string &category, const unordered_map<string, node *> &lookup_table, sp_fmat &rwr_results);
void reassign_annotation_tasks(unordered_map<string, node *> &lookup_table, sp_fmat *category_matrix, float alpha);

// Perform Random Walk with Restart, beginning at the specified node in the tree. 
// Note: start_node_name should not be declared as a reference because of the
// loop in which it is created. 
void random_walk(const string start_node_name, // node at which to begin
				unordered_map<string, node *> &lookup_table,
				sp_fmat *category_matrix,
				float alpha)
{

	float walk_iterations = 10; // number of random walk iterations performed

	node *start_node;

	// Check that the start category is valid
	try {
		start_node = lookup_table[start_node_name];
	}

	catch (out_of_range &) {
		cerr << "Fatal error: could not look up category " << start_node_name << " (probably a bad name)\n";			
		return;
	}


	sp_fmat rwr_results(1, lookup_table.size()); // this row matrix
	rwr_results(1, start_node->category_id) = 1; // there's 100% probability we're at the starting node at t = 0

	// Perform random walks with restart
	for (short j = 0; j < walk_iterations; j++) {
		rwr_results *= *category_matrix;
		rwr_results(1, start_node->category_id) += alpha;
	}


	// Print data
	print_random_walk_output(start_node_name, lookup_table, rwr_results);


	// Inform dispatcher another thread can be created
	reassign_annotation_tasks(lookup_table, category_matrix, alpha);
}


// Print out the supercat scores vector for this category
void print_random_walk_output(const string &category, const unordered_map<string, node *> &lookup_table, sp_fmat &rwr_results) {

	lock_guard<mutex> lock(cout_lock);

	cout << category << "> ";

	// Print out each score
	for (string topcat : top_categories) {
		// How does this work? We print out the topcat name. To get the score, we look up in the rwr_results row vector the entry corresponding to the topcat. 
		cout << topcat << ": " << rwr_results(1, (lookup_table.at(topcat))->category_id) << ", ";
	}

	cout << endl;
}


// Called by a random walker task right before it finishes to suggest that another thread be allocated. 
// This makes a poor man's thread pool. 
void reassign_annotation_tasks(unordered_map<string, node *> &lookup_table, sp_fmat *category_matrix, float alpha) {
	lock_guard<mutex> lock(task_reassign_lock); // mutex will be unlocked when this function exits

	threads_working--; // we were called by a current thread about to stop	

	// Perform BFS to visit all articles accessible from the rootcat.
	while (infile_global->good() && threads_working < MAX_THREADS) {
		string next_category;
		getline(*infile_global, next_category);

		threads_working++;
		thread t(random_walk, next_category, ref(lookup_table), category_matrix, alpha);
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


	// Construct the sparse matrix representing the category graph
	// Note: the size of this matrix is n^2 in the number of categories. But most categories
	// are not connected to each other so we store this as a sparse matrix. 
	unsigned number_of_categories = lookup_table.size(); // number of nodes
	sp_fmat *category_matrix = new sp_fmat(number_of_categories, number_of_categories);

	// Graph structure: G(i,j) represents the probability of travelling from i to j in any given step. 
	// Since this graph is unweighted, G(i,j) is simply:
	//			1 / len(neighbors(i)) if i and j are neighbors
	//			0 					  if i and j are not neighbors

	// Note that this does not take into account the restart probability alpha, which is accounted for later. 

	cerr << "Populating graph matrix...\n";
	populate_graph_matrix(lookup_table, category_matrix, RWR_ALPHA);


	// Data loaded, perform random walks
	cerr << "Performing random walks for all categories...\n";
	
	infile.open("Categories_BFSOrder.txt");
	
	if (infile.good()) {
		threads_working++;
		infile_global = &infile;
		reassign_annotation_tasks(lookup_table, category_matrix, RWR_ALPHA);
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

