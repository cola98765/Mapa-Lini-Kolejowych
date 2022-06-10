#include "Database.h"
#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/ranges.h>
#include <fstream>
#include <iostream>

#include "GeoJSON_conversion.h"
#include "Data Types/Database_Types.hpp"

Database::Database(const std::filesystem::path& database_path)
{
	loadFromFile(database_path);
}

void Database::showInfo()
{
	auto show_amount = [&](std::string_view table)
	{
		auto amount = database.execAndGet(fmt::format(R"(SELECT COUNT(*) FROM "{}";)", table)).getInt64();
		fmt::print("\t {}: {}\n", table, amount);
	};

	fmt::print("\t Timestamp: {}\n", timestamp);
	show_amount(Railway::sql_table_name);
	show_amount(Railway_line::sql_table_name);
	show_amount(Railway_station::sql_table_name);

	fmt::print("\t Min Lat: {:.2f}, Min Lon: {:.2f}\n", minlat, minlon);
	fmt::print("\t Max Lat: {:.2f}, Max Lon: {:.2f}\n", maxlat, maxlon);
}

bool Database::importFromString(std::string_view json_string)
{
	nlohmann::json j;
	j = j.parse(json_string.data());
	auto kb = json_string.size() / 1024;
	auto mb = kb / 1024;
	kb %= 1024;
	fmt::print("Imported string with json data. Size: {}MB {}KB\n", mb, kb);
	return importData(j);
}

bool Database::importFromFile(const std::filesystem::path& json_file)
{
	std::ifstream file(json_file);
	nlohmann::json j;
	file >> j;

	auto file_size = std::filesystem::file_size(json_file);
	auto kb = file_size / 1024;
	auto mb = kb / 1024;
	kb %= 1024;
	fmt::print("Imported {} file. Size: {}MB {}KB\n", json_file.string(), mb, kb);
	return importData(j);
}

void Database::loadFromFile(const std::filesystem::path& database_path)
{
	database = SQLite::Database(database_path.string(), SQLite::OPEN_READONLY);
	try
	{
		database.loadExtension("mod_spatialite.dll", nullptr);
	}
	catch (SQLite::Exception& e)
	{
		fmt::print("{}\n", e.what());
	}

	SQLite::Statement infos(database, fmt::format("SELECT * FROM {}", Variable::sql_table_name));
	while (infos.executeStep())
	{
		std::string_view key = infos.getColumn(0).getText();
		const auto& value = infos.getColumn(1);

		if (key == "timestamp")
			timestamp = value.getText();
		else if (key == "minlon")
			minlon = value.getDouble();
		else if (key == "minlat")
			minlat = value.getDouble();
		else if (key == "maxlon")
			maxlon = value.getDouble();
		else if (key == "maxlat")
			maxlat = value.getDouble();
	}
	showInfo();
}

void Database::saveToFile(const std::filesystem::path& database_path)
{
	database.backup(database_path.string().c_str(), SQLite::Database::BackupType::Save);
}

const std::string& Database::find(std::string_view query, std::string_view type, unsigned limit)
{
	static std::string buffer{};
	auto find_query = Railway_station::sql_search;
	auto location_function = GeoJSON::getRailStation;

	if (type == "rail_line")
	{
		find_query = Railway_line::sql_search;
		location_function = GeoJSON::getRailLine;
	}
	else if (type != "rail_station")
	{
		throw "Wrong type";
	}

	nlohmann::json j;
	auto search = SQLite::Statement(database, find_query.data());
	search.bind(1, fmt::format("%{}%", query));
	search.bind(2, limit);

	while (search.executeStep())
	{
		nlohmann::json found;
		found["id"] = search.getColumn("ID").getText();
		found["name"] = search.getColumn("Name").getText();
		j.push_back(found);
	}

	buffer = j.dump(1);
	return buffer;
}

const std::string& Database::getGeoJSON(std::string_view ID, std::string_view type)
{
	static std::string buffer{};

	if (type == "segment")
	{
		buffer = GeoJSON::getSegmentWithBounds(database, ID).dump(1);
	}
	else if (type == "rail_line")
	{
		buffer = GeoJSON::getRailLineWithBounds(database, ID).dump(1);
	}
	else if (type == "rail_station")
	{
		buffer = GeoJSON::getRailStationWithPoint(database, ID).dump(1);
	}
	else
	{
		throw std::logic_error("Wrong type");
	}
	return buffer;
}

const std::string& Database::getGeoJSON(BoundingBox bounds, int zoom)
{
	static std::string buffer{};
	auto features_collection = GeoJSON::createFeatureCollection();

	//Only lines - cache
	if (zoom < 10)
	{
		return GeoJSON::allRailLines(database);
	}
	//Lines + main rail stations
	else if (zoom == 10)
	{
		GeoJSON::boundingRailLines(database, features_collection, bounds);
		GeoJSON::boundingSegments(database, features_collection, bounds);
		GeoJSON::boundingMainRailStations(database, features_collection, bounds);
	}
	//Lines + rail stations
	else if (zoom >= 11)
	{
		GeoJSON::boundingRailLines(database, features_collection, bounds);
		GeoJSON::boundingSegments(database, features_collection, bounds);
		GeoJSON::boundingRailStations(database, features_collection, bounds);
	}

	fmt::print("Elements: {}\n", features_collection["features"].size());
	buffer = features_collection.dump(1);
	return buffer;
}

void Database::calcMinMaxBoundry(double _minlon, double _minlat, double _maxlon, double _maxlat)
{
	minlon = std::min(minlon, _minlon);
	minlat = std::min(minlat, _minlat);
	maxlon = std::max(maxlon, _maxlon);
	maxlat = std::max(maxlat, _maxlat);
}

bool Database::importData(const nlohmann::json& json_data)
{
	database = SQLite::Database(":memory:", SQLite::OPEN_CREATE | SQLite::OPEN_READWRITE);
	database.loadExtension("mod_spatialite.dll", nullptr);
	SQLite::Transaction transaction(database);

	try
	{
		database.exec(sql_init_spatialite.data());
		database.exec(Railnode::sql_create.data());
		database.exec(Railway::sql_create.data());
		database.exec(Railway_line::sql_create.data());
		database.exec(Railway_station::sql_create.data());
		database.exec(Variable::sql_create.data());
	}
	catch (const SQLite::Exception& e)
	{
		catchSQLiteException(e, "recreating tables");
		return false;
	}

	std::string type{};
	type.reserve(10);
	int invalid = 0;

	for (const auto& element : json_data["elements"])
	{
		bool r = false;
		element["type"].get_to(type);
		if (type == "way")
		{
			if (!importData_Railway(element))
				++invalid;
			else if (element.contains("bounds"))
			{
				const auto& bounds = element["bounds"];
				calcMinMaxBoundry(bounds["minlon"], bounds["minlat"],
					bounds["maxlon"], bounds["maxlat"]);
			}
		}
		else if (type == "relation")
		{
			if (!importData_RailwayLine(element))
				++invalid;
		}
		else if (type == "node")
		{
			if (!importData_RailwayStation(element))
				++invalid;
			else
			{
				const auto lat = element["lat"].get<double>();
				const auto lon = element["lon"].get<double>();
				calcMinMaxBoundry(lon, lat, lon, lat);
			}
		}
	}

	try
	{
		auto insert_statement = SQLite::Statement{ database, Variable::sql_insert.data() };
		auto insert_info = [&insert_statement](std::string_view key, auto value)
		{
			insert_statement.reset();
			insert_statement.bind(1, key.data());
			insert_statement.bind(2, value);
			insert_statement.exec();
		};
		timestamp = json_data["osm3s"]["timestamp_osm_base"].get<std::string>();
		insert_info("timestamp", timestamp);
		insert_info("minlon", minlon);
		insert_info("minlat", minlat);
		insert_info("maxlon", maxlon);
		insert_info("maxlat", maxlat);
	}
	catch (const SQLite::Exception& e)
	{
		catchSQLiteException(e, "inserting information");
		return false;
	}

	//splitIntoTiles();
	transaction.commit();

	fmt::print("Database created: \n");
	if (invalid)
		fmt::print("\t Invalid data: {}\n", invalid);
	showInfo();
	return true;
}

bool Database::importData_Railway(const nlohmann::json& json_data)
{
	try
	{
		auto insert_statement = SQLite::Statement{ database, Railway::sql_insert.data() };

		static auto id = std::string{};
		static auto boundry = std::string{};
		static auto line = std::string{};
		static auto disusage = std::string{};

		id = json_data["id"].dump();
		insert_statement.bind(":id", id);

		const auto& tags = json_data["tags"];
		if (!bindTag(insert_statement, ":line_name", getTag(tags, "ref")))
		{
			insert_statement.reset();
			fmt::print(fmt::fg(fmt::color::orange_red), "Invalid railway ({}): Line number is empty \n", id);
			return false;
		}
		bindTag(insert_statement, ":usage", getTag(tags, "usage"));
		bindTag(insert_statement, ":max_speed", getTag(tags, "maxspeed"));

		//TODO Repair railway tags
		/*if (tags.contains("disused:railway") || tags["railway"] == "disused")
			disusage = "disused";
		else if (tags.contains("abandoned:railway"))
			disusage = "abandoned";
		bindTag(insert_statement, ":disusage", disusage.size() ? &disusage : nullptr);
		disusage.clear();

		if (tags.contains("electrified"))
		{
			bindTag(insert_statement, ":disusage", disusage.size() ? &disusage : nullptr);
		}*/

		const auto& bounds = json_data["bounds"];
		boundry = fmt::format("POLYGON(({0} {1}, {2} {1}, {2} {3}, {0} {3}))",
			bounds["minlon"].dump(), bounds["minlat"].dump(),
			bounds["maxlon"].dump(), bounds["maxlat"].dump());

		insert_statement.bind(":boundry", boundry);

		line = "LINESTRING(";
		for (const auto& point : json_data["geometry"])
		{
			line += fmt::format("{} {}, ", point["lon"].dump(), point["lat"].dump());
		}
		line.pop_back();
		line.pop_back();
		line += ")";
		insert_statement.bind(":line", line);

		insert_statement.exec();
		insert_statement.reset();
		return true;
	}
	catch (const SQLite::Exception& e)
	{
		catchSQLiteException(e, "creating railway");
		return false;
	}
}

bool Database::importData_RailwayLine(const nlohmann::json& json_data)
{
	try
	{
		auto insert_statement = SQLite::Statement{ database, Railway_line::sql_insert.data()};

		static auto id = std::string{};
		static auto color = std::string{};
		static auto boundry = std::string{};
		static auto line = std::string{};

		id = json_data["id"].dump();
		insert_statement.bind(":id", id);

		auto id_hash = std::hash<std::string>{}(id);
		color = fmt::format("{:x}", id_hash % 200 + 55);
		id_hash /= 255;
		color += fmt::format("{:x}", id_hash % 200 + 55);
		id_hash /= 255;
		color += fmt::format("{:x}", id_hash % 200 + 55);
		insert_statement.bind(":color", color);

		const auto& tags = json_data["tags"];
		
		if (!bindTag(insert_statement, ":number", getTag(tags, "ref")))
		{
			insert_statement.reset();
			fmt::print(fmt::fg(fmt::color::orange_red), "Invalid railway line ({}): Line number is empty \n", id);
			return false;
		}
		bindTag(insert_statement, ":name", getTag(tags, "name"));
		bindTag(insert_statement, ":network", getTag(tags, "network"));
		bindTag(insert_statement, ":operator", getTag(tags, "operator"));

		//TODO: Railway line geometric line
		/*static auto link = SQLite::Statement{ database, sql::rail_line_segment_insert };
		for (const auto& segment : json_data["members"])
		{
			if (segment["type"] != "way")
				continue;
			if (segment["role"] != "")
				continue;
			link.bind(1, id);
			link.bind(2, segment["ref"].dump());

			link.exec();
			link.reset();
		}

		line = "MULTILINESTRING (";
		static auto line_statement = SQLite::Statement{ database, sql::query::get_rail_line_line_strings };
		line_statement.bind(1, id);
		while (line_statement.executeStep())
		{
			auto segment = std::string_view{ line_statement.getColumn(0).getText() };
			segment.remove_prefix(10);
			line += fmt::format("{}, ", segment);
		}
		line_statement.reset();
		if (line.size() < 20)
		{
			insert_statement.reset();
			line_statement.reset();
			fmt::print(fmt::fg(fmt::color::orange_red), "Invalid rail line ({}): Line has no segments \n", id);
			return false;
		}

		line.pop_back();
		line.pop_back();
		line += ")";
		insert_statement.bind(":line", line);

		auto boundry_statement = fmt::format("SELECT AsText(Envelope(GeomFromText(\'{}\', 4326)));", line);
		insert_statement.bind(":boundry", database.execAndGet(boundry_statement).getText());*/

		insert_statement.exec();
		insert_statement.reset();
		return true;
	}
	catch (const SQLite::Exception& e)
	{
		catchSQLiteException(e, "creating railway line");
		return false;
	}
}

bool Database::importData_RailwayStation(const nlohmann::json& json_data)
{
	try
	{
		static auto insert_statement = SQLite::Statement{ database, Railway_station::sql_insert.data() };
		static auto id = std::string{};
		static auto point = std::string{};


		id = json_data["id"].dump();
		insert_statement.bind(":id", id);

		const auto& tags = json_data["tags"];

		if(!bindTag(insert_statement, ":name", getTag(tags, "name")))
		{
			insert_statement.reset();
			fmt::print(fmt::fg(fmt::color::orange_red), "Invalid railway station ({}): Name is empty \n", id);
			return false;
		}

		point = fmt::format("POINT({} {})",
			json_data["lon"].dump(), json_data["lat"].dump());
		insert_statement.bind(":point", point);

		unsigned type = 0;
		if (tags.contains("railway"))
		{
			if (tags["railway"] == "station")
				type = 1;
			else if (tags["railway"] == "halt")
				type = 2;
		}
		else if (tags.contains("disused:railway"))
		{
			if (tags["disused:railway"] == "station")
				type = 3;
			else if (tags["disused:railway"] == "halt")
				type = 4;
		}

		if (!type)
		{
			insert_statement.reset();
			fmt::print(fmt::fg(fmt::color::orange_red), "Invalid railway station ({}): Type is empty \n", id);
			return false;
		}

		insert_statement.bind(":type", type);
		insert_statement.exec();
		insert_statement.reset();
		return true;
	}
	catch (const SQLite::Exception& e)
	{
		catchSQLiteException(e, "creating railway station");
		return false;
	}
}

void Database::splitIntoTiles()
{
	auto distance_lon = std::abs(static_cast<int>(maxlon) - static_cast<int>(minlon));
	auto distance_lat = std::abs(static_cast<int>(maxlat) - static_cast<int>(minlat));

	auto tiles_lon = static_cast<int>(distance_lon) + 1;
	auto tiles_lat = static_cast<int>(distance_lat) + 1;
	auto tiles_number = tiles_lat * tiles_lon;

	auto tiles_for_type = [&](std::string_view input_table)
	{
		std::vector<std::string> table_names;
		table_names.reserve(tiles_number);
		for (int i = 0; i < tiles_number; ++i)
		{
			auto table_name = fmt::format("tile_{}_{}", input_table, i);
			database.exec(fmt::format(R"(DROP TABLE IF EXISTS "{0}"; CREATE TABLE "{0}" ("ID" TEXT PRIMARY KEY);)",
				table_name));
			table_names.push_back(fmt::format(R"(INSERT INTO "{}" VALUES ({{}});)", table_name));
		}
		return table_names;
	};
	std::vector<unsigned> buffer;

	auto split = [&](std::string_view type) {
		auto tables = tiles_for_type(type);
		std::vector<std::pair<double, double>> coords;

		std::string_view statement{};
		if (type == "Rail stations")
			statement = "SELECT ID, asGeoJSON(Point) FROM \"{}\";";
		else
			statement = "SELECT ID, asGeoJSON(Boundry) FROM \"{}\";";

		SQLite::Statement all_id(database, fmt::format(fmt::runtime(statement), type));
		while (all_id.executeStep())
		{
			coords.clear();
			auto id = all_id.getColumn(0).getText();
			auto geojson = nlohmann::json::parse(all_id.getColumn(1).getText());

			if (type == "Rail stations")
			{
				std::pair<double, double> coord = geojson["coordinates"];
				getOccupiedTiles(buffer, coord.first, coord.second, coord.first, coord.second);
			}
			else
			{
				coords = geojson["coordinates"][0];
				getOccupiedTiles(buffer, coords[0].first, coords[0].second, coords[2].first, coords[2].second);
			}

			for (const auto& tile : buffer)
			{
				database.exec(fmt::format(fmt::runtime(tables[tile]), id));
			}
		}
	};
	split(Railway::sql_table_name);
	split(Railway_line::sql_table_name);
	split(Railway_station::sql_table_name);
}

std::vector<unsigned>& Database::getOccupiedTiles(std::vector<unsigned>& buffer, double _minlon, double _minlat, double _maxlon, double _maxlat)
{
	buffer.clear();
	auto tiles_lon = std::abs(static_cast<int>(maxlon) - static_cast<int>(minlon)) + 1;

	_minlon = std::max(_minlon, minlon);
	_minlat = std::max(_minlat, minlat);
	_maxlon = std::min(_maxlon, maxlon);
	_maxlat = std::min(_maxlat, maxlat);

	auto start_lon = std::abs(static_cast<int>(_minlon) - static_cast<int>(minlon));
	auto start_lat = std::abs(static_cast<int>(_minlat) - static_cast<int>(minlat));
	auto end_lon = std::abs(static_cast<int>(_maxlon) - static_cast<int>(minlon));
	auto end_lat = std::abs(static_cast<int>(_maxlat) - static_cast<int>(minlat));

	for (auto y = start_lat; y <= end_lat; ++y)
	{
		for (auto x = start_lon; x <= end_lon; ++x)
		{
			auto i = y * tiles_lon + x;
			buffer.push_back(i);
		}
	}

	return buffer;
}

void catchSQLiteException(const SQLite::Exception& e, std::string_view when, std::string_view dump)
{
	fmt::print(fmt::fg(fmt::color::red), "ERROR: SQL Exception when {}: {}\n", when, e.what());
	if (dump.size())
		fmt::print("Dumping potential exception cause: \n {} \n", dump);
}

const std::string* const Database::getTag(const nlohmann::json& tags, const std::string& tag)
{
	if (tags.contains(tag))
	{
		return &(tags[tag].get_ref<const std::string&>());
	}
	return nullptr;
}
