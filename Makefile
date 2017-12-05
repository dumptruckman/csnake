TARGET = csnake

CC = gcc
CFLAGS = -std=gnu99 -Wall -g $(shell pkg-config --cflags glib-2.0)

LINKER = gcc
LFLAGS = -Wall -pthread -lncurses
LIBS = $(shell pkg-config --libs glib-2.0)

SRCDIR = src
OBJDIR = obj

SOURCES := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
rm = rm -f

$(TARGET): $(OBJECTS)
	@$(LINKER) $(OBJECTS) $(LFLAGS) -o $@ $(LIBS)
	@echo "Linking complete!"

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiled "$<" successfully!"

.PHONY: clean
clean:
	@$(rm) $(OBJECTS)
	@echo "Cleanup complete!"

.PHONY: remove
remove:
	@$(rm) $(TARGET)
	@echo "Executable removed!"

test: csnake
	./cnsake 60
