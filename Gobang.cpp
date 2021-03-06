#include "Define.h"
#include "Square.h"
#include "ClientSocket.h"
#include "Gobang.h"
#include <windows.h>
#include <iostream>
#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <cmath>
#include <time.h>
#include <fstream>
using namespace std;

int owndr=-1;
int owndc=-1;
int oppdr = -1;
int oppdc = -1;

#define random(x) (rand()%x)
#define ROWS 15
#define COLS 15
#define ROUNDS 2
const int maxdepth = 4;

#define VERTICAL 0
#define HORIZONTAL 1
#define LEFT 2
#define RIGHT 3


#define M1 35
#define M2 800
#define M3 15000
#define M4 800000
#define M5 10000000

#define O1 20
#define O2 500
#define O3 4000
#define O4 300000
#define O5 8000000

#define VO 7
#define PU 0
#define NO 0

#define NOTFIND 101010101  //哈希表查询没有命中的无效标志

enum ENTRY_TYPE{ exact, lower_bound, upper_bound };

int debug = 0;

typedef struct HASHITEM{
	LONGLONG checksum;
	ENTRY_TYPE entry_type;
	short depth;
	short eval;
}HashItem;

typedef struct MOVE{
	int r;
	int c;
	int score;
}chessmove;
chessmove moveList[10][225];

LONGLONG HashKey64[2][15][15];
LONG HashKey32[2][15][15];

LONGLONG HashValue64;
LONG HashValue32;

HashItem *pTT[2];//置换表头指针

Square board[ROWS][COLS];
int steps = 0;//step 函数被调用的次数（表示多少回合，用于标识是否需要拿掉棋子）
int ownColor = -1, oppositeColor = -1;
char lastmsg[16] = { "" };
int bestrow = -1;
int bestcol = -1;
int movecount;
chessmove TargetBuff[100]; // 归并排序的缓冲队列
int HistoryTable[225]; // 历史得分表

chessmove ownMove[36];//own的落子队列（0不使用）
chessmove oppMove[36];//opp的落子队列（0不使用）

int ownCount=0;//own的落子队列长度
int oppCount=0;//opp的落子队列长度

int ownDis=0;//own已经消失了几个子
int oppDis=0;//opp已经消失了几个子

LONGLONG rand64(void);
LONG rand32(void);

void InitializeHashKey();
void calculateHaskKey32();
void hashMove(int mover, int movec);
int lookUpHashTable(int alpha, int beta, int depth);
void enterHashTable(ENTRY_TYPE entry_type, short eval, short depth);
void resetHistoryTable();
int getHistoryScore(int mover, int movec);
void enterHistoryScore(int mover, int movec, int depth);

int AddMove(int x, int y, int ply)
{
	moveList[ply][movecount].r = x;
	moveList[ply][movecount].c = y;
	movecount++;
	moveList[ply][movecount].score = (30 - abs(7 - x) - abs(7 - y));
	return movecount;
}

int ownNumber()//计算棋盘上目前有多少个自己的子来判断自己的子需不需要消失
{
	int n = 0;
	for (int i = 0; i < 15; i++)
	{
		for (int j = 0; j < 15; j++)
		{
			if (board[i][j].color == ownColor)
			{
				n++;
			}
		}
	}
	return n;
}

int oppNumber()//计算棋盘上目前有多少个自己对方的子来判断对方的子需不需要消失
{
	int n = 0;
	for (int i = 0; i < 15; i++)
	{
		for (int j = 0; j < 15; j++)
		{
			if (board[i][j].color == oppositeColor)
			{
				n++;
			}
		}
	}
	return n;
}

/*
void Merge(chessmove *source, chessmove *target, int l, int m, int r);
void Merge_A(chessmove * source, chessmove* target, int l, int m, int r);
void MergePass(chessmove* source, chessmove* target, const int s, const int n, const BOOL direction);
void MergeSort(chessmove* source, int n, BOOL direction);
*/

void Mergearray(chessmove a[], int first, int mid, int last, chessmove temp[]);
void mergesort(chessmove a[], int first, int last, chessmove temp[]);
//排序
bool MergeSort(chessmove a[], int n);
int createMove(int ply, int side, int minrow,int maxrow,int mincol,int maxcol)
{
	int i, j;
	movecount = 0;
	for (i = minrow; i <= maxrow; i++)
	{
		for (j = mincol; j <= maxcol; j++)
		{
			if (board[i][j].empty)
			{
				AddMove(i, j, ply);
			}
		}
	}
	MergeSort(moveList[ply], movecount);
	return movecount;
}
//////////////////////////////////////////////////////////////////
//以下函数直到noStep()为客户端中不需要实现的函数
//////////////////////////////////////////////////////////////////
void authorize(const char *id, const char *pass) {
	connectServer();
	std::cout << "Authorize " << id << std::endl;
	char msgBuf[BUFSIZE];
	memset(msgBuf, 0, BUFSIZE);
	msgBuf[0] = 'A';
	memcpy(&msgBuf[1], id, 9);
	memcpy(&msgBuf[10], pass, 6);
	int rtn = sendMsg(msgBuf);
	// printf("Authorize Return %d\n", rtn);
	if (rtn != 0) printf("Authorized Failed\n");
}

void gameStart() {
	// char id[12], passwd[10];
	// std::cout << "ID: " << std::endl;
	// std::cin >> id;
	// std::cout << "PASSWD: " << std::endl;
	// std::cin >> passwd;
	authorize(ID, PASSWORD);
	InitializeHashKey();
	std::cout << "Game Start" << std::endl;
	for (int round = 0; round < ROUNDS; round++) {
		roundStart(round);
		oneRound();
		roundOver(round);
	}
	gameOver();
	close();
}

void gameOver() {
	std::cout << "Game Over" << std::endl;
}

void roundStart(int round) {
	resetHistoryTable();
	ownCount = 0;
	oppCount = 0;
	ownDis = 0;
	oppDis = 0;
	std::cout << "Round " << round << " Ready Start" << std::endl;
	for (int r = 0; r < ROWS; r++) {
		for (int c = 0; c < COLS; c++) {
			board[r][c].reset();
		}
	}
	memset(lastmsg, 0, sizeof(lastmsg));
	int rtn = recvMsg();
	if (rtn != 0) return;
	if (strlen(recvBuf) < 2)
		printf("Authorize Failed\n");
	else
		printf("Round start received msg %s\n", recvBuf);
	switch (recvBuf[1]) {
	case 'B':
		ownColor = 0;
		oppositeColor = 1;
		rtn = sendMsg("BB");
		if (rtn != 0) return;
		break;
	case 'W':
		ownColor = 1;
		oppositeColor = 0;
		rtn = sendMsg("BW");
		std::cout << "Send BW" << rtn << std::endl;
		if (rtn != 0) return;
		break;
	default:
		printf("Authorized Failed\n");
		break;
	}
}

void oneRound() {
	int DIS_FREQ = 5, STEP = 1;
	switch (ownColor) {
	case 0:
		while (STEP < 10000) {

			if (STEP != 1 && (STEP - 1) % DIS_FREQ == 0) {
				int ret = observe();       // self disappeared
				if (ret >= 1) break;
				else if (ret != -8) {
					std::cout << "ERROR: Not Self(BLACK) Disappeared" << std::endl;
				}
			}
			step();                        // take action, send message

			if (observe() >= 1) break;     // receive RET Code
			// saveChessBoard();
			if (STEP != 1 && (STEP - 1) % DIS_FREQ == 0) {
				int ret = observe();       // see white disappear
				if (ret >= 1) break;
				else if (ret != -9) {
					std::cout << ret << " ERROR: Not White Disappeared" << std::endl;
				}
			}

			if (observe() >= 1) break;    // see white move
			STEP++;
			// saveChessBoard();
		}
		printf("One Round End\n");
		break;
	case 1:
		while (STEP < 10000) {

			if (STEP != 1 && (STEP - 1) % DIS_FREQ == 0) {
				int ret = observe();       // black disappeared
				if (ret >= 1) break;
				else if (ret != -8) {
					std::cout << "ERROR: Not Black Disappeared" << std::endl;
				}
			}
			if (observe() >= 1) break;    // see black move

			if (STEP != 1 && (STEP - 1) % DIS_FREQ == 0) {
				int ret = observe();      // self disappeared
				if (ret >= 1) break;
				else if (ret != -9) {
					std::cout << "ERROR: Not Self Disappeared" << std::endl;
				}
			}

			step();                        // take action, send message
			if (observe() >= 1) break;     // receive RET Code
			// saveChessBoard();
			STEP++;
		}
		printf("One Round End\n");
		break;
	default:
		break;
	}
}

void roundOver(int round) {
	std::cout << "Round " << round << " Over" << std::endl;
	for (int r = 0; r < ROWS; r++) {
		for (int c = 0; c < COLS; c++) {
			board[r][c].reset();
		}
	}
	ownColor = oppositeColor = -1;
	oppCount = 0;
	ownCount = 0;
	oppDis = 0;
	ownDis = 0;
}

void lastMsg() {
	printf(lastmsg);
	puts("");
}

int observe() {
	int rtn = 0;
	int recvrtn = recvMsg();
	if (recvrtn != 0) return 1;
	printf("receive msg %s\n", recvBuf);
	switch (recvBuf[0]) {
	case 'R':   // return messages
	{
					switch (recvBuf[1]) {
					case '0':    // valid step
						switch (recvBuf[2]) {
						case 'P':   // update chessboard
						{
										int desRow = (recvBuf[3] - '0') * 10 + recvBuf[4] - '0';
										int desCol = (recvBuf[5] - '0') * 10 + recvBuf[6] - '0';
										board[desRow][desCol].color = recvBuf[7] - '0';
										board[desRow][desCol].empty = false;

										if (recvBuf[7] - '0' == ownColor)//是自己的子
										{
											ownCount++;
											ownMove[ownCount].r = desRow;
											ownMove[ownCount].c = desCol;
											
										}
										else if (recvBuf[7] - '0' == oppositeColor) //是对方的子 
										{
											oppCount++;
											oppMove[oppCount].r = desRow;
											oppMove[oppCount].c = desCol;
										}
										memcpy(lastmsg, recvBuf, strlen(recvBuf));
										break;
						}
						case 'D':   // Disappeared
						{
										int desRow = (recvBuf[3] - '0') * 10 + recvBuf[4] - '0';
										int desCol = (recvBuf[5] - '0') * 10 + recvBuf[6] - '0';
										board[desRow][desCol].color = -1;
										board[desRow][desCol].empty = true;
										cout << "dissssssssssssssss"<<endl;
										if (recvBuf[7] - '0' == 0)  // black disappear
										{
											rtn = -8;
										}
										else
										{
											rtn = -9;
										}
										if (recvBuf[7] - '0' == ownColor)//自己的子消失
										{
											ownDis++;
											cout << "owndisdddd" << endl;
										}
										else if (recvBuf[7] - '0' == oppositeColor)
										{
											oppDis++;
											cout << "oppdisdddd" << endl;
										}
										memcpy(lastmsg, recvBuf, strlen(recvBuf));
										break;
						}
						case 'N':   // R0N: enemy wrong step
						{
										break;
						}
						}
						break;
					case '1':
						std::cout << "Error -1: Msg format error\n";
						rtn = -1;
						break;
					case '2':
						std::cout << "Error -2: Coordinate error\n";
						rtn = -2;
						break;
					case '4':
						std::cout << "Error -4: Invalid step\n";
						rtn = -4;
						break;
					default:
						std::cout << "Error -5: Other error\n";
						rtn = -5;
						break;
					}
					break;
	}
	case 'E':
	{
				switch (recvBuf[1]) {
				case '0':
					// game over
					rtn = 2;
					break;
				case '1':
					// round over
					rtn = 1;
				default:
					break;
				}
				break;
	}
	}
	return rtn;
}

void putDown(int row, int col) {
	char msg[6];
	memset(msg, 0, sizeof(msg));
	msg[0] = 'S';
	msg[1] = 'P';
	msg[2] = '0' + row / 10;
	msg[3] = '0' + row % 10;
	msg[4] = '0' + col / 10;
	msg[5] = '0' + col % 10;
	board[row][col].color = ownColor;
	board[row][col].empty = false;
	lastMsg();
	printf("put down (%c%c, %c%c)\n", msg[2], msg[3], msg[4], msg[5]);
	sendMsg(msg);
}

void noStep() {
	sendMsg("SN");
	printf("send msg %s\n", "SN");
}
///////////////////////////////////////////////////////////////////
//以上函数不需要实现
///////////////////////////////////////////////////////////////////

void saveChessBoard()
{
	ofstream file;
	file.open("..\\chess.txt", ios::out | ios::app);
	if (!file.fail())
	{
		for (int i = 0; i < 15; i++)
		{
			for (int j = 0; j < 15; j++)
			{
				if (board[i][j].empty)
				{
					file << " ";
				}
				else
				{
					if (board[i][j].color == 0)
					{
						file << "@";
					}
					else
					{
						file << "O";
					}
				}
			}
			file << endl;
		}
	}
	file << "-----------------------------------------" << endl;


}

//每一步
void step() {
	steps++;
	bestrow = -1;
	bestcol = -1;

	NegaMax(maxdepth, -infinite, infinite);
	if (bestrow == -1 || bestcol == -1)
	{
		std::cout << "nomove" << std::endl;
		exit(1);
	}
	if (!board[bestrow][bestcol].empty)
	{
		std::cout << "wrong step" << std::endl;
		std::cout << bestrow << " " << bestcol << std::endl;
		exit(1);
	}
	putDown(bestrow, bestcol);
	saveChessBoard();
}

int isGameOver(int depth)//判断是否游戏结束，若自己胜返回infinite-1，若输了返回-infinite+1
{
	int v = 0;
	int s = (maxdepth - depth + 1) % 2;
	for (int i = 0; i < 15; ++i)
	{
		for (int j = 0; j < 15; ++j)
		{
			//计算垂直方向的五元组
			int w = cal(i, i + 4, j, j, VERTICAL);
			if (w == M5)
			{
				return infinite;
			}
			else if (w==O5)
			{
				return -infinite;
			}
			//计算水平方向的五元组
			w = cal(i, i, j, j + 4, HORIZONTAL);
			if (w == M5)
			{
				return infinite;
			}
			else if (w == O5)
			{
				return -infinite;
			}
			//计算左上到右下的五元组
			w = cal(i, i + 4, j, j + 4, LEFT);
			if (w == M5)
			{
				return infinite;
			}
			else if (w == O5)
			{
				return -infinite;
			}
			//计算右上到左下的五元组
			w = cal(i - 4, i, j, j + 4, RIGHT);
			if (w == M5)
			{
				return infinite;
			}
			else if (w == O5)
			{
				return -infinite;
			}
		}
	}
	return 0;//若没有结束返回0；
}

//负极大值搜索引擎
int NegaMax(int depth, int alpha, int beta)
{
	int score;
	int Count = 0, i;
	BYTE type;
	int side = (maxdepth - depth) % 2;
	int current = -infinite;
	if (depth > 0)
	{
		int s;
		s = isGameOver(depth);
		if (s != 0)
		{
			if (side == 0)
			{
				return s;
			}
			else
			{
				return -s;
			}
		}
	}
	if (depth <= 0)
	{
		if (side == 0)
		{
			return value();
		}
		else
		{
			return -value();
		}
	}
	
	int owndised = 0;
	int oppdised = 0;
	int discolor = -1;
	int disrow = -1;
	int discol = -1;
	if (side == 0 && depth != maxdepth) //轮到自己下子且不是在模拟的第一步（模拟的第一步服务器已经消了子)
	{
		if ((ownNumber()+ownDis) % 5 == 0 && ownNumber()!=0)
		{
			owndised = 1;
			ownDis++;
			disrow = ownMove[ownDis].r;
			discol = ownMove[ownDis].c;
			discolor = ownColor;
			board[disrow][discol].reset();
			if (disrow != owndr||discol != owndc)
			{
				cout << ownDis << " " << ownNumber() << endl;
				cout << "owndis" << disrow << " " << discol <<" depth:"<<depth<< endl;
				owndr = disrow;
				owndc = discol;
			}
		}
	}
	if (side == 1)
	{
		if ((oppDis+oppNumber()) % 5 == 0 && oppNumber()!=0)
		{
			oppdised = 1;
			oppDis++;
			disrow = oppMove[oppDis].r;
			discol = oppMove[oppDis].c;
			discolor = oppositeColor;
			board[disrow][discol].reset();
			if (disrow != oppdr||discol != oppdc)
			{
				cout << oppMove[0].r << " " << oppMove[0].c << " " << oppMove[1].r << " " << oppMove[1].c << endl;
				cout << oppDis << " " << oppNumber() << endl;
				cout << "oppdis" << disrow << " " << discol <<" depth:"<<depth<< endl;
				oppdr = disrow;
				oppdc = discol;
			}
		}
	}
	
	int minrow = 15;
	int mincol = 15;
	int maxrow = -1;
	int maxcol = -1;
	int changed = 0;
	for (int i = 0; i < 15; i++)
	{
		for (int j = 0; j < 15; j++)
		{
			if (board[i][j].empty == false)
			{
				changed = 1;
				if (i>maxrow)
					maxrow = i;
				if (j>maxcol)
					maxcol = j;
				if (i < minrow)
					minrow = i;
				if (j < mincol)
					mincol = j;
			}
		}
	}
	if (changed == 0)
	{
		minrow = 7;
		mincol = 7;
		maxrow = 7;
		maxcol = 7;
	}
	if (mincol != 0)
		mincol--;
	if (minrow != 0)
		minrow--;
	if (maxrow != 14)
		maxrow++;
	if (maxcol != 14)
		maxcol++;
	Count = createMove(depth, side, minrow,maxrow,mincol,maxcol);
	int eval_is_exact = 0;
	for (i = 0; i < Count; i++)
	{
		moveList[depth][i].score = getHistoryScore(moveList[depth][i].r, moveList[depth][i].c);
	}
	MergeSort(moveList[depth], Count);
	int bestmove = -1;
	int maxxxx = 0;
	if (depth == maxdepth)
	{
		for (int i = 0; i < Count; i++)
		{
			cout <<i<<" "<< moveList[depth][i].r << " " << moveList[depth][i].c << endl;
		}
	}
	for (i = 0; i < Count; i++)
	{
		hashMove(moveList[depth][i].r, moveList[depth][i].c);
		if (side == 0)
		{
			board[moveList[depth][i].r][moveList[depth][i].c].color = ownColor;
			board[moveList[depth][i].r][moveList[depth][i].c].empty = false;
		}
		else
		{
			board[moveList[depth][i].r][moveList[depth][i].c].color = oppositeColor;
			board[moveList[depth][i].r][moveList[depth][i].c].empty = false;
		}
		int q;
		q = isGameOver(depth);
		score = -NegaMax(depth - 1, -beta, -alpha);
		if (ownColor == 0 && depth == maxdepth)
		{
			cout << i << " " << moveList[depth][i].r << " " << moveList[depth][i].c << " " << score << endl;
		}
		board[moveList[depth][i].r][moveList[depth][i].c].reset();
		hashMove(moveList[depth][i].r, moveList[depth][i].c);
		if (score > alpha)
		{
			alpha = score;
			eval_is_exact = 1;
			bestrow = moveList[depth][i].r;
			bestcol = moveList[depth][i].c;
			bestmove = i;
		}
		if (alpha >= beta)
		{
			enterHashTable(lower_bound, score, depth);
			break;
		}
	}
	
	if (owndised)
	{
		board[disrow][discol].color = discolor;
		board[disrow][discol].empty = false;
		ownDis--;
	}
	if (oppdised)
	{
		board[disrow][discol].color = discolor;
		board[disrow][discol].empty = false;
		oppDis--;
	}
	
	if (bestmove != -1)
	{
		enterHistoryScore(moveList[depth][bestmove].r, moveList[depth][bestmove].c, depth);
		if (depth == maxdepth)
		{
			bestrow = moveList[depth][bestmove].r;
			bestcol = moveList[depth][bestmove].c;
		}
	}
	if (eval_is_exact)
	{
		enterHashTable(exact, alpha, depth);
	}
	else
	{
		enterHashTable(upper_bound, alpha, depth);
	}
	return alpha;
}

//估值函数
int value()
{
	int v = 0;
	for (int i = 0; i < 15; ++i)
	{
		for (int j = 0; j < 15; ++j)
		{
			//计算垂直方向的五元组
			int w = cal(i, i + 4, j, j, VERTICAL);
			if (w == O1 || w == O2 || w == O3 || w == O4 || w == O5)
			{
				v -= w;
			}
			else
			{
				v += w;
			}
			//计算水平方向的五元组
			w = cal(i, i, j, j + 4, HORIZONTAL);
			if (w == O1 || w == O2 || w == O3 || w == O4 || w == O5)
			{
				v -= w;
			}
			else
			{
				v += w;
			}
			//计算左上到右下的五元组
			w = cal(i, i + 4, j, j + 4, LEFT);
			if (w == O1 || w == O2 || w == O3 || w == O4 || w == O5)
			{
				v -= w;
			}
			else
			{
				v += w;
			}
			//计算右上到左下的五元组
			w = cal(i - 4, i, j, j + 4, RIGHT);
			if (w == O1 || w == O2 || w == O3 || w == O4 || w == O5)
			{
				v -= w;
			}
			else
			{
				v += w;
			}
			if (board[i][j].color == ownColor)
			{
				v += 100 * (20 - abs(i - 7) - abs(j - 7));
			}
		}
	}
	//if (debug == 1)
	//{
	//	cout << "value:"<<v << endl;
	//}
	return v;
}

//五元组计算函数
int cal(int begin_row, int end_row, int begin_col, int end_col, int direction)
{
	int opnum = 0;//五元组中有多少个敌方的子
	int ownum = 0;//五元组中有多少个自己的子
	int owndis = 0; // 五元组中是否有即将消失的自己的子
	int oppdis = 0; // 五元组中是否有即将消失的对方的子
	if (begin_row<0 || end_row > 14 || begin_col<0 || end_col>14) // 不能组成五元组
	{
		return -100;
	}
	switch (direction)
	{
	case VERTICAL:
		for (int i = begin_row; i <= end_row; ++i)
		{
			if (board[i][begin_col].color == ownColor)
			{
				++ownum;
			}
			else if (board[i][begin_col].color == oppositeColor)
			{
				++opnum;
			}
		}
		break;
	case HORIZONTAL:
		for (int j = begin_col; j <= end_col; ++j)
		{
			if (board[begin_row][j].color == ownColor)
			{
				++ownum;
			}
			else if (board[begin_row][j].color == oppositeColor)
			{
				++opnum;
			}
		}
		break;
	case LEFT:
		for (int i = begin_row, j = begin_col; i <= end_row, j <= end_col; ++i, ++j)
		{
			if (board[i][j].color == ownColor)
			{
				++ownum;
			}
			else if (board[i][j].color == oppositeColor)
			{
				++opnum;
			}
		}
		break;
	case RIGHT:
		for (int i = end_row, j = begin_col; i >= begin_row, j <= end_col; --i, ++j)
		{
			if (board[i][j].color == ownColor)
			{
				++ownum;
			}
			else if (board[i][j].color == oppositeColor)
			{
				++opnum;
			}
		}
		break;
	default:
		exit(1);
	}

	int own = -1;
	int opp = -1;
	if (ownum == 0 && opnum == 0)
	{
		return VO;
	}
	if (ownum != 0 && opnum != 0)
	{
		return PU;
	}
	if (ownum == 5)
	{
		own = M5;
	}
	if (ownum == 4)
	{
		own = M4;
	}
	if (ownum == 3)
	{
		own = M3;
	}
	if (ownum == 2)
	{
		own = M2;
	}
	if (ownum == 1)
	{
		own = M1;
	}

	if (opnum == 5)
	{
		opp = O5;
	}
	if (opnum == 4)
	{
		opp = O4;
	}
	if (opnum == 3)
	{
		opp = O3;
	}
	if (opnum == 2)
	{
		opp = O2;
	}
	if (opnum == 1)
	{
		opp = O1;
	}

	if (own != -1)
	{
		return own;
	}
	if (opp != -1)
	{
		return opp;
	}
}


///////////////////////////////////////////////////////////////////
//以下函数为实现置换表功能
///////////////////////////////////////////////////////////////////

//生成64位随机数
LONGLONG rand64(void)
{
	return rand() ^ ((LONGLONG)rand() << 15) ^
		((LONGLONG)rand() << 30) ^
		((LONGLONG)rand() << 45) ^
		((LONGLONG)rand() << 60);
}

//生成32位随机数
LONG rand32(void)
{
	return rand() ^ ((LONG)rand() << 15) ^ ((LONG)rand() << 30);
}

//生成用于计算哈希值的随机数组
void InitializeHashKey()
{
	int i, j, k;
	srand((unsigned)time(NULL));

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 15; j++)
		{
			for (k = 0; k < 15; k++)
			{
				HashKey64[i][j][k] = rand64();
				HashKey32[i][j][k] = rand32();
			}
		}
	}
	pTT[0] = new HashItem[1024 * 1024];//存储极大值的节点数据
	pTT[1] = new HashItem[1024 * 1024];//存储极小值的节点数据
}

//计算当前棋盘的哈希值
void calculateHaskKey32()
{
	int j, k, chessColor;
	HashValue32 = 0;
	HashValue64 = 0;
	for (j = 0; j < 15; j++)
	{
		for (k = 0; k < 15; k++)
		{
			int color = board[j][k].color;
			if (color != -1)
			{
				HashValue32 = HashValue32^HashKey32[color][j][k];
				HashValue64 = HashValue64^HashKey64[color][j][k];
			}
		}
	}
}

//根据传入的落子点/去除子的点，修改相应的哈希值
void hashMove(int mover, int movec)
{
	HashValue32 = HashValue32^HashKey32[board[mover][movec].color][mover][movec];
	HashValue64 = HashValue64^HashKey64[board[mover][movec].color][mover][movec];
}

//查找哈希表 alpha.beta分别为ab搜索的上下边界，depth是当前搜索的层次
int lookUpHashTable(int alpha, int beta, int depth)
{
	int x;
	HashItem* pht;
	x = HashValue32 & 0xFFFFF; // 如果哈希表大小有改动，这一句需要修改
	pht = &pTT[(maxdepth - depth) % 2][x];
	if (pht->depth >= depth && pht->checksum == HashValue64)
	{
		switch (pht->entry_type)
		{
		case exact://确切
			return pht->eval;
		case lower_bound:
			if (pht->eval >= beta)
				return pht->eval;
			else break;
		case upper_bound:
			if (pht->eval <= alpha)
				return pht->eval;
			else
				break;
		}
	}
	return NOTFIND;
}

//向置换表中插入数据 entry_type数据类型 eval数据值 depth该值层次 
void enterHashTable(ENTRY_TYPE entry_type, short eval, short depth)
{
	int x;
	HashItem* pht;
	x = HashValue32 & 0xFFFFF; //如果哈希表大小有改动，这一句需要修改
	pht = &pTT[(maxdepth - depth) % 2][x];
	pht->checksum = HashValue64;
	pht->entry_type = entry_type;
	pht->eval = eval;
	pht->depth = depth;
}

///////////////////////////////////////////////////////////////////
//以上函数为实现置换表功能
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//以下函数为实现历史启发功能
///////////////////////////////////////////////////////////////////

//清空历史纪录表
void resetHistoryTable()
{
	memset(HistoryTable, 0, 225 * 4);
}

//取给定走法的历史得分
int getHistoryScore(int mover, int movec)
{
	return HistoryTable[mover * 15 + movec];
}

//将一最佳走法汇入历史纪录
void enterHistoryScore(int mover, int movec, int depth)
{
	HistoryTable[mover * 15 + movec] += (1 << depth);
}

//将有序ab合并到c中
void Merge(chessmove a[], int n, chessmove b[], int m, chessmove c[])
{
	int i, j, k;
	i = j = k = 0;
	while (i < n && j < m)
	{
		if (a[j].score < b[j].score)
		{
			c[k++] = b[j++];
		}
		else
		{
			c[k++] = a[j++];
		}
	}
	while (i < n)
	{
		c[k++] = a[i++];
	}
	while (j < m)
	{
		c[k++] = b[j++];
	}
}

//合并a
void Mergearray(chessmove a[], int first, int mid, int last, chessmove temp[])
{
	int i = first, j = mid + 1;
	int m = mid, n = last;
	int k = 0;
	while (i <= m && j <= n)
	{
		if (a[i].score >= a[j].score)
		{
			temp[k++] = a[i++];
		}
		else
		{
			temp[k++] = a[j++];
		}
	}
	while (i <= m)
	{
		temp[k++] = a[i++];
	}
	while (j <= n)
	{
		temp[k++] = a[j++];
	}
	for (i = 0; i < k; i++)
	{
		a[first + i] = temp[i];
	}
}

void mergesort(chessmove a[], int first, int last, chessmove temp[])
{
	if (first < last)
	{
		int mid = (first + last) / 2;
		mergesort(a, first, mid, temp);
		mergesort(a, mid + 1, last, temp);
		Mergearray(a, first, mid, last, temp);
	}
}

//排序
bool MergeSort(chessmove a[], int n)
{
	chessmove *p = new chessmove[n];
	if (p == NULL)
		return false;
	mergesort(a, 0, n - 1, p);
	delete[] p;
	return true;
}


///////////////////////////////////////////////////////////////////
//以上函数为实现历史启发功能（开始于783行）
///////////////////////////////////////////////////////////////////