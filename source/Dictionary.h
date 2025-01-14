/* Dictionary.h
Copyright (c) 2017 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#include <string>
#include <utility>
#include <vector>



// This class stores a mapping from character string keys to values, in a way
// that prioritizes fast lookup time at the expense of longer construction time
// compared to an STL map. That makes it suitable for ship attributes, which are
// changed much less frequently than they are queried.
using DictionaryBase = std::vector<std::pair<const char *, double>>;
class Dictionary : private DictionaryBase {
public:
	Dictionary() noexcept = default;
	Dictionary(const DictionaryBase &base) noexcept : DictionaryBase(base) {}

	// Access a key for modifying it:
	double &operator[](const char *key);
	double &operator[](const std::string &key);
	// Get the value of a key, or 0 if it does not exist:
	double Get(const char *key) const;
	double Get(const std::string &key) const;

	const DictionaryBase &AsBase() const { return *this; }
	
	// Expose certain functions from the underlying vector:
	using std::vector<std::pair<const char *, double>>::empty;
	using std::vector<std::pair<const char *, double>>::begin;
	using std::vector<std::pair<const char *, double>>::end;
};



#endif
