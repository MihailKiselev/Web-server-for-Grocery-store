#include <map>
#include <string>
#include <iostream>
#include <istream>

#include <boost/asio.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>


struct Product {
    std::string id;
    std::string name_product;
    std::string cost;
};

std::string readFromSocket(boost::asio::ip::tcp::socket& socket) {
    boost::asio::streambuf buffer;
    boost::asio::read_until(socket, buffer, "\r\n\r\n");

    std::string requestString;
    std::istream requestStream(&buffer);

    std::string line;
    while (std::getline(requestStream, line) && line != "\r") {
        requestString += line + "\n";
    }

    std::stringstream requestBodyStream;
    if (requestStream && line == "\r") {
        requestBodyStream << requestStream.rdbuf();
    }

    return requestString + "\n" + requestBodyStream.str();
}

void writeToSocket(boost::asio::ip::tcp::socket& socket, const std::string& data) {
    boost::asio::write(socket, boost::asio::buffer(data));
}

void handleSell_product(boost::asio::ip::tcp::socket& socket, const std::string& name_product_sell, const std::string& cost_sell) {
    std::cout << "Received sell product request:" << std::endl;
    std::cout << "Product: " << name_product_sell << std::endl;
    std::cout << "Cost: " << cost_sell << std::endl;

    try {
        sql::mysql::MySQL_Driver* driver;
        std::unique_ptr<sql::Connection> con;

        driver = sql::mysql::get_mysql_driver_instance();
        con.reset(driver->connect("tcp://127.0.0.1:3306", "admin", "admin"));

        con->setSchema("magazin");
      
        std::string query = "SELECT * FROM inventory WHERE name_product='" + name_product_sell + "'";
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(query));
        if (res->next()) {
            boost::property_tree::ptree responseJson;
            responseJson.put("status", "error");
            responseJson.put("message", "Product already exists");

            std::ostringstream oss;
            boost::property_tree::write_json(oss, responseJson, false);
            std::string responseBody = oss.str();

            std::string response = "HTTP/1.1 200 OK\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
            response += "Access-Control-Allow-Headers: Content-Type\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
            response += "\r\n";
            response += responseBody;

            writeToSocket(socket, response);

            return;
        }

        stmt.reset(con->createStatement());
        stmt->execute("INSERT INTO inventory (name_product, cost) VALUES ('" + name_product_sell + "', '" + cost_sell + "')");

        boost::property_tree::ptree responseJson;
        responseJson.put("status", "success");
        responseJson.put("message", "Product added successfully");

        std::ostringstream oss;
        boost::property_tree::write_json(oss, responseJson, false);
        std::string responseBody = oss.str();

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
        response += "Access-Control-Allow-Headers: Content-Type\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
        response += "\r\n";
        response += responseBody;

        writeToSocket(socket, response);
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error: " << e.what() << std::endl;
    }
}

void getProducts(boost::asio::ip::tcp::socket& socket) {

    std::vector<Product> products;

    try {
        sql::mysql::MySQL_Driver* driver;
        std::unique_ptr<sql::Connection> con;

        driver = sql::mysql::get_mysql_driver_instance();
        con.reset(driver->connect("tcp://127.0.0.1:3306", "admin", "admin"));

        con->setSchema("magazin");
      
        sql::Statement* stmt = con->createStatement();
        sql::ResultSet* res = stmt->executeQuery("SELECT id, name_product, cost FROM inventory");

        while (res->next()) {
            std::string id = res->getString("id");
            std::string name_product = res->getString("name_product");
            std::string cost = res->getString("cost");
            products.push_back({ id, name_product, cost });
        }

        for (int i = 0; i < products.size(); i++) {
            std::cout << products[i].name_product << std::endl;
            std::cout << products[i].cost << std::endl;
        }

        boost::property_tree::ptree responseJson;
        boost::property_tree::ptree productsJson;

        for (const auto& product : products) {
            boost::property_tree::ptree productJson;
            productJson.put("id", product.id);
            productJson.put("name_product", product.name_product);
            productJson.put("cost", product.cost);
            productsJson.push_back(std::make_pair("", productJson));
        }

        responseJson.add_child("products", productsJson);

        std::ostringstream oss;
        boost::property_tree::write_json(oss, responseJson, false);
        std::string responseBody = oss.str();

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
        response += "Access-Control-Allow-Headers: Content-Type\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
        response += "\r\n";
        response += responseBody;

        writeToSocket(socket, response);

        std::cout << responseBody << std::endl;
    }
    catch (sql::SQLException& e) {
        std::cout << "SQLException: " << e.what() << std::endl;
    }
}

bool checkLoginCredentials(const std::string& email, const std::string& password) {
    sql::Driver* driver;
    std::unique_ptr<sql::Connection> con;

    try {
        driver = sql::mysql::get_mysql_driver_instance();
        con.reset(driver->connect("tcp://127.0.0.1:3306", "admin", "admin"));

        con->setSchema("users");

        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::string query = "SELECT * FROM user WHERE email='" + email + "' AND password='" + password + "'";
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(query));

        return res->next();
    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error: " << e.what() << std::endl;
    }

    return false;
}

void handleLogin(boost::asio::ip::tcp::socket& socket, const std::string& email, const std::string& password) {
    std::cout << "Received login request:" << std::endl;
    std::cout << "Email: " << email << std::endl;
    std::cout << "Password: " << password << std::endl;

    bool loginSuccessful = checkLoginCredentials(email, password);

    boost::property_tree::ptree responseJson;
    if (loginSuccessful) {
        responseJson.put("status", "success");
        responseJson.put("message", "Login successful");

        responseJson.put("Location", "home.html");
    }
    else {
        responseJson.put("status", "error");
        responseJson.put("message", "Invalid email or password");
        responseJson.put("Location", "home.html");
    }

    std::ostringstream oss;
    boost::property_tree::write_json(oss, responseJson, false);
    std::string responseBody = oss.str();

    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
    response += "Access-Control-Allow-Headers: Content-Type\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
    response += "\r\n";
    response += responseBody;

    std::cout << "Response:" << std::endl;
    std::cout << response << std::endl;

    writeToSocket(socket, response);
}

void handleRegister(boost::asio::ip::tcp::socket& socket, const std::string& userName, const std::string& email, const std::string& password) {
    std::cout << "Received registration request:" << std::endl;
    std::cout << "Username: " << userName << std::endl;
    std::cout << "Email: " << email << std::endl;
    std::cout << "Password: " << password << std::endl;

    sql::Driver* driver;
    std::unique_ptr<sql::Connection> con;

    try {
        driver = sql::mysql::get_mysql_driver_instance();
        con.reset(driver->connect("tcp://127.0.0.1:3306", "admin", "admin"));

        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        stmt->execute("CREATE DATABASE IF NOT EXISTS users");

        con->setSchema("users");

        stmt->execute("CREATE TABLE IF NOT EXISTS user (id INT PRIMARY KEY AUTO_INCREMENT, username VARCHAR(50), email VARCHAR(50), password VARCHAR(50))");
        stmt.reset(con->createStatement());

        std::string query = "SELECT * FROM user WHERE email='" + email + "'";
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(query));
        if (res->next()) {
            boost::property_tree::ptree responseJson;
            responseJson.put("status", "error");
            responseJson.put("message", "User with this email already exists");

            std::ostringstream oss;
            boost::property_tree::write_json(oss, responseJson, false);
            std::string responseBody = oss.str();

            std::string response = "HTTP/1.1 200 OK\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
            response += "Access-Control-Allow-Headers: Content-Type\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
            response += "\r\n";
            response += responseBody;

            writeToSocket(socket, response);

            return;
        }
        stmt.reset(con->createStatement());
        stmt->execute("INSERT INTO user (username, email, password) VALUES ('" + userName + "', '" + email + "', '" + password + "')");

    }
    catch (sql::SQLException& e) {
        std::cout << "MySQL Error: " << e.what() << std::endl;
    }

    boost::property_tree::ptree responseJson;
    responseJson.put("status", "success");
    responseJson.put("message", "Register successful");

    std::ostringstream oss;
    boost::property_tree::write_json(oss, responseJson, false);
    std::string responseBody = oss.str();

    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
    response += "Access-Control-Allow-Headers: Content-Type\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
    response += "\r\n";
    response += responseBody;

    std::cout << "Response:" << std::endl;
    std::cout << response << std::endl;

    writeToSocket(socket, response);
}

void handleConnection(boost::asio::ip::tcp::socket socket) {
    std::string requestData = readFromSocket(socket);
    std::cout << "Received request: " << requestData << std::endl;

    std::string requestType;
    std::string requestBody;
    size_t pos = requestData.find("\n\n");
    if (pos != std::string::npos) {
        requestType = requestData.substr(0, pos);
        requestBody = requestData.substr(pos + 2);
    }


    if (requestType.find("OPTIONS") != std::string::npos) {
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
        response += "Access-Control-Allow-Headers: Content-Type\r\n";
        response += "Content-Length: 0\r\n\r\n";
        writeToSocket(socket, response);

        socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        socket.close();
        return;
    }

    boost::property_tree::ptree requestJson;
    if (!requestBody.empty()) {
        try {
            std::stringstream ss(requestBody);
            boost::property_tree::read_json(ss, requestJson);
        }
        catch (const std::exception& ex) {
            std::cout << "JSON parsing error: " << ex.what() << std::endl;
        }
    }

    std::cout << "requestType:" << requestType << std::endl;
    std::string userName = requestJson.get<std::string>("user_name", "");
    std::string email = requestJson.get<std::string>("email", "");
    std::string password = requestJson.get<std::string>("password", "");

    std::string name_product_sell = requestJson.get<std::string>("name_product_sell", "");
    std::string cost_sell = requestJson.get<std::string>("cost_sell", "");

    if (requestType.substr(0, 4) == "POST") {
        std::cout << "POST" << std::endl;
        if (requestType.substr(5, 6) == "/login") {
            handleLogin(socket, email, password);
        }
        if (requestType.substr(5, 9) == "/register") {
            handleRegister(socket, userName, email, password);
        }
        if (requestType.substr(5, 20) == "/sell_product_server") {
            handleSell_product(socket, name_product_sell, cost_sell);
        }
    }
    if (requestType.substr(0, 3) == "GET") {
        if (requestType.substr(4, 9) == "/products") {
            getProducts(socket);
        }
    }

    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
    response += "Access-Control-Allow-Headers: Content-Type\r\n";
    response += "Content-Length: 0\r\n\r\n";
    writeToSocket(socket, response);

    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
    socket.close();
}

int main() {
    try {
        boost::asio::io_context ioContext;
        boost::asio::ip::tcp::acceptor acceptor(ioContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8080));

        std::cout << "Server started. Listening on port 8080..." << std::endl;

        while (true) {
            boost::asio::ip::tcp::socket socket(ioContext);
            acceptor.accept(socket);

            std::thread(handleConnection, std::move(socket)).detach();
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
    }

    return 0;
}
