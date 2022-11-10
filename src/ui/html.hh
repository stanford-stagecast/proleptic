#pragma once

#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

class HTMLElement
{
public:
  using Attributes = std::map<std::string, std::optional<std::string>>;
  using Children = std::vector<HTMLElement>;

private:
  bool is_text_ = false;
  std::string tag_;
  Attributes attributes_;
  Children children_;

public:
  HTMLElement( const std::string& tag, const Attributes& attributes, const Children& children )
    : tag_( tag )
    , attributes_( attributes )
    , children_( children )
  {}

  friend std::ostream& operator<<( std::ostream& os, const HTMLElement& e )
  {
    using namespace std;
    if ( e.is_text_ ) {
      os << e.tag_;
      return os;
    }
    os << "<";
    os << e.tag_;
    for ( const auto& x : e.attributes_ ) {
      os << " ";
      os << x.first;
      if ( x.second.has_value() ) {
        os << "=";
        os << quoted( *x.second );
      }
    }
    os << ">";
    for ( const auto& x : e.children_ ) {
      os << x;
    }
    os << "</";
    os << e.tag_;
    os << ">";
    return os;
  }

  static HTMLElement text( const std::string& s )
  {
    HTMLElement element( s, {}, {} );
    element.is_text_ = true;
    return element;
  }

  template<class T>
  static HTMLElement to_string( const T& x )
  {
    std::stringstream ss;
    ss << x;
    return text( ss.str() );
  }

  static HTMLElement head( const Children& children ) { return HTMLElement( "head", {}, children ); }
  static HTMLElement body( const Children& children ) { return HTMLElement( "body", {}, children ); }
  static HTMLElement div( const Attributes& attributes, const Children& children )
  {
    return HTMLElement( "div", attributes, children );
  }
  static HTMLElement span( const Attributes& attributes, const Children& children )
  {
    return HTMLElement( "span", attributes, children );
  }
  static HTMLElement script( const Attributes& attributes, const std::string& body )
  {
    return HTMLElement( "script", attributes, { text( body ) } );
  }
  static HTMLElement p( const Attributes& attributes, const Children& children )
  {
    return HTMLElement( "p", attributes, children );
  }
  static HTMLElement h1( const Attributes& attributes, const Children& children )
  {
    return HTMLElement( "h1", attributes, children );
  }
  static HTMLElement form( const Attributes& attributes, const Children& children )
  {
    return HTMLElement( "form", attributes, children );
  }
  static HTMLElement input( const Attributes& attributes, const Children& children )
  {
    return HTMLElement( "input", attributes, children );
  }
};

class HTMLPage
{
  HTMLElement root_;

public:
  HTMLPage( const HTMLElement::Children& children )
    : root_( HTMLElement( "html", {}, children ) )
  {}

protected:
  friend std::ostream& operator<<( std::ostream& os, const HTMLPage& p )
  {
    os << "<!doctype html>";
    os << p.root_;
    return os;
  }
};
