#!/usr/bin/env python3
import os
from http import cookies
from uuid import uuid4

def run():
	#Read cookies from the request header
	request_data = os.environ.get('HTTP_COOKIE', '')
	cookie1 = cookies.SimpleCookie(request_data)
	#Create a SimpleCookie
	db = set()
	with open("./db/known_users.txt", "r") as db_file:
		for line in db_file:
			db.add(line.strip())
	
	new_customer = False
	if not 'cool' in cookie1:
		cookie1['cool'] = str(uuid4())
		#Set cookie attributes if needed
		new_customer = True
	if not cookie1['cool'].value in db:
		db.add(cookie1['cool'].value)
	
	with open("./db/known_users.txt", "w") as db_file:
		for user_id in db:
			db_file.write(user_id + '\n')

	#Show the cookies on a html page
	print("Content-Type: text/html")
	print("Set-Cookie: " + cookie1['cool'].OutputString())
	# print("Set-Cookie: " + )
	print()
	print("<html>")
	print("	<head><title>Cookie Tester</title></head>")
	print("	<body>")
	print("		<h1>Cookie Tester</h1>")
	print("		<h2>Set Cookies:</h2>")
	if cookie1:
		for key, morsel in cookie1.items():
			print(f"		<p>{key}: {morsel.value}</p>")
	if new_customer:
		print("    Hello, this is your first time on this page!<br>")
	else:
		print("    You have been on this page before!<br>")
	print("		</body>")
	print("	</html>")

if __name__ == '__main__':
    run()
