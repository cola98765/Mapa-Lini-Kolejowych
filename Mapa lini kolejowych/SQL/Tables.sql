namespace sql {
auto tables = R"(
SELECT InitSpatialMetaData();

DROP TABLE IF EXISTS "Segments";
DROP TABLE IF EXISTS "Rail lines";
DROP TABLE IF EXISTS "Rail lines segments";

CREATE TABLE "Segments" (
	"ID" INTEGER PRIMARY KEY,
	"Usage" TEXT,
	"Line number" TEXT,
	"Max speed" INTEGER,
	"Electrified" INTEGER
);
SELECT AddGeometryColumn('Segments', 'Boundry', 4326, 'POLYGON', 2, 1);
SELECT AddGeometryColumn('Segments', 'Line', 4326, 'LINESTRING', 2, 1);

CREATE TABLE "Rail lines" (
	"ID" INTEGER PRIMARY KEY,
	"Number" TEXT,
	"Name" TEXT,
	"Network" TEXT,
	"Operator" TEXT
);
SELECT AddGeometryColumn('Rail lines', 'Boundry', 4326, 'POLYGON', 2, 1);

CREATE TABLE "Rail lines segments" (
	"Line_ID" INTEGER,
	"Segment_ID" INTEGER
);

CREATE TABLE "Rail stations" (
	"ID" INTEGER PRIMARY KEY,
	"Name" TEXT,
	"Type" INT
);
SELECT AddGeometryColumn('Rail stations', 'Point', 4326, 'POINT', 2, 1);
)";

auto segment_insert = R"(
	INSERT INTO "Segments" ("ID", "Boundry", "Line", "Usage", "Line number", "Max speed", "Electrified")
	VALUES (:id, GeomFromText(:boundry, 4326), GeomFromText(:line, 4326), :usage, :line_number, :max_speed, :electrified);
)";

auto rail_line_insert = R"(
	INSERT INTO "Rail lines" ("ID", "Boundry", "Number", "Name", "Network", "Operator")
	VALUES (:id, GeomFromText(:boundry, 4326), :number, :name, :network, :operator);
)";

auto rail_line_segment_insert = R"(
	INSERT INTO "Rail lines segments" VALUES (?, ?);
)";

auto rail_station_insert = R"(
	INSERT INTO "Rail stations" ("ID", "Point", "Name", "Type")
	VALUES (:id, GeomFromText(:point, 4326), :name, :type);
)";
}