/* xml parser */

#define XMLRAW   (0)
#define XMLNODE  (1)
#define XMLATTR  (2)


class xmlelement {
protected:
  int    m_type ;              // 0: comments, DTDs, 1: nodes, 2: attributes
  string m_name ;
  string m_content ;
  xmlelement * m_next ;
  xmlelement * m_node ;       // first child node
  xmlelement * m_attrib ;     // attributes
public:
  xmlelement() :
     m_type(0),
     m_next(NULL),
     m_node(NULL),
     m_attrib(NULL)
  {}

  xmlelement( int type, char * name, char * content )
  {
    m_type = type ;
    m_name = name ;
    m_content = content ;
    m_next = NULL ;
    m_node = NULL ;
    m_attrib = NULL ;
  }

  void xmladdnode( char * name, char * content )
  {
     xmlelement * ne = new xmlelement( XMLNODE, name, content ) ;
     ne->m_next = m_node ;
     m_node = ne ;
  }

  void xmldelnode( char * name )
  {
  }

  void xmladdattrib( char * name, char * content )
  {
     xmlelement * na = new xmlelement( XMLATTR, name, content ) ;
     ne->m_next = m_attrib;
     m_attrib = ne ;
  }

  void xmldelattrib( char * name )
  {
  }

  xmlelement * getnode( char * name )
  {
     xmlelement * xmle ;
     xmle = m_node ;
     while( xmle ) {
        if( xmle->name == name ) {
            return xmle ;
        }
        xmle = xmle->m_next ;
    }
    return NULL ;
  }

  xmlelement * getattrib( char * name )
  {
     xmlelement * xmle ;
     xmle = m_attrib ;
     while( xmle ) {
        if( xmle->name == name ) {
            return xmle ;
        }
        xmle = xmle->m_next ;
    }
    return NULL ;
  }

  int parsexml( char * xml ) 
  {
      char * c ;
      while( isspace() );
      if( *c == '<' ) {
         xmlelement * nxml = new xmelement() ;
         // parse type 
         nxml->m_type = 1 ;
         // parse name 
         nxml->name = name ;

         // parse attrib

          // if contents available ?
         // parse content
          nxml->m_content = contents ;
          // while found child node '<'
          nxml = new xmelement();
          nxml->parsexml( c ) ;
          // found '</name>' or '/>' 
          return 1;
      }
  }

