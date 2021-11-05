#include "Server.h"

Server::Server(Database& database)
	:database(database)
{

}

void Server::run()
{
	init_routing();
	app.port(2137).run();
}

void Server::init_routing()
{
	CROW_ROUTE(app, "/bounds/<double>/<double>/<double>/<double>")
		([this](crow::response& res, double min_lon, double min_lat, double max_lon, double max_lat)
			{
				auto start = std::chrono::high_resolution_clock::now();
				res.add_header("Access-Control-Allow-Origin", "*");
				res.add_header("Access-Control-Allow-Headers", "Content-Type");
				res.body = database.getGeoJSON(
fmt::format("POLYGON(({0} {1}, {2} {1}, {2} {3}, {0} {3}))",
					min_lat, min_lon, max_lat, max_lon));
				auto end = std::chrono::high_resolution_clock::now();
				auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
				fmt::print("Request got ready in {} ms\n", duration.count());
				res.end();
			});
}
