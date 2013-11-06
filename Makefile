BINS=minimake
OBJS=minimake.o
MAKEDEPS=Makefile

.PHONY: all clean

all: $(BINS)

minimake: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) -o $@ $(OBJS)

%.o: %.c $(MAKEDEPS)
	$(CC) -c $(CFLAGS) $(LDFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -f $(BINS) $(OBJS)
