#ifndef __list_pool_header_guard__
#define __list_pool_header_guard__

#include <cstddef>
#include <vector>
#include <stdexcept>
#include <iterator>


// I know that this is not Python, nor Rust, but I like self (explicit) and find '->' to be very ugly
#define self (*this)

// a simple macro for stripping the constness from (this), it lets me call a non-const method from a const method. 
// I know it is not reccomended but it comes handy for avoiding code duplication, 
// e.g., for list_pool::value() and list_pool::next()
#define $deconst(type) ( *(const_cast<type*>(this)) )


// class declaration needed for telling list_pool_iterator::Iter about its friend class list_pool;  
// This way the iterator's constructor can be made private and only list_pool objects are allowed to 
// create them and modify their state 
template <typename Value, typename Index>
class list_pool; 

// I create a namespace for the iterator so I have to type list_pool_iterator only once and 
// I can call the iterator itself simply Iter
namespace list_pool_iterator {
    // all the methods of the class are marked noexcept because we control the state 
    // of the iterator and we know that nothing bad can happen 
    template <typename Pool, typename Value, typename Index> 
    class Iter {
        friend class list_pool<Value, Index>; 

        Pool* pool; 
        Index current; 

        void update() noexcept {
            if ( self.current != Index(0) ) {
                self.current = (*self.pool).node( self.current ).next; 
            }
        }
        Value& value() noexcept {
            return (*self.pool).node( self.current ).value; 
        }
        
        // the constructor is private in order to prevent anyone except for list_pool from creating the iterator
        // and modifying its state
        Iter(Pool* pool, Index current) noexcept
            : pool{ pool }, 
            current{ current }
        {}

        public: 
        using value_type = Value;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::forward_iterator_tag;


        // default copy/move ctors and assignment are fine, we dont'have any resource to manage
        Iter(const Iter&) noexcept = default; 
        Iter& operator = (const Iter&) noexcept = default; 
        Iter(Iter&&) noexcept = default; 
        Iter& operator = (Iter&& rhs) noexcept = default; 


        Iter& operator ++ () noexcept {
            self.update(); 
            return self; 
        }
        Iter operator ++ (int) noexcept {
            Iter copy{ self }; 
            self.update(); 
            return copy; 
        }

        Value& operator * () noexcept {
            return self.value(); 
        }


        bool operator == (const Iter& rhs) const noexcept {
            return (self.pool == rhs.pool and self.current == rhs.current); 
        }
        bool operator != (const Iter& rhs) const noexcept {
            return ( not (self == rhs) ); 
        }
    }; 
}


template <typename Value, typename Index = std::size_t>
class list_pool {
    struct Node{
        Value value;
        Index next;

        Node(const Value& value, Index next)
            : value{ value }, 
            next{ next }
        {}
        Node(Value&& value, Index next)
            : value{ std::move(value) },
            next{ next }
        {}

        // default copy/move ctors and assignment are fine, we dont'have any resource to manage.
        // Value and Index will take care of moving themselves
        Node(const Node&) = default; 
        Node& operator = (const Node&) = default; 
        Node(Node&&) noexcept = default; 
        Node& operator = (Node&&) noexcept = default; 
    };
    
    using Size = typename std::vector<Node>::size_type;

    std::vector<Node> pool;
    Index free_node_list; // at the beginning, it is empty

    // of course we must ensure that 0 < index <= pool.size(). 
    // check_index1() and check_index2() will perform this check and throw an exception 
    // when the check fails. 
    Node& node(Index index) noexcept { return self.pool[ index - 1 ]; }
    const Node& node(Index index) const noexcept { return self.pool[ index - 1 ]; }


    public:
    list_pool() 
        : pool{ std::vector<Node>() }, 
        free_node_list{ self.new_list() }
    {}
    explicit list_pool(Size n) : list_pool() { // reserve n nodes in the pool
        self.reserve( n ); 
    } 

    // default copy/move ctors and assignment are fine, std::vector will care of itself
    list_pool(const list_pool&) = default;
    list_pool& operator = (const list_pool&) = default; 
    list_pool(list_pool&&) noexcept = default; 
    list_pool& operator = (list_pool&&) noexcept = default; 


    using iterator = list_pool_iterator::Iter<list_pool, Value, Index>;
    using const_iterator = list_pool_iterator::Iter<const list_pool, const Value, Index>;
    
    // iterators are friends so they can use private methods of the class (faster and noexcept)
    friend iterator; 
    friend const_iterator; 


    // creating an iterator cannot go wrong, thus alla these methods are noexcept
    iterator begin(Index index) noexcept {
        return iterator{ &self, index };  
    }
    iterator end(Index) noexcept { // this is not a typo
        return iterator{ &self, self.end() }; 
    }
    const_iterator begin(Index index) const noexcept {
        return const_iterator{ &self, index }; 
    }
    const_iterator end(Index ) const noexcept {
        return const_iterator{ &self, self.end() }; 
    }
    const_iterator cbegin(Index index) const noexcept {
        return self.begin( index ); 
    }
    const_iterator cend(Index index) const noexcept {
        return self.end( index ); 
    }


    Index new_list() noexcept { // return an is_empty list
        return self.end(); 
    }

    // std::vector<Node>::reserve() might throw, so this cannot be noexcept
    void reserve(Size n) { // reserve n nodes in the pool
        self.pool.reserve( n ); 
    }

    Size capacity() const noexcept { // the capacity of the pool
        return self.pool.capacity(); 
    }
    Size size() const noexcept {
        return self.pool.size(); 
    }

    bool is_empty(Index head) const noexcept {
        return ( self.end() == head ); 
    }

    // should this method be static? 
    Index end() const noexcept { 
        return Index( 0 ); 
    }

    // when an out-of-range index is passed an expception is thrown
    Value& value(Index index) {
        self.check_index2( index ); 
        return self.node( index ).value;
    }
    // the non-const version is called to avoid code duplication
    const Value& value(Index index) const {
        return $deconst(list_pool).value( index ); 
    }

    // when an out-of-range index is passed an expception is thrown
    Index& next(Index index) {
        self.check_index2( index ); 
        return self.node( index ).next;  
    }
    // the non-const version is called to avoid code duplication
    const Index& next(Index index) const {
        return $deconst(list_pool).next( index ); 
    }
    
    Index push_front(const Value& value, Index head) {
        return fpush_front( value, head ); 
    }
    Index push_front(Value&& value, Index head) {
        return fpush_front( std::move(value), head ); 
    }
        
    Index push_back(const Value& val, Index head) {
        return fpush_back( val, head ); 
    }
    Index push_back(Value&& value, Index head) {
        return fpush_back( std::move(value), head );
    }
            
    // If you pass an out-of-range head, check_index1() will throw an expception.
    // index can however be 0 and the method will do nothing.
    Index free(Index head) { // delete first node 
        if ( self.is_empty(head) ) {
            return head; 
        } 

        // we return the index of the deleted node's next node (i.e. the new head of the list).
        // head cannot be returned because it "points" to a deleted node, i.e., it is a dandling index.  

        // here we could also say nothing and simply return self.end() in the case in which head is out of range, 
        // but this silent error could indicate and be the cause of broader and more widespread bugs in the code using this class
        self.check_index1( head ); 
        
        // the node to be deleted is identified and its next (to be returned to the caller of this method) is stored
        Node& node{ self.node( head ) }; 
        Index next{ node.next }; 

        // the node to be deleted is simply prepended to free_node_list. 
        // Important: node.value is not deleted, this will (maybe) happen later when 
        // (due to a call to push_front or push_back) the Value's copy or move assignment will 
        // be called on node.value
        node.next = self.free_node_list; 
        self.free_node_list = head; 
        return next; 
    }
                
    // Why no noexcept? Same reason as for free(), head might be out of range. 
    Index free_list(Index head) { // free entire list
        if ( self.is_empty(head) ) {
            return head; 
        }
        // A simple implementation would be to simple traverse the list 
        // and call self.free() on each node, however this would be wasteful. 
        // It is more elegant (and probably faster) to prepend the entire list identified by head
        // to free_node_list.
        
        // the same reasoning as for free() holds here as well 
        self.check_index1( head ); 
        
        // the tail of the list is identified and it is made aware that now its 
        // next node is the head of the free_node_list 
        Node& tail{ self.node( self.get_tail(head) ) }; 
        tail.next = self.free_node_list; 
 
        // now x becomes the head of the free_node_list 
        self.free_node_list = head; 

        // I return a new empty list. 
        // Now x is very dangerous: the list identified by x is gone and now x, 
        // although perfectly in range, points to a deleted node, i.e. a dandling index.
        return self.new_list(); 
    }

    private: 
    // this method is not marked as "noexcept" because both std::vector<Node>::emplace_back() and 
    // Value& Value::operator = (const Value&) could throw an expectation.
    // The 'f' in front of the names of the method and of the type expresses that this is a "forwarding" reference.
    template <typename fValue> 
    Index fpush_front(fValue&& value, Index head) {
        // head is allowed to be 0, we will simply add the first element to an empty list. 
        // However we must check that head <= self.pool.size().
        self.check_index1( head );

        // the "index" of the newly added node will be returned. 
        // Because we are pushing in front of the current newhead of the list, 
        // it will identify the new head.
        Index newhead; 

        if ( self.is_empty(self.free_node_list) ) {
            // there are no available free nodes in free_node_list, so we must allocate a new one. 
            self.pool.emplace_back( std::forward<fValue>(value), head );  
            newhead = self.pool.size(); // the index + 1, just what we need 

        } else {
            // we reuse the first node of free_node_list 
            newhead = self.free_node_list; 
            
            // the first node of free_node_list (newhead) is popped from free_node_list 
            // and its successor become the new newhead of free_node_list 
            Node& node = self.node( newhead ); 
            self.free_node_list = node.next; 

            // node.value is replaced by the given value.
            // Only at this moment node.value is destroyed by Value's copy or move assignment operator. 
            node.value = std::forward<fValue>( value ); 
            node.next = head; 
        }
        return newhead; 
    }
    
    // this method is not marked as "noexcept" because both std::vector<Node>::emplace_back() and 
    // Value& Value::operator = (const Value&) could throw an expectation.
    // The 'f' in front of the names of the method and of the type expresses that this is a "forwarding" reference.
    template <typename fValue> 
    Index fpush_back(fValue&& value, Index head) {
        // if the list is empty we delegate adding the first element to fpush_front()
        if ( self.is_empty(head) ) {
            return self.fpush_front( std::forward<fValue>(value), head ); 
        }

        Index tail{ self.get_tail(head) }; 

        if ( self.is_empty(self.free_node_list) ) {
            // there are no available free nodes in free_node_list, so we must allocate a new one. 
            self.pool.emplace_back( std::forward<fValue>(value), self.end() );  
            self.next( tail ) = self.pool.size(); // the index + 1, just what we need 
            
        } else {
            // we reuse the first node of free_node_list. 
            self.next(tail) = self.free_node_list; 

            // the first node of free_node_list (newhead) is popped from free_node_list 
            // and its successor become the new head of free_node_list 
            Node& node = self.node( self.free_node_list ); 
            self.free_node_list = node.next; 

            // node.value is replaced by the given value.
            // Only at this moment node.value is destroyed by Value's copy or move assignment operator. 
            node.value = std::forward<fValue>( value ); 
            node.next = self.end(); 
        }
        return head; 
    }

    
    // two simple methods ensuring that the given index is in range
    void check_index1(const Index& index) const {
        if ( index > self.pool.size() ) {
            throw std::invalid_argument{ "the value of the provided index is invalid, too big" }; 
        }
    }
    void check_index2(const Index& index) const {
        if ( self.is_empty(index) ) { 
            throw std::invalid_argument{ "the list should not be empty" }; 
        }
        check_index1( index ); 
    }


    // a helper method for getting the tail of a list given the index of one of its nodes. 
    // The index of the tail is so that self.is_empty( self.next(index) ) is true.
    Index get_tail(Index index) const noexcept {
        Index next; 
        // we already know that index is is_empty(index) is false, so it is safe
        while (true) {
            next = self.node( index ).next;
            if ( self.is_empty(next) ) {
                break; 
            }
            index = next; 
        }
        return index; 
    }
};

#undef self
#endif // __list_pool_header_guard__
