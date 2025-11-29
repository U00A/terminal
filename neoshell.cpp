
#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <lmcons.h>
#include <windows.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace std;

struct Command {
  string name;
  vector<string> args;
  bool background;
};

class NeoShell {
private:
  string username;
  string current_theme;
  vector<string> history;
  map<string, string> aliases;
  map<string, string> env_vars;
  map<string, string> bookmarks;
  map<string, string> command_map;
  vector<string> todo_list;
  bool show_timestamps;
  bool smart_suggest;
  int command_count;
  time_t session_start;

  void initializeCommandMap() {
    // File and Directory Operations
    command_map["list"] = "ls";
    command_map["show"] = "ls";
    command_map["files"] = "ls";

    command_map["where"] = "pwd";
    command_map["whereami"] = "pwd";
    command_map["location"] = "pwd";
    command_map["current"] = "pwd";

    command_map["goto"] = "cd";
    command_map["go"] = "cd";
    command_map["navigate"] = "cd";
    command_map["moveto"] = "cd";

    command_map["makedir"] = "mkdir";
    command_map["createdir"] = "mkdir";
    command_map["newfolder"] = "mkdir";

    command_map["remove"] = "rm";
    command_map["delete"] = "rm";
    command_map["erase"] = "rm";

    command_map["duplicate"] = "cp";

    command_map["move"] = "mv";
    command_map["rename"] = "mv";

    command_map["read"] = "cat";
    command_map["view"] = "cat";
    command_map["display"] = "cat";

    command_map["find"] = "find";
    command_map["search"] = "find";
    command_map["locate"] = "find";

    command_map["edit"] = "nano";
    command_map["modify"] = "nano";

    // Text Operations
    command_map["print"] = "echo";
    command_map["say"] = "echo";
    command_map["write"] = "echo";

    command_map["count"] = "wc";
    command_map["lines"] = "wc";
    command_map["words"] = "wc";

    command_map["filter"] = "grep";
    command_map["match"] = "grep";

    command_map["order"] = "sort";
    command_map["arrange"] = "sort";

    command_map["distinct"] = "uniq";

    command_map["first"] = "head";
    command_map["top"] = "head";

    command_map["last"] = "tail";
    command_map["bottom"] = "tail";

    // System Operations
    command_map["who"] = "whoami";
    command_map["user"] = "whoami";
    command_map["me"] = "whoami";

    command_map["when"] = "date";
    command_map["time"] = "date";
    command_map["now"] = "date";

    command_map["running"] = "ps";
    command_map["processes"] = "ps";
    command_map["tasks"] = "ps";

    command_map["stop"] = "kill";
    command_map["terminate"] = "kill";

    command_map["diskspace"] = "df";
    command_map["space"] = "df";
    command_map["storage"] = "df";

    command_map["memory"] = "free";
    command_map["ram"] = "free";

    command_map["system"] = "uname";

    // Utilities
    command_map["clean"] = "clear";
    command_map["cls"] = "clear";

    command_map["commands"] = "help";
  }

  void getUsername() {
#ifdef _WIN32
    char buffer[UNLEN + 1];
    DWORD size = UNLEN + 1;
    username = GetUserNameA(buffer, &size) ? string(buffer) : "user";
#else
    struct passwd *pw = getpwuid(getuid());
    username = pw ? string(pw->pw_name) : "user";
#endif
    transform(username.begin(), username.end(), username.begin(), ::tolower);
  }

  string getCurrentTime() {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    stringstream ss;
    ss << setfill('0') << setw(2) << ltm->tm_hour << ":" << setw(2)
       << ltm->tm_min << ":" << setw(2) << ltm->tm_sec;
    return ss.str();
  }

  string getCurrentPath() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);
    return string(buffer);
#else
    char buffer[1024];
    return getcwd(buffer, sizeof(buffer)) ? string(buffer) : "";
#endif
  }

  vector<string> split(const string &str, char delimiter) {
    vector<string> tokens;
    stringstream ss(str);
    string token;
    while (getline(ss, token, delimiter)) {
      token.erase(0, token.find_first_not_of(" \t\r\n"));
      token.erase(token.find_last_not_of(" \t\r\n") + 1);
      if (!token.empty())
        tokens.push_back(token);
    }
    return tokens;
  }

  int levenshteinDistance(const string &s1, const string &s2) {
    int m = s1.length(), n = s2.length();
    vector<vector<int>> dp(m + 1, vector<int>(n + 1));

    for (int i = 0; i <= m; i++)
      dp[i][0] = i;
    for (int j = 0; j <= n; j++)
      dp[0][j] = j;

    for (int i = 1; i <= m; i++) {
      for (int j = 1; j <= n; j++) {
        if (s1[i - 1] == s2[j - 1]) {
          dp[i][j] = dp[i - 1][j - 1];
        } else {
          dp[i][j] = 1 + min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
        }
      }
    }
    return dp[m][n];
  }

  vector<string> findSimilarCommands(const string &cmd) {
    vector<pair<int, string>> scored;

    for (const auto &pair : command_map) {
      int dist = levenshteinDistance(cmd, pair.first);
      if (dist <= 2) {
        scored.push_back({dist, pair.first});
      }
    }

    for (const auto &pair : aliases) {
      int dist = levenshteinDistance(cmd, pair.first);
      if (dist <= 2) {
        scored.push_back({dist, pair.first});
      }
    }

    sort(scored.begin(), scored.end());

    vector<string> suggestions;
    for (const auto &p : scored) {
      suggestions.push_back(p.second);
    }
    return suggestions;
  }

  string translateCommand(const string &human_cmd) {
    string lower_cmd = human_cmd;
    transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(), ::tolower);

    if (command_map.find(lower_cmd) != command_map.end()) {
      return command_map[lower_cmd];
    }

    return human_cmd;
  }

  string expandVariables(const string &input) {
    string result = input;
    size_t pos = 0;
    while ((pos = result.find('$', pos)) != string::npos) {
      size_t end = pos + 1;
      while (end < result.length() &&
             (isalnum(result[end]) || result[end] == '_')) {
        end++;
      }
      string var_name = result.substr(pos + 1, end - pos - 1);
      if (env_vars.find(var_name) != env_vars.end()) {
        result.replace(pos, end - pos, env_vars[var_name]);
      }
      pos = end;
    }
    return result;
  }

  void builtinList(const vector<string> &args) {
#ifdef _WIN32
    string cmd = "dir";
    if (args.size() > 1) {
      cmd += " " + args[1];
    }
    system(cmd.c_str());
#else
    string cmd = "ls -lh --color=auto";
    if (args.size() > 1) {
      cmd += " " + args[1];
    }
    system(cmd.c_str());
#endif
  }

  void builtinRead(const vector<string> &args) {
    if (args.size() < 2) {
      cout << "Usage: read <filename>" << endl;
      return;
    }

    ifstream file(args[1]);
    if (!file.is_open()) {
      cout << "Error: Cannot open file '" << args[1] << "'" << endl;
      return;
    }

    string line;
    while (getline(file, line)) {
      cout << line << endl;
    }
    file.close();
  }

  void builtinMakeDir(const vector<string> &args) {
    if (args.size() < 2) {
      cout << "Usage: makedir <directory name>" << endl;
      return;
    }

#ifdef _WIN32
    if (!CreateDirectoryA(args[1].c_str(), NULL)) {
      DWORD error = GetLastError();
      if (error == ERROR_ALREADY_EXISTS) {
        cout << "Directory already exists: " << args[1] << endl;
      } else {
        cout << "Error creating directory: " << args[1] << endl;
      }
    } else {
      cout << "Directory created: " << args[1] << endl;
    }
#else
    if (mkdir(args[1].c_str(), 0755) == 0) {
      cout << "Directory created: " << args[1] << endl;
    } else {
      cout << "Error creating directory: " << args[1] << endl;
    }
#endif
  }

  void builtinRemove(const vector<string> &args) {
    if (args.size() < 2) {
      cout << "Usage: remove <filename>" << endl;
      return;
    }

    if (remove(args[1].c_str()) == 0) {
      cout << "File deleted: " << args[1] << endl;
    } else {
      cout << "Error: Cannot delete file '" << args[1] << "'" << endl;
    }
  }

  void builtinCopy(const vector<string> &args) {
    if (args.size() < 3) {
      cout << "Usage: copy <source> <destination>" << endl;
      return;
    }

    ifstream src(args[1], ios::binary);
    if (!src.is_open()) {
      cout << "Error: Cannot open source file '" << args[1] << "'" << endl;
      return;
    }

    ofstream dst(args[2], ios::binary);
    if (!dst.is_open()) {
      cout << "Error: Cannot create destination file '" << args[2] << "'"
           << endl;
      src.close();
      return;
    }

    dst << src.rdbuf();
    src.close();
    dst.close();
    cout << "File copied: " << args[1] << " -> " << args[2] << endl;
  }

  void builtinMove(const vector<string> &args) {
    if (args.size() < 3) {
      cout << "Usage: move <source> <destination>" << endl;
      return;
    }

    if (rename(args[1].c_str(), args[2].c_str()) == 0) {
      cout << "File moved: " << args[1] << " -> " << args[2] << endl;
    } else {
      cout << "Error: Cannot move file" << endl;
    }
  }

  void builtinPrint(const vector<string> &args) {
    for (size_t i = 1; i < args.size(); i++) {
      cout << args[i];
      if (i < args.size() - 1)
        cout << " ";
    }
    cout << endl;
  }

  void builtinWhoAmI() { cout << username << endl; }

  void builtinDate() {
    time_t now = time(0);
    char *dt = ctime(&now);
    cout << dt;
  }

  void showSmartSuggestion(const string &cmd) {
    vector<string> suggestions = findSimilarCommands(cmd);

    if (!suggestions.empty()) {
      cout << "\nDid you mean:" << endl;
      for (size_t i = 0; i < min(size_t(3), suggestions.size()); i++) {
        cout << "  " << suggestions[i] << endl;
      }
      cout << "\nType 'help' to see all available commands" << endl;
    } else {
      cout << "\nCommand not found: '" << cmd << "'" << endl;
      cout << "Type 'help' or 'commands' to see what's available" << endl;
    }
  }

  void printHelp() {
    cout << "\n=== NeoShell - Human-Friendly Terminal v3.0 ===\n" << endl;

    cout << "FILE & DIRECTORY COMMANDS:" << endl;
    cout << "  list, show, files        - Show files in directory" << endl;
    cout << "  where, whereami          - Show current path" << endl;
    cout << "  goto, go <dir>           - Change directory" << endl;
    cout << "  makedir <name>           - Create directory" << endl;
    cout << "  remove, delete <file>    - Delete file" << endl;
    cout << "  copy <src> <dest>        - Copy file" << endl;
    cout << "  move, rename <old> <new> - Move/rename file" << endl;
    cout << "  read, view <file>        - Display file contents" << endl;
    cout << "  find, search <name>      - Find files" << endl;
    cout << "  edit <file>              - Edit file" << endl;

    cout << "\nTEXT OPERATIONS:" << endl;
    cout << "  print, say <text>        - Display text" << endl;
    cout << "  count, lines <file>      - Count lines" << endl;
    cout << "  filter, match <pattern>  - Search text" << endl;
    cout << "  sort, order              - Sort lines" << endl;
    cout << "  first, top <file>        - Show first lines" << endl;
    cout << "  last, bottom <file>      - Show last lines" << endl;

    cout << "\nSYSTEM COMMANDS:" << endl;
    cout << "  who, whoami, me          - Show current user" << endl;
    cout << "  when, time, now          - Show date/time" << endl;
    cout << "  running, processes       - Show running programs" << endl;
    cout << "  diskspace, space         - Show disk space" << endl;
    cout << "  memory, ram              - Show memory usage" << endl;
    cout << "  system                   - Show system info" << endl;
    cout << "  clear, clean             - Clear screen" << endl;

    cout << "\nNEOSHELL FEATURES:" << endl;
    cout << "  bookmark add/list/go/rm  - Manage bookmarks" << endl;
    cout << "  alias <name>=<cmd>       - Create shortcuts" << endl;
    cout << "  setenv VAR=value         - Set variable" << endl;
    cout << "  calc <expression>        - Calculator" << endl;
    cout << "  note <text>              - Quick note" << endl;
    cout << "  todo add/list/done       - Manage tasks" << endl;
    cout << "  history                  - Command history" << endl;
    cout << "  stats                    - Session statistics" << endl;
    cout << "  theme <name>             - Change theme" << endl;

    cout << "\nADVANCED:" << endl;
    cout << "  !!                       - Repeat last command" << endl;
    cout << "  !<n>                     - Run command #n" << endl;
    cout << "  $VAR                     - Use variable" << endl;
    cout << "\nType 'exit' or 'quit' to leave\n" << endl;
  }

  void showHistory(const vector<string> &args) {
    if (args.size() > 1) {
      if (args[1] == "clear") {
        history.clear();
        cout << "History cleared." << endl;
        return;
      } else if (args[1] == "search" && args.size() > 2) {
        cout << "\nSearch results for: " << args[2] << endl;
        bool found = false;
        for (size_t i = 0; i < history.size(); i++) {
          if (history[i].find(args[2]) != string::npos) {
            cout << setw(4) << (i + 1) << ": " << history[i] << endl;
            found = true;
          }
        }
        if (!found) {
          cout << "No matching commands found" << endl;
        }
        return;
      }
    }

    cout << "\n=== Command History ===" << endl;
    int start = history.size() > 20 ? history.size() - 20 : 0;
    for (size_t i = start; i < history.size(); i++) {
      cout << setw(4) << (i + 1) << ": " << history[i] << endl;
    }
    cout << endl;
  }

  void handleBookmark(const vector<string> &args) {
    if (args.size() < 2) {
      cout << "Usage:" << endl;
      cout << "  bookmark add <name>    - Save current directory" << endl;
      cout << "  bookmark list          - Show all bookmarks" << endl;
      cout << "  bookmark go <name>     - Jump to bookmark" << endl;
      cout << "  bookmark rm <name>     - Remove bookmark" << endl;
      return;
    }

    if (args[1] == "add" && args.size() > 2) {
      bookmarks[args[2]] = getCurrentPath();
      cout << "Bookmarked '" << args[2] << "' -> " << getCurrentPath() << endl;
    } else if (args[1] == "list") {
      if (bookmarks.empty()) {
        cout << "No bookmarks yet. Use 'bookmark add <name>' to create one"
             << endl;
        return;
      }
      cout << "\nBookmarks:" << endl;
      for (const auto &pair : bookmarks) {
        cout << "  " << pair.first << " -> " << pair.second << endl;
      }
      cout << endl;
    } else if (args[1] == "go" && args.size() > 2) {
      if (bookmarks.find(args[2]) != bookmarks.end()) {
        string path = bookmarks[args[2]];
#ifdef _WIN32
        SetCurrentDirectoryA(path.c_str());
#else
        chdir(path.c_str());
#endif
        cout << "Jumped to: " << path << endl;
      } else {
        cout << "Bookmark '" << args[2] << "' not found" << endl;
      }
    } else if (args[1] == "rm" && args.size() > 2) {
      if (bookmarks.erase(args[2])) {
        cout << "Removed bookmark: " << args[2] << endl;
      } else {
        cout << "Bookmark '" << args[2] << "' not found" << endl;
      }
    }
  }

  void handleAlias(const vector<string> &args) {
    if (args.size() < 2) {
      cout << "Usage:" << endl;
      cout << "  alias <name>=<command>  - Create shortcut" << endl;
      cout << "  alias list              - Show all shortcuts" << endl;
      return;
    }

    if (args[1] == "list") {
      if (aliases.empty()) {
        cout << "No custom aliases yet" << endl;
        return;
      }
      cout << "\nAliases:" << endl;
      for (const auto &pair : aliases) {
        cout << "  " << pair.first << " = " << pair.second << endl;
      }
      cout << endl;
    } else {
      string full = args[1];
      for (size_t i = 2; i < args.size(); i++) {
        full += " " + args[i];
      }
      size_t eq = full.find('=');
      if (eq != string::npos) {
        string name = full.substr(0, eq);
        string cmd = full.substr(eq + 1);
        aliases[name] = cmd;
        cout << "Alias created: " << name << " = " << cmd << endl;
      }
    }
  }

  void handleSetEnv(const vector<string> &args) {
    if (args.size() < 2) {
      cout << "Usage: setenv VAR=value" << endl;
      return;
    }

    string full = args[1];
    for (size_t i = 2; i < args.size(); i++) {
      full += " " + args[i];
    }

    size_t eq = full.find('=');
    if (eq != string::npos) {
      string name = full.substr(0, eq);
      string value = full.substr(eq + 1);
      env_vars[name] = value;
      cout << "Variable set: " << name << " = " << value << endl;
    }
  }

  void calculator(const vector<string> &args) {
    if (args.size() < 2) {
      cout << "Usage: calc <expression>" << endl;
      cout << "Example: calc 2+2*3" << endl;
      return;
    }

    string expr;
    for (size_t i = 1; i < args.size(); i++) {
      expr += args[i];
    }

    try {
      double result = 0;
      double current = 0;
      char op = '+';

      for (size_t i = 0; i < expr.length(); i++) {
        if (isdigit(expr[i]) || expr[i] == '.') {
          string num;
          while (i < expr.length() && (isdigit(expr[i]) || expr[i] == '.')) {
            num += expr[i++];
          }
          i--;
          current = stod(num);

          if (op == '+')
            result += current;
          else if (op == '-')
            result -= current;
          else if (op == '*')
            result *= current;
          else if (op == '/')
            result /= current;
        } else if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' ||
                   expr[i] == '/') {
          op = expr[i];
        }
      }

      cout << "Result: " << result << endl;
    } catch (...) {
      cout << "Error: Invalid expression" << endl;
    }
  }

  void showStats() {
    time_t now = time(0);
    int session_time = difftime(now, session_start);

    cout << "\n=== Session Statistics ===" << endl;
    cout << "  Commands executed: " << command_count << endl;
    cout << "  History size: " << history.size() << endl;
    cout << "  Session time: " << (session_time / 60) << "m "
         << (session_time % 60) << "s" << endl;
    cout << "  Aliases: " << aliases.size() << endl;
    cout << "  Bookmarks: " << bookmarks.size() << endl;
    cout << "  Variables: " << env_vars.size() << endl;
    cout << "  Todo items: " << todo_list.size() << endl;
    cout << endl;
  }

  void takeNote(const vector<string> &args) {
    if (args.size() < 2) {
      cout << "Usage: note <your note here>" << endl;
      return;
    }

    string note;
    for (size_t i = 1; i < args.size(); i++) {
      note += args[i] + " ";
    }

    ofstream file("notes.txt", ios::app);
    time_t now = time(0);
    char *dt = ctime(&now);
    dt[strlen(dt) - 1] = '\0';
    file << "[" << dt << "] " << note << endl;
    file.close();

    cout << "Note saved to notes.txt" << endl;
  }

  void handleTodo(const vector<string> &args) {
    if (args.size() < 2) {
      cout << "Usage:" << endl;
      cout << "  todo add <task>      - Add new task" << endl;
      cout << "  todo list            - Show all tasks" << endl;
      cout << "  todo done <n>        - Mark task as done" << endl;
      cout << "  todo clear           - Clear completed tasks" << endl;
      return;
    }

    if (args[1] == "add" && args.size() > 2) {
      string task;
      for (size_t i = 2; i < args.size(); i++) {
        task += args[i] + " ";
      }
      todo_list.push_back("[ ] " + task);
      cout << "Task added: " << task << endl;
    } else if (args[1] == "list") {
      if (todo_list.empty()) {
        cout << "No tasks! Add one with 'todo add <task>'" << endl;
        return;
      }
      cout << "\nTodo List:" << endl;
      for (size_t i = 0; i < todo_list.size(); i++) {
        cout << "  " << (i + 1) << ". " << todo_list[i] << endl;
      }
      cout << endl;
    } else if (args[1] == "done" && args.size() > 2) {
      int idx = stoi(args[2]) - 1;
      if (idx >= 0 && idx < (int)todo_list.size()) {
        todo_list[idx][1] = 'x';
        cout << "Task marked as done" << endl;
      } else {
        cout << "Invalid task number" << endl;
      }
    } else if (args[1] == "clear") {
      auto it = remove_if(todo_list.begin(), todo_list.end(),
                          [](const string &s) { return s.find("[x]") == 0; });
      todo_list.erase(it, todo_list.end());
      cout << "Completed tasks cleared" << endl;
    }
  }

  void showSystemInfo() {
    const string CYAN = "\033[36m";
    const string GREEN = "\033[32m";
    const string YELLOW = "\033[33m";
    const string RESET = "\033[0m";
    const string BOLD = "\033[1m";

    auto getVisibleLength = [](const string &str) {
      size_t len = 0;
      bool inEscape = false;
      for (char c : str) {
        if (c == '\033') {
          inEscape = true;
        } else if (inEscape && c == 'm') {
          inEscape = false;
        } else if (!inEscape) {
          len++;
        }
      }
      return len;
    };

    vector<string> logo = {
        CYAN + "  ######  ######" + RESET, CYAN + "  ######  ######" + RESET,
        CYAN + "  ######  ######" + RESET, "",
        CYAN + "  ######  ######" + RESET, CYAN + "  ######  ######" + RESET,
        CYAN + "  ######  ######" + RESET};

    vector<string> info;
    const int LOGO_WIDTH = 16;

#ifdef _WIN32
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    GetComputerNameA(computerName, &size);

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG totalRAM = memInfo.ullTotalPhys / (1024 * 1024 * 1024);
    DWORDLONG usedRAM =
        (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024 * 1024);

    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);

    DWORD uptime = GetTickCount() / 1000;
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;

    info.push_back(BOLD + GREEN + username + "@" + string(computerName) +
                   RESET);
    info.push_back(string(30, '-'));
    info.push_back(YELLOW + "OS: " + RESET + "Windows");
    info.push_back(YELLOW + "Host: " + RESET + string(computerName));
    info.push_back(YELLOW + "Uptime: " + RESET + to_string(hours) + "h " +
                   to_string(minutes) + "m");

    stringstream cpuInfo;
    cpuInfo << YELLOW << "CPU: " << RESET << siSysInfo.dwNumberOfProcessors
            << " cores";
    info.push_back(cpuInfo.str());

    stringstream memInfo_str;
    memInfo_str << YELLOW << "Memory: " << RESET << usedRAM << "GB / "
                << totalRAM << "GB";
    info.push_back(memInfo_str.str());

    info.push_back(YELLOW + "Shell: " + RESET + CYAN + "NeoShell v3.0" + RESET);

#else
    char hostname[256];
    gethostname(hostname, 256);

    info.push_back(BOLD + GREEN + username + "@" + string(hostname) + RESET);
    info.push_back(string(30, '-'));
    info.push_back(YELLOW + "OS: " + RESET + "Unix/Linux");
    info.push_back(YELLOW + "Host: " + RESET + string(hostname));
    info.push_back(YELLOW + "Shell: " + RESET + CYAN + "NeoShell v3.0" + RESET);
#endif

    cout << "\n";
    size_t maxLines = max(logo.size(), info.size());
    for (size_t i = 0; i < maxLines; i++) {
      if (i < logo.size()) {
        cout << logo[i];
        size_t visibleLen = getVisibleLength(logo[i]);
        int padding = LOGO_WIDTH - static_cast<int>(visibleLen) + 2;
        if (padding < 0)
          padding = 2;
        cout << string(padding, ' ');
      } else {
        cout << string(LOGO_WIDTH + 2, ' ');
      }

      if (i < info.size()) {
        cout << info[i];
      }
      cout << "\n";
    }

    cout << "\n  ";
    cout << "\033[40m   \033[0m";
    cout << "\033[41m   \033[0m";
    cout << "\033[42m   \033[0m";
    cout << "\033[43m   \033[0m";
    cout << "\033[44m   \033[0m";
    cout << "\033[45m   \033[0m";
    cout << "\033[46m   \033[0m";
    cout << "\033[47m   \033[0m";
    cout << "\n\n";
  }

  string getPrompt() {
    string path = getCurrentPath();
    string prompt;

    if (current_theme == "cyber") {
      prompt = "[" + username + "@neo] " + path + "\n> ";
    } else if (current_theme == "minimal") {
      prompt = "$ ";
    } else {
      prompt = username + "@neoshell " + path + "\n> ";
    }

    if (show_timestamps) {
      prompt = "[" + getCurrentTime() + "] " + prompt;
    }

    return prompt;
  }

public:
  NeoShell()
      : show_timestamps(false), smart_suggest(true), command_count(0),
        current_theme("default") {
    getUsername();
    session_start = time(0);
    initializeCommandMap();

    // Default useful aliases
    aliases["ll"] = "ls -la";
    aliases[".."] = "cd ..";
    aliases["..."] = "cd ../..";
    aliases["back"] = "cd ..";
  }

  void run() {
    cout << "\n=== Welcome to NeoShell v3.0 ===" << endl;
    cout << "Advanced Human-Friendly Terminal\n" << endl;
    cout << "Hello, " << username << "!" << endl;
    cout << "Type 'help' for available commands\n" << endl;

    string input;
    while (true) {
      cout << getPrompt();

      if (!getline(cin, input))
        break;

      input.erase(0, input.find_first_not_of(" \t"));
      input.erase(input.find_last_not_of(" \t") + 1);

      if (input.empty())
        continue;

      // Handle history execution
      if (input[0] == '!') {
        if (input == "!!") {
          if (!history.empty()) {
            input = history.back();
            cout << "Executing: " << input << endl;
          } else
            continue;
        } else if (isdigit(input[1])) {
          int idx = stoi(input.substr(1)) - 1;
          if (idx >= 0 && idx < (int)history.size()) {
            input = history[idx];
            cout << "Executing: " << input << endl;
          } else {
            cout << "Invalid history index" << endl;
            continue;
          }
        }
      }

      history.push_back(input);
      command_count++;

      // Expand variables
      input = expandVariables(input);

      // Check aliases
      vector<string> words = split(input, ' ');
      if (!words.empty() && aliases.find(words[0]) != aliases.end()) {
        string rest;
        for (size_t i = 1; i < words.size(); i++) {
          rest += " " + words[i];
        }
        input = aliases[words[0]] + rest;
      }

      // Parse command
      vector<string> args = split(input, ' ');
      if (args.empty())
        continue;

      // Translate human-friendly commands
      string original_cmd = args[0];
      string translated = translateCommand(args[0]);
      if (translated != args[0]) {
        args[0] = translated;
      }

      string cmd = args[0];

      // Built-in commands
      if (cmd == "exit" || cmd == "quit" || original_cmd == "bye") {
        cout << "\nGoodbye, " << username << "!" << endl;
        break;
      } else if (cmd == "help" || original_cmd == "?" ||
                 original_cmd == "commands") {
        printHelp();
      } else if (cmd == "history" || original_cmd == "past" ||
                 original_cmd == "previous") {
        showHistory(args);
      } else if (cmd == "cd" || original_cmd == "goto" ||
                 original_cmd == "go" || original_cmd == "navigate") {
        if (args.size() > 1) {
#ifdef _WIN32
          if (!SetCurrentDirectoryA(args[1].c_str())) {
            cout << "Cannot access directory: " << args[1] << endl;
          }
#else
          if (chdir(args[1].c_str()) != 0) {
            cout << "Cannot access directory: " << args[1] << endl;
          }
#endif
        } else {
          cout << getCurrentPath() << endl;
        }
      } else if (cmd == "pwd" || original_cmd == "where" ||
                 original_cmd == "whereami" || original_cmd == "location") {
        cout << getCurrentPath() << endl;
      } else if (cmd == "clear" || original_cmd == "clean" ||
                 original_cmd == "cls") {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
      } else if (original_cmd == "bookmark") {
        handleBookmark(args);
      } else if (original_cmd == "alias") {
        handleAlias(args);
      } else if (original_cmd == "unalias" && args.size() > 1) {
        aliases.erase(args[1]);
        cout << "Alias removed" << endl;
      } else if (original_cmd == "setenv") {
        handleSetEnv(args);
      } else if (original_cmd == "getenv" && args.size() > 1) {
        if (env_vars.find(args[1]) != env_vars.end()) {
          cout << env_vars[args[1]] << endl;
        } else {
          cout << "Variable not found: " << args[1] << endl;
        }
      } else if (original_cmd == "env" && args.size() > 1 &&
                 args[1] == "list") {
        if (env_vars.empty()) {
          cout << "No variables set" << endl;
        } else {
          cout << "\nEnvironment Variables:" << endl;
          for (const auto &pair : env_vars) {
            cout << "  " << pair.first << "=" << pair.second << endl;
          }
          cout << endl;
        }
      } else if (original_cmd == "calc") {
        calculator(args);
      } else if (original_cmd == "stats") {
        showStats();
      } else if (original_cmd == "sysinfo" || original_cmd == "neofetch") {
        showSystemInfo();
      } else if (original_cmd == "note") {
        takeNote(args);
      } else if (original_cmd == "todo") {
        handleTodo(args);
      } else if (original_cmd == "theme" && args.size() > 1) {
        current_theme = args[1];
        cout << "Theme changed to: " << current_theme << endl;
      } else if (original_cmd == "timestamp" && args.size() > 1) {
        show_timestamps = (args[1] == "on");
        cout << "Timestamps " << (show_timestamps ? "enabled" : "disabled")
             << endl;
      } else if (original_cmd == "suggest" && args.size() > 1) {
        smart_suggest = (args[1] == "on");
        cout << "Smart suggestions " << (smart_suggest ? "enabled" : "disabled")
             << endl;
      } else if (cmd == "ls" || original_cmd == "list" ||
                 original_cmd == "show" || original_cmd == "files") {
        // Restore original command args for builtin function
        args[0] = original_cmd;
        builtinList(args);
      } else if (cmd == "cat" || original_cmd == "read" ||
                 original_cmd == "view" || original_cmd == "display") {
        args[0] = original_cmd;
        builtinRead(args);
      } else if (cmd == "mkdir" || original_cmd == "makedir" ||
                 original_cmd == "createdir" || original_cmd == "newfolder") {
        args[0] = original_cmd;
        builtinMakeDir(args);
      } else if (cmd == "rm" || original_cmd == "remove" ||
                 original_cmd == "delete" || original_cmd == "erase") {
        args[0] = original_cmd;
        builtinRemove(args);
      } else if (cmd == "cp" || original_cmd == "copy" ||
                 original_cmd == "duplicate") {
        args[0] = original_cmd;
        builtinCopy(args);
      } else if (cmd == "mv" || original_cmd == "move" ||
                 original_cmd == "rename") {
        args[0] = original_cmd;
        builtinMove(args);
      } else if (cmd == "echo" || original_cmd == "print" ||
                 original_cmd == "say" || original_cmd == "write") {
        args[0] = original_cmd;
        builtinPrint(args);
      } else if (cmd == "whoami" || original_cmd == "who" ||
                 original_cmd == "user" || original_cmd == "me") {
        builtinWhoAmI();
      } else if (cmd == "date" || original_cmd == "when" ||
                 original_cmd == "time" || original_cmd == "now") {
        builtinDate();
      } else {
        // Execute external command
        string full_cmd = cmd;
        for (size_t i = 1; i < args.size(); i++) {
          full_cmd += " " + args[i];
        }
        int result = system(full_cmd.c_str());
        if (result != 0 && smart_suggest) {
          showSmartSuggestion(original_cmd);
        }
      }
    }
  }
};

int main() {
  NeoShell shell;
  shell.run();
  return 0;
}
