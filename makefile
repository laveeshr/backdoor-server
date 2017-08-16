#Makefile for HTTP 1.1 server backdoor

normal_web_server:server.c
	gcc -o normal_web_server server.c

clean:
	rm normal_web_server
