#include <string>

#include <boost/format.hpp>

#include "multimodal_graph.hh"
#include "db.hh"

using namespace std;

namespace Tempus
{
    Point2D coordinates( const Road::Vertex& v, Db::Connection& db, const Road::Graph& graph )
    {
	string q = (boost::format( "SELECT x(geom), y(geom) FROM tempus.road_node WHERE id=%1%" ) % graph[v].db_id).str();
	Db::Result res = db.exec( q );
	BOOST_ASSERT( res.size() > 0 );
	Point2D p;
	p.x = res[0][0].as<double>();
	p.y = res[0][1].as<double>();
	return p;
    }

    Point2D coordinates( const PublicTransport::Vertex& v, Db::Connection& db, const PublicTransport::Graph& graph )
    {
	string q = (boost::format( "SELECT x(geom), y(geom) FROM tempus.pt_stop WHERE id=%1%" ) % graph[v].db_id).str();
	Db::Result res = db.exec( q );
	BOOST_ASSERT( res.size() > 0 );
	Point2D p;
	p.x = res[0][0].as<double>();
	p.y = res[0][1].as<double>();
	return p;
    }

    Point2D coordinates( const POI* poi, Db::Connection& db )
    {
	string q = (boost::format( "SELECT x(geom), y(geom) FROM tempus.poi WHERE id=%1%" ) % poi->db_id).str();
	Db::Result res = db.exec( q );
	BOOST_ASSERT( res.size() > 0 );
	Point2D p;
	p.x = res[0][0].as<double>();
	p.y = res[0][1].as<double>();
	return p;
    }

    Point2D coordinates( const Multimodal::Vertex& v, Db::Connection& db, const Multimodal::Graph& graph )
    {
	if ( v.type == Multimodal::Vertex::Road )
	{
	    return coordinates( v.road_vertex, db, *v.road_graph );
	}
	else if ( v.type == Multimodal::Vertex::PublicTransport )
	{
	    return coordinates( v.pt_vertex, db, *v.pt_graph );
	}
	// else
	return coordinates( v.poi, db );
    }

    namespace Multimodal
    {
	bool Vertex::operator==( const Vertex& v ) const
	{
	    if ( type != v.type )
		return false;
	    if ( type == Road )
	    {
		return road_graph == v.road_graph && road_vertex == v.road_vertex;
	    }
	    else if (type == PublicTransport )
	    {
		return ( pt_graph == v.pt_graph ) && ( pt_vertex == v.pt_vertex );
	    }
	    // else (poi)
	    return poi == v.poi;
	}

	bool Vertex::operator!=( const Vertex& v ) const
	{
	    return !operator==( v );
	}
	
	bool Vertex::operator<( const Vertex& v ) const
	{
	    if ( type != v.type )
		return (int)type < (int)v.type;
	    
	    if ( type == Vertex::Road )
	    {
		if ( road_graph != v.road_graph )
		    return road_graph < v.road_graph;
		return road_vertex < v.road_vertex;
	    }
	    else if ( type == Vertex::PublicTransport )
	    {
		if ( pt_graph != v.pt_graph )
		    return pt_graph < v.pt_graph;
		return pt_vertex < v.pt_vertex;
	    }
	    // else (poi)
	    return poi < v.poi;
	}
	
	Vertex::Vertex( const Road::Graph* graph, Road::Vertex vertex )
	{
	    type = Road;
	    road_graph = graph;
	    road_vertex = vertex;
	}
	Vertex::Vertex( const PublicTransport::Graph* graph, PublicTransport::Vertex vertex )
	{
	    type = PublicTransport;
	    pt_graph = graph;
	    pt_vertex = vertex;
	}
	Vertex::Vertex( const POI* poi )
	{
	    type = Poi;
	    this->poi = poi;
	}
    
	Edge::ConnectionType Edge::connection_type() const
	{
	    if ( source.type == Vertex::Road )
	    {
		if ( target.type == Vertex::Road )
		{
		    return Road2Road;
		}
		else if ( target.type == Vertex::PublicTransport )
		{
		    return Road2Transport;
		}
		else
		{
		    return Road2Poi;
		}
	    }
	    else if (source.type == Vertex::PublicTransport )
	    {
		if ( target.type == Vertex::Road )
		{
		    return Transport2Road;
		}
		else if (target.type == Vertex::PublicTransport )
		{
		    return Transport2Transport;
		}
	    }
	    else
	    {
		if ( target.type == Vertex::Road )
		{
		    return Poi2Road;
		}
	    }
	    return UnknownConnection;
	}
	
	VertexIterator::VertexIterator( const Multimodal::Graph& graph )
	{
	    graph_ = &graph;
	    boost::tie( road_it_, road_it_end_ ) = boost::vertices( graph_->road );
	    pt_graph_it_ = graph_->public_transports.begin();
	    pt_graph_it_end_ = graph_->public_transports.end();
	    poi_it_ = graph_->pois.begin();
	    poi_it_end_ = graph_->pois.end();
	    boost::tie( pt_it_, pt_it_end_ ) = boost::vertices( pt_graph_it_->second );
	}
	
	void VertexIterator::to_end()
	{
	    BOOST_ASSERT( graph_ != 0 );
	    road_it_ = road_it_end_;
	    pt_graph_it_ = pt_graph_it_end_;
	    pt_it_ = pt_it_end_;
	    poi_it_ = poi_it_end_;
	}
	
	Vertex& VertexIterator::dereference() const
	{
	    BOOST_ASSERT( graph_ != 0 );
	    if ( road_it_ != road_it_end_ )
	    {
		vertex_.type = Vertex::Road;
	    }
	    else
	    {
		if ( pt_it_ != pt_it_end_ )
		{
		    vertex_.type = Vertex::PublicTransport;
		}
		else
		{
		    vertex_.type = Vertex::Poi;
		}
	    }
	    if ( vertex_.type == Vertex::Road )
	    {
		vertex_.road_vertex = *road_it_;
		vertex_.road_graph = &graph_->road;
	    }
	    else if ( vertex_.type == Vertex::PublicTransport )
	    {
		vertex_.pt_graph = &(pt_graph_it_->second);
		vertex_.pt_vertex = *pt_it_;
	    }
	    else if ( vertex_.type == Vertex::Poi )
	    {
		vertex_.poi = &poi_it_->second;
	    }
	    return vertex_;
	}
	
	void VertexIterator::increment()
	{
	    BOOST_ASSERT( graph_ != 0 );
	    if ( road_it_ != road_it_end_ )
	    {
		road_it_++;
	    }
	    else
	    {
		if ( pt_it_ != pt_it_end_ )
		{
		    pt_it_++;
		}

		if ( pt_graph_it_ == pt_graph_it_end_ )
		{
		    if ( poi_it_ != poi_it_end_ )
			poi_it_++;
		}

		while ( pt_it_ == pt_it_end_ && pt_graph_it_ != pt_graph_it_end_ )
		{
		    pt_graph_it_++;
		    if ( pt_graph_it_ != pt_graph_it_end_ )
		    {
			// set on the beginning of the next pt_graph
			boost::tie( pt_it_, pt_it_end_ ) = boost::vertices( pt_graph_it_->second );
		    }
		}
	    }
	}

	bool VertexIterator::equal( const VertexIterator& v ) const
	{
	    // not the same road graph
	    if ( graph_ != v.graph_ )
		return false;

	    bool is_on_road = ( road_it_ != road_it_end_ );
	    if ( is_on_road )
	    {
		return road_it_ == v.road_it_;
	    }

	    // not on the same pt graph
	    if ( pt_graph_it_ != v.pt_graph_it_ )
		return false;
	    
	    // else, same pt graph
	    if ( pt_graph_it_ != pt_graph_it_end_ )
		return pt_it_ == v.pt_it_;

	    // else, on poi
	    return poi_it_ == v.poi_it_;
	}
	
	EdgeIterator::EdgeIterator( const Multimodal::Graph& graph )
	{
	    graph_ = &graph;
	    boost::tie( vi_, vi_end_ ) = vertices( graph );
	    boost::tie( ei_, ei_end_ ) = out_edges( *vi_, *graph_ );
	}

	void EdgeIterator::to_end()
	{
	    BOOST_ASSERT( graph_ != 0 );
	    vi_ = vi_end_;
	    ei_ = ei_end_;
	    BOOST_ASSERT( vi_ == vi_end_ );
	    BOOST_ASSERT( ei_ == ei_end_ );
	}

	Multimodal::Edge& EdgeIterator::dereference() const
	{
	    BOOST_ASSERT( graph_ != 0 );
	    BOOST_ASSERT( ei_ != ei_end_ );
	    return *ei_;
	}

	void EdgeIterator::increment()
	{
	    BOOST_ASSERT( graph_ != 0 );
	    if ( ei_ != ei_end_ )
	    {
		ei_++;
	    }

	    while ( ei_ == ei_end_ && vi_ != vi_end_ )
	    {
		vi_++;
		if ( vi_ != vi_end_ )
		{
		    boost::tie( ei_, ei_end_ ) = out_edges( *vi_, *graph_ );
		}
	    }
	}
	
	bool EdgeIterator::equal( const EdgeIterator& v ) const
	{
	    // not the same road graph
	    if ( graph_ != v.graph_ )
		return false;

	    if ( vi_ != vi_end_ )
		return vi_ == v.vi_ && ei_ == v.ei_;
	    return v.vi_ == v.vi_end_;
	}

	OutEdgeIterator::OutEdgeIterator( const Multimodal::Graph& graph, Multimodal::Vertex source )
	{
	    graph_ = &graph;
	    source_ = source;
	    if ( source.type == Vertex::Road )
	    {
		boost::tie( road_it_, road_it_end_ ) = boost::out_edges( source.road_vertex, graph_->road );
	    }
	    else if ( source.type == Vertex::PublicTransport )
	    {
		boost::tie( pt_it_, pt_it_end_ ) = boost::out_edges( source.pt_vertex, *source.pt_graph );
	    }
	    stop2road_connection_ = 0;
	    road2stop_connection_ = 0;
	    road2poi_connection_ = 0;
	    poi2road_connection_ = 0;
	}

	void OutEdgeIterator::to_end()
	{
	    pt_it_ = pt_it_end_;
	    road_it_ = road_it_end_;
	    stop2road_connection_ = 2;
	    road2stop_connection_ = -1;
	    road2poi_connection_ = -1;
	    poi2road_connection_ = 2;
	}

	Multimodal::Edge& OutEdgeIterator::dereference() const
	{
	    BOOST_ASSERT( graph_ != 0 );
	    Multimodal::Vertex mm_target;
	    edge_.source = source_;

	    if ( source_.type == Vertex::Road )
	    {
		BOOST_ASSERT( road_it_ != road_it_end_ );
		Road::Vertex r_target = boost::target( *road_it_, graph_->road );
		if ( road2stop_connection_ >= 0 && road2stop_connection_ < graph_->road[ *road_it_ ].stops.size() )
		{
		    size_t idx = road2stop_connection_;
		    const PublicTransport::Graph* pt_graph = graph_->road[ *road_it_ ].stops[ idx ]->graph;
		    PublicTransport::Vertex v = graph_->road[ *road_it_ ].stops[ idx ]->vertex;
		    mm_target = Multimodal::Vertex( pt_graph, v );
		}
		else if ( road2poi_connection_ >= 0 && road2poi_connection_ < graph_->road[ *road_it_ ].pois.size() )
		{
		    size_t idx = road2poi_connection_;
		    const POI* poi = graph_->road[ *road_it_ ].pois[ idx ];
		    mm_target = Multimodal::Vertex( poi );
		}
		else
		{
		    mm_target = Multimodal::Vertex( &graph_->road, r_target );
		}
	    }
	    else if ( source_.type == Vertex::PublicTransport )
	    {
		const PublicTransport::Graph* pt_graph = source_.pt_graph;
		PublicTransport::Vertex r_source = source_.pt_vertex;
		if ( stop2road_connection_ == 0 )
		{
		    mm_target = Multimodal::Vertex( &graph_->road, boost::source( (*pt_graph)[ r_source ].road_section, graph_->road ) );
		}
		else if ( stop2road_connection_ == 1 )
		{
		    mm_target = Multimodal::Vertex( &graph_->road, boost::target( (*pt_graph)[ r_source ].road_section, graph_->road ) );
		}
		else
		{
		    PublicTransport::Vertex r_target = boost::target( *pt_it_, *pt_graph );
		    mm_target = Multimodal::Vertex( pt_graph, r_target );
		}
	    }
	    else if ( source_.type == Vertex::Poi )
	    {
		if ( poi2road_connection_ == 0 )
		{
		    mm_target = Multimodal::Vertex( &graph_->road, boost::source( source_.poi->road_section, graph_->road ) );
		}
		else if ( poi2road_connection_ == 1 )
		{
		    mm_target = Multimodal::Vertex( &graph_->road, boost::target( source_.poi->road_section, graph_->road ) );
		}		
	    }
	    edge_.target = mm_target;
	    return edge_;
	}

	void OutEdgeIterator::increment()
	{
	    BOOST_ASSERT( graph_ != 0 );
	    if ( source_.type == Vertex::Road )
	    {
		if ( road2stop_connection_ >= 0 && road2stop_connection_ < graph_->road[*road_it_].stops.size() )
		{
		    road2stop_connection_++;
		}
		else if ( road2poi_connection_ >= 0 && road2poi_connection_ < graph_->road[*road_it_].pois.size() )
		{
		    road2poi_connection_++;
		}
		else
		{
		    road2stop_connection_ = 0;
		    road2poi_connection_ = 0;
		    road_it_++;
		}
	    }
	    else if ( source_.type == Vertex::PublicTransport )
	    {
		if ( stop2road_connection_ < 2 )
		{
		    stop2road_connection_ ++;
		}
		else
		{
		    bool is_at_pt_end = ( pt_it_ == pt_it_end_ );
		    if ( !is_at_pt_end )
		    {
			pt_it_++;
		    }
		}
	    }
	    else if ( source_.type == Vertex::Poi )
	    {
		if ( poi2road_connection_ < 2 )
		{
		    poi2road_connection_ ++;
		}
	    }
	}

	bool OutEdgeIterator::equal( const OutEdgeIterator& v ) const
	{
	    // not the same road graph
	    if ( graph_ != v.graph_ )
		return false;

	    if ( source_ != v.source_ )
		return false;

	    if ( source_.type == Vertex::Road )
	    {
		if ( road_it_ == road_it_end_ )
		    return v.road_it_ == v.road_it_end_;

		return road_it_ == v.road_it_ && road2stop_connection_ == v.road2stop_connection_ && road2poi_connection_ == v.road2poi_connection_;
	    }

	    if ( source_.type == Vertex::PublicTransport )
	    {
		if ( stop2road_connection_ != v.stop2road_connection_ )
		    return false;
		return pt_it_ == v.pt_it_;
	    }

	    // else
	    return poi2road_connection_ == v.poi2road_connection_;
	}

	VertexIndexProperty get( boost::vertex_index_t, const Multimodal::Graph& graph ) { return VertexIndexProperty( graph ); }
	EdgeIndexProperty get( boost::edge_index_t, const Multimodal::Graph& graph ) { return EdgeIndexProperty( graph ); }

	size_t get( const VertexIndexProperty& p, const Multimodal::Vertex& v ) { return p.get_index( v ); }
	size_t get( const EdgeIndexProperty& p, const Multimodal::Edge& e ) { return p.get_index( e ); }

	// FIXME : slow ?
	size_t VertexIndexProperty::get_index( const Vertex& v ) const
	{
	    if ( v.type == Vertex::Road )
	    {
		return boost::get( boost::get( boost::vertex_index, *v.road_graph ), v.road_vertex );
	    }
	    // else
	    size_t n = num_vertices( graph_.road );
	    Multimodal::Graph::PublicTransportGraphList::const_iterator it;
	    for ( it = graph_.public_transports.begin(); it != graph_.public_transports.end(); it++ )	    
	    {
		if ( &it->second != v.pt_graph )
		{
		    n += num_vertices( it->second );
		}
		else
		{
		    n += boost::get( boost::get( boost::vertex_index, *v.pt_graph ), v.pt_vertex );
		    break;
		}
	    }

	    if ( v.type == Vertex::Poi )
	    {
		for ( Graph::PoiList::const_iterator it = graph_.pois.begin(); it != graph_.pois.end(); it++ )
		{
		    n++;
		    if ( &it->second == v.poi )
		    {
			break;
		    }
		}
	    }
	    return n;
	}

	size_t num_vertices( const Graph& graph )
	{
	    size_t n = 0;
	    Multimodal::Graph::PublicTransportGraphList::const_iterator it;
	    for ( it = graph.public_transports.begin(); it != graph.public_transports.end(); it++ )
	    {
		n += num_vertices( it->second );
	    }
	    return n + num_vertices( graph.road ) + graph.pois.size();
	}
	size_t num_edges( const Graph& graph )
	{
	    size_t n = 0;
	    Multimodal::Graph::PublicTransportGraphList::const_iterator it;
	    for ( it = graph.public_transports.begin(); it != graph.public_transports.end(); it++ )
	    {
		n += num_edges( it->second );
		n += num_vertices( it->second ) * 4;
	    }
	    return n + num_edges( graph.road ) * 2 + graph.pois.size() * 4;
	}
	Vertex& source( Edge& e, const Graph& graph )
	{
	    return e.source;
	}
	Vertex& target( Edge& e, const Graph& graph )
	{
	    return e.target;
	}
	pair<VertexIterator, VertexIterator> vertices( const Graph& graph )
	{
	    VertexIterator v_begin( graph ), v_end( graph );
	    v_end.to_end();
	    return std::make_pair( v_begin, v_end );
	}
	pair<EdgeIterator, EdgeIterator> edges( const Graph& graph )
	{
	    EdgeIterator v_begin( graph ), v_end( graph );
	    v_end.to_end();
	    return std::make_pair( v_begin, v_end );
	}
	
	pair<OutEdgeIterator, OutEdgeIterator> out_edges( const Vertex& v, const Graph& graph )
	{
	    OutEdgeIterator v_begin( graph, v ), v_end( graph, v );
	    v_end.to_end();
	    return std::make_pair( v_begin, v_end );
	}

	size_t out_degree( Vertex& v, const Graph& graph )
	{
	    if ( v.type == Vertex::Road )
	    {
		size_t sum = 0;
		Road::OutEdgeIterator ei, ei_end;
		for ( boost::tie(ei, ei_end) = out_edges( v.road_vertex, graph.road ); ei != ei_end; ei++ )
		{
		    sum += graph.road[ *ei ].stops.size() + graph.road[ *ei ].pois.size() + 1;
		}
		return sum;
	    }
	    else if ( v.type == Vertex::PublicTransport )
	    {
		return out_degree( v.pt_vertex, *v.pt_graph ) + 2;
	    }
	    // else (Poi)
	    return 2;
	}

	std::pair< Edge, bool> edge( const Vertex& u, const Vertex& v, const Graph& graph )
	{
	    Multimodal::Edge e;
	    Multimodal::EdgeIterator ei, ei_end;
	    bool found = false;
	    for ( boost::tie( ei, ei_end ) = edges( graph ); ei != ei_end; ei++ )
	    {
		if ( source( *ei, graph ) == u && target( *ei, graph ) == v )
		{
		    found = true;
		    e = *ei;
		}
	    }
	    return std::make_pair( e, found );
	}
    };
	
    ostream& operator<<( ostream& out, const Multimodal::Vertex& v )
    {
	if ( v.type == Multimodal::Vertex::Road )
	{
	    out << "R" << (*v.road_graph)[v.road_vertex].db_id;
	}
	else if ( v.type == Multimodal::Vertex::PublicTransport )
	{
	    out << "PT" << (*v.pt_graph)[v.pt_vertex].db_id;
	}
	else if ( v.type == Multimodal::Vertex::Poi )
	{
	    out << "POI" << v.poi->db_id;
	}
	return out;
    }

    ostream& operator<<( ostream& out, const Multimodal::Edge& e )
    {
	switch (e.connection_type())
	{
	case Multimodal::Edge::Road2Road:
	    out << "Road2Road ";
	    break;
	case Multimodal::Edge::Road2Transport:
	    out << "Road2Transport ";
	    break;
	case Multimodal::Edge::Transport2Road:
	    out << "Transport2Road ";
	    break;
	case Multimodal::Edge::Transport2Transport:
	    out << "Transport2Transport ";
	    break;
	case Multimodal::Edge::Road2Poi:
	    out << "Road2Poi ";
	    break;
	case Multimodal::Edge::Poi2Road:
	    out << "Poi2Road ";
	    break;
	}
	out << "(" << e.source << "," << e.target << ")";
	return out;
    }
};