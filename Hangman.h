//
// Created by hania on 05.01.19.
//

#ifndef HANGMAN_HANGMAN_H
#define HANGMAN_HANGMAN_H
#include <string>
using namespace std;
string state[7]={"\n"
                 "  +---+\n"
                 "  |   |\n"
                 "      |\n"
                 "      |\n"
                 "      |\n"
                 "      |\n"
                 "=========",
                 "\n"
                 "  +---+\n"
                 "  |   |\n"
                 "  O   |\n"
                 "      |\n"
                 "      |\n"
                 "      |\n"
                 "=========",
                 "\n"
                 "  +---+\n"
                 "  |   |\n"
                 "  O   |\n"
                 "  |   |\n"
                 "      |\n"
                 "      |\n"
                 "=========",
                 "\n"
                 "  +---+\n"
                 "  |   |\n"
                 "  O   |\n"
                 " /|   |\n"
                 "      |\n"
                 "      |\n"
                 "=========",
                 "\n"
                 "  +---+\n"
                 "  |   |\n"
                 "  O   |\n"
                 " /|\\  |\n"
                 "      |\n"
                 "      |\n"
                 "=========",
                 "\n"
                 "  +---+\n"
                 "  |   |\n"
                 "  O   |\n"
                 " /|\\  |\n"
                 " /    |\n"
                 "      |\n"
                 "=========",
                 "\n"
                 "  +---+\n"
                 "  |   |\n"
                 "  O   |\n"
                 " /|\\  |\n"
                 " / \\  |\n"
                 "      |\n"
                 "========="};

#endif //HANGMAN_HANGMAN_H
