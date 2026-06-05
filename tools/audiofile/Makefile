CXX := g++

libaudiofile.a: audiofile.o
	ar rcs libaudiofile.a audiofile.o

audiofile.o: audiofile.cpp audiofile.h aupvlist.h
	$(CXX) -std=gnu++11 -O3 -ffast-math -I. -c audiofile.cpp

clean:
	rm -f audiofile.o libaudiofile.a

.PHONY: clean
