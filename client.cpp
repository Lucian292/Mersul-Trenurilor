#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <cstring>
#include <string>
#include <iostream>

using namespace std;

int port;
int main(int argc, char *argv[])
{
  int sd;
  struct sockaddr_in server;

  string comanda1;
  char buf[10];
  bool isRunning = true;

  if (argc != 3)
  {
    printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
    return -1;
  }

  port = atoi(argv[2]);

  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("Eroare la socket().\n");
    return errno;
  }

  while (isRunning)
  {

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
      perror("[client]Eroare la connect().\n");
      return errno;
    }

    while (isRunning)
    {
      //Citim ce doreste utilizatorul si trimitem catre server
      getline(cin, comanda1);

      //transform din string in vector char
      char char_array[comanda1.length() + 1];
      copy(comanda1.begin(), comanda1.end(), char_array);
      char_array[comanda1.length()] = '\0';

      if (write(sd, &char_array, sizeof(char_array)) <= 0)
      {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
      }
      
      char primit[2000];
      bzero(primit, sizeof(primit));
      if (read(sd, primit, 2000) < 0)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }
      if (strcmp(primit, "v-ati deconectat")==0)
        isRunning=false;
      printf("%s", primit);
    }
    /* inchidem conexiunea, am terminat */
    close(sd);
  }
  exit(0);
}