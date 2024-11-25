#include "main.h"

const int WINDOW_WIDTH = 1000;              // 윈도우 창 넓이
const int WINDOW_HEIGHT = 800;              // 윈도우 창 높이
const int NORMAL_RADIUS = 15;               // 윷판 작은 점의 반지름
const int CORNER_RADIUS = 25;               // 윷판 큰 점의 반지름

// 비트맵(Bitmap) 객체를 나타내는 핸들
HBITMAP hDoubleBufferBitmap = NULL;

// 더블 버퍼링을 위한 백그라운드 장치 컨텍스트
HDC hDoubleBufferDC = NULL;

// 게임 보드 경로 정의
const std::vector<int> MAIN_PATH = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 29 };            // 바깥 경로
const std::vector<int> PATH_FROM_5 = { 5, 20, 21, 22, 25, 26, 15, 16, 17, 18, 19, 0, 29 };                                  // 우측 상단 큰 원부터의 경로
const std::vector<int> PATH_FROM_10 = { 10, 23, 24, 22, 27, 28, 0, 29 };                                                    // 좌측 상단 큰 원부터의 경로
const std::vector<int> PATH_FROM_22 = { 22, 27, 28, 0, 29 };                                                                // 가운데 큰 원부터의 경로

// 윷 던지기 결과를 나타내는 열거형
enum class YutResult {
    DO,    // 도: 1칸 이동
    GAE,   // 개: 2칸 이동
    GEOL,  // 걸: 3칸 이동
    YUT,   // 윷: 4칸 이동
    MO     // 모: 5칸 이동
};

// 말의 위치와 상태를 나타내는 구조체
struct Piece {
    int position;                           // 말의 현재 위치
    bool isOnBoard;                         // 말이 게임판 위에 존재하는지 확인
    bool isFinished;                        // 말이 완주했는지 확인
    const std::vector<int>* currentPath;    // 말이 이동할 현재 경로
    std::vector<Piece*> stackedPieces;      // 말이 엎었을 경우

    // 생성자 : 멤버 변수 초기화
    Piece() : position(0), isOnBoard(false), isFinished(false), currentPath(&MAIN_PATH) {}
};

// 게임의 전체 상태를 나타내는 구조체
struct GameState {
    int currentPlayer;                      // 현재 차례
    std::vector<Piece> player1Pieces;       // 플레이어 1의 말
    std::vector<Piece> player2Pieces;       // 플레이어 2의 말
    YutResult currentYutResult;             // 윷 던지기 결과
    int remainingMinutes;
    int remainingSeconds;
    bool isFirstThrow;                      // 첫 번째 던지기
    bool canThrowAgain;                     // 추가 턴
};

// 게임 상태와 경로 점들의 좌표
GameState gameState;
std::vector<POINT> pathPoints(30);

// 게임 초기화
GameState InitializeGame() {
    GameState state;
    state.currentPlayer = 1;                                            // 게임 시작시 플레이어 1부터 시작
    state.player1Pieces = std::vector<Piece>(2);                        // 플레이어 1의 말 2개
    state.player2Pieces = std::vector<Piece>(2);                        // 플레이어 2의 말 2개
    state.remainingMinutes = 10;                                        // 10분으로 시작
    state.remainingSeconds = 0;                                         // 0초로 시작
    state.isFirstThrow = true;                                          // 첫 번째 윷 던지기
    state.canThrowAgain = false;                                        // 게임 시작시 추가 턴 X
    return state;
}

// 윷 던지기 / 랜덤으로 윷 결과 반환
YutResult ThrowYut() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    int result = dis(gen);
    if (result == 0) return YutResult::MO;
    if (result <= 3) return YutResult::YUT;
    if (result <= 7) return YutResult::GEOL;
    if (result <= 11) return YutResult::GAE;
    return YutResult::DO;
}

// 말 이동 / 주어진 말을 이동시키고 상대방 말을 잡았는지, 한 바퀴를 완주했는지 반환
std::pair<bool, bool> MovePiece(Piece& piece, int steps, std::vector<Piece>& opponentPieces) {

    // 현재 위치를 담을 변수
    int currentIndex = 0;

    // 현재 위치를 찾기
    for (size_t i = 0; i < piece.currentPath->size(); ++i) {
        if ((*piece.currentPath)[i] == piece.position) {
            currentIndex = i;
            break;
        }
    }

    // 새로운 위치를 담을 변수
    int newIndex = currentIndex + steps;

    // 끝을 찾는 변수
    bool hasCompletedLap = false;

    if (newIndex >= piece.currentPath->size() - 1) {  // 29(마지막 인덱스)에 도달하거나 넘어갈 경우
        piece.position = 29;  // 29로 이동
        piece.isOnBoard = false;
        piece.isFinished = true;
        hasCompletedLap = true;

        // 스택된 말들도 함께 나간 말로 처리
        for (Piece* stackedPiece : piece.stackedPieces) {
            stackedPiece->position = 29;
            stackedPiece->isOnBoard = false;
            stackedPiece->isFinished = true;
        }
        piece.stackedPieces.clear();

        return std::make_pair(false, hasCompletedLap);
    }

    piece.position = (*piece.currentPath)[newIndex];

    // 상단 대각선일 경우 경로 변경
    if (piece.position == 5 && piece.currentPath == &MAIN_PATH) {
        piece.currentPath = &PATH_FROM_5;
    }
    else if (piece.position == 10 && piece.currentPath == &MAIN_PATH) {
        piece.currentPath = &PATH_FROM_10;
    }
    else if (piece.position == 22) {
        piece.currentPath = &PATH_FROM_22;
    }

    // 스택된 말들도 함께 이동
    for (Piece* stackedPiece : piece.stackedPieces) {
        stackedPiece->position = piece.position;
        stackedPiece->currentPath = piece.currentPath;
    }

    // 상대방 말을 잡았는지 확인
    bool capturedPiece = false;
    for (auto& opponentPiece : opponentPieces) {
        if (opponentPiece.isOnBoard && opponentPiece.position == piece.position) {
            opponentPiece.isOnBoard = false;
            opponentPiece.position = 0;
            opponentPiece.currentPath = &MAIN_PATH;

            // 잡힌 말의 스택된 말들도 함께 처리
            for (Piece* stackedPiece : opponentPiece.stackedPieces) {
                stackedPiece->isOnBoard = false;
                stackedPiece->position = 0;
                stackedPiece->currentPath = &MAIN_PATH;
            }
            opponentPiece.stackedPieces.clear();

            capturedPiece = true;
        }
    }

    return std::make_pair(capturedPiece, hasCompletedLap);
}

// 게임 보드 그리기
void DrawBoard(HDC hdc)
{
    // 배경색 설정 (연한 베이지색)
    HBRUSH hBackgroundBrush = CreateSolidBrush(RGB(245, 245, 220));
    RECT rcClient = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    FillRect(hdc, &rcClient, hBackgroundBrush);
    DeleteObject(hBackgroundBrush);

    // 펜 설정 (검정색)
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    // 게임 보드 영역 계산
    int boardLeft = 50;
    int boardTop = 100;
    int boardSize = 600;
    int centerX = boardLeft + boardSize / 2;
    int centerY = boardTop + boardSize / 2;

    // 경로 점들의 위치 계산
    const int POINTS_PER_SIDE = 6;

    // 하단 경로 점 (15-19, 0)
    for (int i = 0; i <= 5; i++) {
        pathPoints[(i + 15) % 20] = {
            boardLeft + (boardSize * i / 5),
            boardTop + boardSize
        };
    }

    // 우측 경로 점 (1-4)
    for (int i = 1; i <= 4; i++) {
        pathPoints[i] = {
            boardLeft + boardSize,
            boardTop + boardSize - (boardSize * i / 5)
        };
    }

    // 상단 경로 점 (5-10)
    for (int i = 0; i <= 5; i++) {
        pathPoints[5 + i] = {
            boardLeft + boardSize - (boardSize * i / 5),
            boardTop
        };
    }

    // 좌측 경로 점 (11-14)
    for (int i = 1; i <= 4; i++) {
        pathPoints[10 + i] = {
            boardLeft,
            boardTop + (boardSize * i / 5)
        };
    }

    // 대각선 경로 점들
    pathPoints[20] = { boardLeft + 5 * boardSize / 6, boardTop + boardSize / 6 };
    pathPoints[21] = { boardLeft + 2 * boardSize / 3, boardTop + boardSize / 3 };
    pathPoints[22] = { centerX, centerY };
    pathPoints[23] = { boardLeft + boardSize / 6, boardTop + boardSize / 6 };
    pathPoints[24] = { boardLeft + boardSize / 3, boardTop + boardSize / 3 };
    pathPoints[25] = { boardLeft + boardSize / 3, boardTop + 2 * boardSize / 3 };
    pathPoints[26] = { boardLeft + boardSize / 6, boardTop + 5 * boardSize / 6 };
    pathPoints[27] = { boardLeft + 2 * boardSize / 3, boardTop + 2 * boardSize / 3 };
    pathPoints[28] = { boardLeft + 5 * boardSize / 6, boardTop + 5 * boardSize / 6 };

    // 경로 선 그리기
    // 외곽선
    MoveToEx(hdc, boardLeft, boardTop, NULL);
    LineTo(hdc, boardLeft + boardSize, boardTop);
    LineTo(hdc, boardLeft + boardSize, boardTop + boardSize);
    LineTo(hdc, boardLeft, boardTop + boardSize);
    LineTo(hdc, boardLeft, boardTop);

    // 대각선 그리기
    MoveToEx(hdc, boardLeft, boardTop, NULL);
    LineTo(hdc, centerX, centerY);
    LineTo(hdc, boardLeft + boardSize, boardTop + boardSize);

    MoveToEx(hdc, boardLeft + boardSize, boardTop, NULL);
    LineTo(hdc, centerX, centerY);
    LineTo(hdc, boardLeft, boardTop + boardSize);

    // 경로 점 그리기
    HBRUSH hDotBrush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hDotBrush);

    // 모든 경로 점 그리기
    for (size_t i = 0; i < pathPoints.size() - 1; i++) {
        int radius = NORMAL_RADIUS;
        // 모서리 점은 더 크게 (대각선의 끝점은 제외)
        if (i == 0 || i == 5 || i == 10 || i == 15) {
            radius = CORNER_RADIUS;
        }
        Ellipse(hdc,
            pathPoints[i].x - radius, pathPoints[i].y - radius,
            pathPoints[i].x + radius, pathPoints[i].y + radius);
    }

    // 오른쪽 패널 그리기
    int panelLeft = boardLeft + boardSize + 100;
    int panelTop = boardTop;

    // 폰트 설정
    /*
        CreateFont()의 인수
        20: 폰트 높이 (픽셀 단위)
        0, 0, 0: 폰트 너비, 기울기 각도, 방향 각도 (0은 기본값)
        FW_NORMAL: 폰트 두께 (보통)
        FALSE, FALSE, FALSE: 이탤릭체, 밑줄, 취소선 (모두 사용하지 않음)
        HANGEUL_CHARSET: 한글 문자셋 사용
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY: 출력 정밀도, 클리핑 정밀도, 품질 (기본값 사용)
        DEFAULT_PITCH | FF_DONTCARE: 기본 피치와 폰트 패밀리 (시스템이 선택)
        L"맑은 고딕": 사용할 폰트 이름
    */

    HFONT hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        HANGEUL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);

    // 남은 시간 표시
    wchar_t timeStr[50];
    swprintf_s(timeStr, L"남은 시간: %02d:%02d", gameState.remainingMinutes, gameState.remainingSeconds);
    TextOut(hdc, panelLeft, panelTop, timeStr, wcslen(timeStr));

    // 현재 차례 표시
    TextOut(hdc, panelLeft, panelTop + 40, L"현재 차례: ", 6);
    HBRUSH hCurrentPlayerBrush = CreateSolidBrush(gameState.currentPlayer == 1 ? RGB(255, 255, 0) : RGB(0, 255, 0));
    SelectObject(hdc, hCurrentPlayerBrush);
    Ellipse(hdc, panelLeft + 100, panelTop + 40, panelLeft + 120, panelTop + 60);
    DeleteObject(hCurrentPlayerBrush);

    // 남은 말 표시 (플레이어 1)
    TextOut(hdc, panelLeft, panelTop + 80, L"플레이어 1", 6);
    HBRUSH hBeigeBrush = CreateSolidBrush(RGB(245, 245, 220));
    SelectObject(hdc, hBeigeBrush);
    Rectangle(hdc, panelLeft, panelTop + 110, panelLeft + 80, panelTop + 150);

    HBRUSH hYellowBrush = CreateSolidBrush(RGB(255, 255, 0));
    HBRUSH hGreenBrush = CreateSolidBrush(RGB(0, 255, 0));

    // 플레이어 1의 말 그리기
    SelectObject(hdc, hYellowBrush);
    for (size_t i = 0; i < gameState.player1Pieces.size(); i++) {
        if (!gameState.player1Pieces[i].isOnBoard && !gameState.player1Pieces[i].isFinished) {
            Ellipse(hdc, panelLeft + 15 + (i * 30), panelTop + 120, panelLeft + 35 + (i * 30), panelTop + 140);
            wchar_t numberStr[2];
            swprintf_s(numberStr, L"%d", i + 1);
            SetTextColor(hdc, RGB(0, 0, 0));  // 검은색 텍스트
            TextOut(hdc, panelLeft + 22 + (i * 30), panelTop + 122, numberStr, 1);
        }
    }

    // 남은 말 표시 (플레이어 2)
    TextOut(hdc, panelLeft, panelTop + 170, L"플레이어 2", 6);
    SelectObject(hdc, hBeigeBrush);
    Rectangle(hdc, panelLeft, panelTop + 200, panelLeft + 80, panelTop + 240);

    // 플레이어 2의 말 그리기
    SelectObject(hdc, hGreenBrush);
    for (size_t i = 0; i < gameState.player2Pieces.size(); i++) {
        if (!gameState.player2Pieces[i].isOnBoard && !gameState.player2Pieces[i].isFinished) {
            Ellipse(hdc, panelLeft + 15 + (i * 30), panelTop + 210, panelLeft + 35 + (i * 30), panelTop + 230);
            wchar_t numberStr[2];
            swprintf_s(numberStr, L"%d", i + 1);
            SetTextColor(hdc, RGB(0, 0, 0));  // 검은색 텍스트
            TextOut(hdc, panelLeft + 22 + (i * 30), panelTop + 212, numberStr, 1);
        }
    }

    // 윷놀이 판에 말 그리기
    for (const auto& piece : gameState.player1Pieces) {
        if (piece.isOnBoard) {
            SelectObject(hdc, hYellowBrush);

            // 스택된 말 그리기
            int stackSize = piece.stackedPieces.size() + 1;
            for (int i = 0; i < stackSize; ++i) {
                int offset = i * 3;  // 각 스택된 말마다 2픽셀씩 이동

                Ellipse(hdc,
                    pathPoints[piece.position].x - NORMAL_RADIUS + offset,
                    pathPoints[piece.position].y - NORMAL_RADIUS + offset,
                    pathPoints[piece.position].x + NORMAL_RADIUS + offset,
                    pathPoints[piece.position].y + NORMAL_RADIUS + offset);
            }

            // 말 번호 표시 (가장 위에 있는 말의 번호만 표시)
            wchar_t numberStr[2];
            swprintf_s(numberStr, L"%d", &piece - &gameState.player1Pieces[0] + 1);
            SetTextColor(hdc, RGB(0, 0, 0));  // 검은색 텍스트
            int topOffset = (stackSize - 1) * 2;
            TextOut(hdc, pathPoints[piece.position].x - 5 + topOffset, pathPoints[piece.position].y - 8 + topOffset, numberStr, 1);
        }
    }
    for (const auto& piece : gameState.player2Pieces) {
        if (piece.isOnBoard) {
            SelectObject(hdc, hGreenBrush);

            // 스택된 말 그리기
            int stackSize = piece.stackedPieces.size() + 1;
            for (int i = 0; i < stackSize; ++i) {
                int offset = i * 2;  // 각 스택된 말마다 2픽셀씩 이동
                
                Ellipse(hdc,
                    pathPoints[piece.position].x - NORMAL_RADIUS + offset,
                    pathPoints[piece.position].y - NORMAL_RADIUS + offset,
                    pathPoints[piece.position].x + NORMAL_RADIUS + offset,
                    pathPoints[piece.position].y + NORMAL_RADIUS + offset);
            }

            // 말 번호 표시 (가장 위에 있는 말의 번호만 표시)
            wchar_t numberStr[2];
            swprintf_s(numberStr, L"%d", &piece - &gameState.player2Pieces[0] + 1);
            SetTextColor(hdc, RGB(0, 0, 0));  // 검은색 텍스트
            int topOffset = (stackSize - 1) * 2;
            TextOut(hdc, pathPoints[piece.position].x - 5 + topOffset, pathPoints[piece.position].y - 8 + topOffset, numberStr, 1);
        }
    }

    // 플레이어 1 나간말 박스
    TextOut(hdc, panelLeft, panelTop + 260, L"플레이어 1 나간말", 11);
    SelectObject(hdc, hBeigeBrush);
    Rectangle(hdc, panelLeft, panelTop + 290, panelLeft + 80, panelTop + 330);

    // 플레이어 1 나간말 표시
    SelectObject(hdc, hYellowBrush);
    int finishedCount1 = 0;
    for (const auto& piece : gameState.player1Pieces) {
        if (piece.isFinished) {
            Ellipse(hdc, panelLeft + 15 + (finishedCount1 * 30), panelTop + 300, panelLeft + 35 + (finishedCount1 * 30), panelTop + 320);
            wchar_t numberStr[2];
            swprintf_s(numberStr, L"%d", &piece - &gameState.player1Pieces[0] + 1);
            SetTextColor(hdc, RGB(0, 0, 0));  // 검은색 텍스트
            TextOut(hdc, panelLeft + 22 + (finishedCount1 * 30), panelTop + 302, numberStr, 1);
            finishedCount1++;
        }
    }

    // 플레이어 2 나간말 박스
    TextOut(hdc, panelLeft, panelTop + 350, L"플레이어 2 나간말", 11);
    SelectObject(hdc, hBeigeBrush);
    Rectangle(hdc, panelLeft, panelTop + 380, panelLeft + 80, panelTop + 420);

    // 플레이어 2 나간말 표시
    SelectObject(hdc, hGreenBrush);
    int finishedCount2 = 0;
    for (const auto& piece : gameState.player2Pieces) {
        if (piece.isFinished) {
            Ellipse(hdc, panelLeft + 15 + (finishedCount2 * 30), panelTop + 390, panelLeft + 35 + (finishedCount2 * 30), panelTop + 410);
            wchar_t numberStr[2];
            swprintf_s(numberStr, L"%d", &piece - &gameState.player2Pieces[0] + 1);
            SetTextColor(hdc, RGB(0, 0, 0));  // 검은색 텍스트
            TextOut(hdc, panelLeft + 22 + (finishedCount2 * 30), panelTop + 392, numberStr, 1);
            finishedCount2++;
        }
    }

    // 윷 스틱 그리기
    int stickWidth = 30;
    int stickHeight = 60;
    int sticksLeft = panelLeft;
    int sticksTop = panelTop + 440;
    HBRUSH hStickBrush = CreateSolidBrush(RGB(210, 180, 140));
    SelectObject(hdc, hStickBrush);

    int circlesRemoved = 0;
    std::wstring resultStr;
    switch (gameState.currentYutResult) {
    case YutResult::DO: circlesRemoved = 1; resultStr = L"윷 결과 : 도"; break;
    case YutResult::GAE: circlesRemoved = 2; resultStr = L"윷 결과 : 개"; break;
    case YutResult::GEOL: circlesRemoved = 3; resultStr = L"윷 결과 : 걸"; break;
    case YutResult::YUT: circlesRemoved = 4; resultStr = L"윷 결과 : 윷"; break;
    case YutResult::MO: circlesRemoved = 0; resultStr = L"윷 결과 : 모"; break;
    }

    for (int i = 0; i < 4; i++) {
        Rectangle(hdc,
            sticksLeft + (i * (stickWidth + 10)), sticksTop,
            sticksLeft + (i * (stickWidth + 10)) + stickWidth, sticksTop + stickHeight);

        // 4개의 작은 원 그리기
        int circleRadius = 5;
        for (int j = 0; j < 4; j++) {
            // 첫 번째 던지기가 아니고, 현재 스틱이 제거되어야 할 동그라미 수보다 작을 때만 그리기
            if (gameState.isFirstThrow || i >= circlesRemoved) {
                int circleX = sticksLeft + (i * (stickWidth + 10)) + stickWidth / 2;
                int circleY = sticksTop + (j + 1) * (stickHeight / 5);
                Ellipse(hdc,
                    circleX - circleRadius, circleY - circleRadius,
                    circleX + circleRadius, circleY + circleRadius);
            }
        }
    }

    // 윷 결과 표시
    if (!gameState.isFirstThrow) {
        SetTextColor(hdc, RGB(0, 0, 0));  // 검은색 텍스트
        TextOut(hdc, sticksLeft, sticksTop + stickHeight + 50, resultStr.c_str(), resultStr.length());
    }
    // 한 번 더 던질 수 있다는 메시지 표시
    if (gameState.canThrowAgain) {
        SetTextColor(hdc, RGB(0, 0, 0));  // 검은색 텍스트
        TextOut(hdc, sticksLeft, sticksTop + stickHeight + 80, L"한번 더!", 4);
    }

    // 윷 던지기 버튼 위치 조정
    Rectangle(hdc, sticksLeft, sticksTop + stickHeight + 10,
        sticksLeft + 100, sticksTop + stickHeight + 40);
    TextOut(hdc, sticksLeft + 20, sticksTop + stickHeight + 15, L"윷 돌리기", 5);

    // 리소스 정리
    SelectObject(hdc, hOldBrush);
    DeleteObject(hDotBrush);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hYellowBrush);
    DeleteObject(hGreenBrush);
    DeleteObject(hStickBrush);
    DeleteObject(hBeigeBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

// 클릭 처리 / 던지기 버튼 클릭 및 말 이동 처리
void HandleClick(HWND hWnd, int x, int y, GameState& state)
{
    // 게임 보드 영역 계산
    int boardLeft = 50;
    int boardTop = 100;
    int boardSize = 600;
    int panelLeft = boardLeft + boardSize + 100;
    int panelTop = boardTop;

    // 윷 던지기 버튼 영역 
    int sticksTop = panelTop + 440;
    int buttonTop = sticksTop + 70;  // 60 (윷 막대 높이) + 10 (여백)
    RECT throwButton = {
        panelLeft,
        buttonTop,
        panelLeft + 100,
        buttonTop + 30
    };

    if (x >= throwButton.left && x <= throwButton.right &&
        y >= throwButton.top && y <= throwButton.bottom) {
        // 윷 던지기 버튼이 클릭되었을 때
        state.currentYutResult = ThrowYut();

        // 윷 던지기 결과를 문자열로 변환
        std::wstring resultStr;
        int moveCount = 0;
        switch (state.currentYutResult) {
        case YutResult::DO: resultStr = L"도"; moveCount = 1; break;
        case YutResult::GAE: resultStr = L"개"; moveCount = 2; break;
        case YutResult::GEOL: resultStr = L"걸"; moveCount = 3; break;
        case YutResult::YUT: resultStr = L"윷"; moveCount = 4; break;
        case YutResult::MO: resultStr = L"모"; moveCount = 5; break;
        }

        std::vector<Piece>& currentPlayerPieces = (state.currentPlayer == 1) ? state.player1Pieces : state.player2Pieces;
        std::vector<Piece>& opponentPieces = (state.currentPlayer == 1) ? state.player2Pieces : state.player1Pieces;

        // 보드 위의 말 수와 나가지 않은 말 수 확인
        int piecesOnBoard = 0;
        int piecesNotFinished = 0;
        for (const auto& piece : currentPlayerPieces) {
            if (piece.isOnBoard) {
                piecesOnBoard++;
                if (!piece.isFinished) {
                    piecesNotFinished++;
                }
            }
        }

        bool capturedPiece = false;
        bool completedLap = false;

        // 나간 말이 있는지 확인
        bool hasFinishedPiece = std::any_of(currentPlayerPieces.begin(), currentPlayerPieces.end(),
            [](const Piece& p) { return p.isFinished; });

        if (piecesOnBoard == 0 || (piecesNotFinished == 0 && piecesOnBoard < 2)) {
            // 보드 위에 말이 없거나, 나가지 않은 말이 없고 보드 위의 말이 2개 미만인 경우 자동으로 새 말을 냄
            for (auto& piece : currentPlayerPieces) {
                if (!piece.isOnBoard && !piece.isFinished) {
                    piece.isOnBoard = true;
                    piece.position = 0;
                    auto result = MovePiece(piece, moveCount, opponentPieces);
                    capturedPiece = result.first;
                    completedLap = result.second;
                    break;
                }
            }
        }
        else if (!hasFinishedPiece) {
            // 나간 말이 없는 경우에만 선택 메시지 표시
            if (piecesOnBoard == 1) {
                int choice = MessageBox(hWnd, L"새 말을 내시겠습니까?", L"말 선택", MB_YESNO);
                if (choice == IDYES) {
                    // 새 말을 냄
                    for (auto& piece : currentPlayerPieces) {
                        if (!piece.isOnBoard && !piece.isFinished) {
                            piece.isOnBoard = true;
                            piece.position = 0;
                            auto result = MovePiece(piece, moveCount, opponentPieces);
                            capturedPiece = result.first;
                            completedLap = result.second;
                            break;
                        }
                    }
                }
                else {
                    // 기존 말을 움직임
                    for (auto& piece : currentPlayerPieces) {
                        if (piece.isOnBoard && !piece.isFinished) {
                            auto moveResult = MovePiece(piece, moveCount, opponentPieces);
                            capturedPiece = moveResult.first;
                            completedLap = moveResult.second;
                            break;
                        }
                    }
                }
            }
            else {
                // 보드 위에 말이 여러 개인 경우
                auto firstPiece = std::find_if(currentPlayerPieces.begin(), currentPlayerPieces.end(),
                    [](const Piece& p) { return p.isOnBoard && !p.isFinished; });

                if (firstPiece != currentPlayerPieces.end()) {
                    if (!firstPiece->stackedPieces.empty()) {
                        // 스택된 말이 있는 경우 자동으로 이동
                        auto moveResult = MovePiece(*firstPiece, moveCount, opponentPieces);
                        capturedPiece = moveResult.first;
                        completedLap = moveResult.second;
                    }
                    else {
                        // 스택된 말이 없는 경우 사용자에게 선택하게 함
                        int pieceChoice = MessageBox(hWnd, L"어떤 말을 움직이시겠습니까?\n'예'를 선택하면 말 1, '아니오'를 선택하면 말 2가 움직입니다.", L"말 선택", MB_YESNO);
                        Piece& selectedPiece = (pieceChoice == IDYES)
                            ? *firstPiece
                            : *std::next(firstPiece);

                        auto moveResult = MovePiece(selectedPiece, moveCount, opponentPieces);
                        capturedPiece = moveResult.first;
                        completedLap = moveResult.second;
                    }
                }
            }
        }
        else {
            // 나간 말이 있는 경우, 보드 위의 말만 자동으로 이동
            for (auto& piece : currentPlayerPieces) {
                if (piece.isOnBoard && !piece.isFinished) {
                    auto moveResult = MovePiece(piece, moveCount, opponentPieces);
                    capturedPiece = moveResult.first;
                    completedLap = moveResult.second;
                    break;
                }
            }
        }

        // 이동 후 같은 위치에 있는 말 확인 및 스택
        for (auto& piece : currentPlayerPieces) {
            if (piece.isOnBoard && !piece.isFinished) {
                for (auto& otherPiece : currentPlayerPieces) {
                    if (&piece != &otherPiece && otherPiece.isOnBoard && !otherPiece.isFinished &&
                        piece.position == otherPiece.position && piece.currentPath == otherPiece.currentPath) {
                        if (std::find(piece.stackedPieces.begin(), piece.stackedPieces.end(), &otherPiece) == piece.stackedPieces.end()) {
                            piece.stackedPieces.push_back(&otherPiece);
                        }
                    }
                }
            }
        }

        // 말이 한 바퀴를 완주했을 때 처리
        if (completedLap) {
            MessageBox(hWnd, L"말이 한 바퀴를 완주하여 나갔습니다!", L"말 완주", MB_OK);
        }

        // 말이 잡혔거나 윷/모가 나왔을 때 한 번 더 던질 수 있음
        if (capturedPiece || state.currentYutResult == YutResult::YUT || state.currentYutResult == YutResult::MO) {
            state.canThrowAgain = true;
            if (capturedPiece) {
                MessageBox(hWnd, L"상대방 말을 잡았습니다! 한 번 더 던지세요.", L"추가 턴", MB_OK);
            }
        }
        else {
            state.canThrowAgain = false;
            state.currentPlayer = (state.currentPlayer == 1) ? 2 : 1;
        }

        state.isFirstThrow = false;

        // 승리 조건 확인
        bool player1Won = std::all_of(state.player1Pieces.begin(), state.player1Pieces.end(), [](const Piece& p) { return p.isFinished; });
        bool player2Won = std::all_of(state.player2Pieces.begin(), state.player2Pieces.end(), [](const Piece& p) { return p.isFinished; });

        if (player1Won || player2Won) {
            std::wstring winMessage = (player1Won ? L"플레이어 1" : L"플레이어 2") + std::wstring(L"가 승리했습니다!");
            MessageBox(hWnd, winMessage.c_str(), L"게임 종료", MB_OK);
            gameState = InitializeGame();  // 게임 재시작
        }
        InvalidateRect(hWnd, NULL, FALSE); // FALSE로 변경하여 배경을 지우지 않도록 함
    }
    else {
        // 말 이동 로직
        std::vector<Piece>& currentPlayerPieces = (state.currentPlayer == 1) ? state.player1Pieces : state.player2Pieces;
        std::vector<Piece>& opponentPieces = (state.currentPlayer == 1) ? state.player2Pieces : state.player1Pieces;

        for (auto& piece : currentPlayerPieces) {
            if (piece.isOnBoard && !piece.isFinished) {
                int dx = x - pathPoints[piece.position].x;
                int dy = y - pathPoints[piece.position].y;
            }
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // 더블 버퍼 초기화
        HDC hdc = GetDC(hWnd);
        hDoubleBufferDC = CreateCompatibleDC(hdc);
        hDoubleBufferBitmap = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
        SelectObject(hDoubleBufferDC, hDoubleBufferBitmap);
        ReleaseDC(hWnd, hdc);
        return 0;
    }

    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        lpMMI->ptMinTrackSize.x = WINDOW_WIDTH;
        lpMMI->ptMinTrackSize.y = WINDOW_HEIGHT;
        lpMMI->ptMaxTrackSize.x = WINDOW_WIDTH;
        lpMMI->ptMaxTrackSize.y = WINDOW_HEIGHT;
        return 0;
    }

    case WM_DESTROY:
        // 더블 버퍼 리소스 해제
        DeleteObject(hDoubleBufferBitmap);
        DeleteDC(hDoubleBufferDC);
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 더블 버퍼에 그리기
        DrawBoard(hDoubleBufferDC);

        // 더블 버퍼의 내용을 화면에 복사
        BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hDoubleBufferDC, 0, 0, SRCCOPY);

        EndPaint(hWnd, &ps);
    }
    return 0;

    case WM_LBUTTONDOWN:
    {
        int xPos = GET_X_LPARAM(lParam);
        int yPos = GET_Y_LPARAM(lParam);
        HandleClick(hWnd, xPos, yPos, gameState);
        InvalidateRect(hWnd, NULL, FALSE);  // FALSE로 변경하여 배경을 지우지 않도록 함
    }
    return 0;

    case WM_TIMER:
        // 시간 분 초 계산
        if (wParam == 1) {
            gameState.remainingSeconds--;
            if (gameState.remainingSeconds < 0) {
                gameState.remainingMinutes--;
                gameState.remainingSeconds = 59;
            }
            if (gameState.remainingMinutes < 0) {
                KillTimer(hWnd, 1);
                MessageBox(hWnd, L"시간 초과!", L"게임 종료", MB_OK);
                gameState = InitializeGame();  // 게임 재시작
            }
            InvalidateRect(hWnd, NULL, FALSE); // FALSE로 변경하여 배경을 지우지 않도록 함
        }
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // 윈도우 클래스 등록
    const wchar_t CLASS_NAME[] = L"Yut Nori Window Class";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // 윈도우 생성
    HWND hWnd = CreateWindowEx(
        0,                              // Optional window styles
        CLASS_NAME,                     // Window class
        L"Yut Nori Game",               // Window text
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    // 창 크기를 고정하기 위해 다음 코드를 추가합니다 (CreateWindowEx 호출 후):
    SetWindowLong(hWnd, GWL_STYLE, GetWindowLong(hWnd, GWL_STYLE) & ~WS_MAXIMIZEBOX);

    if (hWnd == NULL)
    {
        return 0;
    }

    ShowWindow(hWnd, nCmdShow);

    // 게임 상태 초기화
    gameState = InitializeGame();

    // 타이머 설정 (1초마다 WM_TIMER 메시지 발생)
    SetTimer(hWnd, 1, 1000, NULL);

    // 메시지 루프
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}