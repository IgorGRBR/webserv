local cgi = require "libs.cgi"

local queryStr = os.getenv("QUERY_STRING") or "<empty>"
local pathInfoStr = os.getenv("PATH_INFO") or "<empty>"
local serverSoftwareStr = os.getenv("SERVER_SOFTWARE")
	or error("The server software string is missing")

local paramsBlock = "Here are some parameters this script managed to retrieve:<br>"
	.."Query string: "..queryStr.."<br>"
	.."Path info: "..pathInfoStr.."<br>"
	.."Server software: "..serverSoftwareStr.."<br>"
	-- .."Query table: "..tostring(cgilua.POST).."<br>"

-- local postQueryStr = io.read("*a")
local postQuery, err = cgi.parse("GET")

if err then
	cgi.header(500, {["Content-Type"] = "text/html"})
	io.write([[
<html>
	<head>
		<title>Hello World</title>
	</head>
	<body style="white-space: pre-wrap;">
Whoopsie happened:
]]..err..[[
	</body>
</html>
]])
	return
end

postQuery = postQuery or error("Post query is nil for some reason.")
cgi.header(200, {["Content-Type"] = "text/html"})

local queryBlock = "Here is the query block:<br>"
for k, v in pairs(postQuery) do
	queryBlock = queryBlock..k.." = "..v.."<br>"
end

-- queryBlock = queryBlock.."Here is the raw query:<br>"..postQueryStr

io.write([[
<html>
	<head>
		<title>Hello World</title>
	</head>
	<body style="white-space: pre-wrap;">
<strong>Hello World!</strong>
<br>
]]..paramsBlock..[[
]]..queryBlock..[[
	</body>
</html>
]])

--