initmsocket: msocket.c initmsocket.c
	gcc  initmsocket.c -o initmsocket -L. -lmsocket -lpthread
	./initmsocket