#pragma once

void authorize(const char *id, const char *pass);

void gameStart();

void gameOver();

void roundStart(int round);

void oneRound();

void roundOver(int round);

int observe();

void putDown(int row, int col);

void noStep();

void step();

void saveChessBoard();

int NegaMax(int depth,int alpha,int beta);

int value();

int cal(int begin_row, int end_row, int begin_col, int end_col, int direction);