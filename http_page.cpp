#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>

#include <sstream>
#include <iomanip>
using namespace std;

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
using boost::bad_lexical_cast;

#include "http_server.h"
#include "syslog.h"

int HttpServer::page_dispatcher_root(MHD_Connection * connection, const string & method, ConnectionData * con_cls, const KeyValues &) const throw()
{
	string			data;
	string			value;
	ostringstream	conv;

	con_cls += 0; // ignore

	if(method != "GET" && method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	device->lock();

	try
	{
		data += "<table>";
		data += "<tr><td class=\"title\" colspan=\"13\">" + device->device_name() + "</td></tr>\n";
		data += "<tr><td class=\"heading\">id</td><td class=\"heading\">name</td><td class=\"heading\">address</td><td class=\"heading\">type</td><td class=\"heading\">direction</td><td class=\"heading\">lower</td><td class=\"heading\">upper</td><td class=\"heading\">set</td><td class=\"heading\">read</td><td class=\"heading\">updated</td><td class=\"heading\">resampling</td><td class=\"heading\">value</td><td class=\"heading\">action</td></tr>\n";

		DeviceIOIterator io;

		for(io = device->begin(); io != device->end(); io++)
		{
			data += "<tr>";
			data += "<td>" + lexical_cast<string>(io->id) + "</td>";
			data += "<td class=\"l\">" + io->name + "</td>";
			conv.str("");
			conv << hex << setfill('0') << setw(2) << io->address;
			data += "<td>" + conv.str() + "</td>";
			data += "<td>";

			switch(io->type)
			{
				case(DeviceIO::no_type):	data += "-no type-";	break;
				case(DeviceIO::analog):		data += "analog";		break;
				case(DeviceIO::digital):	data += "digital";		break;
				case(DeviceIO::counter):	data += "counter";		break;
			}

			data += "</td><td>";

			switch(io->direction)
			{

				case(DeviceIO::no_direction):	data += "-no type-";	break;
				case(DeviceIO::input):			data += "input";		break;
				case(DeviceIO::output):			data += "output";		break;
				case(DeviceIO::io):				data += "input/output";	break;
			}

			data += "</td>\n";

			data += "<td>" + lexical_cast<string>(io->lower_boundary) + "</td>";
			data += "<td>" + lexical_cast<string>(io->upper_boundary) + "</td>";

			string stamp;

			if(io->stamp_set > 0)
			{
				char timebuffer[64];
				struct tm *tm = localtime(&io->stamp_set);
				strftime(timebuffer, sizeof(timebuffer), "%Y/%m/%d %H:%M:%S", tm);
				stamp = timebuffer;
			}
			else
				stamp = "-unset-";

			data += "<td>" + stamp + "</td>";

			if(io->stamp_read > 0)
			{
				char timebuffer[64];
				struct tm *tm = localtime(&io->stamp_read);
				strftime(timebuffer, sizeof(timebuffer), "%Y/%m/%d %H:%M:%S", tm);
				stamp = timebuffer;
			}
			else
				stamp = "-unset-";

			data += "<td>" + stamp + "</td>";

			if(io->stamp_updated > 0)
			{
				char timebuffer[64];
				struct tm *tm = localtime(&io->stamp_updated);
				strftime(timebuffer, sizeof(timebuffer), "%Y/%m/%d %H:%M:%S", tm);
				stamp = timebuffer;
			}
			else
				stamp = "-unset-";

			data += "<td>" + stamp + "</td>";

			data += "<td>" +
				make_simple_form
				(
			 		"post", "/resampling",
					"td",
					"io", io->name,
					"set",
					"input", "value", lexical_cast<string>(io->resampling)

				) + "</td>";

			conv.str("");
			conv << dec << fixed << setprecision(io->precision) << io->value;
			value = conv.str();

			data += "<td>" +
				make_simple_form
				(
			 		"post", "/write",
					"td",
					"io", io->name,
					"set",
					"input", "value", value
				) + "</td>";

			data += "<td>" +
				make_simple_form
				(
			 		"post", "/update",
					"td",
					"io", io->name,
					"update"
				) + "</td>";

			data += "</tr>\n";
		}

		data += "<tr><td colspan=\"13\">" +
			make_simple_form
			(
			 	"post", "/update",
				"td",
				"", "",
				"update all"
			) + "</td></tr>";

		data += "</table>\n";
	}
	catch(string e)
	{
		device->unlock();
		return(http_error(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, e));
	}
	catch(...)
	{
		device->unlock();
		return(http_error(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, "Unknown error"));
	}

	device->unlock();
	return(send_html(connection, "/", MHD_HTTP_OK, data));
}

int HttpServer::page_dispatcher_update(MHD_Connection * connection, const string & method, ConnectionData *, const KeyValues & variables) const throw()
{
	string_string_map::const_iterator	it;
	bool								any;
	string								id;
	string								error;
	string								data;
	DeviceIOIterator					io;

	if((method != "POST") && (method != "GET"))
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	device->lock();

	if((it = variables.data.find("io")) == variables.data.end())
		any = true;
	else
	{
		any = false;
		id = it->second;

		if((io = device->find(id)) == device->end())
		{
			device->unlock();
			return(http_error(connection, MHD_HTTP_BAD_REQUEST, string("Bad value: io: ") + id));
		}
	}

	try
	{
		if(any)
			device->update();
		else
			device->update(io);
		error = "OK";
	}
	catch(string e)
	{
		error = string("ERROR: ") + e;
	}
	catch(...)
	{
		error = "generic error";
	}

	device->unlock();

	data = "<p>[" + error + "] update " + id + "</p>\n";

	return(send_html(connection, "/update", MHD_HTTP_OK, data, 1, "/"));
}

int HttpServer::page_dispatcher_resampling(MHD_Connection * connection, const string & method, ConnectionData *, const KeyValues & variables) const throw()
{
	string_string_map::const_iterator	it;
	string								id;
	string								value;
	string								error;
	string								data;
	string								rv;
	DeviceIOIterator					io;

	if((method != "POST") && (method != "GET"))
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if((it = variables.data.find("io")) == variables.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing value: io"));
	id = it->second;

	if((it = variables.data.find("value")) == variables.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing value: value"));
	value = it->second;

	device->lock();

	if((io = device->find(id)) == device->end())
	{
		device->unlock();
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, string("Bad value: io: ") + id));
	}

	try
	{
		device->resampling(io, lexical_cast<int>(value));
		rv = "OK";
		error = "OK";
	}
	catch(string e)
	{
		rv = "ERROR";
		error = string("ERROR: ") + e;
	}
	catch(bad_lexical_cast e)
	{
		rv = "ERROR";
		error = string("ERROR: parameter error: ") + e.what();
	}
	catch(...)
	{
		rv = "ERROR";
		error = "generic error";
	}

	device->unlock();

	data = "<p>[" + rv + "] resampling " + id + " = " + value + ": " + error + "</p>\n";

	return(send_html(connection, "/resampling", MHD_HTTP_OK, data, 1, "/"));
}

int HttpServer::page_dispatcher_read(MHD_Connection * connection, const string & method, ConnectionData *, const KeyValues & variables) const throw()
{
	string_string_map::const_iterator	it;
	string								id;
	string								data;
	string								value;
	string								error = "OK";
	DeviceIOIterator					io;

	if((method != "POST") && (method != "GET"))
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if((it = variables.data.find("io")) == variables.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing value: io"));

	id = it->second;

	device->lock();

	if((io = device->find(id)) == device->end())
	{
		device->unlock();
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, string("Bad value: io: ") + id));
	}

	try
	{
		value = lexical_cast<string>(device->read(io));
	}
	catch(string e)
	{
		error = e;
		value = "ERROR";
	}
	catch(bad_lexical_cast e)
	{
		error = string("parameter error: ") + e.what();
		value = "ERROR";
	}
	catch(...)
	{
		error = "generic error";
		value = "ERROR";
	}

	device->unlock();

	data = "<p>[" + value + "] read " + id + " = " + value + ": " + error + "</p>\n";

	return(send_html(connection, "/read", MHD_HTTP_OK, data, 1, "/"));
}

int HttpServer::page_dispatcher_write(MHD_Connection * connection, const string & method, ConnectionData *, const KeyValues & variables) const throw()
{
	string_string_map::const_iterator	it;
	string								id;
	string								value;
	string								error;
	string								data;
	string								rv;
	DeviceIOIterator					io;

	if((method != "POST") && (method != "GET"))
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if((it = variables.data.find("io")) == variables.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing value: io"));
	id = it->second;

	if((it = variables.data.find("value")) == variables.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing value: value"));
	value = it->second;

	device->lock();

	if((io = device->find(id)) == device->end())
	{
		device->unlock();
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, string("Bad value: io: ") + id));
	}

	try
	{
		device->write(io, lexical_cast<double>(value));
		rv = "OK";
		error = "OK";
	}
	catch(string e)
	{
		rv = "ERROR";
		error = string("ERROR: ") + e;
	}
	catch(bad_lexical_cast e)
	{
		rv = "ERROR";
		error = string("ERROR: parameter error: ") + e.what();
	}
	catch(...)
	{
		rv = "ERROR";
		error = "generic error";
	}

	device->unlock();

	data = "<p>[" + rv + "] write " + id + " = " + value + ": " + error + "</p>\n";

	return(send_html(connection, "/write", MHD_HTTP_OK, data, 1, "/"));
}

int HttpServer::page_dispatcher_debug(MHD_Connection * connection, const string & method, ConnectionData * con_cls, const KeyValues & variables) const throw()
{
	string data, text, devicename;
	string id = "";

	HttpServer::KeyValues	responses;
	HttpServer::KeyValues	headers;
	HttpServer::KeyValues	cookies;
	HttpServer::KeyValues	postdata;
	HttpServer::KeyValues	arguments;
	HttpServer::KeyValues	footer;

	if(method != "GET" && method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	responses	= get_http_values(connection, MHD_RESPONSE_HEADER_KIND);
	headers		= get_http_values(connection, MHD_HEADER_KIND);
	cookies		= get_http_values(connection, MHD_COOKIE_KIND);
	postdata	= get_http_values(connection, MHD_POSTDATA_KIND);
	arguments	= get_http_values(connection, MHD_GET_ARGUMENT_KIND);
	footer		= get_http_values(connection, MHD_FOOTER_KIND);

	device->lock();
	devicename = device->device_name();
	device->unlock();
	
	data += string("<p>method: ") + method + "</p>";
	data +=	"<p>responses";
	data += responses.dump(true);
	data +=	"</p>\n<p>headers";
	data += headers.dump(true);
	data +=	"</p>\n<p>cookies";
	data += cookies.dump(true);
	data +=	"</p>\n<p>postdata arguments";
	data += postdata.dump(true);
	data +=	"</p>\n<p>http footer";
	data += footer.dump(true);
	data +=	"</p>\n<p>GET arguments";
	data += arguments.dump(true);
	data += "</p>\n<p>post args";
	data +=	con_cls->values.dump(true);
	data +=	"</p>\n<p>ALL arguments";
	data += variables.dump(true);
	data += "</p>\n<p>device name: " + devicename;
	data += "</p>\n";

	return(send_html(connection, "debug", MHD_HTTP_OK, data, 5));
};

int HttpServer::page_dispatcher_stylecss(MHD_Connection * connection, const string & method, ConnectionData * con_cls, const KeyValues &) const throw()
{
	string data;

	con_cls += 0; // ignore

	if(method != "GET" && method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	data += "\n"
"table	{\n"
"			border: 2px outset #eee;\n"
"			background-color: #ddd;\n"
"			text-align: center;\n"
"}\n"
"td.title {\n"
"			border: 2px inset #eee;\n"
"			background-color: #ddd;\n"
"			margin: 0px 0px 0px 0px;\n"
"			text-align: center;\n"
"			font-weight: bold;\n"
"}\n"
"td.heading {\n"
"			border: 2px inset #eee;\n"
"			background-color: #ddd;\n"
"			margin: 0px 0px 0px 0px;\n"
"			text-align: center;\n"
"			font-weight: bold;\n"
"}\n"
"td.l {\n"
"			border: 2px inset #eee;\n"
"			background-color: #ddd;\n"
"			margin: 0px 0px 0px 0px;\n"
"			text-align: left;\n"
"}\n"
"td {\n"
"			border: 2px inset #eee;\n"
"			background-color: #ddd;\n"
"			margin: 0px 0px 0px 0px;\n"
"			text-align: right;\n"
"}\n"
".input {\n"
"			width: 40px;\n"
"			border: 2px inset #eee;\n"
"			background-color: #eee;\n"
"			margin: 0px 0px 0px 0px;\n"
"			text-align: right;\n"
"}\n"
;
	return(send_raw(connection, MHD_HTTP_OK, data, "text/css"));
}

