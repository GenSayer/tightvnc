#ifndef DETECT_MAP_H
#define DETECT_MAP_H

// Place this directly inside namespace std if your files expect std::map, 
// or simply keep it global if you strip the "std::" prefix.
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
	
	// Supports: map.find(key) on non-const map instances:
    iterator find(const Key& key) {
        Node* current = head;
        while (current != 0) {
            if (current->data.first == key) {
                return iterator(current);
            }
            current = current->next;
        }
        return end();
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
