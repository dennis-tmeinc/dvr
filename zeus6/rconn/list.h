#ifndef __LIST_H__
#define __LIST_H__

struct list_node {
	void * item ;
	list_node * prev ;
	list_node * next ;
};

// a simple list
class slist {
protected:
	list_node * head ;
	list_node * tail ;
	
public:
	slist() {
		head = tail = NULL ;
	}
	
	// add item to tail of list
	void add( void * item )
	{
		if( tail == NULL ) {
			head = tail = new list_node ;
			tail->item = item ;
			tail->prev = NULL ;
			tail->next = NULL ;
		}
		else {
			list_node * n = new list_node ;
			n->item = item ;
			n->prev = tail ;
			n->next = NULL ;
			tail->next = n ;
			tail = n ;
		}
	}

	// remove this note from list
	list_node * remove( list_node * node ) {
		if( node==NULL ) return NULL;
		list_node * p = node->prev ;
		list_node * n = node->next ;
		if( p ) p->next = n ;
		else head = n ;
		if( n ) n->prev = p ;
		else tail = p ;
		delete node ;
		return n ;
	}

	list_node * first() {
		return head ;
	}

	list_node * last() {
		return tail ;
	}
	
};

#endif	// __LIST_H__
