#include "Nova.h"

#include <iostream>

int main(int argc, char** argv){
    logger.Info("Nova");
    logger.Error("bonjour {0}", 10);
    int argc2;
    std::cin >> argc2;
    return (0);
}