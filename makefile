all:myftp

myftp: server1 client1

server1:
	cd server && $(MAKE)

client1:
	cd client && $(MAKE)

clean:
	cd server && $(MAKE) clean
	cd client && $(MAKE) clean
