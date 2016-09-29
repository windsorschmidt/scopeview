OUTPUT = scopeview
INCLUDES = `pkg-config --cflags gtk+-3.0`
CFLAGS = $(INCLUDES) -Wall
LDFLAGS = `pkg-config --libs gtk+-3.0` -export-dynamic

C_OBJECTS = scopeview.o
scopeview : $(C_OBJECTS)
	$(CC) $(C_OBJECTS) $(LDFLAGS) -o $(OUTPUT)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

all: scopeview

clean:
	rm -f *.o $(OUTPUT)
