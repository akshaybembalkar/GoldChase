test_prg: test_prg.cpp libmap.a goldchase.h
	g++ -std=c++11 test_prg.cpp -o test_prg -L. -lmap -lpanel -lncurses -lpthread -lrt -Wall

libmap.a: Screen.o Map.o supportingFunctions.o
	ar -r libmap.a Screen.o Map.o supportingFunctions.o

Screen.o: Screen.cpp
	g++ -c Screen.cpp

Map.o: Map.cpp
	g++ -c Map.cpp

supportingFunctions.o: supportingFunctions.cpp
	g++ -std=c++11 -c supportingFunctions.cpp

clean:
	rm -f Screen.o Map.o supportingFunctions.o libmap.a test_prg
