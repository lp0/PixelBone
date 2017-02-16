#########
#
# The top level targets link in the two .o files for now.
#
TARGETS += examples/rgb-test
# TARGETS += examples/matrix-test
# TARGETS += examples/tile-test
# TARGETS += examples/scroll
# TARGETS += examples/clear
# TARGETS += examples/game_of_life
# TARGETS += examples/clock
# TARGETS += examples/binary_clock
# TARGETS += examples/test
# TARGETS += examples/2048
# TARGETS += examples/fade-test
# TARGETS += examples/fire
# TARGETS += network/udp-rx
# TARGETS += network/opc-rx

PIXELBONE_OBJS = pixel.o gfx.o matrix.o util.o
PIXELBONE_LIB := libpixelbone.a

all: $(TARGETS)

CFLAGS += \
	-std=c99 \
	-W \
	-Wall \
	-D_DEFAULT_SOURCE \
	-Wp,-MMD,$(dir $@).$(notdir $@).d \
	-Wp,-MT,$@ \
	-I. \
	-O2 \

LDFLAGS += \

LDLIBS += \
	-lpthread \

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -std=c++11 -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<


$(foreach O,$(TARGETS),$(eval $O: $O.o $(PIXELBONE_OBJS)))

$(TARGETS):
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)


.PHONY: clean

clean:
	rm -rf \
		**/*.o \
		*.o \
		.*.o.d \
		*~ \
		$(TARGETS) \

# Include all of the generated dependency files
-include .*.o.d
