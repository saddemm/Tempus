/**
 *   Copyright (C) 2012-2013 IFSTTAR (http://www.ifsttar.fr)
 *   Copyright (C) 2012-2013 Oslandia <infos@oslandia.com>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *   
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/test/unit_test.hpp>
#include "db.hh"
#include "pgsql_importer.hh"
#include "multimodal_graph.hh"

#include <iostream>
#include <string>

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>

std::string g_db_options = getenv("TEMPUS_DB_OPTIONS") ? getenv("TEMPUS_DB_OPTIONS") : "";

using namespace boost::unit_test ;
using namespace Tempus;

#define DB_TEST_NAME "tempus_test_db"

BOOST_AUTO_TEST_SUITE( tempus_core_Db )

BOOST_AUTO_TEST_CASE( testConnection )
{
    COUT << "DbTest::testConnection()" << std::endl;

    std::auto_ptr<Db::Connection> connection;

    // Connection to an non-existing database
    BOOST_CHECK_THROW( connection.reset(new Db::Connection( g_db_options + " dbname=zorglub" )), std::runtime_error);

    // Connection to an existing database
    bool has_thrown = false;
    try
    {
	connection.reset( new Db::Connection( g_db_options + " dbname = " DB_TEST_NAME ) );
    }
    catch ( std::runtime_error& )
    {
	has_thrown = true;
    }
    BOOST_CHECK_MESSAGE( !has_thrown, "Must not throw on an existing database, check that " DB_TEST_NAME " exists" );

    // Do not sigsegv ?
}

BOOST_AUTO_TEST_CASE(testQueries)
{
    COUT << "DbTest::testQueries()" << std::endl;
    std::auto_ptr<Db::Connection> connection( new Db::Connection( g_db_options + " dbname = " DB_TEST_NAME ));

    // test bad query
    BOOST_CHECK_THROW( connection->exec( "SELZECT * PHROM zorglub" ),  std::runtime_error);

    connection->exec( "DROP TABLE IF EXISTS test_table" );
    connection->exec( "CREATE TABLE test_table (id int, int_v int, bigint_v bigint, str_v varchar, time_v time)" );
    connection->exec( "INSERT INTO test_table (id, int_v) VALUES ('1', '42')" );
    connection->exec( "INSERT INTO test_table (id, int_v, bigint_v) VALUES ('2', '-42', '10000000000')" );
    connection->exec( "INSERT INTO test_table (str_v) VALUES ('Hello world')" );
    connection->exec( "INSERT INTO test_table (time_v) VALUES ('13:52:45')" );
    Db::Result res( connection->exec( "SELECT * FROM test_table" ));
    
    BOOST_CHECK_EQUAL( (size_t)4, res.size() );
    BOOST_CHECK_EQUAL( (size_t)5, res.columns() );
    BOOST_CHECK_EQUAL( (int)1, res[0][0].as<int>() );
    BOOST_CHECK_EQUAL( (int)42, res[0][1].as<int>() );
    BOOST_CHECK( res[0][2].is_null() );

    BOOST_CHECK_EQUAL( (int)-42, res[1][1].as<int>());
    BOOST_CHECK_EQUAL( 10000000000ULL, res[1][2].as<unsigned long long>() );

    BOOST_CHECK_EQUAL( std::string("Hello world"), res[2][3].as<std::string>() );

    Tempus::Time t = res[3][4].as<Tempus::Time>();
    BOOST_CHECK_EQUAL( (long)(13 * 3600 + 52 * 60 + 45), t.n_secs );

}
BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(tempus_core_PgImporter)

std::auto_ptr<PQImporter> importer( new PQImporter( g_db_options + " dbname = " DB_TEST_NAME ));

Multimodal::Graph graph;

BOOST_AUTO_TEST_CASE(testConsistency)
{
    COUT << "PgImporterTest::testConsistency()" << std::endl;
    importer->import_constants( graph );
    importer->import_graph( graph );

    // get the number of vertices in the graph
    long n_road_vertices, n_road_edges;
    {
        Db::Result res( importer->query( "SELECT COUNT(*) FROM tempus.road_node" ));
        BOOST_CHECK( res.size() == 1 );
        n_road_vertices = res[0][0].as<long>();
    }
    {
        Db::Result res( importer->query( "SELECT COUNT(*) FROM tempus.road_section" ));
        BOOST_CHECK( res.size() == 1 );
        n_road_edges = res[0][0].as<long>();
    }
    COUT << "n_road_vertices = " << n_road_vertices << " n_road_edges = " << n_road_edges << std::endl;
    BOOST_CHECK( n_road_vertices = boost::num_vertices( graph.road ) );
    BOOST_CHECK( n_road_edges = boost::num_edges( graph.road ) );
    
    // number of PT networks
    {
        Db::Result res( importer->query( "SELECT COUNT(*) FROM tempus.pt_network" ));
        long n_networks = res[0][0].as<long>();
            
        BOOST_CHECK_EQUAL( (size_t)n_networks, graph.public_transports.size() );
        BOOST_CHECK_EQUAL( (size_t)n_networks, graph.network_map.size() );
    }

    Multimodal::Graph::PublicTransportGraphList::iterator it;
    for ( it = graph.public_transports.begin(); it != graph.public_transports.end(); it++ )
    {
	PublicTransport::Graph& pt_graph = it->second;

        long n_pt_vertices, n_pt_edges;
        {
            Db::Result res( importer->query( "SELECT COUNT(*) FROM tempus.pt_stop" ));
	    BOOST_CHECK( res.size() == 1 );
	    n_pt_vertices = res[0][0].as<long>();
        }
	
        {
            Db::Result res( importer->query( "SELECT COUNT(*) FROM tempus.pt_section" ));
            BOOST_CHECK( res.size() == 1 );
            n_pt_edges = res[0][0].as<long>();
        }
	COUT << "n_pt_vertices = " << n_pt_vertices << " num_vertices(pt_graph) = " << num_vertices(pt_graph) << std::endl;
	BOOST_CHECK( n_pt_vertices = boost::num_vertices( pt_graph ) );
	COUT << "n_pt_edges = " << n_pt_edges << " num_edges(pt_graph) = " << num_edges(pt_graph) << std::endl;
	BOOST_CHECK( n_pt_edges = boost::num_edges( pt_graph ) );
    }
}

Multimodal::Vertex vertex_from_road_node_id( db_id_t id, const Multimodal::Graph& lgraph )
{
    Multimodal::VertexIterator vi, vi_end;
    for ( boost::tie( vi, vi_end ) = vertices( lgraph ); vi != vi_end; vi++ )
    {
	if ( vi->type == Multimodal::Vertex::Road && lgraph.road[ vi->road_vertex ].db_id == id )
	{
	    return *vi;
	}
    }
    throw std::runtime_error("bug: should not reach here");
}


BOOST_AUTO_TEST_CASE(testMultimodal)
{
    COUT << "PgImporterTest::testMultimodal()" << std::endl;
    importer->import_constants( graph );
    importer->import_graph( graph );

    size_t nv = 0;
    size_t n_road_vertices = 0;
    size_t n_pt_vertices = 0;
    size_t n_pois = 0;
    {
        Multimodal::VertexIterator vi, vi_end;
        for ( boost::tie(vi, vi_end) = vertices( graph ); vi != vi_end; vi++ )
        {
            nv++;
            if ( vi->type == Multimodal::Vertex::Road )
                n_road_vertices++;
            else if ( vi->type == Multimodal::Vertex::PublicTransport )
                n_pt_vertices++;
            else
                n_pois ++;
        }
    }

    const PublicTransport::Graph& pt_graph = graph.public_transports.begin()->second;
    COUT << "nv = " << nv << std::endl;
    COUT << "n_road_vertices = " << n_road_vertices << " num_vertices(road) = " << num_vertices( graph.road ) << std::endl;
    COUT << "n_pt_vertices = " << n_pt_vertices << " num_vertices(pt) = " << num_vertices( pt_graph ) << std::endl;
    COUT << "n_pois = " << n_pois << " pois.size() = " << graph.pois.size() << std::endl;
    COUT << "num_vertices = " << num_vertices( graph ) << std::endl;
    BOOST_CHECK_EQUAL( nv, num_vertices( graph ) );

    {
        Multimodal::VertexIterator vi, vi_end;
        for ( boost::tie(vi, vi_end) = vertices( graph ); vi != vi_end; vi++ )
        {
            Multimodal::OutEdgeIterator oei, oei_end;
            boost::tie( oei, oei_end ) = out_edges( *vi, graph );
            size_t out_deg = 0;
            for ( ; oei != oei_end; oei++ )
            {
                out_deg++;
            }
            size_t out_deg2 = out_degree( *vi, graph );
            BOOST_CHECK_EQUAL( out_deg, out_deg2 );
        }
    }
    size_t ne = 0;
    size_t n_road2road = 0;
    size_t n_road2transport = 0;
    size_t n_transport2road = 0;
    size_t n_transport2transport = 0;
    size_t n_road2poi = 0;
    size_t n_poi2road = 0;
    
    Road::OutEdgeIterator ri, ri_end;
    Road::Vertex v1 = *(vertices( graph.road ).first);
    boost::tie( ri, ri_end ) = out_edges( v1, graph.road );
    //    BOOST_ASSERT( ri != ri_end );

    {
        Multimodal::EdgeIterator ei, ei_end;
        for ( boost::tie( ei, ei_end ) = edges( graph ); ei != ei_end; ei++ )
        {
            ne++;
            switch ( ei->connection_type() )
            {
            case Multimodal::Edge::Road2Road:
                n_road2road++;
                break;
            case Multimodal::Edge::Road2Transport:
                n_road2transport++;
                break;
            case Multimodal::Edge::Transport2Road:
                n_transport2road++;
                break;
            case Multimodal::Edge::Transport2Transport:
                n_transport2transport++;
                break;
            case Multimodal::Edge::Road2Poi:
                n_road2poi++;
                break;
            case Multimodal::Edge::Poi2Road:
                n_poi2road++;
                break;
            case Multimodal::Edge::UnknownConnection:
                throw std::runtime_error("bug: should not reah here");
            }
        }
    }

    size_t n_stops = 0;
    Road::EdgeIterator pei, pei_end;
    for ( boost::tie( pei, pei_end ) = edges( graph.road ); pei != pei_end; pei++ )
    {
	n_stops += graph.road[ *pei ].stops.size();
    }

    COUT << "ne = " << ne << std::endl;
    COUT << "n_road2road = " << n_road2road << " num_edges(road) = " << num_edges( graph.road ) << std::endl;
    COUT << "n_road2transport = " << n_road2transport << std::endl;
    COUT << "n_transport2road = " << n_transport2road << std::endl;
    COUT << "n_road2poi = " << n_road2poi << std::endl;
    COUT << "n_poi2road = " << n_poi2road << " pois.size = " << graph.pois.size() << std::endl;
    COUT << "n_transport2transport = " << n_transport2transport << " num_edges(pt) = " << num_edges( pt_graph ) << std::endl;
    size_t sum = n_road2road + n_road2transport + n_transport2road + n_transport2transport + n_poi2road + n_road2poi;
    COUT << "sum = " << sum << std::endl;
    COUT << "num_edges = " << num_edges( graph ) << std::endl;
    BOOST_CHECK_EQUAL( sum, num_edges( graph ) );

    // test vertex index
    Multimodal::VertexIndexProperty index = get( boost::vertex_index, graph );
    {
        Multimodal::VertexIterator vi, vi_end;
        for ( boost::tie(vi, vi_end) = vertices( graph ); vi != vi_end; vi++ )
        {
            size_t idx = get( index, *vi );
            if ( vi->type == Multimodal::Vertex::Road )
            {
                BOOST_CHECK( idx < num_vertices( graph.road ) );
            }
        }
    }
 
    // test that graph vertices and edges can be used as a map key
    // i.e. the operator< forms a complete order
    {
	    std::set< Multimodal::Vertex > vertex_set;
            Multimodal::VertexIterator vi, vi_end;
	    for ( boost::tie(vi, vi_end) = vertices( graph ); vi != vi_end; ++vi ) {
		    vertex_set.insert( *vi );
	    }
	    // check whether we have one entry per vertex
	    BOOST_CHECK_EQUAL( vertex_set.size(), num_vertices( graph ));

	    std::set< Multimodal::Edge > edge_set;
            Multimodal::EdgeIterator ei, ei_end;
	    for ( boost::tie(ei, ei_end) = edges( graph ); ei != ei_end; ++ei ) {
		    edge_set.insert( *ei );
	    }
	    // check whether we have one entry per vertex
	    BOOST_CHECK_EQUAL( edge_set.size(), num_edges( graph ));
    }

    // test graph traversal
    {
	std::map<Multimodal::Vertex, boost::default_color_type> colors;
	boost::depth_first_search( graph,
				   boost::dfs_visitor<boost::null_visitor>(),
				   boost::make_assoc_property_map( colors )
				   );
    }

    // test dijkstra
    {
	size_t n = num_vertices( graph );
	std::vector<boost::default_color_type> color_map( n );
	std::vector<Multimodal::Vertex> pred_map( n );
	std::vector<double> distance_map( n );
	std::map< Multimodal::Edge, double > lengths;
	
        {
            Multimodal::EdgeIterator ei, ei_end;
            for ( boost::tie( ei, ei_end ) = edges( graph ); ei != ei_end; ei++ )
            {
                if ( ei->connection_type() == Multimodal::Edge::Road2Road )
                {
                    lengths[ *ei ] = 10.0;
                }
                else
                {
                    lengths[ *ei ] = 1.0;
                }
            }
        }
	Multimodal::Vertex origin, destination;
	origin = vertex_from_road_node_id( 19953, graph );
	destination = vertex_from_road_node_id( 22510, graph );
	
	COUT << "origin = " << origin << std::endl;
	COUT << "destination = " << destination << std::endl;
	
	Multimodal::VertexIndexProperty vertex_index = get( boost::vertex_index, graph );
	COUT << "ok" << std::endl;
	
	boost::dijkstra_shortest_paths( graph,
					origin,
					boost::make_iterator_property_map( pred_map.begin(), vertex_index ),
					boost::make_iterator_property_map( distance_map.begin(), vertex_index ),
					boost::make_assoc_property_map( lengths ),
					vertex_index,
					std::less<double>(),
					boost::closed_plus<double>(),
					std::numeric_limits<double>::max(),
					0.0,
					boost::dijkstra_visitor<boost::null_visitor>(),
					boost::make_iterator_property_map( color_map.begin(), vertex_index )
					);
	COUT << "Dijkstra OK" << std::endl;
    }

    // test public transport sub map
    {
	// 1 // create other public transport networks, if needed
	if ( graph.public_transports.size() < 2 )
	{
	    // get the maximum pt id
	    db_id_t max_id = 0;
	    for ( Multimodal::Graph::PublicTransportGraphList::const_iterator it = graph.public_transports.begin();
		  it != graph.public_transports.end();
		  it++ )
	    {
		if ( it->first > max_id )
		{
		    max_id = it->first;
		}
	    }
	    size_t n_vertices = num_vertices( graph );
	    size_t n_edges = num_edges( graph );

	    // insert a new pt that is a copy of the first
	    graph.public_transports[max_id+1] = graph.public_transports.begin()->second;
	    graph.public_transports.select_all();

	    // unselect the first network
	    std::set<db_id_t> selection = graph.public_transports.selection();
	    selection.erase( graph.public_transports.begin()->first );
	    graph.public_transports.select( selection );

	    size_t n_vertices3 = num_vertices( graph );
	    size_t n_edges3 = num_edges( graph );
	    size_t n_computed_vertices = 0;
	    Multimodal::VertexIterator vi, vi_end;
	    for ( boost::tie( vi, vi_end ) = vertices( graph ); vi != vi_end; vi++ )
	    {
		n_computed_vertices ++;
	    }

	    size_t n_computed_edges = 0;
	    Multimodal::EdgeIterator ei, ei_end;
	    for ( boost::tie( ei, ei_end ) = edges( graph ); ei != ei_end; ei++ )
	    {
		n_computed_edges ++;
	    }

	    // check that number of vertices are still ok
	    BOOST_CHECK_EQUAL( n_vertices, n_vertices3 );
	    // check the use of vertex iterators
	    BOOST_CHECK_EQUAL( n_computed_vertices, n_vertices3 );
	    // check that number of vertices are still ok
	    BOOST_CHECK_EQUAL( n_edges, n_edges3 );
	    // check the use of edges iterators
	    BOOST_CHECK_EQUAL( n_computed_edges, n_edges3 );
	}
    }
}

BOOST_AUTO_TEST_CASE(testRestrictions)
{
    importer->import_constants( graph );
    importer->import_graph( graph );

    // restriction nodes
    db_id_t expected_nodes[][4] = { { 22587, 22510, 22451, 0 },
                                    { 21801, 21652, 21712, 21691 } };
    
    Road::Restrictions restrictions( importer->import_turn_restrictions( graph.road ) );

    Road::Restrictions::RestrictionSequence::const_iterator it;
    int i = 0;
    for ( it = restrictions.restrictions.begin(); it != restrictions.restrictions.end(); ++it, i++ ) {
        Road::Road::VertexSequence seq( it->to_vertex_sequence( graph.road ) );
        int j = 0;
        for ( Road::Road::VertexSequence::const_iterator itt = seq.begin(); itt != seq.end(); ++itt, j++ ) {
            BOOST_CHECK_EQUAL( graph.road[ *itt ].db_id, expected_nodes[i][j] );
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()


