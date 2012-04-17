OUTPUT = scopeview
INCLUDES = `pkg-config --cflags libglade-2.0`
CFLAGS = $(INCLUDES) -Wall
LDFLAGS = `pkg-config --libs libglade-2.0` -export-dynamic

C_OBJECTS = scopeview.o
scopeview : $(C_OBJECTS)
	$(CC) $(C_OBJECTS) $(LDFLAGS) -o $(OUTPUT)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

all: scopeview

clean:
	rm -f *.o $(OUTPUT)
