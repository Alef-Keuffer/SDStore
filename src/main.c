/*!

@mainpage Example use

@startuml{myimage.svg} "Example use" width=5cm

skinparam Shadowing false
skinparam defaultFontName monospace

actor User
participant Client
participant Server
actor Admin

activate User

== Initialization ==
activate Admin
Admin -> Server: ./sdstored configFilePath transformationsFolderPath
deactivate Admin
activate Server

== proc-file ==
User -> Client: ./sdstore proc-file priority inPath outPath [transformations]
activate Client
Client -> Server: ?
Server -> Client: ?
Client -> User: pending
Server -> Client: ?
Client -> User: processing
Server -> Client: ?
Client -> User: finished + bytes read + bytes written
deactivate Client

== status ==
User -> Client: ./sdstore status
activate Client
Client -> Server: ?
Server -> Client: ?
Client -> User: tasks + transformations
deactivate Client

== Termination ==

Server <-? : SIGTERM
deactivate Server


@enduml
 *
*/






int main ()
{
  return 0;
}
