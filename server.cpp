#include <iostream>
#include <thread>
#include <string>
#include <stdio.h>
#include <regex>
#include <httplib.h>
#include <vector>
#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/array.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

const int HTTP_PORT = 8002;
const int TCP_PORT = 8001;


std::vector<std::string> parse_json_message(std::string inp_message) {
	using boost::property_tree::ptree;
		
	try {
	std::istringstream iss_message(inp_message);
	ptree config;
	boost::property_tree::read_json(iss_message, config);
	
	int arr_length = config.get<int>("length");	
	
	std::vector<std::string> ip_list(arr_length);

	ptree &ips = config.get_child("data");

	int i = 0;
	for (auto& item :ips)
		ip_list[i++] = item.second.get<std::string>("ip");

	return ip_list;
	}
	catch (...)
	{	
		std::vector<std::string> ip_l_empty;	
		return ip_l_empty;
	}	
}

//-------------------Ping host-----------------------------------
//---------------------------------------------------------------
const int PING_COUNT = 7;

boost::property_tree::ptree ping_thread_exec(std::string ip_addr)
{	
	boost::property_tree::ptree ping_result;
	    	
	//command to start ping with system.ping
	FILE *cmd = popen(("ping -c " + std::to_string(PING_COUNT) + " " + ip_addr).c_str(), "r"); 
	char result[24] = {0x0};
	std::string r = "";
	while (fgets(result, sizeof(result), cmd) != NULL)
		r += std::string(result);

	std::regex t1("\\s(\\d+)\%\\spacket\\sloss");
	std::regex t2(".*?=\\s([^\\/]*)\\/([^\\/]*)\\/([^\\/]*)\\/(.*?)\\sms");
	
	std::smatch match_;
	
	ping_result.put("ip", ip_addr);

	int per_loss = 100;
	std::string net_stat_names[4] = {"min", "avg", "max", "jitter"};
	if (std::regex_search(r, match_, t1))
	  if (match_.size() > 1)
	  {
	    per_loss = std::stoi(match_.str(1));	
	    if ( per_loss < 100)
		{
		  //if host is available parse ping result
		  if (std::regex_search(r, match_, t2))
		  
			for (unsigned i = 1; i < match_.size(); i++)		
			  ping_result.put(net_stat_names[i - 1], match_.str(i));
		  
		}
	  }
	ping_result.put("per_loss", per_loss );

	pclose(cmd);
	
	return ping_result;
}

//combine results of parsing in json 
boost::property_tree::ptree start_ping_array(const std::vector<std::string> & req)
{
  boost::property_tree::ptree data, result;
  
  int length = 0;

  for (std::string ip_string : req)
  {	
	if (ip_string.length() > 4) {
	  length++;
	  
	  data.push_back(std::make_pair( "", ping_thread_exec(ip_string)) );
	  
	}
  }
  result.put("length", length);
  result.add_child("data", data);	

  return result;
}
//--------------------------------------------------------------------------------

using namespace boost::asio;
using ip::tcp;

void client_session(ip::tcp::socket sock)
{
	//for (;;)
	{
		boost::array<char, 1024> buf = {{ 0 }};
		
		try
		{
		  //boost::system::error_code er;
		  size_t len = sock.read_some(boost::asio::buffer(buf));
		  
		  std::string data;
		  std::copy(buf.begin(), buf.begin() + len, std::back_inserter(data));

		  std::vector<std::string> req_ = parse_json_message( data ); 
		  
		  if (! req_.empty()) { 
			auto ret = std::async(&start_ping_array, req_);			

			auto s = ret.get();
			
			std::ostringstream ost;
			//write json to output buffer
			boost::property_tree::write_json(ost, s);
			std::string responce_str = ost.str();
			
			std::cout << "Pinger returns data " << std::endl;
			boost::asio::write(sock, boost::asio::buffer(responce_str, responce_str.size() ));
		  }
		  //boost::asio::write(sock, boost::asio::buffer("Succes", 6));
		} 
		catch (const std::exception &ex)
		{
			std::cout << "Error: " << ex.what() << std::endl;
		}
	}

	
}


void tcp_server_start()
{
	boost::asio::io_service io_service;

	boost::asio::ip::tcp::endpoint endpoint_acceptor(boost::asio::ip::tcp::v4(), TCP_PORT);
	tcp::acceptor acceptor_(io_service, endpoint_acceptor);

	std::cout << "Start TCP server on port " << TCP_PORT << std::endl;	
	for(;;)
	{
		auto sock = tcp::socket(io_service);
		acceptor_.accept(sock);
	
		std::thread(client_session, std::move(sock)).detach();
	}

	return;
}

//--------------------------------------------------------------------------------


using namespace httplib;

int http_server_start(Server & svr) { 

	  svr.bind_to_port("0.0.0.0", HTTP_PORT);
	  std::cout << "Start HTTP server on port " << HTTP_PORT << std::endl;

	  svr.Get("/info", [](const Request& req, Response& res) { 
		res.set_content("server is run", "text/plain");
	  });

	  svr.Post("/", [](const Request& req, Response& res) { 	
	  
	    std::vector<std::string> req_ = parse_json_message(std::string(req.body));	    

	    if (! req_.empty()) { 
		auto ret = std::async(&start_ping_array, req_);			

		auto s = ret.get();  	    				
	  
		std::ostringstream oss;
		boost::property_tree::write_json(oss, s);
		std::cout << oss.str() << std::endl;
    	    	res.set_content(oss.str(), "application/json");
	    }

  	  });

	  std::thread th_ = std::thread( [&]() {svr.listen_after_bind();} );
	  
	  th_.join();

	return 0;
}

int main() {
	Server svr;

	std::thread ths = std::thread( [&]() {tcp_server_start();} );
	  
	http_server_start(svr);
	
}
 


//socket.connect(tcp::endpoit( boost::asio::ip::address::from_string(ip_addr), port));

