#pragma once
namespace sql {
namespace GeoJson {

constexpr auto all_lines = R"(
SELECT ID, "Number", "Color", "Name", "Network", "Operator", AsGeoJSON(Line) AS 'GeoJson'
	FROM "Rail lines";
)";

constexpr auto lines_from_tiles = R"(
SELECT ID, "Number", "Color", "Name", "Network", "Operator", AsGeoJSON(Line) AS 'GeoJson'
	FROM "Rail lines"
	WHERE ID IN ({});
)";

constexpr auto segments_from_tiles = R"(
SELECT ID, "Line number", "Usage", "Disusage", "Max speed", "Voltage", AsGeoJSON(Line) AS 'GeoJson'
	FROM Segments
	WHERE ID IN ({});
)";

constexpr auto main_stations_from_tiles = R"(
SELECT "ID", "Name", "Type", AsGeoJSON(Point) AS 'GeoJson'
	FROM "Rail stations"
	WHERE "Type" == 1 
		AND ID IN ({});
)";

constexpr auto stations_from_tiles = R"(
SELECT "ID", "Name", "Type", AsGeoJSON(Point) AS 'GeoJson'
	FROM "Rail stations"
	WHERE ID IN ({});
)";

constexpr auto get_segment = R"(
SELECT "ID", "Line number", "Usage", "Disusage", "Max speed", "Voltage", AsGeoJSON(Line) AS 'GeoJson', AsGeoJSON(Boundry) AS 'Boundry'
	FROM "Segments"
	WHERE ID = ?;
)";

constexpr auto get_rail_line = R"(
SELECT "ID", "Number", "Color", "Name", "Network", "Operator", AsGeoJSON(Line) AS 'GeoJson', AsGeoJSON(Boundry) AS 'Boundry'
	FROM "Rail lines"
	WHERE ID = ?;
)";

constexpr auto get_station = R"(
SELECT "ID", "Name", "Type", AsGeoJSON(Point) AS 'GeoJson'
	FROM "Rail stations"
	WHERE ID = ?;
)";
}}