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
#include "Player.h"
#include <stdlib.h>
#include <time.h>
#include <atomic>
#include <vector>
#include "Hangman.h"

using namespace std;


list<int> clients;
list<Player*> players;
const int noOfWords = 100;
const int playersRequired = 2;
//string words[noOfWords];
vector<string> words;
string word;
string covered;
int activePlayers = 0;

int socky = socket(PF_INET, SOCK_STREAM, 0);
bool game = false;
atomic<bool> run(true);
bool guess = false;


bool isInGame(int player) {
    for (auto &i : players)
        if (i->address == player)
            return true;
    return false;
}

void sendMessage(string str, int address){
    char msg[100];
    int msgsize;
    strcpy(msg, str.c_str());
    msgsize = str.size();
    write(address, msg, static_cast<size_t>(msgsize));
}

void waitForAGuess(){
    while(!guess && run){
        if(activePlayers<=1) {
            game = false;
            break;
        }
    }
}

void showScore(Player player){
    string msg;
    msg = "Scores: ";
    write(player.address, msg.c_str(), msg.size());
    for(auto &i : players){
        msg = to_string(i->address) + ": " + to_string(i->score) + " \n";
        write(player.address, msg.c_str(), msg.size());
    }
    write(player.address, state[player.fails].c_str(), state[player.fails].size());
}

void resetPlayer(Player *player){
    player->score = 0;
    player->fails = 0;
    player->active = false;
}


void gameService(){
    int index;
    char msg[100];
    int msgsize;
    while(game && run){
        index = static_cast<int>(rand() % words.size());
        word = words[index];
        words.erase(words.begin()+index);
        cout<<"The word is "<<word<<endl;
        covered="";
        for(unsigned int a = 0; a<word.size(); a++)
            covered += "*";
        msgsize = static_cast<int>(word.size());
        while(strcmp(word.c_str(), covered.c_str()) != 0 && game && run) {
            strcpy(msg, covered.c_str());
            cout<<"Covered in a game service "<<covered<<endl;
            for (auto &i : players) {
                showScore(*i);
                write(i->address, msg, static_cast<size_t>(msgsize));
            }
            waitForAGuess();
            guess = false;
        }
        string msg = "The word is guessed, it was " + word;
        for(auto &i: players) {
            write(i->address, msg.c_str(), msg.size());
        }
    }
    Player *winner;
    winner = players.front();
    for(auto &i : players){
        if(i->score>winner->score) winner = i;
    }
    list<Player*> winners;
    winners.push_back(winner);
    for(auto &i: players){
        if(i->score==winner->score&&i != winner) {
            winners.push_back(i);
            cout<<"Its a draw!!!"<<endl;
        }
    }
    string msgWinner = "You are the winner! The score is "+ to_string(winner->score);
    string msgLoser = "The winner is ";
    for(auto &i: winners)
        msgLoser+= to_string(i->address);
    msgLoser+=". The score is "+ to_string(winner->score);
    for(auto &i : players) {
        for (auto &a : winners)
            if (a == i) {
                write(i->address, msgWinner.c_str(), msgWinner.size());
                break;
            }
        write(i->address, msgLoser.c_str(), msgLoser.size());
        resetPlayer(i);
    }
    while(players.size()!=0)
        players.pop_back();
}


void waitForPlayers(){
    while(run){
        if(players.size() >= playersRequired){
            cout<<"Starting a new game "<<endl;
            game = true;
            thread gameThread(gameService);
            cout<<"Thread join "<<endl;
            gameThread.join();
            cout<<"Game ended"<<endl;
            game = false;
        }
    }
    exit(0);
}


void clientService(int i) {
    char msg[100];
    Player currentPlayer = Player();
    currentPlayer.address = i;
    currentPlayer.score = 0;
    currentPlayer.fails = 0;
    currentPlayer.active = false;
    while (run) {
        char m[2];
        memset(m, 0, 2);
        read(i, m, 2);
        if (strcmp(m, "1") == 0) {
            return;
        }
        if (strcmp(m, "0") == 0) {
            if (players.size() < playersRequired) {
                if (!isInGame(i)) {
                    cout << "Player " << i << " ready" << endl;
                    currentPlayer.active = true;
                    activePlayers++;
                    players.push_back(&currentPlayer);
                    cout<<"No of players "<<players.size()<<endl;
                }
            } else {
                cout << "Too many players" << endl;
                strcpy(msg, "Rejected, game started ");
                write(i, msg, sizeof(msg));
            }
        } else if (currentPlayer.active) {
            if (isInGame(i) && game) {
                if (currentPlayer.fails < 6) {
                    int prevScore = currentPlayer.score;
                    for (unsigned int a = 0; a < word.size(); a++) {
                        string letter;
                        letter = word[a];
                        if (m == letter) {
                            covered[a] = word[a];
                            sendMessage("You guessed!", i);
                            currentPlayer.score += 1;
                            cout << "current Player score " << currentPlayer.score << endl;
                            for (auto &z: players) {
                                cout << "Player " << z->address << " " << z->score << endl;
                            }
                        }
                    }
                    if (prevScore == currentPlayer.score) currentPlayer.fails++;
                    guess = true;
                } else {
                    currentPlayer.active = false;
                    activePlayers--;
                    write(i, "You can't guess, waiting for the game to finish...", 50);
                }
            }
        }
    }
}


void shutdownRequest() {
    string input;
    while (run) {
        cin >> input;
        if (input == "shutdown") {
            cout << "Shutdown request" << endl;
            run = false;
        }
    }
}

void getWordList() {
    ifstream file;
    string word;
    file.open("./words.txt");
    if (file.is_open()) {
        while (!file.eof()) {
            file >> word;
            words.push_back(word);
        }
        file.close();
    }
     else cout<<"Cant open file"<<endl;
}



int main(int argc, char **argv) {
    uint16_t host = atoi(argv[2]);
    uint16_t network = htons(host);
    sockaddr_in zmienna{};
    zmienna.sin_family = AF_INET;
    zmienna.sin_port = network;
    zmienna.sin_addr.s_addr = inet_addr(argv[1]);
    int bindResult = bind(socky, (sockaddr *) &zmienna, sizeof(zmienna));

    if (bindResult < 0) {
        cout<<"Error while creating an ip connection"<<endl;
        exit(1);
    }
    else {
        srand(time(NULL));

        getWordList();

        char msg[100];
        int msgsize;

        cout << "Server running" << endl;
        thread shutdownThread(shutdownRequest);
        thread startGame(waitForPlayers);


        while (run) {
            listen(socky, 1);
            int i = accept(socky, 0, 0);
            clients.push_back(i);
            strcpy(msg, "Connected ");
            msgsize = 9;
            write(i, msg, msgsize);
            thread clientServiceThread(clientService, i);
            clientServiceThread.detach();
        }
        shutdownThread.join();
        startGame.join();
        close(socky);
    }
}