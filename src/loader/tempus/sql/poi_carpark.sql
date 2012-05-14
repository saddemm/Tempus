/*

POIs for car park

POI import with type 1 (Car Park)

Following fields are required :

pname : poi name
gid : id (generated by shp2pgsql import)
parking_transport_typo for parkings
geom
*/

insert into
    tempus.poi (
            id
            , poi_type
            , pname
            , parking_transport_type
            , road_section_id
            , abscissa_road_section
            , geom
        )
select
    gid as id
    , 1::integer as poi_type
    , pname as pname
    , parking_transport_type as parking_transport_type
    , st_line_locate_point(geom_road, geom)::double precision as abscissa_road_section
    , geom
from (
    select
        poi.*
        , poi.geom as geom
        , first_value(rs.id) over nearest as road_section_id
        , first_value(rs.geom) over nearest as geom_road
        , row_number() over nearest as nth
     from
        _tempus_import.poi as poi
     join
        tempus.road_section as rs
     on
		-- only consider road sections within xx meters
		-- poi further than this distance will not be included
        st_dwithin(poi.geom, rs.geom, 30)
    window
        -- nearest row geometry for each poi
        nearest as (partition by poi.gid order by st_distance(poi.geom, rs.geom)) as poi_ratt
where
    -- only take one rattachment
    nth = 1
    ;
    
