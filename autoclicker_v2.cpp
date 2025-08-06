#include <windows.h>
#include <commctrl.h>   // Pour InitCommonControlsEx, UPDOWN_CLASS, TRACKBAR_CLASS
#include <mmsystem.h>   // Pour timeBeginPeriod et timeEndPeriod
#include <thread>
#include <atomic>
#include <random>
#include <string>
#include <fstream>
#include <sstream>
// #include <chrono>       // Plus nécessaire pour cette version simplifiée
// #include <cmath>        // Plus nécessaire pour cette version simplifiée

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib") // Lier avec la bibliothèque winmm.lib pour timeBeginPeriod

// Définition de constantes pour les IDs de contrôle
#define ID_TOGGLE_MODE_BUTTON 1
#define ID_START_STOP_BUTTON 2
#define ID_SET_KEY_BUTTON 3
#define ID_CLICK_SELECT_COMBOBOX 4
#define ID_CPS_MINUS_BUTTON 5 // Bouton pour diminuer les CPS
#define ID_CPS_PLUS_BUTTON 6  // Bouton pour augmenter les CPS

// Message personnalisé pour démarrer l'attente de la touche d'activation
#define WM_START_KEY_WAITING (WM_APP + 1)

// Handles des contrôles de l'interface
HWND hwndSlider, hwndToggle, hwndBtn, hwndKeyBtn, hwndKeyLabel, hwndCpsLabel, hwndClickSelect;
HWND hwndCpsMinusBtn, hwndCpsPlusBtn; // Handles pour les boutons +/-

int cps = 15;
const int MAX_CPS = 5000; // NOUVEAU : Définition de la valeur maximale des CPS à 10000
const int MIN_CPS = 1;    // Définition de la valeur minimale des CPS

std::atomic<bool> clicking(false);
std::atomic<bool> running(false);
std::atomic<bool> waitingForKey(false);
bool toggleMode = false;
int activationKey = VK_F6;
int clickButton = 0; // 0 for Left, 1 for Right, 2 for Middle

std::mt19937 rng(std::random_device{}());

HBRUSH hBrushDark = NULL; // Pinceau pour le mode sombre

// --- Fonctions de gestion de la configuration ---
void SaveConfig() {
    std::ofstream f("config.ini");
    if (f.is_open()) {
        f << cps << "\n" << clickButton << "\n" << activationKey << "\n" << toggleMode;
        f.close();
    }
}

void LoadConfig() {
    std::ifstream f("config.ini");
    if (f.is_open()) {
        f >> cps >> clickButton >> activationKey >> toggleMode;
        f.close();
    }
    // Assurer que les valeurs chargées sont dans les limites
    if (cps < MIN_CPS) cps = MIN_CPS;
    if (cps > MAX_CPS) cps = MAX_CPS;
}

// --- Fonction pour simuler un clic de souris ---
void DoClick(int btn) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    if (btn == 0) input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    else if (btn == 1) input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    else input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
    SendInput(1, &input, sizeof(INPUT));

    ZeroMemory(&input, sizeof(INPUT));
    input.type = INPUT_MOUSE;
    if (btn == 0) input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    else if (btn == 1) input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    else input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
    SendInput(1, &input, sizeof(INPUT));
}

// --- Thread d'auto-clic ---
void ClickThread() {
    std::uniform_real_distribution<> jitter(0.9, 1.1); // Petite variation pour rendre les clics moins robotiques
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);

    while (running) {
        if (clicking) {
            DoClick(clickButton);
            // Calcul du délai basé directement sur le CPS demandé
            // La précision est maintenant gérée par timeBeginPeriod et QueryPerformanceCounter
            double delayMs = 1000.0 / cps * jitter(rng);

            LARGE_INTEGER start, now;
            QueryPerformanceCounter(&start);
            do {
                QueryPerformanceCounter(&now);
                double elapsedMs = (now.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
                if (elapsedMs >= delayMs) {
                    break;
                }
                std::this_thread::yield(); // Cède le temps processeur, efficace avec timeBeginPeriod(1)
            } while (true);
        } else {
            Sleep(10);
        }
    }
}

// --- Thread de surveillance des touches d'activation ---
void KeyMonitor() {
    bool wasPressed = false;
    while (running) {
        bool isPressed = (GetAsyncKeyState(activationKey) & 0x8000) != 0;
        if (toggleMode) {
            if (isPressed && !wasPressed) {
                clicking = !clicking;
            }
        } else {
            clicking = isPressed;
        }
        wasPressed = isPressed;
        Sleep(10);
    }
}

// --- Fonction pour obtenir le nom des touches/boutons ---
std::string GetKeyName(int vk) {
    UINT scan = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    char name[128];
    if (scan != 0 && GetKeyNameTextA(scan << 16, name, sizeof(name))) {
        return name;
    }

    switch (vk) {
        case VK_LBUTTON: return "Mouse1 (Left)";
        case VK_RBUTTON: return "Mouse2 (Right)";
        case VK_MBUTTON: return "Mouse3 (Middle)";
        case VK_XBUTTON1: return "Mouse4";
        case VK_XBUTTON2: return "Mouse5";
        case VK_BACK: return "Backspace";
        case VK_TAB: return "Tab";
        case VK_RETURN: return "Enter";
        case VK_SHIFT: return "Shift";
        case VK_CONTROL: return "Ctrl";
        case VK_MENU: return "Alt";
        case VK_PAUSE: return "Pause";
        case VK_CAPITAL: return "Caps Lock";
        case VK_ESCAPE: return "Esc";
        case VK_SPACE: return "Space";
        case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down";
        case VK_END: return "End";
        case VK_HOME: return "Home";
        case VK_LEFT: return "Left Arrow";
        case VK_UP: return "Up Arrow";
        case VK_RIGHT: return "Right Arrow";
        case VK_DOWN: return "Down Arrow";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_NUMLOCK: return "Num Lock";
        case VK_SCROLL: return "Scroll Lock";
        case VK_LSHIFT: return "Left Shift";
        case VK_RSHIFT: return "Right Shift";
        case VK_LCONTROL: return "Left Ctrl";
        case VK_RCONTROL: return "Right Ctrl";
        case VK_LMENU: return "Left Alt";
        case VK_RMENU: return "Right Alt";
        case VK_OEM_1: return "Semi-colon (;)";
        case VK_OEM_PLUS: return "Plus (+)";
        case VK_OEM_COMMA: return "Comma (,)";
        case VK_OEM_MINUS: return "Minus (-)";
        case VK_OEM_PERIOD: return "Period (.)";
        case VK_OEM_2: return "Question Mark (?)";
        case VK_OEM_3: return "Tilde (`)";
        case VK_OEM_4: return "Open Bracket ([)";
        case VK_OEM_5: return "Backslash (\\)";
        case VK_OEM_6: return "Close Bracket (])";
        case VK_OEM_7: return "Quote (')'";
        default:
            if (vk >= '0' && vk <= '9') return std::string(1, (char)vk);
            if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
            return "Unknown";
    }
}

// --- Met à jour les labels de l'interface ---
void UpdateLabels() {
    std::string cpsTxt = "CPS: " + std::to_string(cps); // Affichage "CPS: [valeur]"
    SetWindowTextA(hwndCpsLabel, cpsTxt.c_str());

    // Met à jour la position du slider pour qu'elle corresponde à la valeur cps
    SendMessage(hwndSlider, TBM_SETPOS, TRUE, cps);

    std::string keyTxt = "Activation Key: " + GetKeyName(activationKey);
    SetWindowTextA(hwndKeyLabel, keyTxt.c_str());
}

// --- Procédure de fenêtre principale (WndProc) ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static std::thread clicker, keyThread;

    switch (msg) {
        case WM_CREATE: // Message envoyé lors de la création de la fenêtre
            hBrushDark = CreateSolidBrush(RGB(30, 30, 30));
            SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrushDark);

            hwndCpsLabel = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD, 30, 10, 220, 20, hwnd, NULL, NULL, NULL);
            SetWindowTextA(hwndCpsLabel, ("CPS: " + std::to_string(cps)).c_str());

            // Création des boutons +/- pour le CPS
            hwndCpsMinusBtn = CreateWindowA("BUTTON", "-", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10, 30, 20, 25, hwnd, (HMENU)ID_CPS_MINUS_BUTTON, NULL, NULL);
            hwndCpsPlusBtn = CreateWindowA("BUTTON", "+", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 250, 30, 20, 25, hwnd, (HMENU)ID_CPS_PLUS_BUTTON, NULL, NULL);

            // NOUVEAU : Ajustement de la largeur du slider pour une meilleure "sensation" de précision
            // Bien que la précision absolue reste limitée par la résolution d'écran,
            // une plus grande largeur peut aider à des mouvements plus fins.
            // Et la plage est maintenant jusqu'à MAX_CPS.
            hwndSlider = CreateWindowEx(0, TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 35, 30, 210, 30, hwnd, NULL, NULL, NULL);
            SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELPARAM(MIN_CPS, MAX_CPS)); // Plage jusqu'à MAX_CPS (10000)
            SendMessage(hwndSlider, TBM_SETPOS, TRUE, cps);

            hwndClickSelect = CreateWindowA("COMBOBOX", NULL, CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE, 30, 70, 120, 100, hwnd, (HMENU)ID_CLICK_SELECT_COMBOBOX, NULL, NULL);
            SendMessage(hwndClickSelect, CB_ADDSTRING, 0, (LPARAM)"Left Click");
            SendMessage(hwndClickSelect, CB_ADDSTRING, 0, (LPARAM)"Right Click");
            SendMessage(hwndClickSelect, CB_ADDSTRING, 0, (LPARAM)"Middle Click");
            SendMessage(hwndClickSelect, CB_SETCURSEL, clickButton, 0);

            hwndToggle = CreateWindowA("BUTTON", "Toggle Mode", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 170, 70, 100, 20, hwnd, (HMENU)ID_TOGGLE_MODE_BUTTON, NULL, NULL);
            SendMessage(hwndToggle, BM_SETCHECK, toggleMode ? BST_CHECKED : BST_UNCHECKED, 0);

            hwndKeyBtn = CreateWindowA("BUTTON", "Set Activation Key", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 30, 100, 120, 25, hwnd, (HMENU)ID_SET_KEY_BUTTON, NULL, NULL);
            hwndKeyLabel = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD, 30, 130, 250, 20, hwnd, NULL, NULL, NULL);

            hwndBtn = CreateWindowA("BUTTON", "Start", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 90, 170, 120, 30, hwnd, (HMENU)ID_START_STOP_BUTTON, NULL, NULL);

            UpdateLabels();
            break;

        case WM_HSCROLL: // Message du slider
            if ((HWND)lParam == hwndSlider) {
                cps = SendMessage(hwndSlider, TBM_GETPOS, 0, 0); // Récupère la position du slider (CPS)
                UpdateLabels();
            }
            break;

        case WM_COMMAND: // Message des contrôles (boutons, combobox)
            switch (LOWORD(wParam)) {
                case ID_TOGGLE_MODE_BUTTON: // Clic sur le bouton "Toggle Mode"
                    toggleMode = SendMessage(hwndToggle, BM_GETCHECK, 0, 0); // Récupère l'état de la checkbox
                    UpdateLabels();
                    break;
                case ID_START_STOP_BUTTON: // Clic sur le bouton "Start/Stop"
                    if (!running) {
                        running = true;
                        clicking = false;
                        clicker = std::thread(ClickThread);
                        keyThread = std::thread(KeyMonitor);
                        SetWindowTextA(hwndBtn, "Stop");
                    } else {
                        running = false;
                        clicking = false;
                        if (clicker.joinable()) clicker.join();
                        if (keyThread.joinable()) keyThread.join();
                        SaveConfig();
                        SetWindowTextA(hwndBtn, "Start");
                    }
                    break;
                case ID_SET_KEY_BUTTON: // Clic sur le bouton "Set Activation Key"
                    SetWindowTextA(hwndKeyBtn, "Press key...");
                    // Désactiver les contrôles pertinents
                    EnableWindow(hwndSlider, FALSE);
                    EnableWindow(hwndToggle, FALSE);
                    EnableWindow(hwndClickSelect, FALSE);
                    EnableWindow(hwndBtn, FALSE);
                    EnableWindow(hwndCpsMinusBtn, FALSE);
                    EnableWindow(hwndCpsPlusBtn, FALSE);
                    // Envoie un message personnalisé pour démarrer l'attente après le clic du bouton
                    PostMessage(hwnd, WM_START_KEY_WAITING, 0, 0);
                    break;
                case ID_CPS_MINUS_BUTTON: // Clic sur le bouton '-'
                    if (cps > MIN_CPS) {
                        cps--;
                        UpdateLabels();
                    }
                    break;
                case ID_CPS_PLUS_BUTTON: // Clic sur le bouton '+'
                    if (cps < MAX_CPS) {
                        cps++;
                        UpdateLabels();
                    }
                    break;
                case ID_CLICK_SELECT_COMBOBOX: // Changement de sélection dans le combobox
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        int index = SendMessage(hwndClickSelect, CB_GETCURSEL, 0, 0);
                        clickButton = index;
                    }
                    break;
            }
            break;

        case WM_START_KEY_WAITING: // Message personnalisé pour démarrer l'attente de touche
            waitingForKey = true;
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
            if (waitingForKey) {
                if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
                    activationKey = (int)wParam;
                }
                else if (msg == WM_LBUTTONDOWN) activationKey = VK_LBUTTON;
                else if (msg == WM_RBUTTONDOWN) activationKey = VK_RBUTTON;
                else if (msg == WM_MBUTTONDOWN) activationKey = VK_MBUTTON;
                else if (msg == WM_XBUTTONDOWN) {
                    if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) activationKey = VK_XBUTTON1;
                    else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2) activationKey = VK_XBUTTON2;
                }

                waitingForKey = false;
                SetWindowTextA(hwndKeyBtn, "Set Activation Key");
                UpdateLabels();
                // Réactiver tous les contrôles
                EnableWindow(hwndSlider, TRUE);
                EnableWindow(hwndToggle, TRUE);
                EnableWindow(hwndClickSelect, TRUE);
                EnableWindow(hwndBtn, TRUE);
                EnableWindow(hwndCpsMinusBtn, TRUE);
                EnableWindow(hwndCpsPlusBtn, TRUE);
            }
            break;

        case WM_CTLCOLORSTATIC: // Pour le mode sombre des labels
            {
                HDC hdcStatic = (HDC)wParam;
                SetTextColor(hdcStatic, RGB(255, 255, 255));
                SetBkMode(hdcStatic, TRANSPARENT);
                return (LRESULT)hBrushDark;
            }
        case WM_CTLCOLORBTN: // Pour le mode sombre des boutons
            {
                HDC hdcButton = (HDC)wParam;
                SetTextColor(hdcButton, RGB(255, 255, 255));
                SetBkMode(hdcButton, TRANSPARENT);
                return (LRESULT)hBrushDark;
            }

        case WM_CLOSE: // Message envoyé lors de la fermeture de la fenêtre (bouton X)
            running = false;
            clicking = false;
            if (clicker.joinable()) clicker.join();
            if (keyThread.joinable()) keyThread.join();
            SaveConfig();
            DestroyWindow(hwnd);
            if (hBrushDark) {
                DeleteObject(hBrushDark);
                hBrushDark = NULL;
            }
            break;

        case WM_DESTROY: // Message envoyé après la destruction de la fenêtre
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- Fonction WinMain (point d'entrée de l'application) ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // NOUVEAU : Demander au système d'exploitation d'augmenter la résolution du timer.
    // Une valeur de 1ms est généralement le minimum supporté.
    // Cela rendra Sleep(0) et QueryPerformanceCounter plus précis.
    timeBeginPeriod(1);

    // Initialise les contrôles communs (nécessaire pour le Trackbar)
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_BAR_CLASSES }; // ICC_BAR_CLASSES pour le slider
    InitCommonControlsEx(&icex);

    LoadConfig(); // Charge la configuration sauvegardée

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "AutoClickerModern";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    HWND hwnd = CreateWindowA("AutoClickerModern", "🖱️ Auto Clicker - Fast & Black",
        WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 260,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // NOUVEAU : Très important : Rétablir la résolution du timer à sa valeur par défaut
    // lorsque l'application se termine, pour ne pas impacter les autres applications.
    timeEndPeriod(1);

    return 0;
}