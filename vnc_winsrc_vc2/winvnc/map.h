#ifndef DETECT_MAP_H
#define DETECT_MAP_H

#include <string.h>	// memset - see map_value below

// Minimal std::map replacement for MSVC 4.1 / Win32s.
//
// The compiler's bundled STL cannot compile the instantiations this project
// needs.  This is a singly-linked list with map-like syntax: O(n) lookup, which
// is fine for the only user here (vncKeymap's keysym->VK tables, a few hundred
// entries, built once at startup).
//
// WIN32S / MSVC 4.1 REVIEW NOTES - behaviour that differs from real std::map and
// that callers must not rely on:
//
//   * Iteration order is REVERSE INSERTION order, not sorted key order.
//     vncKeymap never iterates, so this does not matter - but do not add code
//     that assumes ordering.
//
//   * operator[] INSERTS a default-constructed value when the key is absent,
//     like std::map.  vncKeymap uses find() for lookups and operator[] only when
//     building the tables, which is correct.
//
//   * No erase(), no size(), no insert(), no reverse iterators.  Nothing needs
//     them.
//
//   * COPYING IS NOT IMPLEMENTED.  See the private declarations below: the
//     compiler-generated copy constructor and operator= would duplicate 'head',
//     giving two maps that own the same node list, and both destructors would
//     free it.  key_mapper is a file-scope object that is never copied, so
//     declaring them private turns any future copy into a link error rather than
//     a double-free.
//
// VC++ 2.x: GLOBAL NAMESPACE, NO NESTED CLASSES.
//
// The MSVC 4.1 version lived in "namespace std" and declared value_type, Node
// and const_iterator as classes nested inside the template.  VC++ 2.x
// support for both is unreliable - namespaces arrived late in the 2.x/4.x line
// and the first template implementation mishandles classes nested in a class
// template - so this version:
//
//   * is declared in the global namespace, exactly as list.h already is, and
//     the callers say map<...> rather than std::map<...>;
//   * builds the pieces as separate top-level templates (map_value, map_node,
//     map_const_iterator) and gives map<> typedefs with the old names, so
//     map<K,V>::const_iterator, ::iterator and ::value_type still work.
//
// Behaviour is unchanged.

template <class Key, class Value>
struct map_value {
    Key first;
    Value second;
    // VC++ 2.x: "Key()" / "Value()" on a plain type gives C4304 ("simple type
    // cast conversion of no expression") and does not zero it.  Every type
    // stored here is plain data (CARD32, CARD8, int, char *, rfbCapabilityInfo),
    // so zero-filling gives exactly what std::map value-initialisation would.
    map_value() { memset(this, 0, sizeof(*this)); }
    map_value(const Key& k, const Value& v) : first(k), second(v) {}
};

template <class Key, class Value>
struct map_node {
    map_value<Key, Value> data;
    map_node<Key, Value>* next;
    // New entry for operator[]: zero-filled value (see map_value), given key.
    map_node(const Key& k, map_node<Key, Value>* n) : next(n) { data.first = k; }
};

template <class Key, class Value>
class map_const_iterator {
private:
    map_node<Key, Value>* current;
public:
    map_const_iterator() : current(0) {}
    map_const_iterator(map_node<Key, Value>* p) : current(p) {}

    // Reference access mimics iter->second
    map_value<Key, Value>* operator->() const { return &(current->data); }
    map_value<Key, Value>& operator*() const { return current->data; }

    // Post-increment syntax for loops: iter++
    map_const_iterator<Key, Value> operator++(int) {
        map_const_iterator<Key, Value> temp = *this;
        if (current != 0) current = current->next;
        return temp;
    }

    bool operator==(const map_const_iterator<Key, Value>& other) const { return current == other.current; }
    bool operator!=(const map_const_iterator<Key, Value>& other) const { return current != other.current; }
};

template <class Key, class Value>
class map {
public:
    typedef map_value<Key, Value> value_type;
    typedef map_const_iterator<Key, Value> const_iterator;
    typedef map_const_iterator<Key, Value> iterator;

private:
    map_node<Key, Value>* head;

public:
    map() : head(0) {}

    ~map() {
        clear();
    }

private:
    // Not implemented - see the note at the top of this file.
    // MSVC 4.1: the template parameter list is MANDATORY in type positions.
    // Bare "map" inside the class template gives
    //     error C2955: 'map' : class template name expecting parameter list
    // because MSVC 4.1 predates the injected-class-name rule.  Constructor and
    // destructor NAMES are fine unqualified; parameters and return types are not.
    map(const map<Key, Value>&);
    map<Key, Value>& operator=(const map<Key, Value>&);

public:

    void clear() {
        map_node<Key, Value>* current = head;
        while (current != 0) {
            map_node<Key, Value>* nextNode = current->next;
            delete current;
            current = nextNode;
        }
        head = 0;
    }

    // Supports: map[key] = value syntax
    Value& operator[](const Key& key) {
        map_node<Key, Value>* current = head;
        while (current != 0) {
            if (current->data.first == key) {
                return current->data.second;
            }
            current = current->next;
        }
        // If key not found, insert a fresh node at the head
        head = new map_node<Key, Value>(key, head);
        return head->data.second;
    }

    // Supports: map.find(key).  iterator and const_iterator are the same
    // type, so one const overload serves both the const and non-const callers.
    map_const_iterator<Key, Value> find(const Key& key) const {
        map_node<Key, Value>* current = head;
        while (current != 0) {
            if (current->data.first == key) {
                map_const_iterator<Key, Value> found(current);
                return found;
            }
            current = current->next;
        }
        map_const_iterator<Key, Value> none(0);
        return none;
    }

    // Supports: loop bounds tracking
    //
    // VC++ 2.x: iterators are built as named locals of the real template type.
    // "return const_iterator(head);" - constructing through the typedef -
    // crashed the compiler (C1001 INTERNAL COMPILER ERROR, msc1.cpp line 1056).
    map_const_iterator<Key, Value> begin() const { map_const_iterator<Key, Value> it(head); return it; }
    map_const_iterator<Key, Value> end() const { map_const_iterator<Key, Value> it(0); return it; }
};

#endif // DETECT_MAP_H
