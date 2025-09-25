#!/usr/bin/env python3
import os
from http import cookies

def run():
	#Read cookies from the request header
	request_data = os.environ.get('HTTP_COOKIE', '')
	cookie1 = cookies.SimpleCookie(request_data)
	#Create a SimpleCookie
	cookie2 = cookies.SimpleCookie()
	cookie2['cool'] = 'a cool webserv'
	cookie2['awesome'] = 'an awesome webserv'
	#Set cookie attributes if needed
	cookie2['cool']['path'] = '/awesome/cgi/'
	cookie2['awesome']['path'] = '/awesome/cgi/'

	#Show the cookies on a html page
	print("Content-type: text/html")
	print("Set-Cookie: " + cookie2['cool'].OutputString())
	print("Set-Cookie: " + cookie2['awesome'].OutputString())
	print()
	print("<html>")
	print("	<head><title>Cookie Tester</title></head>")
	print("	<body>")
	print("		<h1>Cookie Tester</h1>")
	print("		<h2>Set Cookies:</h2>")
	if cookie2:
		for key, morsel in cookie2.items():
			print(f"		<p>{key}: {morsel.value}</p>")
	print("		<h2>Received Cookies:</h2>")
	if cookie1:
		for key, morsel in cookie1.items():
			print(f"		<p>{key}: {morsel.value}</p>")
	print("		</body>")
	print("	</html>")

if __name__ == '__main__':
    run()
