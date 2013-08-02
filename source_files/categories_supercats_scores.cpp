/**************** categories_supercats_scores.cpp
	Reads a file listing wikipedia categories and their parents.
	Then reads a second file listing all Wikipedia articles and their immediate categories. 
	Generates a file representing the strength of the relationship between each article and each of the 25 or so supercategories. 

	Algorithm: random walk with restart. The heavy time and memory requirements of this algorithm mean that multiple threads are used. 

	Data structure: an unordered_map maps category names to a node *. Each node contains the category name and
	the name of the node's parents. 

	Written June-August 2013 by Daniel Richman. 

	Note: This program requires C++11 support. Compile with -std=c++11. 
***************/

#include <algorithm>
#include <future>
#include <fstream>
#include <iostream>
#include <list>
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
	
	visit_status vs = visit_status::WHITE;

	// Map from a node name to a probability that a random walker from this node would be at that node at whatever timestep the algorithm is currently on. 
	// This an unordered_map in each node, rather than as a single float in each node, to allow multiple random walks to run simultaneously in separate threads. 
	unordered_map<string, float> * random_walk_connections;
};

// Constant representing no connection from this cat to this topcat
const short NO_CONNECTION = -1;

// Count # of neighbors of a given node
int count_neighbors_of(node *n) {
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
		n->random_walk_connections = nullptr;

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

// Perform Random Walk with Restart, beginning at the specified node in the tree. 
void random_walk(const string &start_node_name, // node at which to begin
				unordered_map<string, node *> &lookup_table,
				float alpha = 0.01, // probability of returning to the top node at any given step)
				float walk_iterations = 10) // number of random walk iterations performed
{
	node *start_node;

	// Check that the start category is valid
	try {
		start_node = lookup_table[start_node_name];
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


	// Because there are so many nodes, we use a BFS to keep track of which nodes we must compute random-walker probability for. 

	list<node *> *tovisit_now = new list<node *>;
	list<node *> *tovisit_next = new list<node *>;
	list<node *> *allvisited = new list<node *>; // keeps track of all visited nodes so we know which random-walker nodes we might have reached by this step
	
	unordered_map<node *, bool> *visit_statuses = new unordered_map<node *, bool>;

	tovisit_now->push_back(start_node);
	allvisited->push_back(start_node);

	// Perform, walk_iterations times,
	// a breadth-first-search step followed by a random walker recomputation step
	for (int j = 0; j < walk_iterations; j++) {
		cout << j << endl;

		// Perform one layer of BFS.  
		while (!tovisit_now->empty()) {
			node *nextnode = tovisit_now->front();
			tovisit_now->pop_front();

			// Visit all children and parents of this node. 
			for (node *child : nextnode->children) {

				// Make sure the node we're now considering has never been visited before and is not already scheduled to be visited. 
				if (visit_statuses->find(child) == visit_statuses->end()) {

					tovisit_next->push_back(child);
					(*visit_statuses)[child] = true;
				}
			}

			for (node *parent : nextnode->parents) {

				// Make sure the node we're now considering has never been visited before and is not already scheduled to be visited. 
				if (visit_statuses->find(parent) == visit_statuses->end()) {

					tovisit_next->push_back(parent);
					(*visit_statuses)[parent] = true;
				}
			}

			allvisited->push_back(nextnode);
		}

		// tovisit_now is now empty. Swap it and tovisit_next: on the next BFS cycle, we will visit all the nodes in what is currently tovisit_next. 
		list<node *> *tmp = tovisit_now;
		tovisit_now = tovisit_next;
		tovisit_next = tmp;
		
		cout << "\tBFS done\n";

		// Perform random walk calculations on all nodes in allvisited, since these are the nodes the random walker might have visited by this timestamp
		for (node *n : *allvisited) {

			float probability = 0; // probability the random walker is at this node at the current timestep

			// For all parent nodes and children nodes of n, add up probabilities that we're now there. 
			for (node *p : n->parents) {
				try {
					float parent_last_prob = rw_probabilities_last->at(p->name);
					
					probability += parent_last_prob / count_neighbors_of(p);
				}
				catch (out_of_range &) {
					// this parent node wasn't in rw_probabilities_last, so there is no chance of reaching node n *from* node p
				}
			}
			
			for (node *c : n->children) {
				try {
					float child_last_prob = rw_probabilities_last->at(c->name);
					
					probability += child_last_prob / count_neighbors_of(c);
				}
				catch (out_of_range &) {
					// this parent node wasn't in rw_probabilities_last, so there is no chance of reaching node n *from* node p
				}
			}
			
			// Make final adjustments to probability for n, given that we return randomly to the starting node. 
			probability *= (1 - alpha);
			
			if (n == start_node) {
				probability += alpha;
			}
			
			(*rw_probabilities_current)[n->name] = probability;
		}
		
		// Next cycle: rw_probabilities_current will become rw_probabilities_last and the old rw_probabilities_last will go away. 
		unordered_map<string, float> * tmp2 = rw_probabilities_last;
		tmp2->clear(); // now empty and suitable for reuse
		
		rw_probabilities_last = rw_probabilities_current; // save current state
		rw_probabilities_current = tmp2; // empty
	}
	
	
	// Reached the end of the random walk iterations. Save the data we're interested in. 
	start_node->random_walk_connections = new unordered_map<string, float>;
	
	// Iterate through all the categories we're interested in and save their state
	for (string cat : top_categories) {
		(*(start_node->random_walk_connections))[cat] = (*rw_probabilities_last)[cat];
	}
	

	// Clean up. 
	delete rw_probabilities_last;
	delete rw_probabilities_current;
}


// Manage execution of random_walk calls. 
void perform_all_annotations(unordered_map<string, node *> &lookup_table) {
	
	//int concurrent_tasks = 16; // 8 cpus * 2 (for hyper-threading)
	
	unsigned tasks_completed = 0;
	
	string cats[] = {"American_Roman_Catholics", "Basketball_players_from_Pennsylvania"};
	
	for (string c : cats) {
		random_walk(c, lookup_table);
		
		tasks_completed++;
		
		if (tasks_completed > 2)
			break;
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

	// We don't update distances in this search, just print them
	top->vs = visit_status::GRAY;

	tovisit.push(top);

	// Perform BFS to visit all articles accessible from the rootcat. 
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

		if (nextnode->random_walk_connections != nullptr) {

			// Print out the supercat scores vector for this category
			cout << nextnode->name << "> ";

			// Print out each score
			for (auto it = nextnode->random_walk_connections->cbegin(); it != nextnode->random_walk_connections->cend(); it++) {

				cout << it->first << ": " << it->second << ", ";
			}

			cout << endl;
		}
	}
}


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
	perform_all_annotations(lookup_table);

	// Output all categories and their annotated values
	tree_dump_annotations(root_category, lookup_table);
}

