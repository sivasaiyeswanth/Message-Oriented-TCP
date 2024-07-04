
# Message oriented TCP
This project is a TCP-based messaging library written in C that resembles the UDP protocol but with the reliability of TCP for ordered data transmission. It integrates socket APIs with the standard TCP stack to enable the seamless flow of data, leveraging multithreading for efficient management of incoming and outgoing traffic.


## Requirements
C Compiler (eg GCC)

POSIX-compliant operating system (eg Linux)
## Run Locally

Clone the repository

```bash
  git clone https://github.com/codema09/Message-Oriented-TCP
```

Run the following in order on terminal in the project directory

```bash
  make-f makelibmsocket.mak makelib
```
```bash
 make-f makeinitmsocket.mak initmsocket
```
```bash
 make-f makeuser.mak user1
```
```bash
 make-f makeuser.mak user2
```
```bash
 ./user1 with appropriate arguments for running user1
```
```bash
 ./user2 with appropriate arguments for running user2
```
For Cleaning
```bash
 make-f makeclean.mak clean
```

