#ifndef MINIOS_OS_H
#define MINIOS_OS_H

#endif //MINIOS_OS_H
#include <mutex>
#include <condition_variable>
#include <vector>

using namespace std;

class os {
public:
    os();
    ~os();
    void run();

    [[noreturn]] void kernel();

    static void initFileSystem();

    static void initTemps();

private:
    condition_variable cv;
    bool ready;

    // 初始化文件系统
    static void createFileSys();

    static void reset();

    // 创建文件
    void createFile(const string& filename, int type);

    // 删除文件
    void deleteFile(const string& filename);

    // 显示文件列表
    void showFileList();

    // 显示文件内容
    void cd(const string& filename);

    // 保存用户信息到文件
    bool saveUserToFile(int u);

    // 保存 FCB 到文件
    bool saveFcbToFile(int f);

    // 保存位示图到文件
    bool saveBitMapToFile();

    // 保存 FAT 到文件
    bool saveFatBlockToFile();

    // 保存文件内容到文件
    bool saveFileSys(int f, string content);

    // 寻找空的块
    int getEmptyBlock();

    // 寻找空的 FCB
    int getEmptyFcb();

    // 保存文件内容到文件
    bool saveFileSys(int f, std::vector<int> content);

    // 用户注册
    void userRegister();

    // 创建目录
    int makeDirectory(int u);

    // 删除目录
    bool deleteFileSystemFile(int f);

    // 用户登录
    void userLogin();

    // 获取 FCB
    vector<int> getFcbs(int fcb);

    // 显示文件信息
    void displayFileInfo();

    vector<int> openDirectory(int f);

    int makeFile();

    int removeDirectory(string name);

    void findAllFiles(vector<int>& files, int fcb);

    void showTime();

    void showVersion();

    int importFileFromOut(string arg);

    int makeFile(string name, string content);

    int exportFileToOut(string arg);

    string openFile(int n);

    bool rename(string arg);

    //打开文件
    bool openFileMode(string arg);

    bool reWrite(int f);

    bool appendWrite(int f);

    void displayFileInfo(string args);

    bool lseek(int f, int n);

    int removeFile(string name);

    int findAllFilesForRemove(vector<int>& files, int fcb);

    bool update();

    void updateData();

    //实现锁功能：
    bool os::flock(string argument);
    //查看头部几行和尾部几行的功能：
    void os::head(int fileFcb, int numLines);
    void os::tail(int fileFcb, int numLines);
    int filePointer = 0;  // Global file pointer for simplicity

    //移动文件：
    bool os::moveFile(const string& sourcePath, const string& destPath);
    bool os::copyFile(string arg);
};
