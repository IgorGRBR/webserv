local cgi = require "libs.cgi"

local queryStr = os.getenv("QUERY_STRING") or "<empty>"
local pathInfoStr = os.getenv("PATH_INFO") or "<empty>"
local serverSoftwareStr = os.getenv("SERVER_SOFTWARE")
	or error("The server software string is missing")


local httpMethod = os.getenv("REQUEST_METHOD")

local postQuery, err
local text = ""

if httpMethod == "POST" then
	postQuery, err = cgi.parse("POST")
end

if err then
	error("Error happened when running a CGI script: "..err)
end

if postQuery then
	text = string.gsub(postQuery["text"], postQuery["find"], postQuery["replace"])
end

local paramsBlock = "Here are some parameters this script managed to retrieve:<br>"
	.."Query string: "..queryStr.."<br>"
	.."Path info: "..pathInfoStr.."<br>"
	.."Post query table: "..tostring(postQuery).."<br>"

cgi.header(200, {["Content-Type"] = "text/html"})
io.write([[
<html>
	<head>
		<title>Hello World</title>
	</head>
	<body style="white-space: pre-wrap;">
<strong>Hello World!</strong>
<br>
]]..paramsBlock..[[
<form method="post" enctype="application/x-www-form-urlencoded" action="/awesome/cgi/templater.lua">
<label for="text input">Write some text!</label>
<br>
<input id="text input" name="text" type="text" value="]]..text..[["/>
<br>
<label for="find">String to find</label>
<br>
<input id="find" name="find" type="text"/>
<br>
<label for="replace">Replace with...</label>
<br>
<input id="replace" name="replace" type="text"/>
<br>
<button>Send</button>
</form>
	</body>
</html>
]])
