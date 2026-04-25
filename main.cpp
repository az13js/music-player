#include <windows.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <algorithm>

#define ID_BTN_OPEN 101
#define ID_BTN_OPEN_FOLDER 107
#define ID_BTN_PLAY 102
#define ID_BTN_PAUSE 103
#define ID_BTN_STOP 104
#define ID_BTN_PREV 108
#define ID_BTN_NEXT 109
#define ID_TRACKBAR 105
#define ID_TIMER 106
#define ID_LISTBOX 110

static std::wstring g_filePath = L"";
static HWND g_hwndTrackbar = NULL;
static HWND g_hwndTimeLabel = NULL;
static HWND g_hwndListbox = NULL;
static int g_totalLength = 0;
static bool g_isPlaying = false;
static bool g_isDragging = false;
static int g_currentIndex = -1;
static std::vector<std::wstring> g_playlist;
static std::wstring g_folderPath;

static std::wstring FormatTime(int seconds) {
  int mins = seconds / 60;
  int secs = seconds % 60;
  wchar_t buf[32];
  swprintf(buf, 32, L"%02d:%02d", mins, secs);
  return buf;
}

static std::wstring GetFileName(const std::wstring& path) {
  size_t pos = path.find_last_of(L"\\/");
  if (pos != std::wstring::npos) {
    return path.substr(pos + 1);
  }
  return path;
}

static bool IsAudioFile(const std::wstring& path) {
  std::wstring ext = path;
  size_t pos = ext.find_last_of(L".");
  if (pos != std::wstring::npos) {
    ext = ext.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext == L".mp3" || ext == L".wav" || ext == L".wma" || ext == L".mid" || ext == L".midi";
  }
  return false;
}

static void RefreshPlaylist() {
  SendMessage(g_hwndListbox, LB_RESETCONTENT, 0, 0);
  for (size_t i = 0; i < g_playlist.size(); i++) {
    SendMessage(g_hwndListbox, LB_ADDSTRING, 0, (LPARAM)GetFileName(g_playlist[i]).c_str());
  }
  if (g_currentIndex >= 0 && g_currentIndex < (int)g_playlist.size()) {
    SendMessage(g_hwndListbox, LB_SETCURSEL, g_currentIndex, 0);
  }
}

static void LoadFolder(const std::wstring& folderPath) {
  g_playlist.clear();
  g_folderPath = folderPath;
  
  WIN32_FIND_DATA findData;
  std::wstring searchPath = folderPath + L"\\*";
  HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
  
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wstring fileName = findData.cFileName;
        if (IsAudioFile(fileName)) {
          g_playlist.push_back(folderPath + L"\\" + fileName);
        }
      }
    } while (FindNextFile(hFind, &findData));
    FindClose(hFind);
  }
  
  std::sort(g_playlist.begin(), g_playlist.end());
  RefreshPlaylist();
  g_currentIndex = -1;
}

static void PlayFile(const std::wstring& filePath, HWND hwnd, int index) {
  mciSendString(L"close myaudio", NULL, 0, NULL);
  g_filePath = filePath;
  std::wstring cmd = L"open \"" + g_filePath + L"\" alias myaudio";
  mciSendString(cmd.c_str(), NULL, 0, NULL);
  
  mciSendString(L"set myaudio time format milliseconds", NULL, 0, NULL);
  
  wchar_t lenBuf[64] = {};
  mciSendString(L"status myaudio length", lenBuf, 64, NULL);
  g_totalLength = _wtoi(lenBuf) / 1000;
  if (g_totalLength <= 0) g_totalLength = 1;
  
  SendMessage(g_hwndTrackbar, TBM_SETPOS, TRUE, 0);
  std::wstring initTime = L"00:00 / " + FormatTime(g_totalLength);
  SetWindowText(g_hwndTimeLabel, initTime.c_str());
  
  g_currentIndex = index;
  SendMessage(g_hwndListbox, LB_SETCURSEL, index, 0);
  
  mciSendString(L"play myaudio", NULL, 0, NULL);
  g_isPlaying = true;
  SetTimer(hwnd, ID_TIMER, 200, NULL);
  
  std::wstring title = L"Music Player - " + GetFileName(filePath);
  SetWindowText(hwnd, title.c_str());
}

static void PlayNext(HWND hwnd) {
  if (g_playlist.empty()) return;
  int nextIndex = g_currentIndex + 1;
  if (nextIndex >= (int)g_playlist.size()) {
    nextIndex = 0;
  }
  PlayFile(g_playlist[nextIndex], hwnd, nextIndex);
}

static void PlayPrev(HWND hwnd) {
  if (g_playlist.empty()) return;
  int prevIndex = g_currentIndex - 1;
  if (prevIndex < 0) {
    prevIndex = (int)g_playlist.size() - 1;
  }
  PlayFile(g_playlist[prevIndex], hwnd, prevIndex);
}

static void UpdateProgress(HWND hwnd) {
  if (!g_isPlaying || g_isDragging || g_filePath.empty()) return;
  
  wchar_t posBuf[64] = {};
  wchar_t lenBuf[64] = {};
  mciSendString(L"status myaudio position", posBuf, 64, NULL);
  mciSendString(L"status myaudio length", lenBuf, 64, NULL);
  
  int pos = _wtoi(posBuf);
  int len = _wtoi(lenBuf);
  
  if (len > 0) {
    int percent = (pos * 100) / len;
    if (percent > 100) percent = 100;
    SendMessage(g_hwndTrackbar, TBM_SETPOS, TRUE, percent);
    
    int currentSec = (pos * g_totalLength) / len;
    std::wstring timeText = FormatTime(currentSec) + L" / " + FormatTime(g_totalLength);
    SetWindowText(g_hwndTimeLabel, timeText.c_str());
  }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_COMMAND: {
    int wmId = LOWORD(wParam);
    int wmEvent = HIWORD(wParam);
    switch (wmId) {
    case ID_BTN_OPEN: {
      OPENFILENAME ofn = {};
      wchar_t szFile[MAX_PATH] = {};
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = hwnd;
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = MAX_PATH;
      ofn.lpstrFilter = L"音频文件\0*.mp3;*.wav;*.wma;*.mid;*.midi\0所有文件\0*.*\0";
      ofn.nFilterIndex = 1;
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

      if (GetOpenFileName(&ofn)) {
        g_playlist.clear();
        g_playlist.push_back(szFile);
        g_currentIndex = 0;
        RefreshPlaylist();
        PlayFile(szFile, hwnd, 0);
      }
      break;
    }
    case ID_BTN_OPEN_FOLDER: {
      BROWSEINFO bi = {};
      bi.hwndOwner = hwnd;
      bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
      LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
      if (pidl) {
        wchar_t folderPath[MAX_PATH];
        if (SHGetPathFromIDList(pidl, folderPath)) {
          LoadFolder(folderPath);
          std::wstring title = L"Music Player - " + std::wstring(folderPath);
          SetWindowText(hwnd, title.c_str());
        }
        CoTaskMemFree(pidl);
      }
      break;
    }
    case ID_BTN_PLAY: {
      if (!g_filePath.empty()) {
        mciSendString(L"play myaudio", NULL, 0, NULL);
        g_isPlaying = true;
        SetTimer(hwnd, ID_TIMER, 200, NULL);
      } else if (!g_playlist.empty()) {
        PlayFile(g_playlist[0], hwnd, 0);
      }
      break;
    }
    case ID_BTN_PAUSE: {
      mciSendString(L"pause myaudio", NULL, 0, NULL);
      g_isPlaying = false;
      KillTimer(hwnd, ID_TIMER);
      break;
    }
    case ID_BTN_STOP: {
      mciSendString(L"stop myaudio", NULL, 0, NULL);
      mciSendString(L"seek myaudio to start", NULL, 0, NULL);
      g_isPlaying = false;
      KillTimer(hwnd, ID_TIMER);
      SendMessage(g_hwndTrackbar, TBM_SETPOS, TRUE, 0);
      std::wstring stopTime = L"00:00 / " + FormatTime(g_totalLength);
      SetWindowText(g_hwndTimeLabel, stopTime.c_str());
      break;
    }
    case ID_BTN_PREV: {
      PlayPrev(hwnd);
      break;
    }
    case ID_BTN_NEXT: {
      PlayNext(hwnd);
      break;
    }
    case ID_LISTBOX: {
      if (wmEvent == LBN_DBLCLK) {
        int sel = (int)SendMessage(g_hwndListbox, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR && sel >= 0 && sel < (int)g_playlist.size()) {
          PlayFile(g_playlist[sel], hwnd, sel);
        }
      }
      break;
    }
    }
    break;
  }
  case WM_TIMER: {
    UpdateProgress(hwnd);
    
    wchar_t statusBuf[64] = {};
    mciSendString(L"status myaudio mode", statusBuf, 64, NULL);
    if (wcsncmp(statusBuf, L"stopped", 7) == 0 && g_isPlaying) {
      g_isPlaying = false;
      KillTimer(hwnd, ID_TIMER);
      SendMessage(g_hwndTrackbar, TBM_SETPOS, TRUE, 100);
      if (!g_playlist.empty() && g_currentIndex >= 0) {
        PlayNext(hwnd);
      }
    }
    return 0;
  }
  case WM_HSCROLL: {
    HWND hwndCtl = (HWND)lParam;
    if (hwndCtl == g_hwndTrackbar) {
      int code = LOWORD(wParam);
      if (code == TB_THUMBTRACK) {
        g_isDragging = true;
        int percent = SendMessage(g_hwndTrackbar, TBM_GETPOS, 0, 0);
        int currentSec = (percent * g_totalLength) / 100;
        std::wstring timeText = FormatTime(currentSec) + L" / " + FormatTime(g_totalLength);
        SetWindowText(g_hwndTimeLabel, timeText.c_str());
      } else if (code == TB_THUMBPOSITION) {
        g_isDragging = false;
        int percent = SendMessage(g_hwndTrackbar, TBM_GETPOS, 0, 0);
        
        wchar_t lenBuf[64] = {};
        mciSendString(L"status myaudio length", lenBuf, 64, NULL);
        int rawLen = _wtoi(lenBuf);
        
        if (rawLen > 0) {
          int rawPos = (percent * rawLen) / 100;
          std::wstring cmd = L"seek myaudio to " + std::to_wstring(rawPos);
          mciSendString(cmd.c_str(), NULL, 0, NULL);
          if (g_isPlaying) {
            mciSendString(L"play myaudio", NULL, 0, NULL);
          }
        }
      }
    }
    break;
  }
  case WM_DESTROY:
    mciSendString(L"close myaudio", NULL, 0, NULL);
    KillTimer(hwnd, ID_TIMER);
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  InitCommonControls();
  CoInitialize(NULL);

  const wchar_t CLASS_NAME[] = L"MyWindowClass";

  WNDCLASS wc = {};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

  RegisterClass(&wc);

  HWND hwnd = CreateWindowEx(
    0, CLASS_NAME, L"Music Player", WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, 650, 450,
    NULL, NULL, hInstance, NULL
  );

  if (hwnd == NULL) return 0;

  CreateWindowEx(0, L"BUTTON", L"打开文件", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    20, 10, 80, 25, hwnd, (HMENU)ID_BTN_OPEN, hInstance, NULL);

  CreateWindowEx(0, L"BUTTON", L"打开文件夹", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    110, 10, 80, 25, hwnd, (HMENU)ID_BTN_OPEN_FOLDER, hInstance, NULL);

  CreateWindowEx(0, L"BUTTON", L"上一首", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    200, 10, 60, 25, hwnd, (HMENU)ID_BTN_PREV, hInstance, NULL);

  CreateWindowEx(0, L"BUTTON", L"播放", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    270, 10, 60, 25, hwnd, (HMENU)ID_BTN_PLAY, hInstance, NULL);

  CreateWindowEx(0, L"BUTTON", L"暂停", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    340, 10, 60, 25, hwnd, (HMENU)ID_BTN_PAUSE, hInstance, NULL);

  CreateWindowEx(0, L"BUTTON", L"停止", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    410, 10, 60, 25, hwnd, (HMENU)ID_BTN_STOP, hInstance, NULL);

  CreateWindowEx(0, L"BUTTON", L"下一首", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
    480, 10, 60, 25, hwnd, (HMENU)ID_BTN_NEXT, hInstance, NULL);

  g_hwndTrackbar = CreateWindowEx(0, TRACKBAR_CLASSW, NULL,
    WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
    20, 45, 500, 25, hwnd, (HMENU)ID_TRACKBAR, hInstance, NULL);
  SendMessage(g_hwndTrackbar, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
  SendMessage(g_hwndTrackbar, TBM_SETPOS, TRUE, 0);

  g_hwndTimeLabel = CreateWindowEx(0, L"STATIC", L"00:00 / 00:00",
    WS_CHILD | WS_VISIBLE | SS_CENTER,
    530, 45, 100, 25, hwnd, NULL, hInstance, NULL);

  g_hwndListbox = CreateWindowEx(WS_EX_CLIENTEDGE, L"LISTBOX", NULL,
    WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
    20, 80, 590, 300, hwnd, (HMENU)ID_LISTBOX, hInstance, NULL);

  const wchar_t* envFolder = _wgetenv(L"MUSIC_FOLDER");
  if (envFolder) {
    std::wstring folderPath = envFolder;
    DWORD attr = GetFileAttributes(folderPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
      LoadFolder(folderPath);
    }
  }

  ShowWindow(hwnd, nCmdShow);

  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  CoUninitialize();
  return 0;
}