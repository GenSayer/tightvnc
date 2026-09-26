#ifndef DETECT_MAP_H
#define DETECT_MAP_H

// Minimal std::map replacement for MSVC 4.1, whose bundled STL cannot compile
// the real thing here.  This is a singly-linked list with map-like syntax; it is
// O(n) per lookup, which is fine because the only user is CapsContainer with a
// few dozen entries.
//
// WIN32S/MSVC 4.1 REVIEW NOTES (behaviour differences from real std::map that
// callers must not rely on):
//
//   * Iteration order is reverse insertion order, not sorted key order.
//     CapsContainer only iterates in its destructor, so this does not matter -
//     but do not add code that assumes ordering.
//   * operator[] inserts a default-constructed value when the key is absent,
//     like std::map.  Note the consequence in CapsContainer::GetDescription
//     and IsEnabled: they call operator[] on a map, so a *lookup* of an unknown
//     code silently inserts an entry.  Both are guarded by IsKnown() first, so
//     the behaviour is correct, but it is why those guards must stay.
//   * find() is const but returns a mutable-through-pointer iterator; that is
//     deliberate, because CapsContainer::Enable needs to write through it.
//   * No erase(), no size(), no insert().  Nothing needs them.
//   * Copying is disabled - see the private declarations below.
//
// Placing this in namespace std is technically illegal, but MSVC 4.1 accepts it
// and the existing sources say std::map<...>.
namespace std {

template <class Key, class Value>
class map {
public:
    // A pair structure to mimic std::pair and support iter->first / iter->second
    struct value_type {
        Key first;
        Value second;
        value_type() : first(Key()), second(Value()) {}
        value_type(const Key& k, const Value& v) : first(k), second(v) {}
    };

private:
    struct Node {
        value_type data;
        Node* next;
        Node(const Key& k, const Value& v, Node* n) : data(k, v), next(n) {}
    };

    Node* head;

public:
    // Forward declaration of iterator to support const_iterator syntax
    class const_iterator {
    private:
        Node* current;
    public:
        const_iterator() : current(0) {}
        const_iterator(Node* p) : current(p) {}

        // Reference access mimics iter->second
        value_type* operator->() const { return &(current->data); }
        value_type& operator*() const { return current->data; }

        // Post-increment syntax for loops: iter++
        const_iterator operator++(int) {
            const_iterator temp = *this;
            if (current != 0) current = current->next;
            return temp;
        }

        bool operator==(const const_iterator& other) const { return current == other.current; }
        bool operator!=(const const_iterator& other) const { return current != other.current; }
    };

    // Typedefs for code clarity and matching user files
    typedef const_iterator iterator;

    map() : head(0) {}

    ~map() {
        clear();
    }

private:
    // Copying is NOT implemented.  The default compiler-generated copy
    // constructor and operator= would copy 'head' verbatim, giving two maps
    // that own the same node list - the second destructor then frees nodes
    // that were already freed, and CapsContainer's destructor additionally
    // does "delete[] iter->second" over them.
    //
    // Nothing in the viewer copies a map today (the four CapsContainer members
    // of ClientConnection are never copied), so declaring these private and
    // leaving them undefined turns any future copy into a link error instead of
    // a heap corruption.  Do not "fix" this by defining them unless a deep copy
    // is actually implemented.
    // MSVC 4.1: the template parameter list is MANDATORY in type positions.
    // Bare "map" inside the class template gives
    //     error C2955: 'map' : class template name expecting parameter list
    // because MSVC 4.1 predates the injected-class-name rule.  Constructor and
    // destructor NAMES are fine unqualified; parameters and return types are not.
    map(const map<Key, Value>&);
    map<Key, Value>& operator=(const map<Key, Value>&);

public:

    void clear() {
        Node* current = head;
        while (current != 0) {
            Node* nextNode = current->next;
            delete current;
            current = nextNode;
        }
        head = 0;
    }

    // Supports: map[key] = value syntax
    Value& operator[](const Key& key) {
        Node* current = head;
        while (current != 0) {
            if (current->data.first == key) {
                return current->data.second;
            }
            current = current->next;
        }
        // If key not found, insert a fresh node at the head
        head = new Node(key, Value(), head);
        return head->data.second;
    }

    // Supports: map.find(key)
    const_iterator find(const Key& key) const {
        Node* current = head;
        while (current != 0) {
            if (current->data.first == key) {
                return const_iterator(current);
            }
            current = current->next;
        }
        return end();
    }

    // Supports: loop bounds tracking
    const_iterator begin() const { return const_iterator(head); }
    const_iterator end() const { return const_iterator(0); }
};

} // namespace std

#endif // DETECT_MAP_H
