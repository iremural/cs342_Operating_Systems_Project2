# cs342_Operating_Systems_Project2
 
Implemented a multithreaded server. The server searches a keyword in an input text file and returns the line numbers that contain that keyword at least once. The search keyword is send by the client via a shared memory region. Server creates a new thread to handle the request from each client. Client sends the request and waits until it gets the result from the server. While the line numbers are arriving, prints them on the screen. Several concurrent client processes can send a request to the server.
