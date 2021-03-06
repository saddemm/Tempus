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

#include <iostream>
#include <iomanip>
#include <boost/format.hpp>

#include "wps_service.hh"
#include "multimodal_graph.hh"
#include "request.hh"
#include "roadmap.hh"
#include "db.hh"
#include "utils/graph_db_link.hh"
#include <boost/timer/timer.hpp>
#include "plugin_factory.hh"
#include "tempus_services.hh"

using namespace Tempus;

//
// This file contains implementations of services offered by the Tempus WPS server.
// Variables are their XML schema are defined inside constructors.
// And the execute() method read from the XML tree or format the resulting XML tree.
// Pretty boring ...
//

namespace WPS {
///
/// "plugin_list" service, lists loaded plugins.
///
/// Output var: plugins, list of plugin names
///
PluginListService::PluginListService() : Service( "plugin_list" ) {
    add_output_parameter( "plugins" );
}

Service::ParameterMap PluginListService::execute( const ParameterMap& /*input_parameter_map*/ ) const
{
    ParameterMap output_parameters;

    xmlNode* root_node = XML::new_node( "plugins" );
    std::vector<std::string> names( PluginFactory::instance()->plugin_list() );

    for ( size_t i=0; i<names.size(); i++ ) {
        xmlNode* node = XML::new_node( "plugin" );
        XML::new_prop( node,
                       "name",
                       names[i] );

        Plugin::OptionDescriptionList options = PluginFactory::instance()->option_descriptions( names[i] );
        Plugin::OptionDescriptionList::const_iterator it;

        for ( it = options.begin(); it != options.end(); it++ ) {
            xmlNode* option_node = XML::new_node( "option" );
            XML::new_prop( option_node, "name", it->first );
            XML::new_prop( option_node, "description", it->second.description );
            xmlNode* default_value_node = XML::new_node( "default_value" );
            xmlNode* value_node = 0;

            switch ( it->second.type() ) {
            case BoolVariant:
                value_node = XML::new_node( "bool_value" );
                break;
            case IntVariant:
                value_node = XML::new_node( "int_value" );
                break;
            case FloatVariant:
                value_node = XML::new_node( "float_value" );
                break;
            case StringVariant:
                value_node = XML::new_node( "string_value" );
                break;
            default:
                throw std::invalid_argument( "Plugin " + names[i] + ": unknown type for option " + it->first );
            }

            XML::new_prop( value_node, "value", it->second.default_value.str() );
            XML::add_child( default_value_node, value_node );
            XML::add_child( option_node, default_value_node );

            XML::add_child( node, option_node );
        }

        Plugin::Capabilities params = PluginFactory::instance()->plugin_capabilities( names[i] );
        for ( std::vector<CostId>::const_iterator cit = params.optimization_criteria().begin();
              cit != params.optimization_criteria().end();
              cit ++ ) {
            xmlNode* param_node = XML::new_node( "supported_criterion" );
            XML::add_child( param_node, XML::new_text( boost::lexical_cast<std::string>(static_cast<int>(*cit)) ) );

            XML::add_child( node, param_node );
        }
        {
            xmlNode *support_node = XML::new_node("intermediate_steps");
            XML::add_child( support_node, XML::new_text( params.intermediate_steps() ? "true" : "false" ) );
            XML::add_child( node, support_node );
        }
        {
            xmlNode *support_node = XML::new_node("depart_after");
            XML::add_child( support_node, XML::new_text( params.depart_after() ? "true" : "false" ) );
            XML::add_child( node, support_node );
        }
        {
            xmlNode *support_node = XML::new_node("arrive_before");
            XML::add_child( support_node, XML::new_text( params.arrive_before() ? "true" : "false" ) );
            XML::add_child( node, support_node );
        }

        XML::add_child( root_node, node );
    }

    output_parameters[ "plugins" ] = root_node;
    return output_parameters;
}

///
/// "constant_list" service, outputs list of constants contained in the database (road type, transport type, transport networks).
///
/// Output var: transport_modes.
/// Output var: transport_networks.
///
ConstantListService::ConstantListService() : Service( "constant_list" ) {
    add_input_parameter( "plugin" );
    add_output_parameter( "transport_modes" );
    add_output_parameter( "transport_networks" );
    add_output_parameter( "metadata" );
}

Service::ParameterMap ConstantListService::execute( const ParameterMap& input_parameter_map ) const
{
    ParameterMap output_parameters;

    Service::check_parameters( input_parameter_map, input_parameter_schema_ );
    const xmlNode* plugin_node = input_parameter_map.find( "plugin" )->second;
    const std::string plugin_str = XML::get_prop( plugin_node, "name" );
    Plugin* plugin = PluginFactory::instance()->plugin( plugin_str );

    if ( plugin == nullptr ) {
        throw std::invalid_argument( "Cannot find plugin " + plugin_str );
    }

    const RoutingData* rd = plugin->routing_data();

    {
        xmlNode* root_node = XML::new_node( "transport_modes" );
        Tempus::Multimodal::Graph::TransportModes::const_iterator it;

        for ( it = rd->transport_modes().begin(); it != rd->transport_modes().end(); it++ ) {
            xmlNode* node = XML::new_node( "transport_mode" );
            XML::new_prop( node, "id", it->first );
            XML::new_prop( node, "name", it->second.name() );
            XML::new_prop( node, "is_public_transport", it->second.is_public_transport() );
            XML::new_prop( node, "need_parking", it->second.need_parking() );
            XML::new_prop( node, "is_shared", it->second.is_shared() );
            XML::new_prop( node, "must_be_returned", it->second.must_be_returned() );
            XML::new_prop( node, "traffic_rules", it->second.traffic_rules() );
            XML::new_prop( node, "speed_rule", it->second.speed_rule() );
            XML::new_prop( node, "toll_rules", it->second.toll_rules() );
            XML::new_prop( node, "engine_type", it->second.engine_type() );
            XML::add_child( root_node, node );
        }

        output_parameters[ "transport_modes" ] = root_node;
    }

    {
        xmlNode* root_node = XML::new_node( "transport_networks" );
        const RoutingData::NetworkMap& nm = rd->network_map();
        for ( auto it = nm.begin(); it != nm.end(); it++ ) {
            xmlNode* node = XML::new_node( "transport_network" );
            XML::new_prop( node, "id", it->first );
            XML::new_prop( node, "name", it->second.name() );
            XML::add_child( root_node, node );
        }

        output_parameters[ "transport_networks" ] = root_node;
    }

    {
        xmlNode* root_node = XML::new_node( "metadata" );
        for ( auto kv : rd->metadata() ) {
            xmlNode* node = XML::new_node( "m" );
            XML::new_prop( node, "key", kv.first );
            XML::new_prop( node, "value", kv.second );
            XML::add_child( root_node, node );
        }

        output_parameters[ "metadata" ] = root_node;
    }

    return output_parameters;
}

Tempus::db_id_t road_vertex_id_from_coordinates( Db::Connection& db, double x, double y )
{
    //
    // Call to the stored procedure
    //
    std::string q = ( boost::format( "SELECT tempus.road_node_id_from_coordinates(%.3f, %.3f)" ) % x % y ).str();
    Db::Result res = db.exec( q );

    if ( (res.size() == 0) || (res[0][0].is_null()) ) {
        return 0;
    }

    return res[0][0].as<Tempus::db_id_t>();
}

Tempus::db_id_t road_vertex_id_from_coordinates_and_modes( Db::Connection& db, double x, double y, const std::vector<db_id_t>& modes )
{
    std::string array_modes = "array[";
    for ( size_t i = 0; i < modes.size(); i++ ) {
        array_modes += (boost::format("%d") % modes[i]).str();
        if (i<modes.size()-1) {
            array_modes += ",";
        }
    }
    array_modes += "]";
    std::string q = ( boost::format( "SELECT tempus.road_node_id_from_coordinates_and_modes(%.3f, %.3f, %s)" ) % x % y % array_modes ).str();
    Db::Result res = db.exec( q );

    if ( (res.size() == 0) || (res[0][0].is_null()) ) {
        return 0;
    }

    return res[0][0].as<Tempus::db_id_t>();
}

void parse_constraint( const xmlNode* node, Tempus::Request::TimeConstraint& constraint )
{
    constraint.type( lexical_cast<int>( XML::get_prop( node, "type" ) ) );

    std::string date_time = XML::get_prop( node, "date_time" );
    const char* date_time_str = date_time.c_str();
    int day, month, year, hour, min;
    sscanf( date_time_str, "%04d-%02d-%02dT%02d:%02d", &year, &month, &day, &hour, &min );
    constraint.set_date_time( boost::posix_time::ptime( boost::gregorian::date( year, month, day ),
                                                        boost::posix_time::hours( hour ) + boost::posix_time::minutes( min ) ) );
}

db_id_t get_vertex_id_from_point( const xmlNode* node, Db::Connection& db )
{
    Tempus::db_id_t id;
    double x, y;

    bool has_vertex = XML::has_prop( node, "vertex" );
    bool has_x = XML::has_prop( node, "x" );
    bool has_y = XML::has_prop( node, "y" );

    if ( ! ( ( has_x & has_y ) ^ has_vertex ) ) {
        throw std::invalid_argument( "Node " + XML::to_string( node ) + " must have either x and y or vertex" );
    }

    // if "vertex" attribute is present, use it
    if ( has_vertex ) {
        std::string v_str = XML::get_prop( node, "vertex" );
        id = lexical_cast<Tempus::db_id_t>( v_str );
    }
    else {
        // should have "x" and "y" attributes
        std::string x_str = XML::get_prop( node, "x" );
        std::string y_str = XML::get_prop( node, "y" );

        x = lexical_cast<double>( x_str );
        y = lexical_cast<double>( y_str );

        id = road_vertex_id_from_coordinates( db, x, y );

        if ( id == 0 ) {
            throw std::invalid_argument( ( boost::format( "Cannot find vertex id for %.3f, %.3f" ) % x % y ).str() );
        }
    }

    return id;
}

db_id_t get_vertex_id_from_point_and_modes( const xmlNode* node, Db::Connection& db, const std::vector<db_id_t> modes )
{
    Tempus::db_id_t id;
    double x, y;

    bool has_vertex = XML::has_prop( node, "vertex" );
    bool has_x = XML::has_prop( node, "x" );
    bool has_y = XML::has_prop( node, "y" );

    if ( ( has_x && has_y ) == has_vertex ) {
        throw std::invalid_argument( "Node " + XML::to_string( node ) + " must have either x and y or vertex" );
    }

    // if "vertex" attribute is present, use it
    if ( has_vertex ) {
        std::string v_str = XML::get_prop( node, "vertex" );
        id = lexical_cast<Tempus::db_id_t>( v_str );
    }
    else {
        // should have "x" and "y" attributes
        std::string x_str = XML::get_prop( node, "x" );
        std::string y_str = XML::get_prop( node, "y" );

        x = lexical_cast<double>( x_str );
        y = lexical_cast<double>( y_str );

        id = road_vertex_id_from_coordinates_and_modes( db, x, y, modes );

        if ( id == 0 ) {
            throw std::invalid_argument( ( boost::format( "Cannot find vertex id for %.3f, %.3f" ) % x % y ).str() );
        }
    }
    return id;
}

db_id_t get_vertex_id_from_point_and_mode( const xmlNode* node, Db::Connection& db, db_id_t mode )
{
    std::vector<db_id_t> modes;
    modes.push_back( mode );
    return get_vertex_id_from_point_and_modes( node, db, modes );
}

///
/// "result" service, get results from a path query.
///
/// Output var: results, see roadmap.hh
///
SelectService::SelectService() : Service( "select" ) {
    add_input_parameter( "plugin" );
    add_input_parameter( "request" );
    add_input_parameter( "options" );
    add_output_parameter( "results" );
    add_output_parameter( "metrics" );
}

Service::ParameterMap SelectService::execute( const ParameterMap& input_parameter_map ) const
{
    ParameterMap output_parameters;
    /*
     */
    // Ensure XML is OK
    Service::check_parameters( input_parameter_map, input_parameter_schema_ );
    const xmlNode* plugin_node = input_parameter_map.find( "plugin" )->second;
    const std::string plugin_str = XML::get_prop( plugin_node, "name" );
    Plugin* plugin = PluginFactory::instance()->plugin( plugin_str );

    if ( plugin == nullptr ) {
        throw std::invalid_argument( "Cannot find plugin " + plugin_str );
    }

    Tempus::Request request;

    // get options
    VariantMap options;
    {
        const xmlNode* options_node = input_parameter_map.find( "options" )->second;
        const xmlNode* field = XML::get_next_nontext( options_node->children );

        while ( field && !xmlStrcmp( field->name, ( const xmlChar* )"option" ) ) {
            std::string name = XML::get_prop( field, "name" );

            const xmlNode* value_node = XML::get_next_nontext( field->children );
            Tempus::VariantType t = Tempus::IntVariant;

            if ( !xmlStrcmp( value_node->name, ( const xmlChar* )"bool_value" ) ) {
                t = Tempus::BoolVariant;
            }
            else if ( !xmlStrcmp( value_node->name, ( const xmlChar* )"int_value" ) ) {
                t = Tempus::IntVariant;
            }
            else if ( !xmlStrcmp( value_node->name, ( const xmlChar* )"float_value" ) ) {
                t = Tempus::FloatVariant;
            }
            else if ( !xmlStrcmp( value_node->name, ( const xmlChar* )"string_value" ) ) {
                t = Tempus::StringVariant;
            }

            const std::string value = XML::get_prop( value_node, "value" );

                
            options[name] = Variant::from_string( value, t );

            field = XML::get_next_nontext( field->next );
        }
    }
    std::unique_ptr<PluginRequest> plugin_request( plugin->request(options) );
    std::unique_ptr<Result> result;

    // pre_process
    {
        Db::Connection db( plugin->db_options() );

        // now extract actual data
        xmlNode* request_node = input_parameter_map.find( "request" )->second;

        const xmlNode* field = XML::get_next_nontext( request_node->children );

        // first, parse allowed modes
        {
            const xmlNode* sfield = field;
            // skip until the first allowed mode
            while ( sfield && xmlStrcmp( sfield->name, (const xmlChar*)"allowed_mode") ) {
                sfield = XML::get_next_nontext( sfield->next );                    
            }
            // loop over allowed modes
            while ( sfield && !xmlStrcmp( sfield->name, ( const xmlChar* )"allowed_mode" ) ) {
                db_id_t mode = lexical_cast<db_id_t>( sfield->children->content );
                request.add_allowed_mode( mode );
                sfield = XML::get_next_nontext( sfield->next );
            }
        }

        bool has_walking = std::find( request.allowed_modes().begin(), request.allowed_modes().end(), TransportModeWalking ) != request.allowed_modes().end();
        bool has_private_bike = std::find( request.allowed_modes().begin(), request.allowed_modes().end(), TransportModePrivateBicycle ) != request.allowed_modes().end();
        bool has_private_car = std::find( request.allowed_modes().begin(), request.allowed_modes().end(), TransportModePrivateCar ) != request.allowed_modes().end();

        // add walking if private car && private bike
        if ( !has_walking && has_private_car && has_private_bike ) {
            request.add_allowed_mode( TransportModeWalking );
        }
        // add walking if no other private mode is present
        if ( !has_walking && !has_private_car && !has_private_bike ) {
            request.add_allowed_mode( TransportModeWalking );
        }

        Request::Step origin;
        origin.set_location( get_vertex_id_from_point_and_mode( field, db, request.allowed_modes()[0] ) );
            
        request.set_origin( origin );

        // parking location id, optional
        const xmlNode* n = XML::get_next_nontext( field->next );

        if ( !xmlStrcmp( n->name, ( const xmlChar* )"parking_location" ) ) {
            request.set_parking_location( get_vertex_id_from_point( n, db ) );
            field = n;
        }

        // optimizing criteria
        field = XML::get_next_nontext( field->next );
        request.set_optimizing_criterion( 0, lexical_cast<int>( field->children->content ) );
        field = XML::get_next_nontext( field->next );

        while ( !xmlStrcmp( field->name, ( const xmlChar* )"optimizing_criterion" ) ) {
            request.add_criterion( static_cast<CostId>(lexical_cast<int>( field->children->content )) );
            field = XML::get_next_nontext( field->next );
        }

        // steps, 1 .. N
        while ( field && !xmlStrcmp( field->name, (const xmlChar*)"step" ) ) {
            Request::Step step;
            const xmlNode* subfield;
            // destination id
            subfield = XML::get_next_nontext( field->children );
            step.set_location( get_vertex_id_from_point_and_mode( subfield, db, request.allowed_modes()[0] ) );

            // constraint
            subfield = XML::get_next_nontext( subfield->next );
            Request::TimeConstraint constraint;
            parse_constraint( subfield, constraint );
            step.set_constraint( constraint );

            // private_vehicule_at_destination
            std::string val = XML::get_prop( field, "private_vehicule_at_destination" );
            step.set_private_vehicule_at_destination( val == "true" );

            // next step
            field = XML::get_next_nontext( field->next );
            if ( field && !xmlStrcmp( field->name, (const xmlChar*)"step" ) ) {
                request.add_intermediary_step( step );
            }
            else {
                // destination
                request.set_destination( step );
            }
        }

        // then call process
        result.reset( plugin_request->process( request ).release() );
    }

    // metrics
    xmlNode* metrics_node = XML::new_node( "metrics" );

    for ( auto metric : plugin_request->metrics() ) {
        xmlNode* metric_node = XML::new_node( "metric" );
        XML::new_prop( metric_node, "name", metric.first );
        XML::new_prop( metric_node, "value",
                       plugin_request->metric_to_string( metric.first ) );

        XML::add_child( metrics_node, metric_node );
    }

    output_parameters[ "metrics" ] = metrics_node;

    // result
    xmlNode* root_node = XML::new_node( "results" );

    if ( result->size() == 0 ) {
        output_parameters["results"] = root_node;
        return output_parameters;
    }

    Tempus::Result::const_iterator rit;

    const RoutingData* rd = plugin->routing_data();

    for ( rit = result->begin(); rit != result->end(); ++rit ) {
        const Tempus::Roadmap& roadmap = *rit;

        xmlNode* result_node = XML::new_node( "result" );

        for ( Roadmap::StepConstIterator sit = roadmap.begin(); sit != roadmap.end(); sit++ ) {
            xmlNode* step_node = 0;
            const Roadmap::Step* gstep = &*sit;

            if ( sit->step_type() == Roadmap::Step::RoadStep ) {
                const Roadmap::RoadStep* step = static_cast<const Roadmap::RoadStep*>( &*sit );
                step_node = XML::new_node( "road_step" );

                XML::set_prop( step_node, "road", step->road_name() );
                XML::set_prop( step_node, "end_movement", to_string( step->end_movement() ) );
            }
            else if ( sit->step_type() == Roadmap::Step::PublicTransportStep ) {
                const Roadmap::PublicTransportStep* step = static_cast<const Roadmap::PublicTransportStep*>( &*sit );

                if ( ! rd->network( step->network_id() ) ) {
                    throw std::runtime_error( ( boost::format( "Can't find PT network ID %1%" ) % step->network_id() ).str() );
                }

                step_node = XML::new_node( "public_transport_step" );

                const PublicTransport::Network& network = rd->network( step->network_id() ).get();

                XML::set_prop( step_node, "network", network.name() );

                XML::set_prop( step_node, "departure_stop", step->departure_name() );
                XML::set_prop( step_node, "arrival_stop", step->arrival_name() );
                XML::set_prop( step_node, "route", step->route() );
                XML::set_prop( step_node, "trip_id", to_string(step->trip_id()) );
                XML::set_prop( step_node, "departure_time", to_string(step->departure_time()) );
                XML::set_prop( step_node, "arrival_time", to_string(step->arrival_time()) );
                XML::set_prop( step_node, "wait_time", to_string(step->wait()) );
            }
            else if ( sit->step_type() == Roadmap::Step::TransferStep ) {

                const Roadmap::TransferStep* step = static_cast<const Roadmap::TransferStep*>( &*sit );
                if ( step->source().type() == MMVertex::Road && step->target().type() == MMVertex::Transport ) {
                    if ( ! rd->network( step->target().network_id().get() ) ) {
                        throw std::runtime_error( ( boost::format( "Can't find PT network ID %1%" ) % step->target().network_id().get() ).str() );
                    }
                    step_node = XML::new_node( "road_transport_step" );
                    XML::set_prop( step_node, "type", "2" );
                    XML::set_prop( step_node, "road", step->initial_name() );
                    XML::set_prop( step_node, "network", rd->network( step->target().network_id().get() )->name() );
                    XML::set_prop( step_node, "stop", step->final_name() );
                }
                else if ( step->source().type() == MMVertex::Transport && step->target().type() == MMVertex::Road ) {
                    if ( ! rd->network( step->source().network_id().get() ) ) {
                        throw std::runtime_error( ( boost::format( "Can't find PT network ID %1%" ) % step->source().network_id().get() ).str() );
                    }
                    step_node = XML::new_node( "road_transport_step" );
                    XML::set_prop( step_node, "type", "3" );
                    XML::set_prop( step_node, "road", step->final_name() );
                    XML::set_prop( step_node, "network", rd->network( step->source().network_id().get() )->name() );
                    XML::set_prop( step_node, "stop", step->initial_name() );
                }
                else if ( step->source().type() == MMVertex::Road && step->target().type() == MMVertex::Poi ) {
                    step_node = XML::new_node( "transfer_step" );
                    XML::set_prop( step_node, "type", "5" );
                    XML::set_prop( step_node, "road", step->initial_name() );
                    XML::set_prop( step_node, "poi", step->final_name() );
                    XML::set_prop( step_node, "final_mode", to_string( step->final_mode() ) );
                }
                else if ( step->source().type() == MMVertex::Poi && step->target().type() == MMVertex::Road ) {
                    step_node = XML::new_node( "transfer_step" );
                    XML::set_prop( step_node, "type", "6" );
                    XML::set_prop( step_node, "road", step->final_name() );
                    XML::set_prop( step_node, "poi", step->initial_name() );
                    XML::set_prop( step_node, "final_mode", to_string( step->final_mode() ) );
                }
                else if ( step->source().type() == MMVertex::Road && step->target().type() == MMVertex::Road ) {
                    step_node = XML::new_node( "transfer_step" );
                    XML::set_prop( step_node, "type", "1" );
                    XML::set_prop( step_node, "road", step->final_name() );
                    XML::set_prop( step_node, "poi", "0" );
                    XML::set_prop( step_node, "final_mode", to_string( step->final_mode() ) );
                }
            }

            BOOST_ASSERT( step_node );

            // transport_mode
            XML::new_prop( step_node, "transport_mode", to_string(sit->transport_mode()) );

            for ( Tempus::Costs::const_iterator cit = gstep->costs().begin(); cit != gstep->costs().end(); cit++ ) {
                xmlNode* cost_node = XML::new_node( "cost" );
                XML::new_prop( cost_node,
                               "type",
                               to_string( cit->first ) );
                XML::new_prop( cost_node,
                               "value",
                               to_string( cit->second ) );
                XML::add_child( step_node, cost_node );
            }

            XML::set_prop( step_node, "wkb", sit->geometry_wkb() );

            XML::add_child( result_node, step_node );
        }

        // total costs

        Costs total_costs( get_total_costs(roadmap) );
        for ( Tempus::Costs::const_iterator cit = total_costs.begin(); cit != total_costs.end(); cit++ ) {
            xmlNode* cost_node = XML::new_node( "cost" );
            XML::new_prop( cost_node,
                           "type",
                           to_string( cit->first ) );
            XML::new_prop( cost_node,
                           "value",
                           to_string( cit->second ) );
            XML::add_child( result_node, cost_node );
        }

        {
            xmlNode* starting_dt_node = XML::new_node( "starting_date_time" );
            std::string dt_string = boost::posix_time::to_iso_extended_string( roadmap.starting_date_time() );
            XML::add_child( starting_dt_node, XML::new_text( dt_string ) );
            XML::add_child( result_node, starting_dt_node );
        }

        // path trace
        if ( roadmap.trace().size() ) {
            xmlNode * trace_node = XML::new_node( "trace" );

            for ( size_t i = 0; i < roadmap.trace().size(); i++ ) {
                xmlNode *edge_node = XML::new_node("edge");
                const ValuedEdge& ve = roadmap.trace()[i];

                XML::set_prop( edge_node, "wkb", ve.geometry_wkb() );

                MMVertex orig = ve.source();
                MMVertex dest = ve.target();

                xmlNode *orig_node = 0;
                if ( orig.type() == MMVertex::Road ) {
                    orig_node = XML::new_node("road");
                    XML::set_prop(orig_node, "id", to_string(orig.id()));
                }
                else if ( orig.type() == MMVertex::Transport ) {
                    orig_node = XML::new_node("pt");
                    XML::set_prop(orig_node, "id", to_string(orig.id()));
                }
                else if ( orig.type() == MMVertex::Poi ) {
                    orig_node = XML::new_node("poi");
                    XML::set_prop(orig_node, "id", to_string(orig.id()));
                }
                if (orig_node) {
                    XML::add_child(edge_node, orig_node);
                }

                xmlNode *dest_node = 0;
                if ( dest.type() == MMVertex::Road ) {
                    dest_node = XML::new_node("road");
                    XML::set_prop(dest_node, "id", to_string(dest.id()));
                }
                else if ( dest.type() == MMVertex::Transport ) {
                    dest_node = XML::new_node("pt");
                    XML::set_prop(dest_node, "id", to_string(dest.id()));
                }
                else if ( dest.type() == MMVertex::Poi ) {
                    dest_node = XML::new_node("poi");
                    XML::set_prop(dest_node, "id", to_string(dest.id()));
                }
                if (dest_node) {
                    XML::add_child(edge_node, dest_node);
                }

                VariantMap::const_iterator vit;
                for ( vit = ve.values().begin(); vit != ve.values().end(); ++vit ) {
                    xmlNode *n = 0;
                    if (vit->second.type() == BoolVariant) {
                        n = XML::new_node("b");
                    }
                    else if (vit->second.type() == IntVariant) {
                        n = XML::new_node("i");
                    }
                    else if (vit->second.type() == FloatVariant) {
                        n = XML::new_node("f");
                    }
                    else if (vit->second.type() == StringVariant) {
                        n = XML::new_node("s");
                    }
                    if (n ) {
                        XML::set_prop(n, "k", vit->first);
                        XML::set_prop(n, "v", vit->second.str());
                        XML::add_child(edge_node, n);
                    }
                }
                XML::add_child(trace_node, edge_node);
            }
            XML::add_child(result_node, trace_node);
        }

        XML::add_child( root_node, result_node );
    } // for each result

    output_parameters[ "results" ] = root_node;

#ifdef TIMING_ENABLED
    timer.stop();
    std::cout << timer.elapsed().wall*1.e-9 << " in select " << plugin->name()<< "\n";
#endif
#undef TIMING
    return output_parameters;
}

} // WPS namespace
