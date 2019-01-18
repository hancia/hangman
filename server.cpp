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
#include <condition_variable>
#include <mutex>
#include <vector>
#include<ctime>
#include "Hangman.h"
#include "Alphabet.h"

using namespace std;

list<int> clients;
list<Player*> players;
const int playersRequired = 2;
vector<string> words;
string word;
string covered;
int activePlayers = 0;
int readyTimeout = 2;
int moveTimeout = 2;

int socky = socket(PF_INET, SOCK_STREAM, 0);
atomic<bool> game(false);
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

void handleReachingError(Player currentPlayer) {
    if (currentPlayer.active) {
        if (game) {
            currentPlayer.active = false;
            activePlayers--;
            playersCV.notify_all();
        } else {
            players.remove(&currentPlayer);
        }
    }
}

void sendMessagetoPlayer(string str, Player *player){
    int writeResult = static_cast<int>(write(player->address, str.c_str(), str.size()));
    if(writeResult < 0){
        handleReachingError(*player);
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
    msg = "";
    for(auto &i : players){
            msg += to_string(i->address) + ": " + to_string(i->score) + " \n";
    }
    sendMessagetoPlayer(msg, &player);
    sendMessagetoPlayer(state[player.fails], &player);
}

void resetPlayer(Player *player){
    player->score = 0;
    player->fails = 0;
    player->active = false;
    player->letters = alphabet;
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
        for(auto &i: players){
            i->letters = alphabet;
        }
    }
    for( auto &i : players)
    {
        sendMessagetoPlayer("Game over\n", i);
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
    }
    for(auto &i: players)
       resetPlayer(i);
}


void startNewGame(){
    while(run){
        unique_lock<mutex> lock(newPlayersMtx);
        newPlayersCV.wait(lock, []{return activePlayers>=playersRequired;});
        cout<<"Starting a new game "<<endl;
        game = true;
        gameService();
        cout<<"Game ended"<<endl;
        game = false;
    }
    exit(0);
}

bool checkPossibleMove(Player* currentPlayer, char* m) {
    bool possibleMove = false;
    if (currentPlayer->fails < 6) {
        for (unsigned int idx = 0; idx < currentPlayer->letters.size(); idx++) {
            if (*m == currentPlayer->letters[idx]) {
                possibleMove = true;
                currentPlayer->letters.erase(currentPlayer->letters.begin() + idx);
                return possibleMove;
            }
            possibleMove = false;
        }
    }
    return possibleMove;
}

//void readyTimeoutHandler(Player *player){
//    clock_t start = clock();
//    while(!game && run){
//        double time = double(clock()-start)/CLOCKS_PER_SEC;
//        if(time >= readyTimeout){
//            cout<<"Player "<<player->address<<" inactive"<<endl;
//            sendMessagetoPlayer("You have been inactive, you are being removed from queue", player);
//            //sendMessagetoPlayer("Rm", player);
//            players.remove(player);
//            break;
//        }
//    }
//}

void clientService(int i) {
    Player *currentPlayer = new Player();
    currentPlayer->address = i;
//    thread readyTimeoutThread(readyTimeoutHandler, currentPlayer);
//    readyTimeoutThread.detach();
    while (run) {
        char m[2];
        memset(m, 0, 2);
        int readBytes = read(i, m, 2);
        if (readBytes <= 0) {
            cout << "Couldn't reach client " << i << endl;
            handleReachingError(*currentPlayer);
            return;
        }
        if (strcmp(m, "1") == 0) {
            handleReachingError(*currentPlayer);
            cout << "Player " << i << " left" << endl;
            return;
        }
        if (strcmp(m, "0") == 0) {
            if (activePlayers < playersRequired) {
                if (!isInGame(i)) {
                    cout << "Player " << i << " ready" << endl;
                    currentPlayer->active = true;
                    activePlayers++;
                    players.push_back(currentPlayer);
                    newPlayersCV.notify_one();
                    cout << "No of players " << players.size() << endl;
                }
            } else {
                cout << "Too many players" << endl;
                string msgRej = "Rejected, game started ";
                int writeResult = static_cast<int>(write(i, msgRej.c_str(), msgRej.size()));
                if (writeResult < 0) {
                    return;
                }
            }
        } else if (currentPlayer->active) {
            if (isInGame(i) && game) {
                bool possibleMove = checkPossibleMove(currentPlayer, m);
                if (possibleMove) {
                    int prevScore = currentPlayer->score;
                    for (unsigned int a = 0; a < word.size(); a++) {
                        if (*m == word[a]) {
                            covered[a] = word[a];
                            sendMessagetoPlayer("You guessed!", currentPlayer);
                            currentPlayer->score += 1;
                            cout << "current Player score " << currentPlayer->score << endl;
                            for (auto &z: players) {
                                cout << "Player " << z->address << " " << z->score << endl;
                            }
                        }
                    }
                    if (prevScore == currentPlayer->score) currentPlayer->fails++;
                    guessMtx.lock();
                    guess = true;
                    guessMtx.unlock();
                    guessCV.notify_one();
                } else {
                    sendMessagetoPlayer("You already tried this letter\n", currentPlayer);
                }
            } else {
                currentPlayer->active = false;
                activePlayers--;
                playersCV.notify_one();
                sendMessagetoPlayer("You can't guess, waiting for the game to finish...", currentPlayer);
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