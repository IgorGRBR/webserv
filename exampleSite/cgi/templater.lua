#!/usr/bin/luajit
local cgilua = require 'cgilua'

cgilua.htmlheader()
cgilua.put([[
<html>
<head>
  <title>Hello World</title>
</head>
<body>
  <strong>Hello World!</strong>
</body>
</html>]])

-- io.write("\n\n")
-- io.write("Hello world!\n")
-- os.getenv -- Gets the envvar