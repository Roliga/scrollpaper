all:
	g++ -Wall -g -o main main.cpp -lsfml-graphics -lsfml-window -lsfml-system -lX11 -lXext -lICE -lXrandr -lpthread
