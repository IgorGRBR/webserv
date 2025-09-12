require 'libs.SAPI'
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
</html>
]])
