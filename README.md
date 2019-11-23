1- compile by writing make on terminal
2- type ./<executablefile> -N number-of-threads -I filename ipaddress port (By default number of threads are 4 and no logging is done but if you want to change number of theads and want to do logging then use -N and -I flags.  e.g ./httpserver -N 6 -I log.txt localhost 8080)
3- Server is up and running. Now cd Client
4- Do curl commmands on different terminal table