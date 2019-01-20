//
// Created by hania on 04.01.19.
//

#ifndef HANGMAN_PLAYER_H
#define HANGMAN_PLAYER_H
#include <vector>

class Player {
public:
    int address;
    int score;
    int fails;
    bool active;
    bool moved;
    std::vector<char> letters;
    Player(){
        this->active = false;
        this->score = 0;
        this->fails = 0;
        this->moved = false;
        this ->letters = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','r','s','t','q','u','w','v','x','y','z'};
    }
};


#endif //HANGMAN_PLAYER_H
