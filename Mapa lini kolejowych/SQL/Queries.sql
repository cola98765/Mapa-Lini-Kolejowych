namespace sql {
namespace query {
auto point_from_node = R"(
SELECT "Point"
	FROM "Nodes"
	WHERE "ID"=?;
)";

auto ways_coords = R"(
SELECT n.Latitude, n.Longtitude 
	FROM "Ways" AS 'w'
	INNER JOIN "Ways - Nodes" AS 'wn' ON w.ID = wn.Way_ID
	INNER JOIN "Nodes" AS 'n' ON n.ID = wn.Node_ID; 
)";

auto segments_in_bound = R"(
SELECT ID, AsGeoJSON(Line) AS 'GeoJson'
	FROM Segments
	WHERE TRUE <= Intersects(Boundry, GeomFromText(?, 4326));
)";
}}