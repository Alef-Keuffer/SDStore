#include <stdio.h>
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
Admin -> Server: ./server ⟨configFilePath⟩ ⟨transformationsFolderPath⟩
deactivate Admin
activate Server

== proc-file ==
User -> Client: ./client proc-file ⟨priority⟩ ⟨src⟩ ⟨dst⟩ ⟨transformation⟩⁺
activate Client
Client -> Server: ⟨client_pid_str⟩ ⟨PROC_FILE⟩ ⟨priority⟩ ⟨src⟩ ⟨dst⟩ ⟨num_ops⟩ ⟨ops⟩⁺
Server -> Client: pending
Server -> Client: processing
Server -> Client: finished + bytes read + bytes written
deactivate Client

== status ==
User -> Client: ./client status
activate Client
Client -> Server: ⟨client_pid_str⟩ ⟨STATUS⟩
Server -> Client: ⟨get_status_str()⟩
deactivate Client

== Termination ==

Server <-? : SIGTERM or SIGINT
deactivate Server


@enduml
 *
*/



void test1(const char*format, ...) {
  printf(format);
}

int main ()
{
  int i = 2;
  test1("%s%d\n", "alef", i);
  return 0;
}
