// list.h - minimal std::list replacement for MSVC 4.1 / Win32s
//
// This file is part of the TightVNC Win32s / Windows 3.1 port.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
//
// vncServer.h and vncClient.h use list<HWND>, list<vncClientId> and
// list<vncClient*>.  They relied on "#include <list.h>", i.e. the STL that
// shipped with the compiler.  MSVC 4.1's bundled STL cannot compile these
// instantiations here for the same reason its map<> could not (see map.h):
// the template parser chokes on the nested iterator types once the container
// is used as a class member of a class that is itself forward-declared.
//
// This is a hand-written singly-linked list with just enough of the std::list
// interface for the server.  It is deliberately NOT a general-purpose
// container.
//
// SUPPORTED (this is the complete set the server uses):
//   push_back(v)   push_front(v)   front()   pop_front()
//   begin()  end()  empty()  size()  clear()
//   erase(iterator)                 - returns void, unlike std::list
//   erase(iterator, iterator)       - whole-list form only
//   copy construction and assignment (deep)
//   iterator: *i, ++i (prefix), i++ (postfix), i != j, i == j
//             (NOT i-> : see the note on operator-> below)
//
// DELIBERATELY ABSENT: pop_back, insert, remove, sort, reverse, back, rbegin,
// const_iterator, allocators.  If you need one, add it here - do not switch
// back to the compiler's STL.
//
// IMPORTANT SEMANTIC DIFFERENCES from std::list - the server code depends on
// the first two, so do not "fix" them:
//
//  1. erase(i) INVALIDATES i.  Every call site in vncServer.cpp breaks out of
//     its loop immediately after erasing, so this is safe as written.  Do not
//     add code that continues iterating after an erase.
//
//  2. Iteration order for push_front is front-to-back, i.e. most recently
//     pushed first.  m_notifyList uses push_front and only ever iterates to
//     find-and-erase or to notify everything, so order does not matter there.
//
//  3. size() is O(n).  It is called from AuthClientCount()/UnauthClientCount(),
//     which are not on the update path.
//
//  4. Copy construction and assignment are deep copies (vncServer::ClientList()
//     needs both).  clear() is also provided because operator= needs it.
//
// ---------------------------------------------------------------------------

#ifndef VNC_LIST_H__
#define VNC_LIST_H__

#include <string.h>		// memset, used by front()

// ===========================================================================
// DECLARED IN THE GLOBAL NAMESPACE, NOT IN namespace std.
//
// This was the cause of
//     error C2955: 'list' : class template name expecting parameter list
// on the very first file compiled (AdministrationControls.cpp).
//
// The template itself was fine.  The problem was the line that used to sit at
// the bottom of this file:
//
//     namespace std { template <class T> class list { ... }; }
//     using std::list;                  // <-- C2955 here
//
// MSVC 4.1 predates the C++ standard's namespace rules and does not support a
// USING-DECLARATION that names a class TEMPLATE.  It parses "using std::list;"
// as a reference to the template as a complete type, finds no parameter list,
// and reports C2955 - and it does so from inside list.h itself, which is why the
// error appeared for every translation unit regardless of what that unit used.
//
// Note that map.h has the same structure but does NOT hit this, because nothing
// there needs a using-declaration: vncKeymap.cpp writes the qualified name
// "std::map<CARD32,CARD8>" explicitly, so the template is only ever named
// through its namespace.
//
// The server writes list<> UNQUALIFIED everywhere:
//     typedef list<HWND> vncNotifyList;              (vncServer.h)
//     typedef list<vncClientId> vncClientList;       (vncClient.h)
//     typedef list<RECT> rectlist;                   (RectList.h)
// and nothing in the tree writes "std::list" outside of comments.  So the
// namespace bought nothing and cost a compile error - the template simply goes
// in the global namespace, which is also where the pre-standard <list.h> that
// this file replaces put it.
// ===========================================================================

template <class T>
class list {
private:
	struct node {
		T      value;
		node  *next;
		node(const T& v) : value(v), next(0) {}
	};

	node *head;
	node *tail;

public:
	// -------------------------------------------------------------------
	// Iterator.
	//
	// Holds the current node and, so that erase() can unlink in a singly
	// linked list, its predecessor.
	// -------------------------------------------------------------------
	class iterator {
	public:
		node *cur;
		node *prev;

		iterator() : cur(0), prev(0) {}
		iterator(node *c, node *p) : cur(c), prev(p) {}

		T& operator*() const { return cur->value; }

		// operator-> is NOT provided.
		//
		// It used to be "T* operator->() const { return &cur->value; }", which
		// made MSVC 4.1 emit, for every instantiation:
		//
		//   warning C4284: return type for 'list<short>::iterator::operator ->'
		//   is not a UDT or reference to a UDT.  Will produce errors if applied
		//   using infix notation
		//
		// The warning is correct and unavoidable: the element types here are
		// vncClientId (short), HWND (void *) and RECT.  For the first two, T is
		// not a class, so "i->something" is meaningless - the compiler is warning
		// that the operator can never be used as written.
		//
		// Nothing in the server uses "i->" on a list iterator (verified across
		// vncServer.cpp, vncClient.cpp and vncDesktop.cpp; the only "->" uses on
		// iterators are in map.h's consumers, where the element IS a class).
		// Removing it silences four warnings per build and loses nothing.
		//
		// If a list of class objects ever needs it, add it back guarded by the
		// instantiation - or just write (*i).member, which always works.

		// Prefix
		iterator& operator++() {
			if (cur != 0) {
				prev = cur;
				cur = cur->next;
			}
			return *this;
		}
		// Postfix
		iterator operator++(int) {
			iterator tmp = *this;
			++(*this);
			return tmp;
		}

		int operator==(const iterator& o) const { return cur == o.cur; }
		int operator!=(const iterator& o) const { return cur != o.cur; }
	};

	list() : head(0), tail(0) {}

	~list() {
		clear();
	}

	// Copy construction and assignment ARE needed, and must be deep copies:
	//
	//   vncServer::ClientList()  (vncServer.cpp) does
	//       vncClientList clients;
	//       clients = m_authClients;      // <- operator=
	//       return clients;               // <- copy constructor
	//
	// The compiler-generated versions would copy head/tail verbatim, so the
	// returned list and m_authClients would share nodes and both destructors
	// would free them.  That is a double-free on every call - and this function
	// is called from vncServer::KillAuthClients and the properties dialog.
	//
	// ===================================================================
	// MSVC 4.1: THE TEMPLATE PARAMETER LIST IS MANDATORY HERE.
	//
	// These must be written "list<T>", not bare "list".  Standard C++ injects
	// the class name into its own scope, so inside a class template "list"
	// alone means "list<T>" - but MSVC 4.1 predates that rule and reports
	//
	//     error C2955: 'list' : class template name expecting parameter list
	//
	// for every use of the bare name as a TYPE.  Note that the constructor and
	// destructor NAMES ("list()" / "~list()") are correct as they stand; it is
	// only type positions - parameters, return types - that need <T>.
	//
	// The same applies to map.h, whose copy-protection declarations were
	// written the same way and have been corrected in the same manner.
	// ===================================================================
	list(const list<T>& o) : head(0), tail(0) {
		copyFrom(o);
	}

	list<T>& operator=(const list<T>& o) {
		if (this != &o) {
			clear();
			copyFrom(o);
		}
		return *this;
	}

	void clear() {
		node *n = head;
		while (n != 0) {
			node *next = n->next;
			delete n;
			n = next;
		}
		head = tail = 0;
	}

private:
	// "const list<T>&", not "const list&" - see the note above.
	void copyFrom(const list<T>& o) {
		node *n = o.head;
		while (n != 0) {
			push_back(n->value);
			n = n->next;
		}
	}

public:
	void push_back(const T& v) {
		node *n = new node(v);
		if (n == 0)
			return;					// MSVC 4.1: new returns 0, does not throw
		if (tail == 0) {
			head = tail = n;
		} else {
			tail->next = n;
			tail = n;
		}
	}

	void push_front(const T& v) {
		node *n = new node(v);
		if (n == 0)
			return;
		n->next = head;
		head = n;
		if (tail == 0)
			tail = n;
	}

	iterator begin() { return iterator(head, 0); }
	iterator end()   { return iterator(0, 0); }

	// front() / pop_front() are used by vncClient::SendRectangles.
	// front() on an empty list is undefined in std::list; here it returns a
	// default-constructed T rather than dereferencing null, because
	// SendRectangles' loop condition is checked separately from the access.
	T front() {
		if (head != 0)
			return head->value;
		T empty;
		memset(&empty, 0, sizeof(empty));
		return empty;
	}

	void pop_front() {
		if (head == 0)
			return;
		node *n = head;
		head = head->next;
		if (head == 0)
			tail = 0;
		delete n;
	}

	// Range erase, for "rects.erase(rects.begin(), rects.end())".
	// Only the whole-list form is supported, which is the only form used.
	void erase(iterator first, iterator last) {
		if (first.cur == head && last.cur == 0) {
			clear();
			return;
		}
		// Partial range: walk and unlink one at a time.  Not used today.
		iterator i = first;
		while (i != last && i.cur != 0) {
			iterator next = i;
			++next;
			erase(i);
			i = next;
		}
	}

	int empty() const { return head == 0; }

	unsigned int size() const {
		unsigned int c = 0;
		node *n = head;
		while (n != 0) { c++; n = n->next; }
		return c;
	}

	// Unlink the element the iterator refers to.  The iterator is invalid
	// afterwards - see the note at the top of this file.
	void erase(iterator i) {
		if (i.cur == 0)
			return;
		if (i.prev == 0) {
			// Removing the head.
			head = i.cur->next;
		} else {
			i.prev->next = i.cur->next;
		}
		if (i.cur == tail)
			tail = i.prev;
		delete i.cur;
	}
};

// (No "} // namespace std" and no "using std::list;" - see the long note above.
// The template is declared directly in the global namespace, which is where the
// pre-standard <list.h> this file replaces also put it.)

#endif // VNC_LIST_H__
