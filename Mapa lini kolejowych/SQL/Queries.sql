namespace sql {
namespace query {
auto point_from_node = R"(
SELECT "Point"
	FROM "Nodes"
	WHERE "ID"=?;
)";

auto segments_in_bound = R"(
SELECT ID, "Usage", "Line number", "Max speed", "Electrified", AsGeoJSON(Line) AS 'GeoJson'
	FROM Segments
	WHERE TRUE <= Intersects(Boundry, GeomFromText(?, 4326));
)";

auto stations_in_bound = R"(
SELECT ID, Name, Type, AsGeoJSON(Point) AS 'GeoJson'
	FROM "Rail stations"
	WHERE TRUE <= Intersects(Point, GeomFromText(?, 4326));
)";
}}