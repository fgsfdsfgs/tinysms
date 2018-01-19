#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>

#include "psg.h"
#include "ym2413.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")

#define WIDTH 256
#define HEIGHT 192
#define TARGET_WIDTH (WIDTH*3)
#define TARGET_HEIGHT (HEIGHT*3)

static uint32_t spr_sega[] = {
#include "spr_sega.inc"
};

static uint32_t spr_logo[] = {
#include "spr_logo.inc"
};

static uint32_t spr_text1[] = {
#include "spr_text1.inc"
};

static uint32_t spr_text2[] = {
#include "spr_text2.inc"
};

static uint32_t spr_copyright1[] = {
#include "spr_copyright1.inc"
};

static uint32_t spr_copyright2[] = {
#include "spr_copyright2.inc"
};

static uint8_t pal_sega[] = {
  0x00, 0x00, 0x00,
  0x00, 0x00, 0xC0,
  0xC0, 0xC0, 0xC0
};

static uint8_t pal_text[] = {
  0x00, 0x00, 0x00,
  0x00, 0x40, 0xC0,
  0xC0, 0xC0, 0xC0
};

typedef struct {
  int32_t x;
  int32_t y;
  uint8_t blink;
} STAR;

static STAR stars[58] = {
  { 83,   2}, {147,   2}, {  6,   4}, { 62,   4}, {118,   4},
  {174,   4}, {222,   4}, { 19,  10}, {187,  10}, { 38,  12},
  { 67,  18}, {227,  18}, { 14,  20}, { 78,  20}, {198,  20},
  {166,  22}, { 35,  26}, { 54,  28}, {190,  28}, {107,  33},
  {  3,  34}, { 83,  34}, {203,  34}, {174,  36}, {214,  36},
  {131,  37}, {243,  42}, { 30,  44}, { 70,  44}, {134,  44},
  { 99,  49}, {187,  49}, { 19,  50}, {151,  53}, {238,  60},
  {124,  62}, { 59,  66}, {107,  66}, {171,  66}, { 22,  68},
  { 86,  68}, {206,  68}, {227,  74}, {122,  79}, { 35,  82},
  {195,  82}, {174,  84}, {  6,  92}, {102,  92}, {230, 100},
  {142, 103}, { 97, 105}, { 19, 106}, { 69, 108}, { 30, 116},
  { 67, 122}, {203, 122}, {174, 124},
};

static uint8_t vgm[175866] = {
#include "ymvgm.inc"
};
static int32_t vgm_len = 175866;

static int32_t blinkTime = 0;
static int32_t blinkStar = -1;
static int32_t numStars = 58;

static int32_t fallingStar = 0;
static int32_t fX = 0, fY = 0, fOldX = 0, fOldY = 0;
static int32_t fSpdX = 0, fSpdY = 0;

static int16_t *soundBuf = 0;
static int32_t soundBufLen = 11235945;
static int32_t soundLen = 0;

static PSGCONTEXT *psg = NULL;
static OPLL *opl = NULL;

BITMAPINFO bmiScreen;
HDC hDC;
HWND hWnd;
uint32_t *screen = NULL;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void DrawPixels(HWND hWnd);

int GetTime(void);

void InitScreen(void);
void DrawFrame(void);
void PutPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
void PutSprite(uint32_t *spr, uint8_t *pal, int w, int h, int x, int y);
void PutFallingStar(int fX, int fY, int fOldX, int fOldY, int put);
void PutStars(STAR *stars, int32_t num_stars);
void PutFloor(int ys, int t);

void InitSound(void *buf, int32_t len);
void StartSound(void);
void EndSound(void);
int32_t GenerateSong(int16_t *buf, int32_t bufLen);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
  MSG  msg;
  WNDCLASSW wc = {0};

  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpszClassName = L"SMS Demo";
  wc.hInstance     = hInstance;
  wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
  wc.lpfnWndProc   = WndProc;
  wc.hCursor       = LoadCursor(0, IDC_ARROW);

  RegisterClassW(&wc);
  CreateWindowW(
    wc.lpszClassName, wc.lpszClassName,
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
    100, 100, TARGET_WIDTH+16, TARGET_HEIGHT+38,
    NULL, NULL, hInstance, NULL
  );

  soundBuf = calloc(soundBufLen, 2);
  soundLen = GenerateSong(soundBuf, soundBufLen);
  InitSound(soundBuf, soundLen * 2);

  hWnd = GetActiveWindow();
  hDC = GetDC(hWnd);
  InitScreen();
  StartSound();

  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  srand(time(NULL));

  return (int) msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  static uint32_t idTimer = 1;
  switch(msg) {
    case WM_CREATE:
      SetTimer(hWnd, idTimer = 1, 16, NULL);
      return 0;

    case WM_TIMER:
      if (wParam == 1) {
        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
      }
      break;

    case WM_PAINT:
      DrawPixels(hWnd);
      break;

    case WM_DESTROY:
      KillTimer(hWnd, 1); 
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void DrawPixels(HWND hWnd) {
  PAINTSTRUCT ps;
  RECT r;

  GetClientRect(hWnd, &r);

  if (r.bottom == 0) return;

  DrawFrame();

  HDC hDC = BeginPaint(hWnd, &ps);
  StretchDIBits(
    hDC, 
    0, 0, TARGET_WIDTH, TARGET_HEIGHT,
    0, 0, WIDTH, HEIGHT,
    screen, &bmiScreen,
    DIB_RGB_COLORS, SRCCOPY
  );
  EndPaint(hWnd, &ps);
}

// utils

int GetTime(void) {
  static int32_t globalTime = 0;
  static int32_t startTime = 0;
  globalTime = GetTickCount();
  if (!startTime) startTime = globalTime;
  globalTime -= startTime;
  return globalTime;
}

// drawing stuff

void InitScreen(void) {
  ZeroMemory(&bmiScreen, sizeof(BITMAPINFO));
  bmiScreen.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmiScreen.bmiHeader.biWidth = WIDTH;
  bmiScreen.bmiHeader.biHeight = HEIGHT;
  bmiScreen.bmiHeader.biPlanes = 1;
  bmiScreen.bmiHeader.biBitCount = 32;
  bmiScreen.bmiHeader.biCompression = BI_RGB;
  screen = calloc(WIDTH * HEIGHT, sizeof(uint32_t));
}

void DrawFrame(void) {
  if (blinkStar >= 0) {
    blinkTime--;
    if (blinkTime <= 0) {
      stars[blinkStar].blink = 0;
      blinkStar = -1;
    }
  } else if (rand() % 100 == 0) {
    blinkTime = 6;
    blinkStar = rand() % numStars;
    stars[blinkStar].blink = 1;
  }

  PutStars(stars, numStars);

  int t = GetTime();

  if (fallingStar) {
    fX += fSpdX;
    fY += fSpdY;
    if (t & 1) fSpdY += 1;
    if (fX > 1 && fY > 1 && fX < WIDTH-1 && fY < 142) {
      PutFallingStar(fX, fY, fOldX, fOldY, 1);
      fOldX = fX;
      fOldY = fY;
    } else {
      PutFallingStar(fX, fY, fOldX, fOldY, 0);
      fallingStar = 0;
    }
  } else if ((rand() & 0x7F) == 0) {
    fX = 2 + rand() % (WIDTH - 3);
    fY = 2;
    fOldX = fX; fOldY = fY;
    fSpdX = (rand() & 1) ? -4 : 4;
    fSpdY = 1;
    fallingStar = 1;
  }

  PutSprite(spr_sega, pal_sega, 80, 25, 88, 8);
  PutSprite(spr_logo, pal_text, 192, 18, 40, 40);
  PutSprite(spr_text1, pal_text, 128, 16, 41, 72);
  PutSprite(spr_text2, pal_text, 176, 16, 44, 97);
  PutSprite(spr_copyright1, pal_text, 96, 8, 80, 120);
  PutSprite(spr_copyright2, pal_text, 32, 7, 208, 120);
  PutFloor(142, t);
}

__inline void PutPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  screen[(HEIGHT - y) * WIDTH + x] = b | (g << 8) | (r << 16);
}

void PutSprite(uint32_t *spr, uint8_t *pal, int w, int h, int x, int y) {
  for (int sy = 0; sy < h; ++sy) {
    for (int sx = 0; sx < w; ++sx) {
      uint32_t i = (sy * w + sx) >> 4;
      uint32_t ci = 3 * ((spr[i] >> ((sx & 0xF) << 1)) & 0x3);
      if (ci) PutPixel(x+sx, y+sy, pal[ci+0], pal[ci+1], pal[ci+2]);
    }
  }
}

void PutFallingStar(int fX, int fY, int fOldX, int fOldY, int put) {
  PutPixel(fOldX, fOldY, 0x00, 0x00, 0x00);
  PutPixel(fOldX+1, fOldY, 0x00, 0x00, 0x00);
  PutPixel(fOldX, fOldY+1, 0x00, 0x00, 0x00);
  PutPixel(fOldX-1, fOldY, 0x00, 0x00, 0x00);
  PutPixel(fOldX, fOldY-1, 0x00, 0x00, 0x00);
  if (put) {
    PutPixel(fX, fY, 0xC0, 0xC0, 0x80);
    PutPixel(fX+1, fY, 0xC0, 0xC0, 0x80);
    PutPixel(fX, fY+1, 0xC0, 0xC0, 0x80);
    PutPixel(fX-1, fY, 0xC0, 0xC0, 0x80);
    PutPixel(fX, fY-1, 0xC0, 0xC0, 0x80);
  }
}

void PutStars(STAR *stars, int32_t numStars) {
  for (STAR *s = stars; s < stars + numStars; ++s) {
    if (!s->blink) {
      PutPixel(s->x, s->y, 0x00, 0x80, 0xC0);
      PutPixel(s->x+1, s->y, 0x00, 0x00, 0x00);
      PutPixel(s->x, s->y+1, 0x00, 0x00, 0x00);
      PutPixel(s->x-1, s->y, 0x00, 0x00, 0x00);
      PutPixel(s->x, s->y-1, 0x00, 0x00, 0x00);
    } else {
      PutPixel(s->x, s->y, 0xC0, 0xC0, 0xC0);
      PutPixel(s->x+1, s->y, 0x00, 0x80, 0xC0);
      PutPixel(s->x, s->y+1, 0x00, 0x80, 0xC0);
      PutPixel(s->x-1, s->y, 0x00, 0x80, 0xC0);
      PutPixel(s->x, s->y-1, 0x00, 0x80, 0xC0);
    }
  }
}

int FloorTexture(int x, int y) {
  x = abs(x) & 1;
  y = abs(y) & 1;
  return x ^ y;
}

void PutFloor(int ys, int t) {
  static const uint8_t red[] = {
    0x00, 0x40, 0x80, 0xC0,
    0xC0, 0x80, 0x40, 0x00
  };
  static const uint8_t blu[] = {
    0x00, 0x40, 0xC0, 0x80
  };
  static const float horizon = 20.f;
  static const float fov = 200.f;
  static float dy = 0.f;
  uint8_t r = red[(t / 8500) % 8], b = blu[(t / 34000) % 4]; 
  for (int y = ys; y <= HEIGHT; ++y) {
    for (int x = 0; x < WIDTH; ++x) {
      float px = x - WIDTH / 2.f;
      float py = fov;
      float pz = 1.f / ((y - ys) + horizon);
      float sx = px * pz + 1;
      float sy = py * pz + dy;
      float scale = 0.5f;
      float c = FloorTexture(sx * scale + WIDTH / 2.f, sy * scale);
      PutPixel(x, y, r, 0x40 + c * 0x40, b);
    }
  }
  dy += 0.25f;
}

// sound

#define PLAYER_RATE         44100
#define PLAYER_NUMCHANNELS  1
#define PLAYER_LATENCY      20

static const WAVEFORMATEX wavinfo = {
  WAVE_FORMAT_PCM,
  PLAYER_NUMCHANNELS,
  PLAYER_RATE,
  PLAYER_RATE * 2 * PLAYER_NUMCHANNELS,
  2 * PLAYER_NUMCHANNELS,
  16, 0
};

static HWAVEOUT hWav = 0;
static WAVEHDR wav = { 
  0, 0, 0, 0,
  WHDR_BEGINLOOP | WHDR_ENDLOOP,
  0xFFFFFFFF, 0, 0
};
static int wavInit = 0;

void InitSound(void *buf, int32_t len) {
  wav.dwBufferLength = len;
  wav.lpData = buf;

  if (waveOutOpen(&hWav, WAVE_MAPPER, &wavinfo, 0, 0, 0))
    return;

  waveOutPrepareHeader(hWav, &wav, sizeof(WAVEHDR));
  wavInit = 1;
}

void StartSound(void) {
  if (wavInit) waveOutWrite(hWav, &wav, sizeof(WAVEHDR));
}

void EndSound(void) {
  if (wavInit) {
    waveOutReset(hWav);
    waveOutClose(hWav);
    wavInit = 0;
  }
}

int32_t GenerateSong(int16_t *buf, int32_t bufLen) {
  int32_t *temp = calloc(bufLen, 4);
  int32_t *bufs[2] = {temp, temp};
  int32_t *opltemp = calloc(bufLen, 4);
  int32_t *oplbufs[2] = {opltemp, opltemp};

  psg = PSGInit(3579540, 44100);
  PSGReset(psg);
  opl = OPLLInit(3579540, 44100);
  OPLLReset(opl);
  OPLLSetMuteMask(opl, 0);

  int wait = 0;
  int samps = 0;
  int offset = 0;
  for (int i = 0; i < vgm_len;) {
    if (wait) {
      wait--;
      samps++;
      continue;
    }

    if (samps >= 882) {
      int len = bufs[0] - temp;
      if (len + samps > bufLen) {
        printf("buffer overflow: left %d want %d\n", bufLen - len, samps);
        samps = bufLen - len;
        break;
      }
      PSGUpdate(psg, bufs, samps);
      bufs[0] += samps;
      bufs[1] += samps;
      len = oplbufs[0] - opltemp;
      if (len + samps > bufLen) {
        printf("buffer overflow: left %d want %d\n", bufLen - len, samps);
        samps = bufLen - len;
        break;
      }
      OPLLCalcStereo(opl, oplbufs, samps);
      oplbufs[0] += samps;
      oplbufs[1] += samps;
      offset += samps;
      samps = 0;
    }

    uint8_t cmd = vgm[i];
    uint8_t a, b, c;
    switch (cmd) {
      case 0x4F:
        // ignore this
        a = vgm[++i];
        break;
      case 0x50:
        PSGWrite(psg, vgm[++i]);
        break;
      case 0x51:
        a = vgm[++i];
        b = vgm[++i];
        OPLLWriteReg(opl, a, b);
        break;
      case 0x61:
        a = vgm[++i];
        b = vgm[++i];
        wait = a | (b << 8);
        break;
      case 0x62:
        wait = 735;
        break;
      case 0x63:
        wait = 882;
        break;
      case 0x66:
        i = vgm_len;
        break;
      default:
        printf("unimpl opcode %02x at %x\n", cmd, i);
        break;
    }

    ++i;
  }

  if (bufs[0] < temp + bufLen)
    PSGUpdate(psg, bufs, samps);
  if (oplbufs[0] < opltemp + bufLen)
    OPLLCalcStereo(opl, oplbufs, samps);

  for (int i = 0; i < bufLen; ++i) {
    buf[i] = temp[i] / 4 + opltemp[i] * 2;
  }

  PSGShutdown(psg);
  OPLLShutdown(opl);
  free(temp);
  free(opltemp);
  return bufLen;
}
