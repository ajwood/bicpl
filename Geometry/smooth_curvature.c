#include  <mni.h>
#include  <priority_queue.h>

private  void  get_vertex_distances(
    polygons_struct   *polygons,
    int               poly,
    int               vertex,
    Real              distance,
    Real              distances[] );
private  int  get_smoothing_points(
    polygons_struct   *polygons,
    Real              smoothing_distance,
    Real              distances[],
    Point             *smoothing_points[] );
private  Real  get_average_curvature(
    Point   *point,
    int     n_smoothing_points,
    Point   smoothing_points[] );

public  Real  get_smooth_surface_curvature(
    polygons_struct   *polygons,
    int               poly,
    int               vertex,
    Real              smoothing_distance )
{
    int      n_smoothing_points;
    Point    *smoothing_points;
    Real     curvature, *distances;

    ALLOC( distances, polygons->n_points );

    get_vertex_distances( polygons, poly, vertex, smoothing_distance,
                          distances );

    n_smoothing_points = get_smoothing_points( polygons, smoothing_distance,
                                               distances, &smoothing_points );

    if( n_smoothing_points > 0 )
    {
        curvature = get_average_curvature(
              &polygons->points[polygons->indices[
                     POINT_INDEX( polygons->end_indices, poly, vertex )]],
              n_smoothing_points, smoothing_points );
    }
    else
        curvature = 0.0;

    if( n_smoothing_points > 0 )
        FREE( smoothing_points );

    FREE( distances );

    return( curvature );
}

typedef  struct
{
    int   index_within_poly;
    int   poly_index;
} queue_struct;

#define   GREATER_THAN_DISTANCE     -1.0
#define   ALREADY_DONE              -2.0

private  void  get_vertex_distances(
    polygons_struct   *polygons,
    int               poly,
    int               vertex,
    Real              distance,
    Real              distances[] )
{
    int                    i, p, size, point_index, next_point_index;
    Real                   dist;
    queue_struct           entry;
    int                    n_polys, *polys;
    PRIORITY_QUEUE_STRUCT( queue_struct )   queue;

    for_less( i, 0, polygons->n_points )
        distances[i] = GREATER_THAN_DISTANCE;

    point_index = polygons->indices[
                     POINT_INDEX( polygons->end_indices, poly, vertex )];

    distances[point_index] = 0.0;

    ALLOC( polys, polygons->n_items );

    INITIALIZE_PRIORITY_QUEUE( queue );

    entry.poly_index = poly;
    entry.index_within_poly = vertex;
    INSERT_IN_PRIORITY_QUEUE( queue, entry, 0.0 );

    while( !IS_PRIORITY_QUEUE_EMPTY( queue ) )
    {
        REMOVE_FROM_PRIORITY_QUEUE( queue, entry, dist );

        point_index = polygons->indices[
                     POINT_INDEX( polygons->end_indices, entry.poly_index,
                                  entry.index_within_poly )];

        if( distances[point_index] > distance )
            break;

        n_polys = get_polygons_around_vertex( polygons, entry.poly_index,
                                              entry.index_within_poly, polys,
                                              polygons->n_items );

        for_less( i, 0, n_polys )
        {
            size = GET_OBJECT_SIZE( *polygons, polys[i] );

            for_less( p, 0, size )
            {
                next_point_index = polygons->indices[
                             POINT_INDEX( polygons->end_indices, polys[i], p )];

                if( distances[next_point_index] == GREATER_THAN_DISTANCE ||
                    distances[next_point_index] > distances[point_index] )
                {
                    dist = distances[point_index] +
                           distance_between_points(
                                    &polygons->points[point_index],
                                    &polygons->points[next_point_index] );

                    if( distances[next_point_index] == GREATER_THAN_DISTANCE &&
                        dist < distance ||
                        dist < distances[next_point_index] )
                    {
                        distances[next_point_index] = dist;
                        entry.index_within_poly = p;
                        entry.poly_index = polys[i];
                        INSERT_IN_PRIORITY_QUEUE( queue, entry, -dist );
                    }
                }
            }
        }
    }

    DELETE_PRIORITY_QUEUE( queue );
    FREE( polys );
}

private  int  get_smoothing_points(
    polygons_struct   *polygons,
    Real              smoothing_distance,
    Real              distances[],
    Point             *smoothing_points[] )
{
    int    poly, vertex, i, p, point_index, next_point_index;
    int    n_polys, *polys, n_smoothing_points, size;
    Real   dist, ratio;
    Point  point;

    ALLOC( polys, polygons->n_items );
    n_smoothing_points = 0;

    for_less( poly, 0, polygons->n_items )
    {
        size = GET_OBJECT_SIZE( *polygons, poly );

        for_less( vertex, 0, size )
        {
            point_index = polygons->indices[
                     POINT_INDEX( polygons->end_indices, poly, vertex )];

            if( distances[point_index] >= 0.0 )
            {
                n_polys = get_polygons_around_vertex( polygons, poly,
                                           vertex, polys, polygons->n_items );

                for_less( i, 0, n_polys )
                {
                    size = GET_OBJECT_SIZE( *polygons, polys[i] );

                    for_less( p, 0, size )
                    {
                        next_point_index = polygons->indices[
                             POINT_INDEX( polygons->end_indices, polys[i], p )];

                        if( distances[next_point_index] ==
                                      GREATER_THAN_DISTANCE )
                        {
                            dist = distances[point_index] +
                                distance_between_points(
                                      &polygons->points[point_index],
                                      &polygons->points[next_point_index] );

                            if( dist != distances[point_index] )
                            {
                                ratio = (smoothing_distance -
                                         distances[point_index]) /
                                        (dist - distances[point_index]);
                                INTERPOLATE_POINTS( point,
                                         polygons->points[point_index],
                                         polygons->points[next_point_index],
                                         ratio );
                                ADD_ELEMENT_TO_ARRAY( *smoothing_points,
                                      n_smoothing_points, point,
                                      DEFAULT_CHUNK_SIZE );
                            }
                        }
                    }
                }

                distances[point_index] = ALREADY_DONE;
            }
        }
    }

    FREE( polys );

    return( n_smoothing_points );
}

private  Real  get_average_curvature(
    Point   *point,
    int     n_smoothing_points,
    Point   smoothing_points[] )
{
    int      i;
    Real     curvature, angle;
    Point    centroid;

    get_points_centroid( n_smoothing_points, smoothing_points, &centroid );

    curvature = 0.0;
    for_less( i, 0, n_smoothing_points )
    {
        angle = get_angle_between_points( &smoothing_points[i], point,
                                          &centroid );

        curvature += angle * RAD_TO_DEG - 180.0;
    }

    return( curvature / (Real) n_smoothing_points );
}