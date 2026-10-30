# Feature: PostgreSQL Integration for Packet Storage

## Justification

We implemented PostgreSQL integration into the packet sniffing feature of WireShark, allowing for the automatic storage of packets into a user-defined database.
A user can connect to a database by clicking on the Tools button in the hotbar, then clicking on SQL, which brings up a dialog for a database name, a username, and a passkey, along with a connect and disconnect button.
Once a user puts in valid credentials and connects to a database, all tracked packets will be inserted into the database.

This feature is useful when you want to store a large amount of packets and query them later with custom criteria, such as when you want to sort them by a certain value, or get all packets within a certain source / destination range within a specific timeframe.

## Modified Files

psql_shell.h, psql_shell.cpp
- This is a new file which contains the bulk of our integration code. It handles the connection to the database and registers the dissector which listens to packet dissections.

psql_database_helper.h, psql_database_helper.cpp
- This is a new file which serves as a helper class for our SQL database queries. It registers an internal table name and its relevant data columns, and helps in constructing queries for initializing our database table and inserting values into the database table.

main_window.h
- This is a modified file, and it was modified to add an event which triggers when our SQL button is clicked.

main_window_slots.cpp
- This is a modified file, and it was modified to include our SQL dialog.

sql_dialog.h, sql_dialog.cpp
- This is a new file which constructs a dialog box for the user to put in the database name, their username, and their passkey, and gives buttons for the user to connect and disconnect from the database.