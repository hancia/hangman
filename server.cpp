#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <list>
#include <thread>
#include <fstream>
#include "Message.h"
#include "Player.h"
#include <stdlib.h>
#include <time.h>

using namespace std;

//list of clients connected to a server
list<int> clients;
//list of players taking part in current game
list<Player*> players;
const int noOfWords = 100;
const int playersRequired = 2;
string words[noOfWords];
string word;
string covered;


int sockyy = socket(PF_INET, SOCK_STREAM, 0);
bool game = false;
bool run = true;
bool guess = false;

//checking if a player is currently in a game
bool isInGame(int player) {
    for (auto &i : players)
        if (i->address == player)
            return true;
    return false;
}

void sendMessage(string msg, int address){
    Message message{};
    strcpy(message.msg, msg.c_str());
    message.msgsize = msg.size();
    write(address, message.msg, static_cast<size_t>(message.msgsize));
}

void waitForAGuess(){
    while(!guess){}
}

void showScore(int address){
    string msg;
    msg = "Scores: ";
    write(address, msg.c_str(), msg.size());
    for(auto &i : players){
        msg = to_string(i->address) + ": " + to_string(i->score) + " \n";
        write(address, msg.c_str(), msg.size());
    }
}


void gameService(){
    int index;
    Message message{};
    while(game){
        index = rand() % noOfWords;
        word = words[index];
        cout<<"The word is "<<word<<endl;
        covered="";
        for(unsigned int a = 0; a<word.size(); a++)
            covered += "*";
        message.msgsize = static_cast<int>(word.size());
        while(strcmp(word.c_str(), covered.c_str()) != 0) {
            strcpy(message.msg, covered.c_str());
            cout<<"Covered in a game service "<<covered<<endl;
            for (auto &i : players) {
                showScore(i->address);
                write(i->address, message.msg, static_cast<size_t>(message.msgsize));
            }
            waitForAGuess();
            guess = false;
        }
        string msg = "The word is guessed, it was " + word;
        for(auto &i: players) {
            write(i->address, msg.c_str(), msg.size());
        }
    }
    exit(0);
}

void waitForPlayers(){
    while(run){
        if(players.size() >= playersRequired){
            game = true;
            thread gameThread(gameService);
            gameThread.join();
            game = false;
        }
    }
    exit(0);
}

//processes the message received from a client
void clientService(int i) {
    Message message{};
    Player currentPlayer = Player();
    currentPlayer.address = i;
    currentPlayer.score = 0;
    while (run) {
        char msg[2];
        memset(msg,0,2);
        read(i, msg, 2);
        cout<<"msg: "<<msg<<endl;
        if (strcmp(msg, "0") == 0) {
            if (players.size() < playersRequired) {
                if (!isInGame(i)) {
                    cout << "Player " << i << " ready" << endl;
                    players.push_back(&currentPlayer);
                }
            } else {
                cout << "Too many players" << endl;
                strcpy(message.msg, "Rejected, game started ");
                message.msgsize = 22;
                write(i, message.msg, static_cast<size_t>(message.msgsize));
            }
        }
        else {
            if (isInGame(i) && game)
                for (unsigned int a = 0; a < word.size(); a++) {
                    string letter;
                    letter = word[a];
                    if (strcmp(msg, letter.c_str()) == 0) {
                        covered[a] = word[a];
                        sendMessage("You guessed!", i);
                        currentPlayer.score+=1;
                        cout<<"current Player score "<<currentPlayer.score<<endl;
                        for(auto &z: players){
                            cout<<"Player "<<z->address<< " "<<z->score<<endl;
                        }
                        guess = true;
                    }
                }
        }
    }
    exit(0);
}

//server shutdown thread
void shutdownRequest() {
    string input;
    while (run) {
        cin >> input;
        if (input == "shutdown") {
            cout << "Shutdown request" << endl;
            run = false;
        }
    }
    exit(0);
}

void getWordList(){
    ifstream file;
    file.open("words.txt");
    int i=0;
    while(!file.eof()){
        file>>words[i];
        i++;
    }
    file.close();
}



int main(int argc, char **argv) {
    uint16_t host = atoi(argv[2]);
    uint16_t network = htons(host);
    sockaddr_in zmienna{};
    zmienna.sin_family = AF_INET;
    zmienna.sin_port = network;
    zmienna.sin_addr.s_addr = inet_addr(argv[1]);
    bind(sockyy, (sockaddr *) &zmienna, sizeof(zmienna));
    srand (time(NULL));

    getWordList();

    Message message{};


    thread shutdownThread(shutdownRequest);
    thread startGame(waitForPlayers);


    while (run) {
        listen(sockyy, 1);
        int i = accept(sockyy, 0, 0);
        clients.push_back(i);
        strcpy(message.msg, "Connected ");
        message.msgsize = 9;
        write(i, message.msg, message.msgsize);
        thread clientServiceThread(clientService, i);
        clientServiceThread.detach();
    }
    shutdownThread.join();
    startGame.join();
    close(sockyy);
}