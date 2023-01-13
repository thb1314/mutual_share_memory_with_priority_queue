CCFLAG      = -std=c++14 -O3
SOFLAG      = $(CCFLAG) -shared -Wall
INCLUDE     = -I. $(shell python3 -m pybind11 --include)
CC          = g++


SOURCE_CPP  = $(shell find . -name '*.cpp')
CPP_OBJ     = $(SOURCE_CPP:.cpp=.cpp.o)


all: $(CPP_OBJ)
	$(CC) $(SOFLAG) -o ./shmem_with_priority_queue$(shell python3-config --extension-suffix) $^ $(LDFLAG)
	rm ./*.d
	rm ./*.o

%.cpp.o: %.cpp
	$(CC) $(CCFLAG) $(INCLUDE) -M -MT $@ -o $(@:.o=.d) $<
	$(CC) $(CCFLAG) $(INCLUDE) -fPIC -o $@ -c $<

	
.PHONY: test
test:
	make clean
	make
	python $(SOURCE_PY)

.PHONY: clean
clean:
	rm -rf ./*.d ./*.o ./*.so 