#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
using boost::bad_lexical_cast;

#include "http_server.h"
#include "syslog.h"

int HttpServer::page_dispatcher_root(MHD_Connection * connection, const string & method, ConnectionData * con_cls, const KeyValues &) const throw()
{
	string	data, devicename;

	con_cls += 0; // ignore

	if(method != "GET" && method != "POST")
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	device->lock();

	try
	{
		devicename = device->device_name();

		data += "<div class=\"ge1\">" + devicename + "</div>\n";

		DeviceIOIterator io;

		for(io = device->begin(); io != device->end(); io++)
		{
			data += " <div class=\"ai1\">" + io->name + " </div>\n";
			data += " <div class=\"ai2\">";

			switch(io->type)
			{

				case(DeviceIO::no_type):	data += "<no type>";	break;
				case(DeviceIO::analog):		data += "analog";		break;
				case(DeviceIO::digital):	data += "digital";		break;
				case(DeviceIO::counter):	data += "counter";		break;
			}

			data += " </div>\n";
			data += " <div class=\"ai2\">";

			switch(io->direction)
			{

				case(DeviceIO::no_direction):	data += "<no type>";		break;
				case(DeviceIO::input):			data += "input";			break;
				case(DeviceIO::output):			data += "output";			break;
				case(DeviceIO::io):				data += "input/output";		break;
			}

			data += " </div>\n";

			data += " <div class=\"ai2\">";
			data += lexical_cast<string>(io->lower_boundary) + string("-") + lexical_cast<string>(io->upper_boundary);
			data += " </div>\n";

			data += " <div class=\"ai2\">";
			data += lexical_cast<string>(io->value);
			data += " </div>\n";

#if 0
			data +=
				make_simple_form
				(
			 		"post", "/get_analog_input",
					"ai4",
					"input", lexical_cast<string>(ix),
					"get",
					"ai5", "resampling", "1"
				);
			data += " </div>\n";
#endif
		}
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

int HttpServer::page_dispatcher_read(MHD_Connection * connection, const string & method, ConnectionData *, const KeyValues & variables) const throw()
{
	string_string_map::const_iterator	it;
	string								input;
	int									resampling;
	string								data;
	string								value;
	string								error = "OK";
	bool								skip;

	if((method != "POST") && (method != "GET"))
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if((it = variables.data.find("input")) == variables.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing value: input"));

	input		= it->second;
	resampling	= 0;
	skip		= false;

	if((it = variables.data.find("resampling")) != variables.data.end())
	{
		try
		{
			resampling = lexical_cast<int>(it->second);
		}
		catch(bad_lexical_cast e)
		{
			error = string("parameter error: ") + e.what();
			value = "ERROR";
			skip = true;
		}
	}

	if(!skip)
	{
		device->lock();

		try
		{
			if(resampling)
				value = lexical_cast<string>(device->read(lexical_cast<int>(input), lexical_cast<int>(resampling)));
			else
				value = lexical_cast<string>(device->read(lexical_cast<int>(input)));
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
	}

	data = "<p>[" + value + "] read " + input + " (" + lexical_cast<string>(resampling) + ") = " + value + ": " + error + "</p>\n";

	return(send_html(connection, "/read", MHD_HTTP_OK, data, 10, "/"));
}

int HttpServer::page_dispatcher_write(MHD_Connection * connection, const string & method, ConnectionData *, const KeyValues & variables) const throw()
{
	string_string_map::const_iterator	it;
	string								output;
	string								value;
	string								error;
	string								data;
	string								rv;

	if((method != "POST") && (method != "GET"))
		return(http_error(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed"));

	if((it = variables.data.find("output")) == variables.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing value: output"));

	output = it->second;

	if((it = variables.data.find("value")) == variables.data.end())
		return(http_error(connection, MHD_HTTP_BAD_REQUEST, "Missing value: value"));

	value = it->second;

	device->lock();

	try
	{
		device->write(lexical_cast<int>(output), lexical_cast<int>(value));
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

	data = "<p>[" + rv + "] write " + output + " = " + value + ": " + error + "</p>\n";

	return(send_html(connection, "/write", MHD_HTTP_OK, data, 10, "/"));
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
".ge1		{\n"
"			border: 2px inset #eee;\n"
"			background-color: #ddd;\n"
"			text-align: center;\n"
"}\n"
".ai1		{\n"
"			border: 2px inset #fff;\n"
"			float: left;\n"
"			width: 160pt;\n"
"			background-color: #eee;\n"
"			margin: 4px 4px 4px 0px;\n"
"			text-align: center;\n"
"}\n"
".ai2		{\n"
"			border-top: 1px solid #ccc;\n"
"			height: 18pt;\n"
"			clear: both;\n"
"}\n"
".ai3		{\n"
"			border-right: 1px solid #ccc;\n"
"			float: left;\n"
"			height: 18pt;\n"
"			width: 40pt;\n"
"			text-align: right;\n"
"			margin: 0px;\n"
"			padding: 0px 4px 0px 0px;\n"
"}\n"
".ai4		{\n"
"			border: 0px solid cyan;\n"
"			float: left;\n"
"			height: 18pt;\n"
"			width: 68pt;\n"
"			text-align: right;\n"
"}\n"
".ai5		{\n"
"			width: 20pt;\n"
"			text-align: right;\n"
"			background-color: #f4f4f4;\n"
"			border: 2px inset #dadada;\n"
"			margin: 0px;\n"
"			padding: 0px;\n"
"			font-size: 75%;\n"
"}\n"
".ao1		{\n"
"			border: 2px inset #fff;\n"
"			float: left;\n"
"			width: 170pt;\n"
"			background-color: #eee;\n"
"			margin: 4px 4px 4px 0px;\n"
"			text-align: center;\n"
"}\n"
".ao2		{\n"
"			border-top: 1px solid #ccc;\n"
"			clear: both;\n"
"}\n"
".ao3		{\n"
"			border-right: 1px solid #ccc;\n"
"			float: left;\n"
"			height: 18pt;\n"
"			width: 60pt;\n"
"			text-align: right;\n"
"			margin: 0px;\n"
"			padding: 0px 4px 0px 0px;\n"
"}\n"
".ao4		{\n"
"			border: 0px solid cyan;\n"
"			float: left;\n"
"			height: 18pt;\n"
"			width: 100pt;\n"
"			text-align: right;\n"
"}\n"
".ao5		{\n"
"			width: 20pt;\n"
"			text-align: right;\n"
"			background-color: #f4f4f4;\n"
"			border: 2px inset #dadada;\n"
"			margin: 0px;\n"
"			padding: 0px;\n"
"			font-size: 75%;\n"
"}\n"
".di1		{\n"
"			border: 2px inset #fff;\n"
"			float: left;\n"
"			width: 106pt;\n"
"			background-color: #eee;\n"
"			margin: 4px 4px 4px 0px;\n"
"			text-align: center;\n"
"}\n"
".di2		{\n"
"			border-top: 1px solid #ccc;\n"
"			clear: both;\n"
"}\n"
".di3		{\n"
"			border-right: 1px solid #ccc;\n"
"			float: left;\n"
"			height: 19pt;\n"
"			width: 12pt;\n"
"			text-align: right;\n"
"			margin: 0px;\n"
"			padding: 0px 4px 0px 0px;\n"
"}\n"
".di4		{\n"
"			border: 0px solid red;\n"
"}\n"
".do1		{\n"
"			border: 2px inset #fff;\n"
"			float: left;\n"
"			width: 120pt;\n"
"			background-color: #eee;\n"
"			margin: 4px 4px 4px 0px;\n"
"			text-align: center;\n"
"}\n"
".do2		{\n"
"			border-top: 1px solid #ccc;\n"
"			clear: both;\n"
"}\n"
".do3		{\n"
"			border-right: 1px solid #ccc;\n"
"			float: left;\n"
"			height: 18pt;\n"
"			width: 12pt;\n"
"			text-align: right;\n"
"			margin: 0px;\n"
"			padding: 0px 4px 0px 0px;\n"
"}\n"
".do4		{\n"
"			border: 0px solid red;\n"
"}\n"
".dc1		{\n"
"			border: 2px inset #fff;\n"
"			float: left;\n"
"			width: 150pt;\n"
"			background-color: #eee;\n"
"			margin: 4px 4px 4px 0px;\n"
"			text-align: center;\n"
"}\n"
".dc2		{\n"
"			border-top: 1px solid #ccc;\n"
"			clear: both;\n"
"}\n"
".dc3		{\n"
"			border-right: 1px solid #ccc;\n"
"			float: left;\n"
"			height: 18pt;\n"
"			width: 34pt;\n"
"			text-align: right;\n"
"			margin: 0px;\n"
"			padding: 0px;\n"
"}\n"
".dc4		{\n"
"			border: 0px solid red;\n"
"}\n"
;
	return(send_raw(connection, MHD_HTTP_OK, data, "text/css"));
}

