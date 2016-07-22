ashell : driver.o  
	g++ -ansi -Wall -g -o ashell driver.o  

driver.o : driver.cpp  
	g++ -ansi -Wall -g -c driver.cpp

clean : 
	rm -rf ashell driver.o   
