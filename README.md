# Message Oriented TCP |  Prof. Arobinda Gupta and Prof. Sandip Chakraborty                                                                                       [Mar 2024]
•  Developed a TCP-based message library in C for reliable and ordered delivery, using unrelaible UDP protocol.
•  Created a software layer integrating socket APIs with the TCP stack for seamless pipeline for data transmission
•  Implemented dual-threading to manage read/write buffers for messages as separate tables for in and out traffic.
•  Synchronized shared buffer access between library thread and individual user's process using mutex locks and process level synchronization using Semaphores.
