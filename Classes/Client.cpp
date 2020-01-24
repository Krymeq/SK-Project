//
// Created by dejmian on 05.01.2020.
//

#include <sys/epoll.h>
#include <Utils/utils.h>
#include <cstring>
#include "Client.h"
#include "Server.h"
#include "Game.h"

Client::Client(std::string _login, int _fd) {
    fd = _fd;
    login = _login;
    epoll_event ee{EPOLLIN | EPOLLRDHUP, {.ptr=this}};
    epoll_ctl(Client::epollFd, EPOLL_CTL_ADD, _fd, &ee);
}

Client::~Client() {
    Server::deleteClientFromMap(login);
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

void Client::handleEvent(uint32_t events) {
    int roundValue = 0;
    std::string message;
//    todo nie wiem czy to dobrze ze w tej kolejnosci
    if (events & ~EPOLLIN) {
        error(0, errno, "Event %#04x on client socket %d with login: '%s'. Disconnect client ...", events, fd,
              login.c_str());
        delete (this);
    } else if (events & EPOLLIN) {
        char buffer[BUFFER_SIZE];
        int bytes = readData(fd, buffer, &roundValue);
//        Pass only if there is data in buffer and the game already has stared
        if (bytes > 0 && Game::getRound() != 0) {
            if (!isCorrectRound(Game::getRound(), roundValue)) {
                error(0, errno, "Aborting invalid message from client '%s' with fd %d !\n", login.c_str(), fd);
            } else {
//                BECAUSE OF ROUND NUMBER - DO NOT NEED TO PLACE MUTEX HERE
                message = std::string(buffer);
                message = message.substr(0, bytes);
                printf("--------------------- Received message from Client '%s': '%s' \n", login.c_str(), message.c_str());
//                Assign last answers and default (0) points for them
                lastAnswers = extractPhrase(message);
                lastScore.insert(lastScore.end(), lastAnswers.size(), 0);
                inactiveRoundsNumber = 0;

                Game::pushClientToTimeRankingWhenPossible(this);
            }
        }
    }
}


void Client::sendAnswersAndPoints() {
    char messageBuffer[BUFFER_SIZE];
    std::string tempString = "Total points:" + std::to_string(totalScore) + ";\t";
    for (int i = 0; i < (int) lastAnswers.size(); ++i) {
        tempString += " Answer: '" + lastAnswers[i] + "' points: "+ std::to_string(lastScore[i]) + ";\t";
    }
    strcpy(messageBuffer, tempString.c_str());
//    Must be Game::getRound()-1 because round number is increasing after sleep and before new round beginning
    writeData(fd, messageBuffer, Game::getRound()-1);
}

// Getters and setters
std::basic_string<char> Client::getLogin() {
    return login;
}

void Client::setLogin(std::string &login) {
    Client::login = login;
}

float Client::getScore() {
    return totalScore;
}

void Client::setScore(float score) {
    Client::totalScore = score;
}

void Client::recalculateTotalScore() {
    for(float value: lastScore){
        totalScore += value;
    }
}
