namespace sql {
auto input_tabels = R"(
DROP TABLE IF EXISTS "Nodes";
DROP TABLE IF EXISTS "Ways";
DROP TABLE IF EXISTS "Relations";

DROP TABLE IF EXISTS "Relations-Ways";
DROP TABLE IF EXISTS "Relations-Nodes";
DROP TABLE IF EXISTS "Ways-Nodes";

CREATE TABLE "Nodes" (
	ID INTEGER PRIMARY KEY,
	Point TEXT NOT NULL,
	Tags TEXT
);

CREATE TABLE "Ways" (
	ID INTEGER PRIMARY KEY,
	Boundry TEXT NOT NULL,
	Points TEXT NOT NULL,
	Tags TEXT
);

CREATE TABLE "Relations" (
	ID INTEGER PRIMARY KEY,
	Boundry TEXT NOT NULL,
	Tags TEXT
);

CREATE TABLE "Relations - Ways" (
	Relation_ID INTEGER,
	Way_ID INTEGER
);

CREATE TABLE "Relations - Nodes" (
	Relation_ID INTEGER,
	Node_ID INTEGER
);
)";

auto node_insert = R"(
	INSERT INTO "Nodes" VALUES (:id, :point, :tags);
)";

auto way_insert = R"(
	INSERT INTO "Ways" VALUES (:id, :bounds, :points, :tags);
)";

auto relation_insert = R"(
	INSERT INTO "Relations" VALUES (:id, :bounds, :tags);
)";

auto relation_way_insert = R"(
	INSERT INTO "Relations - Ways" VALUES (?, ?);
)";
}