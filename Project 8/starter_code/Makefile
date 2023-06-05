all: server app1 app2a app2b
	@echo All done!

CFLAGS = -Wall -Werror -lpthread -g

server: server.o udp.o server_functions.o
	$(CC) $(CFLAGS) -o $@ $^

app1: app1.o client.o udp.o
	$(CC) $(CFLAGS) -o $@ $^

app2a: app2a.o client.o udp.o
	$(CC) $(CFLAGS) -o $@ $^

app2b: app2b.o  client.o udp.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf server app1 app2a app2b *.o
	@echo Clean done!
