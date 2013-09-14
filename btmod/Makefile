all: launch terminate a.out debug profile extable

extable: extable.c
	gcc -o extable extable.c

launch: launch_bt.c
	gcc -Wall -o launch launch_bt.c -lpthread

terminate: terminate_bt.c
	gcc -Wall -o terminate terminate_bt.c -lpthread

debug: debug_bt.c
	gcc -Wall -o debug debug_bt.c -lpthread

profile: profile_bt.c
	gcc -Wall -o profile profile_bt.c -lpthread

shuttle: shuttle.c
	gcc -Wall -o shuttle shuttle.c -lpthread

a.out:
	gcc -static a.c

clean:
	rm -f launch a.out terminate debug profile shuttle
