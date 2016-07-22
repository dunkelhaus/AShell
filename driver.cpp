#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <string>
#include <cstring>
#include <termios.h>
#include <ctype.h>
#include <deque>
#include <vector>
#define PATH_MAX 4096

using namespace std;

deque<string> pastcommands;
int arrowpress = 0;

void ResetCanonicalMode(int fd, struct termios *savedattributes)
{
  tcsetattr(fd, TCSANOW, savedattributes);
}

void SetNonCanonicalMode(int fd, struct termios *savedattributes)
{
  struct termios TermAttributes;

  // Make sure stdin is a terminal. 
  if(!isatty(fd))
    _exit(0);
  // Save the terminal attributes so we can restore them later. 
  tcgetattr(fd, savedattributes);
  // Set the funny terminal modes. 
  tcgetattr (fd, &TermAttributes);
  TermAttributes.c_lflag &= ~(ICANON | ECHO); // Clear ICANON and ECHO. 
  TermAttributes.c_cc[VMIN] = 1;
  TermAttributes.c_cc[VTIME] = 0;
  tcsetattr(fd, TCSAFLUSH, &TermAttributes);
}

// Note: This function returns a pointer to a substring of the original string.
// If the given string was allocated dynamically, the caller must not overwrite
// that pointer with the returned value, since the original pointer must be
// deallocated using the same allocator with which it was allocated.  The return
// value must NOT be deallocated using free() etc.
char *trim(char *str)
{
  char *end;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}

void cd(char* next, int flag)
{
  string base = "/home/" + string(getenv("USER")) + "/";
  struct stat buffer;
  stat(next, &buffer);
  if (flag == 1)
  {
    chdir(base.c_str());
  }
  else if (S_ISREG(buffer.st_mode))
  {
    write(STDOUT_FILENO, next, strlen(next));
    write(STDOUT_FILENO, " not a directory!\n", 18);
    return;
  }
  else if (S_ISDIR(buffer.st_mode))
  {
    chdir(next);
  }
  else
  {
    write(0, "Error changing directory.\n", 26);
  }
}

char* presentWD()
{
  char* cwd = new char[PATH_MAX];
  getcwd(cwd, PATH_MAX);
  return cwd;
}

void writePermissions(int permissions)
{
  if (permissions == 0)
    write(STDOUT_FILENO, "---", 3);
  else if (permissions ==  4)
    write(STDOUT_FILENO, "r--", 3);
  else if (permissions == 2)
    write(STDOUT_FILENO, "-w-", 3);
  else if (permissions == 1)
    write(STDOUT_FILENO, "--x", 3);
  else if (permissions == 6)
    write(STDOUT_FILENO, "rw-", 3);
  else if (permissions == 3)
    write(STDOUT_FILENO, "-wx", 3);
  else if (permissions == 5)
    write(STDOUT_FILENO, "r-x", 3);
  else if (permissions == 7)
    write(STDOUT_FILENO, "rwx", 3);
  else if (permissions == 14)
    write(STDOUT_FILENO, "d", 1);
  else if (permissions == 10)
    write(STDOUT_FILENO, "-", 1);
  else
    write(STDOUT_FILENO, "", 0);
}

void ls(char* next, int flag)
{
  char* location = new char[PATH_MAX];
  location = presentWD();
  strcat(location, "/");
  struct stat results;
  DIR* dirp;

  if (flag == 1)
    dirp = opendir(".");
  else if (opendir(next))
    dirp = opendir(next);
  else
  {
    write(STDOUT_FILENO, "Failed to open directory \"", 27);
    write(STDOUT_FILENO, next, strlen(next));
    write(STDOUT_FILENO, "/\" \n", 4);
    return;
  }
  dirent *dp;

  while ((dp = readdir(dirp)) != NULL)
  {
    stat(dp -> d_name, &results);
    writePermissions(((results.st_mode & S_IFDIR) >> 12) + 10);
    writePermissions((results.st_mode & S_IRWXU) >> 6);
    writePermissions((results.st_mode & S_IRWXG) >> 3);
    writePermissions((results.st_mode & S_IRWXO) >> 0);
    
    write(STDOUT_FILENO, " ", 1);
    write(STDOUT_FILENO, dp -> d_name, strlen(dp -> d_name));
    write(STDOUT_FILENO, "\n", 1);
  }

  delete location;
}

void ff(char* arg1, char* arg2)
{
  
}

void exit()
{
  _exit(0);
}

void execute(vector<char*> arguments, int argCount, int flag)
{ 
  char *args[arguments.size()];
  copy(arguments.begin(), arguments.end(), args);
  
  args[argCount] = '\0';
  if (args[0][0] == '\n')
    return;
  // write(STDOUT_FILENO, args[0], strlen(args[0]));

  if (flag == 1)
  {
    execvp(*args, args);
    exit(0);
    return;
  }

  else
  {
    pid_t pid = fork();
  
    if (pid == 0) // child process
    {
      execvp(*args, args);
      perror("Failed to execute command");
      exit(0);
    }
  
    else // parent process
    {
      // int status;
      // while(wait(&status) > 0) { /* no-op */ ; }
      waitpid(pid, NULL, 0);
      // exit(0);
    }
  }
}

void writePrompt()
{
  string cwd = presentWD();
  int length = cwd.length();
  int begin = 0;
  string lastdir;
  string prepend = "/...";
  string postpend = "% ";
  string path;

  if (length < 16)
  {
    cwd += postpend;
    write(STDOUT_FILENO, cwd.c_str(), cwd.length());
    return;
  }

  if (length >= 16)
  {
    for (int i = length; i >= 0; i--)
    {
      if (cwd[i] == '/')
      {
        begin = i;
        break;
      }
    }
  }
  lastdir = cwd.substr(begin, length);
  path = prepend + lastdir + postpend;
  write(STDOUT_FILENO, path.c_str(), path.length());
}

int arrowCode(char* commandchar)
{
  // cout << commandchar[0] << endl;
  if (commandchar[0] == 0x1B)
  {
    read(STDIN_FILENO, commandchar, 1);
    if (commandchar[0] == 0x5B)
    {
      read(STDIN_FILENO, commandchar, 1);
      if (commandchar[0] == 0x41) // up
        return 1;

      if (commandchar[0] == 'B') // down
        return 2;
    }

    return 3; // other escape character
  }
  
  return 0; // not an escape character
}

void history(int arrowcode, int i)
{
  write(0, "", 0);
  if (arrowcode == 1)
  {
    if (arrowpress >= pastcommands.size())
    {
      write(STDOUT_FILENO, "\a", 1);
      arrowpress = pastcommands.size() - 1;
      return;
    }
    
    while (i > 0)
    {
      write(STDOUT_FILENO, "\b \b", 3);
      i--;
    }
    write(STDOUT_FILENO, pastcommands[arrowpress].c_str(), pastcommands[arrowpress].length());
  }
  else if (arrowcode == 2)
  {
    if (arrowpress == -1)
    {
      while (i > 0)
      {
        write(STDOUT_FILENO, "\b \b", 3);
        i--;
      }
      return;
    }
    if (arrowpress < 0)
    {
      write(STDOUT_FILENO, "\a", 1);
      arrowpress = -1;
      return;
    }

    while (i > 0)
    {
      write(STDOUT_FILENO, "\b \b", 3);
      i--;
    }
    write(STDOUT_FILENO, pastcommands[arrowpress].c_str(), pastcommands[arrowpress].length());
  }
}

void getCommand(char *command)  // writes prompt and reads command
{
  writePrompt();
  char* commandchar = new char[1];
  int i = 0;

  while (commandchar[0] != '\n')
  {
    read(STDIN_FILENO, commandchar, 1);
    int code = arrowCode(commandchar);

    if (code != 0)
    {
      if (code == 1)
      {
        arrowpress++;
        history(1, i);
        if (arrowpress >= 0)
        {
          strcpy(command, pastcommands[arrowpress].c_str());
          i = pastcommands[arrowpress].length();
        }
      }

      if (code == 2)
      {
        arrowpress--;
        history(2, i);
        if (arrowpress >= 0)
        {
          strcpy(command, pastcommands[arrowpress].c_str());
          i = pastcommands[arrowpress].length();
        }
        if (arrowpress == -1)
        {
          command[0] = '\n';
          command[1] = '\0';
          i = 0;
        }
      } 

      continue;
    }
    if (commandchar[0] == 0x7F)
    {
      i--;

      if (i < 0)
      {
        i++;
        write(STDOUT_FILENO, "\a", 1);
        continue;
      }
      else
      {
        write(STDOUT_FILENO, "\b \b", 3);
        if (command[i])
          command[i] = '\0';
        continue;
      }
    }

    write(STDOUT_FILENO, commandchar, 1);
    command[i] = commandchar[0];
    i++;
  }

  strtok(command, "\n");
  if (command[0] != '\n')
  {
    if (pastcommands.size() >= 10)
    {
      pastcommands.pop_back();
      pastcommands.push_front(string(command));
    }
    else
    {
      pastcommands.push_front(string(command));
    }
  }
 
}  // getCommand()

void pipeHandler(vector<string> pipes);
void rightredirHandler(vector<string> rightredir);
void leftredirHandler(vector<string> leftredir);

bool findPipes(char *command)
{
  vector<string> pipes;
  int flag = 0;
  char* temp;
  int length = strlen(command);

  for (int i = 0; i < length; i++)
    if (command[i] == '|')
      flag = 1;
  
  if (flag == 0)
  {
    return false;
  }
  else
  {
    temp = strtok(command, "|");
    
    while(temp)
    {
      pipes.push_back(string(temp));
      temp = strtok(NULL, "|");
    }
  }
  pipeHandler(pipes);
  return true;
}

bool findLeftRedirects(char *command)
{
  vector<string> leftredir;
  int flag = 0;
  char* temp;
  int length = strlen(command);

  for (int i = 0; i < length; i++)
    if (command[i] == '<')
      flag = 1;
  
  if (flag == 0)
  {
    return false;
  }
  else
  {
    temp = strtok(command, "<");
    while(temp)
    {
      temp = trim(temp);
      leftredir.push_back(string(temp));
      temp = strtok(NULL, "<");
    }
  }
  leftredirHandler(leftredir);
  return true;
}

bool findRightRedirects(char *command)
{
  vector<string> rightredir;
  int flag = 0;
  char* temp;
  int length = strlen(command);

  for (int i = 0; i < length; i++)
    if (command[i] == '>')
      flag = 1;
  
  if (flag == 0)
  {
    return false;
  }
  else
  {
    temp = strtok(command, ">");
    
    while(temp)
    {     
      temp = trim(temp);
      rightredir.push_back(string(temp));
      temp = strtok(NULL, ">");
    }
  }
  rightredirHandler(rightredir);
  return true;
}

int processCommand(char *command, int flag)  // returns 0 on proper exit
{
  if (command[0] == '\n')
    return 0;
  // write(0, command, strlen(command));
  static const char *commands[] = {"cd", "pwd", "ls", "ff", "exit"};
  vector<char*> arguments;
  char *ptr;
  int argCount = 0, commandNum;
  ptr = strtok(command, " ");

  while (ptr)
  {
    arguments.push_back(ptr);
    argCount++;
    ptr = strtok(NULL, " ");
  }  // while more arguments in the command line

  arguments[argCount] = '\0';

  if (argCount > 0)
  {
    for (commandNum = 0; commandNum < 5; commandNum++)
    {
      if (strcmp(arguments[0], commands[commandNum]) == 0)
        break;
    }  // for each possible command

    switch (commandNum)
    {
      case 0: if (argCount > 1) {cd(arguments[1], 0);} else {cd(NULL, 1);} break;
      case 1: 
      {
        char* cwd = presentWD();
        string pwd = string(cwd);
        pwd += "\n";
        int length = pwd.length();
        write(STDOUT_FILENO, pwd.c_str(), length);
        break;
      }
      case 2: if (argCount > 1) {ls(arguments[1], 0);} else {ls(NULL, 1);} break;
      case 3: 
      {
        if (argCount == 2) 
        {
          char* original = new char[1];
          original[0] = '.';
          ff(arguments[1], original);
        }

        else if (argCount >= 3)
          ff(arguments[1], arguments[2]);
        
        else
          write(STDOUT_FILENO, "ff command requires a filename!\n", 32);

        break;
      }
      case 4: exit(); break;
      default: execute(arguments, argCount, flag); break;
    }
  }

  return 1;
}


void pipeHandler(vector<string> pipes)
{
  int fds[2];
  pipe(fds);
  pid_t pid;
  char* command1 = new char[256];
  char* command2 = new char[256];

  strcpy(command1, pipes[0].c_str());
  strcpy(command2, pipes[1].c_str());

  if (fork() == 0)
  {
    dup2(fds[0], STDIN_FILENO);
    close(fds[1]);
    processCommand(command2, 1);
    exit(0);
  }

  else if ((pid = fork()) == 0)
  {
    dup2(fds[1], STDOUT_FILENO);
    close(fds[0]);
    close(0);
    processCommand(command1, 1);
    exit(0);
  } 
  
  else 
  {
    waitpid(pid, NULL, 0);
    close(fds[0]);
    close(fds[1]);
  }
}

void rightredirHandler(vector<string> rightredir)
{
  int fds[2];
  int count; 
  int fd;    
  char c;     
  pid_t pid;  
  char* file = new char[256];
  char* command = new char[256];
  strcpy(command, rightredir[0].c_str());
  strcpy(file, rightredir[1].c_str());

  pipe(fds);

  if (fork() == 0) 
  {
    fd = open(file, O_RDWR | O_CREAT, 0666);
    dup2(fds[0], STDIN_FILENO);
    close(fds[1]);

    while ((count = read(0, &c, 1)) > 0)
      write(fd, &c, 1); 

    exit(0);
  }
 
  else if ((pid = fork()) == 0)
  {
    dup2(fds[1], STDOUT_FILENO);
    close(fds[0]);
    processCommand(command, 1);
    exit(0);
  }
 
  else
  {  
    waitpid(pid, NULL, 0);
    close(fds[0]);
    close(fds[1]);
  }
}

void leftredirHandler(vector<string> leftredir)
{ 
  int fds[2]; 
  int count;  
  int fd;     
  char c;     
  pid_t pid;  
  char* file = new char[256];
  char* command = new char[256];
  strcpy(command, leftredir[0].c_str());
  strcpy(file, leftredir[1].c_str());

  pipe(fds);
  if (fork() == 0) 
  {
    dup2(fds[0], STDIN_FILENO);
    close(fds[1]);
    processCommand(command, 1);
    close(fds[0]);
    exit(0);
  }
 
  else if ((pid = fork()) == 0) 
  {
    fd = open(file, O_RDWR, 0666);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[0]);
    
    while ((count = read(fd, &c, 1)) > 0)
    {
      write(1, &c, 1);
    }
    close(fds[1]);
    exit(0);
  } 
  
  else
  {  
    waitpid(pid, NULL, 0);
    close(fds[0]);
    close(fds[1]);
  }
}

void run() // reads and processes commands until proper exit
{
  struct termios SavedTermAttributes;

  SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);

  char command[256];

  while(true)
  {
    getCommand(command);

    if (findPipes(command))
    {
      int status;
      while(wait(&status) > 0) { /* no - op */ ; } 
      continue;
    }

    if (findRightRedirects(command))
    {
      int status;
      while(wait(&status) > 0) { /* no - op */ ; } 
      continue;
    }

    
    if (findLeftRedirects(command))
    {
      int status;
      while(wait(&status) > 0) { /* no - op */ ; } 
      continue;
    }
    
    processCommand(command, 0);
  }
}  // run()

int main()
{
  run();
  return 0;
}
