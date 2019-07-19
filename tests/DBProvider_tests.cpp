#include <DBProvider/DBProvider.h>
#include "../include/DBProvider/DBProviderLogger.h"
#include <iostream>
#include <pqxx/pqxx>

bool testStandardConnectionMethod()
{
	using namespace pqxx;

	try
	{
		connection conn("dbname = Doors user = doo password = rc hostaddr = 127.0.0.1 port = 5432");

		if (conn.is_open())
		{
			std::cout << "Opened database successfully: " << conn.dbname() << std::endl;
			std::cout << "Disconnecting from: " << conn.dbname() << std::endl;
			conn.disconnect();
			return true;
		}
		
		std::cout << "Can't open database" << std::endl;
		conn.disconnect();

		return false;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return false;
	}

}

bool testDBProviderLogger()
{
	using namespace DBProviderLogger;
	try
	{
		auto testLog = new DBLog();
		testLog->setPathAndOpen("../build/DBProvider.dir/log.txt");
		testLog->addLog(DEBUG, "test my log");
		testLog->addLog(DEBUG, "test my log one more time");

		startLog("../build/DBProvider.dir/global_log.txt");
		logDebug("test my global log");
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return false;
	}

	return true;
}

bool testCustomConnectionMethod(std::string credentials)
{
	try
	{
		auto *customConn = new DBConnection(credentials);
		customConn->setConnection();

		std::cout << "dbname= " << customConn->info.databaseName << std::endl;
		std::cout << "user= " << customConn->info.username << std::endl;
		std::cout << "password= " << customConn->info.password << std::endl;
		std::cout << "hostaddr= " << customConn->info.host << std::endl;
		std::cout << "port= " << customConn->info.portNumber << std::endl;

		delete customConn;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return false;
	}

	return true;
}

bool testPrintObjectsData(std::string creds)
{
	auto *dbProvider = new DBProvider(creds);
	std::string getObjects =
		"SELECT /*sequences */\
				f.sequence_schema AS obj_schema, f.sequence_name AS obj_name, 'sequence' AS obj_type\
				from information_schema.sequences f\
			UNION ALL\
			SELECT /*tables */\
				f.table_schema AS obj_schema, f.table_name AS obj_name, 'tables' AS obj_type\
				from information_schema.tables f\
			WHERE f.table_schema in('public', 'io', 'common', 'secure')";
	
	auto resOfQuery = dbProvider->query(getObjects);

	printObjectsData(resOfQuery);
	delete dbProvider;
	return true;
}

int main()
{
	std::string creds = "Doors:doo:rc:127.0.0.1:5432";
	bool isStdConnectionWorks = testStandardConnectionMethod();
	bool isLogWorks = testDBProviderLogger();

	std::cout << "\nRUNNING: Test custom connection method" << std::endl;
	
	if (testCustomConnectionMethod(creds))
	{
		std::cout << "SUCCEEDED: Test custom connection method" << std::endl;
	}
	
	std::cout << "\nRUNNING: Test print objects data method" << std::endl;
	if (testPrintObjectsData(creds))
	{
		std::cout << "SUCCEEDED: Test print objects data method" << std::endl;
	}
	
	return 0;
}