local cgi = require "libs.cgi"

local queryStr = os.getenv("QUERY_STRING") or "<empty>"
local pathInfoStr = os.getenv("PATH_INFO") or "<empty>"
local serverSoftwareStr = os.getenv("SERVER_SOFTWARE")
	or error("The server software string is missing")

local paramsBlock = "Here are some parameters this script managed to retrieve:<br>"
	.."Query string: "..queryStr.."<br>"
	.."Path info: "..pathInfoStr.."<br>"
	.."Server software: "..serverSoftwareStr.."<br>"

cgi.parse("GET")

cgi.header(200, {
	["Content-Type"] = "text/html",
	["Transfer-Encoding"] = "chunked",
})

local part1 = [[
<html>
	<head>
		<title>Hello World</title>
	</head>
	<body style="white-space: pre-wrap;">
<strong>This entire webpage was constructed and sent to server as chunked!</strong>
]]

local part2 = [[
<br>
]]..paramsBlock..[[
	</body>
</html>
]]

io.write([[
]]..tostring(part1:len()).."\n"..[[
]]..part1.."\n"..[[
]]..tostring(part2:len()).."\n"..[[
]]..part2.."\n"..[[
0
]])