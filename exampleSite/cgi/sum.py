import cgi

form = cgi.FieldStorage()

result = ""

numbersString = form.getvalue("text")

try:
	if numbersString:
		strings = numbersString.split(' ')

		resultNum = 0
		for numStr in strings:
			resultNum += float(numStr)

		result = str(resultNum)
except ValueError:
	result = "One of the values is not a number"


print("Content-type: text/html\n\n")
# print(f"Here is the text field: {form.getvalue("text")}")
print(f"""
<html>
	<head>
		<title>Hello World</title>
	</head>
	<body style="white-space: pre-wrap;">
<strong>Hello World!</strong>
<br>
<form method="post" enctype="application/x-www-form-urlencoded" action="/awesome/cgi/sum.py">
<label for="text input">Write some numbers separated with space!</label>
<br>
<input id="text input" name="text" type="text"/>
<br>
<button>Send</button>
</form>
{result}
	</body>
</html>
""")
