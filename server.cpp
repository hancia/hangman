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

list<Player*> players;
const int playersRequired = 3;
vector<string> words;
string word;
string covered;
int activePlayers = 0;
int readyTimeout = 100;
int moveTimeout = 100;
int moveWarning = 100;

int socky = socket(PF_INET, SOCK_STREAM, 0);
atomic<bool> game(false);
atomic<bool> run(true);
bool guess = false;
mutex guessMtx;
mutex playersMtx;
mutex newPlayersMtx;
mutex gameStartedMtx;
condition_variable guessCV;
condition_variable playersCV;
condition_variable newPlayersCV;
condition_variable gameStartedCV;

bool isInGame(int player) {
    for (auto &i : players)
        if (i->address == player && i->active)
            return true;
    return false;
}

bool isPlayer(Player *player){
        for (auto &i : players) {
            if (i == player)
                return true;
        }
    return false;
}

void handleReachingError(Player *currentPlayer) {
    if (currentPlayer->active) {
        if (game) {
            currentPlayer->active = false;
            activePlayers--;
            playersCV.notify_all();
            return;
        } else {
            activePlayers--;
            playersCV.notify_all();
            players.remove(currentPlayer);
            return;
        }
    }
}

void sendMessagetoPlayer(string str, Player *player){
    int writeResult = static_cast<int>(write(player->address, str.c_str(), str.size()));
    if(writeResult < 0){
        handleReachingError(player);
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
    string msg, st;
    cout<<"Players "<<players.size()<<endl;
    msg = "Scores ";
    for(auto &i : players){
            if(i->active)
            msg += to_string(i->address) + ": " + to_string(i->score) + " #\n";
    }
    msg.append("$\n");
    sendMessagetoPlayer(msg, &player);
    st = "Hangman ";
    st.append(state[player.fails]);
    st.append("$\n");
    sendMessagetoPlayer(st, &player);
}

void resetPlayer(Player *player){
    player->score = 0;
    player->fails = 0;
    player->active = false;
    player->letters = alphabet;
}


void gameService(){
    int index;
    string msg;
    thread playerNumberHandler(playersLeft);
    playerNumberHandler.detach();
    for(auto &i : players){
        sendMessagetoPlayer("New \n$\n", i);
    }
    while(game && run){
        index = static_cast<int>(rand() % words.size());
        word = words[index];
        words.erase(words.begin()+index);
        cout<<"The word is "<<word<<endl;
        covered="";
        for(unsigned int a = 0; a<word.size(); a++)
            covered += "*";
        while(strcmp(word.c_str(), covered.c_str()) != 0 && game && run) {
            msg = "Word " + covered + " \n$\n";
            for (auto &i : players) {
                showScore(*i);
                sendMessagetoPlayer(msg, i);
            }
            unique_lock<mutex> lock(guessMtx);
            guessCV.wait(lock);
            guess = false;
            guessMtx.unlock();
        }
        string msg2 = "Server The word was " + word;
        msg2.append("\n$\n");
        for(auto &i: players) {
            sendMessagetoPlayer(msg2, i);
        }
        for(auto &i: players){
            i->letters = alphabet;
        }
    }
    for( auto &i : players)
    {
        sendMessagetoPlayer("Server Game over\n$\n", i);
    }
    Player *winner;
    winner = players.front();
    for(auto &i : players){
        if(i->score>winner->score) winner = i;
    }
    list<Player*> winners;
    winners.push_back(winner);
    string msgWinner = "Server You are the winner! The score is "+ to_string(winner->score)+ "\n$\n";
    for(auto &i: players){
        if(i->score==winner->score&&i != winner) {
            winners.push_back(i);
            cout<<"Its a draw!!!"<<endl;
            msgWinner = "Server It's a draw! \n$\n";
        }
    }
    string msgLoser = "Server The winner is ";
    for(auto &i: winners)
        msgLoser+= to_string(i->address)+ " ";
    msgLoser+=". The score is "+ to_string(winner->score) + "\n$\n";
    for(auto &i : players) {
        for (auto &a : winners)
            if (a == i) {
                sendMessagetoPlayer(msgWinner, i);
                break;
            }
        sendMessagetoPlayer(msgLoser, i);
    }
    auto idx = players.begin();
    while(idx != players.end()){
        if ((*idx)->active)
        {
            activePlayers--;
            resetPlayer(*idx);
            idx++;
        }
        else
        {
            cout << "Removing inactive player " <<(*idx)->address<< endl;
            players.erase(idx++);
        }
    }
}


void startNewGame(){
    while(run){
        unique_lock<mutex> lock(newPlayersMtx);
        newPlayersCV.wait(lock, []{return activePlayers>=playersRequired;});
        cout<<"Starting a new game "<<endl;
        game = true;
        gameStartedCV.notify_all();
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

void readyTimeoutHandler(Player *player){
    clock_t start = clock();
    while(!game && run){
        if(!player->active) {
            double time = double(clock() - start) / CLOCKS_PER_SEC;
            if (time >= readyTimeout) {
                cout << "Player " << player->address << " inactive" << endl;
                sendMessagetoPlayer(" Server You have been inactive, you are being removed from queue\n$\n", player);
                players.remove(player);
                break;
            }
        }
        else {
            cout<<"Player "<<player->address<<" active"<<endl;
            break;
        }
    }
}

void moveTimoutHandler(Player *player){
    unique_lock<mutex> lock(gameStartedMtx);
    gameStartedCV.wait(lock);
    clock_t start = clock();
    bool notified = false;
    while(game && run && !player->moved && player->active){
        double time = double(clock() - start) / CLOCKS_PER_SEC;
        if(time >= moveWarning && time < (moveTimeout + moveWarning) && !notified){
            string msg ="Server If you dont move in " + to_string(moveTimeout) +" s you will lose\n$\n";
            sendMessagetoPlayer(msg,player);
            notified = true;
        }
        if (time >= moveTimeout + moveWarning && notified) {
            cout << "Player " << player->address << " inactive" << endl;
            sendMessagetoPlayer("Server You have been inactive, you can't play\n$\n", player);
            player->active = false;
            activePlayers--;
            playersCV.notify_all();
            break;
        }
    }
}

void clientService(int i) {
    Player *currentPlayer = new Player();
    currentPlayer->address = i;
    thread readyTimeoutThread(readyTimeoutHandler, currentPlayer);
    readyTimeoutThread.detach();
    while (run) {
        currentPlayer->moved = false;
        thread moveMonitor(moveTimoutHandler, currentPlayer);
        moveMonitor.detach();
        char m[2];
        memset(m, 0, 2);
        int readBytes = read(i, m, 2);
        if (readBytes <= 0) {
            cout << "Couldn't reach client " << i << endl;
            handleReachingError(currentPlayer);
            return;
        }
        else currentPlayer->moved = true;
        if (strcmp(m, "1") == 0) {
            handleReachingError(currentPlayer);
            cout << "Player " << i << " left" << endl;
            return;
        }
        if (strcmp(m, "0") == 0) {
            if (activePlayers < playersRequired) {
                if (!isPlayer(currentPlayer)) {
                        cout << "Player " << i << " ready" << endl;
                        sendMessagetoPlayer("Server You are in the lobby\n$\n", currentPlayer);
                        currentPlayer->active = true;
                        activePlayers++;
                        players.push_back(currentPlayer);
                        newPlayersCV.notify_all();
                        cout << "Number of active players " << activePlayers << endl;
                }else {
                    if (!currentPlayer->active) {
                        sendMessagetoPlayer("Server You are in the lobby\n$\n", currentPlayer);
                        currentPlayer->active = true;
                        activePlayers++;
                        newPlayersCV.notify_all();
                    }
                }
            } else {
                if(!isInGame(i)) {
                    cout << "Too many players" << endl;
                    string msgRej = "Server Rejected, game started \n$\n";
                    int writeResult = static_cast<int>(write(i, msgRej.c_str(), msgRej.size()));
                    if (writeResult < 0) {
                        return;
                    }
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
                            sendMessagetoPlayer("Server You guessed!\n$\n", currentPlayer);
                            currentPlayer->score += 1;
                        }
                    }
                    if (prevScore == currentPlayer->score) {
                        currentPlayer->fails++;
                        sendMessagetoPlayer("Server Wrong answer :( \n$\n", currentPlayer);
                    }
                    guessMtx.lock();
                    guess = true;
                    guessMtx.unlock();
                    guessCV.notify_one();
                } else {
                    if(currentPlayer->fails < 6)
                        sendMessagetoPlayer("Server You already tried this letter\n$\n", currentPlayer);
                    else{
                        currentPlayer->active = false;
                        activePlayers--;
                        playersCV.notify_all();
                        sendMessagetoPlayer("Server You can't guess, waiting for the game to finish...\n$\n", currentPlayer);
                    }
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

        cout << "Server running" << endl;
        thread shutdownThread(shutdownRequest);
        thread startGame(startNewGame);


        while (run) {
            listen(socky, 1);
            int i = accept(socky, 0, 0);
            string msg = "Id " + to_string(i) +  "\n $\n";
            int writeResult = static_cast<int>(write(i, msg.c_str(), msg.size()));
            if(writeResult < 0){
                cout<<"Couldn't send message to "<<i<<endl;
            }
            else {
                thread clientServiceThread(clientService, i);
                clientServiceThread.detach();
            }
        }
        shutdownThread.join();
        startGame.join();
        close(socky);
    }
}