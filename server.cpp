#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <iostream>
#include "pugixml.hpp"
#include <stdio.h>
#include <iostream>
#include <ctime>

using namespace std;

bool check_password(char* name, char password[]);
bool check_user(char user[]);
void auth(void *arg, char message[], int *acces_level_ptr, char (&ptr_nume_utilizator)[50]);
void DepartingTrains(void *arg, char station[], char trains[], const char* currentTime);
void ArrivingTrains(void *arg, char station[], char trains[], const char* currentTime);
void getCurrentTime(char timeStr[100]);
void updateTime();
void logout(void *arg,int *acces_level_ptr, char *ptr_nume_utilizator);
void displayTrainInfo(char trainName[], char (&train_information)[]);
int get_days_in_month(int year, int month);
string add_minutes(const std::string& time, int minutes);
int update_schedule(const char* message);
static void *thread(void *arg);

char timeStr[100];

typedef struct thData
{
    int idThread;
    int cl;
} thData;


int main()
{   //De fiecare data cand deschidem serverul, baza de date va fi adusa la starea initiala
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("default trains.xml");
    if (!result) {
        std::cerr << "Error parsing XML file: " << result.description() << std::endl;
        return 1;
    }
    doc.save_file("Trains.xml");

    int PORT;
    int i = 0;
    printf("\nIntroduceti PORT-ul dorit pentru a porni serverul: ");
    scanf("%d", &PORT);
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sd; //socket
    pthread_t th[100];

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }
    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }
    if (listen(sd, 2) == -1)
    {
        perror("[server]Eroare la listen().\n");
        return errno;
    }

    while (1)
    {
        int client;

        thData *td;
        socklen_t length = sizeof(from);
        printf("Port->%d\n", PORT);
        fflush(stdout);

        if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
        {
            perror("[server]Eroare la accept().\n");
            continue;
        }
        printf("S-a conectat:%s cu portul %d \n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));

        td = (struct thData *)malloc(sizeof(struct thData));
        td->idThread = i++;
        td->cl = client;
        printf("\nClientul este %d\n", td->cl);
        //se va crea cate un thread pentru fiecare client, iar in thread se vor apela functiile aferente
        pthread_create(&th[i], NULL, &thread, td);
    }
}

static void *thread(void *arg)
{   //thread-ul unui client
    struct thData tdL;
    char message[300]={'\0'}; // buffer to hold the message
    tdL = *((struct thData *)arg);
    printf("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
    fflush(stdout);

    int acces_level=1;
    char nume_utilizator[50];

    while (1)
    {
        tdL = *((struct thData *)arg);
        
        // read the message from the client

        if (!read(tdL.cl, message, 300))
        {
            printf("[Thread %d]\n", tdL.idThread);
            perror("Eroare 1 la read() de la client.\n");
        }
        if (strncmp(message, "info", 4) == 0 && acces_level == 2 )
        {
            char train_information[2000];
            updateTime();
            displayTrainInfo(message,  train_information);
            if (write(tdL.cl, train_information, 2000) <= 0)
            {
                printf("[Thread %d] ", tdL.idThread);
                perror("[Thread]Eroare la write() catre client.\n");
            }
        }
        else if (strncmp(message, "sosiri", 6) == 0 && acces_level == 2 )
        {
            char trains[500];
            updateTime();
            ArrivingTrains((struct thData *)arg, message, trains, timeStr);
            if (strlen(trains) == 0) {
                if (write(tdL.cl, "Nu s-au gasit trenuri disponibile pentru statia introdusa\n", 59) <= 0)
                {
                    printf("[Thread %d] ", tdL.idThread);
                    perror("[Thread]Eroare la write() catre client.\n");
                }
            } else {
                if (write(tdL.cl, trains, 500) <= 0)
                {
                    printf("[Thread %d] ", tdL.idThread);
                    perror("[Thread]Eroare la write() catre client.\n");
                }
            }
        }
        else if (strncmp(message, "login", 5) == 0 && acces_level == 1)
        {   
            auth((struct thData *)arg, message, &acces_level, nume_utilizator); 
        }
        else if (strncmp(message, "plecari", 7) == 0 && acces_level == 2 )
        {
            char trains[500];
            updateTime();
            DepartingTrains((struct thData *)arg, message, trains, timeStr);
            if (strlen(trains) == 0) {
                if (write(tdL.cl, "Nu s-au gasit trenuri disponibile pentru statia introdusa\n", 59) <= 0)
                {
                    printf("[Thread %d] ", tdL.idThread);
                    perror("[Thread]Eroare la write() catre client.\n");
                }
            } else {
                if (write(tdL.cl, trains, 500) <= 0)
                {
                    printf("[Thread %d] ", tdL.idThread);
                    perror("[Thread]Eroare la write() catre client.\n");
                }
            }
        }
        else if (strncmp(message, "intarziere", 10) == 0 && acces_level == 2)
        {
            char msg[2000];
            strcpy(message, message+11);
            updateTime();
            int raspuns = update_schedule(message);
            if(raspuns==1){
                strcpy(msg, "\nNu a fost gasit trenul\n");
            }
            else if(raspuns ==2){
                char train_information[2000];
                strcpy(msg, "\nBaza de date a fost actualizata. Informatia actualizata pentru trenul mentionat este\n");
                char buff[10], train_name[5];
                sscanf(message, "%s", train_name);
                sprintf(buff, "info %s", train_name);
                displayTrainInfo(buff,train_information);
                strcat(msg,train_information);
            }
            if (write(tdL.cl, msg, 2000) <= 0)
            {
                printf("[Thread %d] ", tdL.idThread);
                perror("[Thread]Eroare la write() catre client.\n");
            }
        }
        else if (strcmp(message, "logout") == 0 && acces_level == 2)
        {
            logout((struct thData *)arg, &acces_level, nume_utilizator);  
            char msg[300];
            strcpy(msg, "\nv-ati delogat\n");
            if (write(tdL.cl, msg, 300) <= 0)
            {
                printf("[Thread %d] ", tdL.idThread);
                perror("[Thread]Eroare la write() catre client.\n");
            }
        }
        else if (strcmp(message, "help") == 0)
        {
            char help_msg[1000];
            strcpy(help_msg, "\n---FUNCTIONALITATEA COMENZILOR---\nlogin        (folositi login nume_utilizator pentru a va loga)\ninfo         (folositi info_tren id_tren pentru a vedea tot orarul trenului respectiv)\nsosiri       (folositi sosiri nume_statie pentru a obtine informatii despre \n             trenurile si ora cand acestea ajung in statia respectiva)\nplecari      (folositi plecari nume_statie pentru a obtine informatie despre\n             trenurile si ora cand acestea pleaca din statia respectiva)\nintarzieri   (folositi intirzieri id_tren minute_intarziate pentru a introduce\n             informatiile despre posibilele intarzieri)\nlogout       (pentru a va deloga)\nexit         (pentru a va deconecta de la server)\n");
            if (write(tdL.cl, help_msg, 1000) <= 0)
            {
                printf("[Thread %d] ", tdL.idThread);
                perror("[Thread]Eroare la write() catre client.\n");
            }
        }
        else if (strcmp(message, "exit") == 0)
        {
            char msg[300];
            strcpy(msg, "\nv-ati deconectat\n");
            if (write(tdL.cl, msg, 300) <= 0)
            {
                printf("[Thread %d] ", tdL.idThread);
                perror("[Thread]Eroare la write() catre client.\n");
            }
            logout((struct thData *)arg, &acces_level, nume_utilizator);
            shutdown(tdL.cl, SHUT_RDWR);
            close((intptr_t)arg);
            return (NULL);
        }
        else
        {
            char msg[300];
            strcpy(msg, "\ncomanda invalida\n");
            if (write(tdL.cl, msg, 300) <= 0)
            {
                printf("[Thread %d] ", tdL.idThread);
                perror("[Thread]Eroare la write() catre client.\n");
            }
        }
        printf("[Thread %d]Mesajul a fost receptionat...%s\n", tdL.idThread, message);
    }
    // am terminat cu acest client, inchidem conexiunea 
    close((intptr_t)arg);

    return (NULL);
}

void auth(void *arg, char message[], int *acces_level_ptr, char (&ptr_nume_utilizator)[50]){
    struct thData tdL;
    tdL = *((struct thData *)arg);
    char user[100];
                strcpy(user, message + 6);
                user[strlen(user)] = '\0';
                char msg[300];

                // Check if the user exists
                if (check_user(user)) {
                    // cerem sa introduca parola

                    strcpy(msg, "\nIntroduceti parola\n");
                    if (write(tdL.cl, msg, 300) <= 0)
                    {
                        printf("[Thread %d] ", tdL.idThread);
                        perror("[Thread]Eroare la write() catre client.\n");
                    }
                    printf("[Thread %d]Mesajul a fost receptionat...%s\n", tdL.idThread, message);
                    if (!read(tdL.cl, message, 300))
                    {
                        printf("[Thread %d]\n", tdL.idThread);
                        perror("Eroare 2 la read() de la client.\n");
                    }

                    if (check_password(user, message)) {
                        strcpy(msg, "\nv-ati autentificat cu succes\n");
                        if (write(tdL.cl, msg, 300) <= 0)
                        {
                            printf("[Thread %d] ", tdL.idThread);
                            perror("[Thread]Eroare la write() catre client.\n");
                        }
                        *acces_level_ptr=2;
                        strcpy(ptr_nume_utilizator,user);
                    } else {
                            strcpy(msg, "parola gresita\n");;
                            if (write(tdL.cl, msg, 300) <= 0)
                            {
                                printf("[Thread %d] ", tdL.idThread);
                                perror("[Thread]Eroare la write() catre client.\n");
                            }               
                    }
                } else {
                    strcat(msg, "user inexistent\n");
                            if (write(tdL.cl, msg, 300) <= 0)
                            {
                                printf("[Thread %d] ", tdL.idThread);
                                perror("[Thread]Eroare la write() catre client.\n");
                            }
                }
}

bool check_user(char name[]) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("users.xml");
    if (!result) {
        std::cerr << "Error parsing XML file: " << result.description() << std::endl;
        return 1;
    }

    pugi::xml_node user = doc.child("user");

    for(user = doc.child("user"); user; user=user.next_sibling())
    {
        if(strcmp(user.child("name").text().get(), name) == 0 && strcmp(user.child("isAuth").text().get(), "0") == 0){
            return true;
        }
    }

    return false;
}

bool check_password(char name[],char password[]) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("users.xml");
    if (!result) {
        std::cerr << "Error parsing XML file: " << result.description() << std::endl;
        return 1;
    }

    pugi::xml_node user = doc.child("user");

    for(user = doc.child("user"); user; user=user.next_sibling())
    {
        if(strcmp(user.child("name").text().get(), name) == 0){
            if(strcmp(user.child("password").text().get(), password) == 0){
                user.child("isAuth").text().set("1");
                doc.save_file("users.xml");
                return true;
            }
        }
            
    }

    return false;
}

void DepartingTrains(void *arg, char station[], char trains[500], const char* currentTime){
    struct thData tdL;
    tdL = *((struct thData *)arg);

    strcpy(station, station + 8);
    station[strlen(station)] = '\0'; 

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("Trains.xml");
    if (!result) {
        cerr << "Failed to parse XML file: " << result.description() << endl;
    }

    // Find the root element
    pugi::xml_node root = doc.child("Trenuri");
    if (!root) {
        cerr << "Could not find root element 'Trenuri'" << endl;
    }

    strcpy(trains, "");

    bool stationExists = false;
    // Iterate over the <Tren> elements and extract the relevant information
    for (pugi::xml_node tren : root.children("Tren")) {
        string trenName = tren.attribute("name").as_string();

        // Iterate over the <Statie> elements and extract the relevant information
        for (pugi::xml_node statie : tren.child("Statii").children("Statie")) {
        string statieName = statie.attribute("name").as_string();
        if (statieName == station) {
            stationExists = true;
            if (statie.attribute("oraP") && strcmp(statie.attribute("oraP").value(), currentTime) >= 0) //daca ora plecarii este mai mare decat ora actuala
            {
                //cout<<"ora plecarii "<< statie.attribute("oraP").value()<<" ora curenta "<<timeStr<< " valoarea comparatiei "<< strcmp(statie.attribute("oraP").value(), currentTime)<<endl;
                // Append the train information to the 'trains' array
                strcat(trains, trenName.c_str());
                strcat(trains, " leave on ");
                strcat(trains, statie.attribute("oraP").as_string());
                strcat(trains, "\n");
            }
        }
        }
    }
    if (!stationExists) {
        strcpy(trains, "The specified station does not exist.\n");
    }
}

void ArrivingTrains(void *arg, char station[], char trains[500], const char* currentTime){
    struct thData tdL;
    tdL = *((struct thData *)arg);

    strcpy(station, station + 7);
    station[strlen(station)] = '\0'; 

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("Trains.xml");
    if (!result) {
        cerr << "Failed to parse XML file: " << result.description() << endl;
    }

    // Find the root element
    pugi::xml_node root = doc.child("Trenuri");
    if (!root) {
        cerr << "Could not find root element 'Trenuri'" << endl;
    }

    strcpy(trains, "");

    bool stationExists = false;
    // Iterate over the <Tren> elements and extract the relevant information
    for (pugi::xml_node tren : root.children("Tren")) {
        string trenName = tren.attribute("name").as_string();

        // Iterate over the <Statie> elements and extract the relevant information
        for (pugi::xml_node statie : tren.child("Statii").children("Statie")) {
        string statieName = statie.attribute("name").as_string();
        if (statieName == station) {
            stationExists = true;
            if (statie.attribute("oraS") && strcmp(statie.attribute("oraS").value(), currentTime) >= 0) //daca ora plecarii este mai mare decat ora actuala
            {
                //cout<<"ora plecarii "<< statie.attribute("oraS").value()<<" ora curenta "<<timeStr<< " valoarea comparatiei "<< strcmp(statie.attribute("oraS").value(), currentTime)<<endl;
                // Append the train information to the 'trains' array
                strcat(trains, trenName.c_str());
                strcat(trains, " arriving at ");
                strcat(trains, statie.attribute("oraS").as_string());
                strcat(trains, "\n");
            }
        }
        }
    }
    if (!stationExists) {
        strcpy(trains, "The specified station does not exist.\n");
    }
}

void getCurrentTime(char timeStr[100]) {// Gets the current time as a string
  time_t rawTime;
  time(&rawTime);

  struct tm* timeInfo;
  timeInfo = localtime(&rawTime);

  // Format the time as a string in the 'timeStr' array
  strftime(timeStr, 100, "%Y-%m-%d %H:%M:%S", timeInfo);
}

void updateTime() {// Updates the 'time' attribute of the 'Trenuri' node in the XML file
    getCurrentTime(timeStr);
//   strcpy(timeStr, timeStr+11);
    strcpy(timeStr+16, "\0");
  // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("Trains.xml");
    if (!result) {
        cerr << "Failed to parse XML file: " << result.description() << endl;
        return;
    }

    // Find the root element
    pugi::xml_node root = doc.child("Trenuri");
    if (!root) {
        cerr << "Could not find root element 'Trenuri'" << endl;
        return;
    }

    // Update the 'time' attribute
    root.attribute("time").set_value(timeStr);

    // Save the modified XML file
    doc.save_file("Trains.xml");
}

void logout(void *arg,int *acces_level_ptr, char ptr_nume_utilizator[]){
    *acces_level_ptr=1;
    struct thData tdL;
    tdL = *((struct thData *)arg);  

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("users.xml");
    if (!result) {
        std::cerr << "Error parsing XML file: " << result.description() << std::endl;;
    }

    pugi::xml_node user = doc.child("user");

    for(user = doc.child("user"); user; user=user.next_sibling())
    {
        if(strcmp(user.child("name").text().get(), ptr_nume_utilizator) == 0 && strcmp(user.child("isAuth").text().get(), "1") == 0)
        {
            user.child("isAuth").text().set("0");
            doc.save_file("users.xml");
        }
    }
}

void displayTrainInfo(char trainName[], char (&train_information)[]){
    bool afisare_mesaj_ajungere=false;
    strcpy(train_information, "");
    strcpy(trainName, trainName + 5);
    trainName[strlen(trainName)] = '\0'; 

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file("Trains.xml");
    if (!result) {
        cerr << "Failed to parse XML file: " << result.description() << endl;
    }

    pugi::xml_node root = doc.child("Trenuri");
    if (!root) {
        cerr << "Could not find root element 'Trenuri'" << endl;
    }

    for (pugi::xml_node tren : root.children("Tren")) {
        if (string(tren.attribute("name").value()) == trainName) {
            strcat(train_information, "Trenul ");
            strcat(train_information, trainName);
            strcat(train_information, " trece prin urmatoarele statii:\n");
        for (pugi::xml_node statie : tren.child("Statii").children("Statie")) 
        {
            string stationName = statie.attribute("name").value();
            int delay = statie.attribute("Tintarziere").as_int();
            strcat(train_information, " ");
            strcat(train_information, stationName.c_str());
            if (statie.attribute("oraS")) {
                strcat(train_information, ", Ora sosirii - ");
                strcat(train_information, statie.attribute("oraS").as_string());
            }
            if (statie.attribute("oraP")) {
                strcat(train_information, ": Ora plecarii - ");
                strcat(train_information, statie.attribute("oraP").as_string());
                strcat(train_information, " ");
            }
            if (delay > 0) {
                char n_str[10];
                sprintf(n_str, "%d", delay);
                strcat(train_information, ", Intarziere - ");
                strcat(train_information, n_str);
                strcat(train_information, " minute");
            }
            if(strcmp(root.attribute("time").value(),statie.attribute("oraS").value()) <= 0 && !afisare_mesaj_ajungere){
                strcat(train_information, ", Urmeaza sa ajunga");
                afisare_mesaj_ajungere=true;
            }
            if(strcmp(root.attribute("time").value(),statie.attribute("oraP").value()) < 0 && !afisare_mesaj_ajungere){
                strcat(train_information, ", Urmeaza sa plece");
                afisare_mesaj_ajungere=true;
            }
            strcat(train_information, "\n");
        }
        return;
        }
    }
    sprintf(train_information, "Trenul %s nu a fost gasit\n", trainName); 
}

int get_days_in_month(int year, int month) {
  // Check for leap year
  if (month == 2) {
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
      return 29;
    } else {
      return 28;
    }
  }

  // Return the number of days for the other months
  static const int days[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };
  return days[month - 1];
}

string add_minutes(const std::string& time, int minutes) {
  // Convert the input string to a tm struct
  std::tm tm = {};
  strptime(time.c_str(), "%Y-%m-%d %H:%M", &tm);

  // Add the minutes to the tm struct
  tm.tm_min += minutes;

  // Handle cases where the number of minutes is greater than 60
  while (tm.tm_min >= 60) {
    tm.tm_min -= 60;
    tm.tm_hour++;
  }
  while (tm.tm_min < 0) {
    tm.tm_min += 60;
    tm.tm_hour--;
  }

  // Handle cases where the number of hours is negative or greater than 23
  while (tm.tm_hour >= 24) {
    tm.tm_hour -= 24;
    tm.tm_mday++;
  }
  while (tm.tm_hour < 0) {
    tm.tm_hour += 24;
    tm.tm_mday--;
  }

  // Handle cases where the number of days is negative or greater than the
  // maximum allowed value for the current month
  while (tm.tm_mday > get_days_in_month(tm.tm_year + 1900, tm.tm_mon + 1)) {
    tm.tm_mday -= get_days_in_month(tm.tm_year + 1900, tm.tm_mon + 1);
    tm.tm_mon++;
  }
  while (tm.tm_mday <= 0) {
    tm.tm_mon--;
    tm.tm_mday += get_days_in_month(tm.tm_year + 1900, tm.tm_mon + 1);
  }

  // Handle cases where the number of months is negative or greater than 11
  while (tm.tm_mon >= 12) {
    tm.tm_mon -= 12;
    tm.tm_year++;
  }
  while (tm.tm_mon < 0) {
    tm.tm_mon += 12;
    tm.tm_year--;
  }

  // Convert the tm struct back to a string
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &tm);
  return buffer;
}

int update_schedule(const char* message) {
    pugi::xml_document doc;
    doc.load_file("Trains.xml");
    // Parse the message to get the train name and delay time
    std::string train_name;
    int delay_minutes;
    sscanf(message, "%s %d", &train_name[0], &delay_minutes);

    // Get the current time from the "time" attribute of the "Trenuri" element
    string current_time = doc.child("Trenuri").attribute("time").value();

    // Find the train with the given name
    pugi::xml_node train = doc.child("Trenuri").find_child_by_attribute("Tren", "name", train_name.c_str());
    if (!train) {
        // Train with the given name was not found, do nothing
        return 1;
    }

    // Find the current station by comparing the current time with the arrival
    // and departure times of each station
    pugi::xml_node current_station;
    for (pugi::xml_node station : train.child("Statii")) {
        std::string arrival_time = station.attribute("oraS").value();
        std::string departure_time = station.attribute("oraP").value();
        if (current_time < arrival_time || current_time < departure_time) {
        current_station = station;
        break;
        }
    }
    // if (!current_station) {
    //     std::cout<< "Train is not at any station, do nothing";
    //     return;
    // }

    // Add the delay minutes to the "Tintarziere" attribute of the current station
    int current_delay = current_station.attribute("Tintarziere").as_int();
    current_station.attribute("Tintarziere").set_value(current_delay + delay_minutes);

    // Update the arrival and departure times of the current station
    if(current_time.compare(current_station.attribute("oraS").value()) < 0 && current_time.compare(current_station.attribute("oraP").value()) < 0){
        current_station.attribute("oraP").set_value(add_minutes(current_station.attribute("oraP").value(), delay_minutes).c_str());
        current_station.attribute("oraS").set_value(add_minutes(current_station.attribute("oraS").value(), delay_minutes).c_str());
    }
    else if(current_time.compare(current_station.attribute("oraS").value()) >= 0 && current_time.compare(current_station.attribute("oraP").value()) < 0){
        current_station.attribute("oraP").set_value(add_minutes(current_station.attribute("oraP").value(), delay_minutes).c_str());
    }
    //   current_station.attribute("oraP").set_value(add_minutes(current_station.attribute("oraP").value(), delay_minutes).c_str());
    //   current_station.attribute("oraS").set_value(add_minutes(current_station.attribute("oraS").value(), delay_minutes).c_str());

    // Update the arrival and departure times of the remaining stations
    pugi::xml_node station = current_station;
    while (station = station.next_sibling()) {
        station.attribute("oraP").set_value(add_minutes(station.attribute("oraP").value(), delay_minutes).c_str());
        station.attribute("oraS").set_value(add_minutes(station.attribute("oraS").value(), delay_minutes).c_str());

        // Add the delay minutes to the "Tintarziere" attribute of each remaining station
        int delay = station.attribute("Tintarziere").as_int();
        station.attribute("Tintarziere").set_value(delay + delay_minutes);
    }
    doc.save_file("Trains.xml");
    return 2;
}