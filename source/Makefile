PROG	= tcpTester

BINDIR	= .
SRCDIR	= .
OBJDIR	= ./obj

SRCS	= $(wildcard $(SRCDIR)/*.c)
OBJS	= $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o)))

INCLUDE	= 

UNAME_OS := $(shell uname -s)
ifeq ($(UNAME_OS),Linux)
	LDFLAGS	= -lpthread -lrt
	CFLAGS	= $(DFLAGS) -O4 -Wall
else ifeq ($(UNAME_OS),Darwin)
	LDFLAGS	= -lpthread
	CFLAGS	= $(DFLAGS) -O3 -Wall
endif



$(PROG): $(OBJS) $(LIBS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<
all: $(PROG)

clean:
	-rm -f $(OBJS) $(PROG) *.log

install:	$(PROG)
	@echo Installing $(PROG) in $(BINDIR)
	#@-strip $(PROG)
	@if [ $(BINDIR) != . ]; then \
	(rm -f $(BINDIR)/$(PROG); $(INSTALL) $(PROG) $(BINDIR)); fi

