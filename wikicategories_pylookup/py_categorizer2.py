# py_categorizer2.py
# replaces pl_categorizer.pl

# Daniel Richman, July 2013

import bisect
import os
import re # regex
import requests # HTTP and JSON library
import sys # argv, path
from urllib import quote_plus

########################################
# Declare binary search functions
def alphabetic_binary_search(file_ptr, key_to_find, munge_fn):

	# setup min and max
	min_loc = 0
	file_ptr.seek(0, 2) # seek to end
	max_loc = file_ptr.tell()

	current_key = None

	while min_loc < max_loc and current_key != key_to_find:
		file_ptr.seek((min_loc + max_loc) / 2, 0)

		current_line_tuple = get_current_full_line(file_ptr)
		current_key = extract_key(current_line_tuple[1])

		#print min_loc, max_loc
		#print file_ptr.tell(), " ", current_key

		if current_key == key_to_find: # yay
			return current_line_tuple[1]

		if current_key < key_to_find:
			min_loc = file_ptr.tell() # end of line
			continue

		if current_key > key_to_find:
			max_loc = current_line_tuple[0] # beginning of line: don't include a piece of the current (rejected) line in the possible range
			continue

	# If we got here, it means we exited the while loop without returning the found line==>we failed
	return None

# Returns the line from the file that contains the location the file ptr is currently at
def get_current_full_line(file_ptr):

	# find the previous \n in the file (or the beginning of the file)
	while 1:
		current_char = file_ptr.read(1)
		if current_char == '\n' or file_ptr.tell() == 0:
			break

		file_ptr.seek(-2, 1) # seeking -2 seems to move the file pointer 1 char back
		#print "at ", file_ptr.tell()

	# reached a \n or the beginning of the file: now read a line
	# because of the order of the tuple, the first item in the tuple is the location of the BEGINNING of the line
	return (file_ptr.tell(), file_ptr.readline())
##############################################################################

# Given a line from the file, extracts the key from the line
def extract_key(string):
	key = string.split(">")[0]
	key = re.sub('[^\w]', "", key)
	key = key.replace("_", "")
	key = key.lower()
	return key

###############################################
# Begin processing input

query = " ".join(sys.argv[1:])
re.sub('[^\w\s]', "", query) # remove symbols from the query

query = quote_plus(query)

# Query Solr server
# seine.cs.ucsb.edu
r = requests.get("http://128.111.44.157:8983/solr/collection1/query_clustering?q=" + query + "&wt=json")
solr_output = r.json() # parse JSON in one line!

responses = solr_output["response"]["docs"]

# Query articles/categories mapping db

# Access ArticlesSupercats.txt using an absolute path so this script can be called anywhere
this_script_path = sys.path[0]
if len(this_script_path) > 0:
	this_script_path = this_script_path + "/"

with open(this_script_path + "../ArticlesSupercats.txt", "rb") as fi: # open file in binary mode so we can do relative seeks in the binary search

	for r in responses:
		print r
		find_result =  alphabetic_binary_search(fi, extract_key(r["title"]), extract_key)
		if find_result is not None:
			print find_result
