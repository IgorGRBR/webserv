-- The `cgi` table is used to store CGI-related functions and utilities.
local cgi = {}

-- The `statuses` table contains common HTTP status codes and their corresponding messages.
local statuses = {
    [200] = "OK",
    [201] = "Created",
    [204] = "No Content",
    [301] = "Moved Permanently",
    [302] = "Found",
    [307] = "Temporary Redirect",
    [400] = "Bad Request",
    [401] = "Unauthorized",
    [403] = "Forbidden",
    [404] = "Not Found",
    [405] = "Method Not Allowed",
    [410] = "Gone",
    [413] = "Payload Too Large",
    [429] = "Too Many Requests",
    [500] = "Internal Server Error",
    [501] = "Not Implemented",
    [502] = "Bad Gateway",
    [503] = "Service Unavailable",
    [504] = "Gateway Timeout"
}

-- The following code checks if the script is running in a terminal or command prompt.
-- If it is, a message is printed indicating that the parameters will be empty.
-- This is done to handle different environments where the script may run.
-- if os.getenv("SHELL") or os.getenv("TERM") then
--     error("Running in terminal mode. Parameters will be empty.\r\n")
-- else
--     if os.getenv("OS") == "Windows_NT" then
--         local file = io.open("CON", "w")
--         if file then
--             file:close()
--             io.write("Running in terminal mode. Parameters will be empty.\r\n")
--         end
--     end
-- end

-- The `cgi.header` function is responsible for writing the HTTP headers to the output.
-- It takes the HTTP status code and optional headers as arguments.
-- The function iterates over the headers and writes them to the output.
function cgi.header(code, headers)
    local message = statuses[code] or "Unknown" -- Get the status message corresponding to the status code.
    local sheaders = {}
    for key in pairs(headers or {}) do
        table.insert(sheaders, key)
    end
    table.sort(sheaders) -- Sort the headers alphabetically.

    for _, key in ipairs(sheaders) do
        local value = headers[key]
        io.write(tostring(key) .. ": " .. tostring(value) .. "\r\n") -- Write each header to the output.
    end
    io.write("\r\n") -- Write a blank line to separate headers from the body.
end

-- The `cgi.parse` function is responsible for parsing the CGI parameters.
-- It takes the HTTP method as an argument and returns a table of parsed parameters.
function cgi.parse(method)
    local params = {}
    -- Check if the script is running in a terminal or command prompt.
    -- If it is, or if it's a Windows system with CON output, return an empty parameter table.
    -- if os.getenv("SHELL") or os.getenv("TERM") or
    --     (os.getenv("OS") == "Windows_NT" and io.open("CON", "w") and io.close()) then
    --     return params
    -- end
    if method == "GET" then
        local query = os.getenv("QUERY_STRING") -- Get the query string from the environment variables.
        if not query or query == "" then
            return nil, "No query string found."
        end
        -- Parse the query string and populate the `params` table with key-value pairs.
        for pair in query:gmatch("[^&]+") do
            local key, value = pair:match("([^=]*)=(.*)")
            if key and value then
                key = key:gsub("+", " "):gsub("%%(%x%x)", function(h)
                    return string.char(tonumber(h, 16))
                end)
                value = value:gsub("+", " "):gsub("%%(%x%x)", function(h)
                    return string.char(tonumber(h, 16))
                end)
                if params[key] == nil then
                    params[key] = value
                elseif type(params[key]) == "string" then
                    params[key] = {params[key], value}
                else
                    table.insert(params[key], value)
                end
            end
        end
    elseif method == "POST" then
        local length = tonumber(os.getenv("CONTENT_LENGTH")) -- Get the content length from the environment variables.
        local ctype = os.getenv("CONTENT_TYPE") -- Get the content type from the environment variables.
        if not length or length == 0 then
            return nil, "No content length specified."
        end
        if not ctype or ctype ~= "application/x-www-form-urlencoded" then
            return nil, "Invalid content type."
        end
        local data = io.read(length) -- Read the post data from the input.
        if not data or data == "" then
            return nil, "No post data found."
        end
        -- Parse the post data and populate the `params` table with key-value pairs.
        for pair in data:gmatch("[^&]+") do
            local key, value = pair:match("([^=]*)=(.*)")
            if key and value then
                key = key:gsub("+", " "):gsub("%%(%x%x)", function(h)
                    return string.char(tonumber(h, 16))
                end)
                value = value:gsub("+", " "):gsub("%%(%x%x)", function(h)
                    return string.char(tonumber(h, 16))
                end)
                if params[key] == nil then
                    params[key] = value
                elseif type(params[key]) == "string" then
                    params[key] = {params[key], value}
                else
                    table.insert(params[key], value)
                end
            end
        end
    else
        return nil, "Invalid request method."
    end
    return params
end

-- The `cgi.cookies` function is responsible for parsing and returning the HTTP cookies.
-- It reads the cookies from the environment variables and returns a table of key-value pairs.
function cgi.cookies()
    local temp = {} -- Temp table for the cookies to be stored and returned.
    local cookies = os.getenv("HTTP_COOKIE") or ""
    for cookie in cookies:gmatch("([^;]+)") do
        local name, value = cookie:match("^%s*(.-)%s*=%s*(.-)%s*$")
        if name and value then
            temp[name] = value
        end
    end
    return temp
end

-- The `cgi.cookie` function is responsible for generating an HTTP cookie string.
-- It takes the cookie name, value, optional expiration time, and secure flag as arguments.
-- It returns a string representing the cookie.
function cgi.cookie(name, value, expires, secure)
    local t
    local cookie = name .. "=" .. value
    if expires then
        t = os.time() + expires
    else
        t = os.time() + 24 * 60 * 60 -- Default expiration time: 24 hours.
    end
    local date = os.date("!%a, %d %b %Y %H:%M:%S GMT", t)
    if secure then
        cookie = cookie .. "; expires=" .. date .. "; SameSite=Strict; Secure"
    else
        cookie = cookie .. "; expires=" .. date
    end
    return cookie
end

-- The `cgi.upload` function is responsible for handling file uploads.
-- It takes a filename as an argument and writes the uploaded file to disk.
-- It returns a boolean indicating the success of the upload and an optional error message.
function cgi.upload(filename)
    local file = io.open(filename, "wb")
    if not file then
        return false, "Failed to open file for writing."
    end
    local size = 4096 -- Chunk size for reading the file data.
    while true do
        local chunk = io.read(size)
        if not chunk then
            break
        end
        file:write(chunk)
    end

    file:close()
    return true, "File uploaded successfully."
end

-- The `cgi.enchtml` function is responsible for encoding HTML special characters.
-- It takes a string as an argument and returns the encoded string.
function cgi.enchtml(str)
    return str and
        (str:gsub("&", "&amp;"):gsub("<", "&lt;"):gsub(">", "&gt;")
            :gsub('"', "&quot;"):gsub("'", "&#039;"))
end

-- The `cgi.params` function is a convenience function that returns the parsed parameters.
-- It determines the request method and calls the `cgi.parse` function accordingly.
-- It returns the parsed parameters or an error message if parsing fails.
function cgi.params()
    local method = os.getenv("REQUEST_METHOD")
    if not method then
        return nil, "Request method not found."
    end
    return cgi.parse(method)
end

-- The `cgi.tag` function is a utility function for generating HTML tags.
-- It takes the tag name, attributes table, and optional content as arguments.
-- It returns a string representing the HTML tag.
function cgi.tag(name, attributes, ...)
    local tstr = "<" .. name
    if attributes then
        for key, value in pairs(attributes) do
            tstr = tstr .. " " .. key .. "=\"" .. value .. "\""
        end
    end

    local content = {...}
    if #content > 0 then
        tstr = tstr .. ">"
        for i, item in ipairs(content) do
            if type(item) == "function" then
                tstr = tstr .. item()
            else
                tstr = tstr .. item
            end
        end
        tstr = tstr .. "</" .. name .. ">"
    else
        tstr = tstr .. ">"
    end
    return tstr
end

-- The `cgi.trace` function is responsible for generating an error message with stack trace.
-- It takes an error message as an argument and returns an HTML string representing the error page.
function cgi.trace(msg)
    local trace = debug.traceback()
    local html = cgi.tag("html", nil,
        cgi.tag("head", nil, cgi.tag("title", nil, "An error has occurred!"),
            cgi.tag("style", {type = "text/css"}, [[
                body {
                    font-family: Arial, Helvetica, sans-serif;
                }
                h1#error {
                    color: red;
                }
            ]]) -- Basic styling.
        ),
        cgi.tag("body", nil, cgi.tag("h1", {id = "error"}, "Internal Server Error"),
            cgi.tag("p", nil, "An error has occurred: " .. cgi.tag("kbd", nil, msg)),
            cgi.tag("h2", nil, "Stack Trace:"),
            cgi.tag("pre", nil, cgi.enchtml(trace)) -- Browsers will misinterpret part of the trace as an unclosed HTML tag.
        )
    )
    return html
end

return cgi -- Returns the CGI table, with all of its functions.
