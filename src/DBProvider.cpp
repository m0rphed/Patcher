#include "DBProvider/DBProvider.h"
#include "Shared/TextTable.h"

#include <pqxx/pqxx>
#include <pqxx/transaction>
#include <string>
#include <iostream>

using namespace std;

DBProvider::DBProvider(const std::string args)
{
	_connection = new DBConnection(args);
	_connection->setConnection();
}

DBProvider::~DBProvider()
{
	delete _connection;
}

vector<ObjectData> DBProvider::getObjects() const
{
	// example:
	// output - public, myFunction, function, <param1, param2, param3>
	//          common, myTable,    table,    <>
	std::string sql_getObjects = "SELECT /*sequences */"
		"f.sequence_schema AS obj_schema,"
		"f.sequence_name AS obj_name,"
		"'sequence' AS obj_type "
		"FROM information_schema.sequences f "
		"UNION ALL "
		"SELECT /*tables */ "
		"f.table_schema AS obj_schema,"
		"f.table_name AS obj_name,"
		"'table' AS obj_type "
		"FROM information_schema.tables f "
		"WHERE f.table_schema in"
		"('public', 'io', 'common', 'secure');";;

	auto resOfQuery = query(sql_getObjects);
	std::vector<ObjectData> objects;

	for (pqxx::result::const_iterator row = resOfQuery.begin(); row != resOfQuery.end(); ++row)
	{
		const std::vector<std::string> parameters;
		objects.push_back(ObjectData(	row["obj_name"].as<std::string>(),
										row["obj_type"].as<std::string>(),
										row["obj_schema"].as<std::string>(), parameters));
	}

	return objects;
}

ScriptData DBProvider::getScriptData(const ObjectData &data) const // Temporary only for tables
{
	if (data.type == "table")
	{
		return getTableData(data);
	}
	else if (data.type == "function")
	{
		return getFunctionData(data);
	}
	else if (data.type == "trigger")
	{
		return getTriggerData(data);
	}
	else if (data.type == "index")
	{
		return getIndexData(data);
	}
	else if (data.type == "table")
	{
		return getViewData(data);
	}
	else if (data.type == "view")
	{
		return getViewData(data);
	}
	else if (data.type == "sequence")
	{
		return getSequenceData(data);
	}
}

// Checks if specified object exists in database
 bool DBProvider::doesCurrentObjectExists(const std::string scheme, const std::string name, const std::string type) const
 {
	bool res = false;
	if (type == "table")
	{
		res = tableExists(scheme, name);
	}

	if (type == "sequence")
	{
		res = sequenceExists(scheme, name);
	}

	if (type == "view")
	{
		res = viewExists(scheme, name);
	}

	if (type == "trigger")
	{
		res = triggerExists(scheme, name);
	}

	if (type == "function")
	{
		res = functionExists(name);
	}

	if (type == "index")
	{
		res = indexExists(name);
	}

	return res;
}

pqxx::result DBProvider::query(const std::string stringSQL) const
{	
	// Connection must be already set
	if (!_connection->getConnection())
	{
		throw new std::exception("ERROR: Connection was not set.\n");
	}

	pqxx::work trans(*_connection->getConnection(), "trans");

	// Get result from database
	pqxx::result res = trans.exec(stringSQL);
	trans.commit();
	return res;
}

bool DBProvider::tableExists(const std::string& tableSchema, const std::string& tableName) const
{
	// Define SQL request
	string q =
		ParsingTools::interpolateAll(
			"SELECT EXISTS (SELECT * "
			"FROM information_schema.tables "
			"WHERE table_schema = '${}' "
			"AND table_name = '${}');",
			vector<string> { tableSchema, tableName });

	auto queryResult = query(q);
	return queryResult.begin()["exists"].as<bool>();
}

bool DBProvider::sequenceExists(const std::string& sequenceSchema, const std::string& sequenceName) const
{
	string q =
		ParsingTools::interpolateAll(
			"SELECT EXISTS (SELECT * "
			"FROM information_schema.sequences "
			"WHERE sequence_schema = '${}' "
			"AND sequence_name = '${}');",
			vector<string> { sequenceSchema, sequenceName });

	auto queryResult = query(q);
	return queryResult.begin()["exists"].as<bool>();
}

bool DBProvider::functionExists(const std::string& name)
{
	return true;
}

bool DBProvider::indexExists(const std::string& name)
{
	return true;
}

bool DBProvider::viewExists(const std::string& tableSchema, const std::string& tableName) const
{
	string q =
		ParsingTools::interpolateAll(
			"SELECT EXISTS (SELECT * "
			"FROM information_schema.views "
			"WHERE table_schema = '${}' "
			"AND table_name = '${}');",
			vector<string> { tableSchema, tableName });

	auto queryResult = query(q);
	return queryResult.begin()["exists"].as<bool>();
}

bool DBProvider::triggerExists(const std::string& triggerSchema, const std::string& triggerName) const
{
	const string q =
		ParsingTools::interpolateAll(
			"SELECT EXISTS (SELECT * "
			"FROM information_schema.triggers "
			"WHERE trigger_schema = '${}' "
			"AND trigger_name = '${}');",
			vector<string> { triggerSchema, triggerName });

	auto resOfQuery = query(q);
	return false;
}

Table DBProvider::getTable(const ObjectData & data) const
{
	Table table;

	// Get table type
	string queryString = "SELECT * FROM information_schema.tables t " 
		"WHERE t.table_schema = '" + data.schema + "' AND t.table_name = '" + data.name + "'";
	table.type = getSingleValue(queryString, "table_type");

	// Getting columns information
	queryString = "SELECT * FROM information_schema.columns c JOIN (SELECT a.attname, format_type(a.atttypid, a.atttypmod) "
		"FROM pg_attribute a JOIN pg_class b ON a.attrelid = b.relfilenode "
		"WHERE a.attnum > 0 "
		"AND NOT a.attisdropped "
		"AND b.oid = '" + data.schema + "." + data.name + "'::regclass::oid) i "
		"ON c.column_name = i.attname "
		"JOIN (SELECT t.table_schema, t.table_name, t.column_name, pgd.description "
		"FROM pg_catalog.pg_statio_all_tables as st "
		"INNER JOIN pg_catalog.pg_description pgd on(pgd.objoid = st.relid) "
		"INNER JOIN information_schema.columns t on(pgd.objsubid = t.ordinal_position "
		"AND  t.table_schema = st.schemaname and t.table_name = st.relname) "
		"WHERE t.table_catalog = '" + _connection->info.databaseName + "' "
		"AND t.table_schema = '" + data.schema + "' "
		"AND t.table_name = '" + data.name + "') j "
		"ON c.column_name = j.column_name "
		"WHERE c.table_schema = '" + data.schema + "' AND c.table_name = '" + data.name + "'";
	pqxx::result result = query(queryString); // SQL query result, contains information in table format
	for (pqxx::result::const_iterator row = result.begin(); row != result.end(); ++row)
	{
		Column column;
		column.name = row["column_name"].c_str();
		column.type = row["format_type"].c_str();
		column.defaultValue = row["column_default"].c_str();
		column.description = row["description"].c_str();
		column.setNullable(row["is_nullable"].c_str());

		table.columns.push_back(column);
	}

	//Getting constarints
	queryString = "SELECT "
		"tc.constraint_type, "
		"tc.table_schema, "
		"tc.constraint_name, "
		"tc.table_name, "
		"kcu.column_name, "
		"ccu.table_schema AS foreign_table_schema, "
		"ccu.table_name AS foreign_table_name, "
		"ccu.column_name AS foreign_column_name, "
		"cc.check_clause "
		"FROM "
		"information_schema.table_constraints AS tc "
		"LEFT JOIN information_schema.key_column_usage AS kcu "
		"ON tc.constraint_name = kcu.constraint_name "
		"AND tc.table_schema = kcu.table_schema "
		"left JOIN information_schema.constraint_column_usage AS ccu "
		"ON ccu.constraint_name = tc.constraint_name "
		"AND ccu.table_schema = tc.table_schema "
		"AND tc.constraint_type = 'FOREIGN KEY' "
		"LEFT JOIN information_schema.check_constraints cc "
		"ON cc.constraint_name = tc.constraint_name "
		"WHERE tc.table_name = '" + data.name + "' "
		"AND tc.table_schema = '" + data.schema + "' "
		"AND COALESCE(cc.check_clause, '') NOT ILIKE '%IS NOT NULL%' ";
	result = query(queryString);
	for (pqxx::result::const_iterator row = result.begin(); row != result.end(); ++row)
	{
		Constraint constraint;
		constraint.type = row["constraint_type"].c_str();
		constraint.name = row["constraint_name"].c_str();
		constraint.columnName = row["column_name"].c_str();
		constraint.checkClause = row["check_clause"].c_str();
		constraint.foreignTableSchema = row["foreign_table_schema"].c_str();
		constraint.foreignTableName = row["foreign_table_name"].c_str();
		constraint.foreignColumnName = row["foreign_column_name"].c_str();

		cout << constraint.type << " " << constraint.name << " " << constraint.checkClause << endl;

		table.constraints.push_back(constraint);
	}

	// Getting table owner
	queryString = "SELECT * FROM pg_tables t where schemaname = '" + data.schema + "' and tablename = '" + data.name + "'";
	table.owner = getSingleValue(queryString, "tableowner");

	// Getting table description
	queryString = "SELECT obj_description('" + data.schema + "." + data.name + "'::regclass::oid)";
	table.description = getSingleValue(queryString, "obj_description");

	// Getting triggers
	queryString = "SELECT * FROM information_schema.triggers t WHERE t.trigger_schema = '" 
		+ data.schema + "' and t.event_object_table = '" + data.name + "'";
	result = query(queryString);
	for (pqxx::result::const_iterator row = result.begin(); row != result.end(); ++row)
	{
		// Getting triggers
	}

	return table;
}

string DBProvider::getSingleValue(const string &queryString, const string &columnName) const
{
	pqxx::result result = query(queryString);
	pqxx::result::const_iterator row = result.begin();
	return row[columnName].c_str();
}

ScriptData DBProvider::getTableData(const ObjectData & data) const
{
	Table table = getTable(data); // Getting information about object

	// "CREATE TABLE" block - initialization of all table's columns
	string scriptString = "CREATE ";
	if (table.type != "BASE TABLE")
	{
		scriptString += table.type = " ";
	}
	scriptString += "TABLE " + data.schema + "." + data.name + " (";
	for (const Column &column : table.columns)
	{
		scriptString += "\n" + column.name + " " + column.type;
		if (!column.defaultValue.empty())
		{
			scriptString += " DEFAULT " + column.defaultValue;
		}
		if (!column.isNullable())
		{
			scriptString += " NOT NULL";
		}
		scriptString += ",";
	}

	// Creation of constraints
	for (Constraint constraint : table.constraints)
	{
		scriptString += "\nCONSTRAINT " + constraint.name + " " + constraint.type + " ";
		if (constraint.type == "PRIMARY KEY" || "UNIQUE")
		{
			scriptString += "(" + constraint.columnName + ")";
		}
		else if (constraint.type == "FOREIGN KEY")
		{
			scriptString += "(" + constraint.columnName + ")\n";
			scriptString += "REFERENCES " + constraint.foreignTableSchema + "." + constraint.foreignTableName +
				" (" + constraint.foreignColumnName + ")";
		}
		else if (constraint.type == "CHECK")
		{
			cout << "ASda " << constraint.checkClause << endl;
			scriptString += constraint.checkClause;
		}
		scriptString += ",";
	}

	if (!table.columns.empty())
	{
		scriptString.pop_back(); // Removing an extra comma at the end
	}
	scriptString += "\n);\n\n";

	// "OWNER TO" block to make the owner user
	scriptString += "ALTER TABLE " + data.schema + "." + data.name + " OWNER TO " + table.owner + ";\n\n";

	// "COMMENT ON TABLE" block
	if (!table.description.empty())
	{
		scriptString += "COMMENT ON TABLE " + data.schema + "." + data.name + " IS '" + table.description + "';\n\n";
	}

	// "COMMENT ON COLUMN blocks
	for (const Column &column : table.columns)
	{
		if (!column.description.empty())
		{
			scriptString += "COMMENT ON COLUMN " + data.schema + "." + data.name + "." + column.name + " IS '" + column.description + "';\n\n";
		}
	}

	ScriptData scriptData = ScriptData(data, scriptString);
	scriptData.name += ".sql";
	return scriptData;
}

ScriptData DBProvider::getFunctionData(const ObjectData & data) const
{
	return ScriptData();
}

ScriptData DBProvider::getViewData(const ObjectData & data) const
{
	return ScriptData();
}

ScriptData DBProvider::getSequenceData(const ObjectData & data) const
{
	return ScriptData();
}

ScriptData DBProvider::getTriggerData(const ObjectData & data) const
{
	return ScriptData();
}

ScriptData DBProvider::getIndexData(const ObjectData & data) const
{
	return ScriptData();
}

void printObjectsData(pqxx::result queryResult)
{
	// Iterate over the rows in our result set.
	// Result objects are containers similar to std::vector and such.
	for (	pqxx::result::const_iterator row = queryResult.begin();
			row != queryResult.end();
			++row )
	{
		std::cout
			<< row["obj_schema"].as<std::string>() << "\t"
			<< row["obj_name"].as<std::string>() << "\t"
			<< row["obj_type"].as<std::string>()
			<< std::endl;
	}
}

bool Column::isNullable() const
{
	return this->nullable_;
}

void Column::setNullable(string value)
{
	transform(value.begin(), value.end(), value.begin(), tolower);
	if (value == "yes")
	{
		this->nullable_ = true;
	}
	else
	{
		this->nullable_ = false;
	}
}
