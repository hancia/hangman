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
#include <condition_variable>
#include <mutex>
#include "Hangman.h"

using namespace std;


list<int> clients;
list<Player*> players;
const int playersRequired = 2;
vector<string> words;
string word;
string covered;
int activePlayers = 0;

int socky = socket(PF_INET, SOCK_STREAM, 0);
bool game = false;
atomic<bool> run(true);
bool guess = false;
mutex guessMtx;
mutex playersMtx;
mutex newPlayersMtx;
condition_variable guessCV;
condition_variable playersCV;
condition_variable newPlayersCV;

bool isInGame(int player) {
    for (auto &i : players)
        if (i->address == player)
            return true;
    return false;
}

void sendMessagetoPlayer(string str, Player *player){
    int writeResult = static_cast<int>(write(player->address, str.c_str(), str.size()));
    if(writeResult < 0){
        if(player->active) {
            player->active = false;
            activePlayers--;
        }
    }
}

void playersLeft(){
    unique_lock<mutex> lock(playersMtx);
    playersCV.wait(lock, []{return activePlayers <= 1;});
    cout<<"Ending game..."<<endl;
    guessCV.notify_all();
    game = false;
}

void showScore(Player player){
    string msg;
    msg = "Scores: ";
    write(player.address, msg.c_str(), msg.size());
    for(auto &i : players){
        msg = to_string(i->address) + ": " + to_string(i->score) + " \n";
        sendMessagetoPlayer(msg, i);
    }
    sendMessagetoPlayer(state[player.fails], &player);
}

void resetPlayer(Player *player){
    player->score = 0;
    player->fails = 0;
    player->active = false;
}


void gameService(){
    int index;
    char msg[100];
    thread playerNumberHandler(playersLeft);
    playerNumberHandler.detach();
    while(game && run){
        index = static_cast<int>(rand() % words.size());
        word = words[index];
        words.erase(words.begin()+index);
        cout<<"The word is "<<word<<endl;
        covered="";
        for(unsigned int a = 0; a<word.size(); a++)
            covered += "*";
        while(strcmp(word.c_str(), covered.c_str()) != 0 && game && run) {
            strcpy(msg, covered.c_str());
            for (auto &i : players) {
                showScore(*i);
                sendMessagetoPlayer(msg, i);
            }
            unique_lock<mutex> lock(guessMtx);
            guessCV.wait(lock);
            guess = false;
            guessMtx.unlock();
        }
        string msg = "The word is guessed, it was " + word;
        for(auto &i: players) {
            sendMessagetoPlayer(msg, i);
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
        msgLoser+= to_string(i->address)+ " ";
    msgLoser+=". The score is "+ to_string(winner->score);
    for(auto &i : players) {
        for (auto &a : winners)
            if (a == i) {
                sendMessagetoPlayer(msgWinner, i);
                break;
            }
        sendMessagetoPlayer(msgLoser, i);
        resetPlayer(i);
    }
    while(players.size()!=0)
        players.pop_back();
}


void startNewGame(){
    while(run){
        unique_lock<mutex> lock(newPlayersMtx);
        newPlayersCV.wait(lock, []{return players.size()>=playersRequired;});
        cout<<"Starting a new game "<<endl;
        game = true;
        gameService();
        cout<<"Game ended"<<endl;
        game = false;
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
        int readBytes = read(i, m, 2);
        if(readBytes < 0){
            cout<<"Couldn't reach client "<<i<<endl;
            if(currentPlayer.active){
                currentPlayer.active = false;
                activePlayers--;
            }
        }
        if (strcmp(m, "1") == 0) {
            if(currentPlayer.active){
                currentPlayer.active = false;
                activePlayers--;
            }
            playersCV.notify_one();
            return;
        }
        if (strcmp(m, "0") == 0) {
            if (players.size() < playersRequired) {
                if (!isInGame(i)) {
                    cout << "Player " << i << " ready" << endl;
                    currentPlayer.active = true;
                    activePlayers++;
                    players.push_back(&currentPlayer);
                    newPlayersCV.notify_one();
                    cout<<"No of players "<<players.size()<<endl;
                }
            } else {
                cout << "Too many players" << endl;
                strcpy(msg, "Rejected, game started ");
                int writeResult = static_cast<int>(write(i, msg, sizeof(msg)));
                if(writeResult < 0) {
                    return;
                }
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
                            sendMessagetoPlayer("You guessed!", &currentPlayer);
                            currentPlayer.score += 1;
                            cout << "current Player score " << currentPlayer.score << endl;
                            for (auto &z: players) {
                                cout << "Player " << z->address << " " << z->score << endl;
                            }
                        }
                    }
                    if (prevScore == currentPlayer.score) currentPlayer.fails++;
                    guessMtx.lock();
                    guess = true;
                    guessMtx.unlock();
                    guessCV.notify_one();
                } else {
                    currentPlayer.active = false;
                    activePlayers--;
                    playersCV.notify_one();
                    sendMessagetoPlayer("You can't guess, waiting for the game to finish...", &currentPlayer);
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

        cout << "Server running" << endl;
        thread shutdownThread(shutdownRequest);
        thread startGame(startNewGame);


        while (run) {
            listen(socky, 1);
            int i = accept(socky, 0, 0);
            clients.push_back(i);
            string msg = "Connect";
            int writeResult = static_cast<int>(write(i, msg.c_str(), msg.size()));
            if(writeResult < 0){
                cout<<"Couldn't send message to "<<i<<endl;
            }
            else {
                cout<<"Entering client thread" <<endl;
                thread clientServiceThread(clientService, i);
                clientServiceThread.detach();
            }
        }
        shutdownThread.join();
        startGame.join();
        close(socky);
    }
}