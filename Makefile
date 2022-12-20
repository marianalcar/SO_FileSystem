# Makefile
# Sistemas Operativos, DEI/IST/ULisboa
#
# This makefile should be run from the *root* of the project

CC ?= gcc
LD ?= gcc
CLANG_FORMAT ?= clang-format

# space separated list of directories with header files
INCLUDE_DIRS := fs .
# this creates a space separated list of -I<dir> where <dir> is each of the values in INCLUDE_DIRS
INCLUDES := $(addprefix -I, $(INCLUDE_DIRS))

SOURCES  := $(wildcard */*.c)
HEADERS  := $(wildcard */*.h)
OBJECTS  := $(SOURCES:.c=.o)
TARGET_EXECS := $(patsubst %.c,%,$(wildcard tests/*.c))

# VPATH is a variable used by Makefile which finds *sources* and makes them available throughout the codebase
# vpath %.h <DIR> tells make to look for header files in <DIR>
vpath # clears VPATH
vpath %.h $(INCLUDE_DIRS)

CFLAGS += -std=c17 -D_POSIX_C_SOURCE=200809L -pthread
CFLAGS += $(INCLUDES)

# Warnings
CFLAGS += -fdiagnostics-color=always -Wall -Werror -Wextra -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-default -Wswitch-enum -Wundef -Wunreachable-code -Wunused
# Warning suppressions
CFLAGS += -Wno-sign-compare

# optional debug symbols: run make DEBUG=no to deactivate them
ifneq ($(strip $(DEBUG)), no)
  CFLAGS += -g
endif

# optional O3 optimization symbols: run make OPTIM=no to deactivate them
ifeq ($(strip $(OPTIM)), no)
  CFLAGS += -O0
else
  CFLAGS += -O3
endif

# convenience variables for extending compiler options (e.g. to add sanitizers)
CFLAGS += $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

# A phony target is one that is not really the name of a file
# https://www.gnu.org/software/make/manual/html_node/Phony-Targets.html
.PHONY: all clean depend fmt test

all: $(TARGET_EXECS)


# The following target can be used to invoke clang-format on all the source and header
# files. clang-format is a tool to format the source code based on the style specified
# in the file '.clang-format'.
# More info available here: https://clang.llvm.org/docs/ClangFormat.html

# The $^ keyword is used in Makefile to refer to the right part of the ":" in the
# enclosing rule. See https://www.cs.colby.edu/maxwell/courses/tutorials/maketutor/

fmt: $(SOURCES) $(HEADERS)
	echo; \
	echo; \
	if ! command -v $(CLANG_FORMAT) 2>/dev/null >/dev/null; then \
		echo "clang-format not installed. cannot format"; \
		exit 1; \
	fi; \
	if [ $$($(CLANG_FORMAT) --version | grep --only-matching -E 'version [0-9]+' | cut -d' ' -f2) -lt 12 ]; then \
		echo "clang-format is too old (version 12+ required). cannot format"; \
		echo "if using Ubuntu, install clang-format-12 (apt install clang-format-12) and set CLANG_FORMAT to clang-format-12"; \
		echo; \
		echo "e.g.:"; \
		echo '$$ export CLANG_FORMAT=clang-format-12'; \
		echo '$$ make fmt'; \
		echo; \
		echo 'You can also add "export CLANG_FORMAT=clang-format-12" to ~/.profile and never have to do it again.'; \
		exit 1; \
	fi; \
	$(CLANG_FORMAT) -i $^

# Add dependency of target executables in TécnicoFS (to be linked with it)
$(TARGET_EXECS): fs/operations.o fs/state.o
# ^ Note the lack of a rule.
# make uses a set of default rules, one of which compiles C binaries
# the CC, LD, CFLAGS and LDFLAGS are used in this rule
# There is also an implicit dependency of an executable name in an object file (.o) with the same name


# The following target runs all tests
# Since it depends on all tests, it will trigger their compilation automatically.

# $$f is "$f" escaped under the make program.

test: $(TARGET_EXECS)
	retcode=0; \
	for f in $^; do \
		echo "Running test $$f"; \
		$$f || (retcode=1; echo FAIL); \
		echo; \
	done; \
	exit $$retcode


clean:
	rm -f $(OBJECTS) $(TARGET_EXECS)


# This generates a dependency file, with some default dependencies gathered from the include tree
# The dependencies are gathered in the file autodep. You can find an example illustrating this GCC feature, without Makefile, at this URL: https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/GCC/options/MM
# Run `make depend` whenever you add new includes in your files
depend : $(SOURCES)
	$(CC) $(INCLUDES) -MM $^ > autodep
