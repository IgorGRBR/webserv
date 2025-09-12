SAPI = {
	Request = {
		getpostdata = function(n)
			return io.read()
		end,
		servervariable = function(varname)
			return os.getenv(varname)
		end,
	},
	Response = {
		contenttype = function(typ)
			io.write("Content-Type: "..typ.."\n")
		end,
		errorlog = function(message)
			io.stderr:write(message)
		end,
		header = function(key, value)
			io.write(key..": "..value.."\n")
		end,
		redirect = function(url)
			io.write("Location: "..url.."\n")
		end,
		write = function(...)
			io.write("\n")
			local function writer(x, ...)
				if x then
					io.write(x)
					writer(...)
				end
			end
			writer(...)
		end,
	},
}

-- local cgi = require "libs.cgi"
-- 
-- cgi.header(200, {})

-- print("Hello world!")
