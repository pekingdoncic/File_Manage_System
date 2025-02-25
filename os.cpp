#include "user.h"
#include "fcb.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>

#define userDataAddress 1048576 // �û���������ʼ��ַ
#define maxBlockCount 9216  // ������
#define maxUserCount 10 // ����û���

#define modifedTimesLength 7
#define userLength (25+4*2)*10  // 4*2 ���ڻ��з�
#define fatLength (4+2)*9216
#define bitMapLength (1+2)*9216

// �洢Ŀ¼�ṹ
vector<int> dirStack;
vector<int> catalogStack;   // ���ڴ洢Ŀ¼��ջ
int currentCatalog = 0; // ��ǰĿ¼�� FCB ��
vector<int> filesInCatalog; // ��ǰĿ¼�µ��ļ�

using namespace std;

/*
 * user
 * int isused; // 1λ���Ƿ�ʹ��
    string username; // 12λ���û���
    string password; //��� 8 λ������ĸ�������
    int root; // 4λ���û���Ŀ¼ FCB
 * */
User* user;
int nowUser = -1;    // ��ǰ�û�

mutex fileMutex;
mutex m;

// �߳� 1���������߳�
thread::id cmd;
// �߳� 2���ں��߳�
thread::id kernels;

int modifedTimes = 0;   // �޸Ĵ���
int message = 0;    // ����ʶ�����ĸ�ָ��
string argument;   // ���ڴ洢ָ��Ĳ���
int* fatBlock;  // fatBlock
int* bitMap;    // bitMap

/*
 * fcb
 *  int isused; // 1λ���Ƿ�ʹ��
    int isHide; // 1λ���Ƿ�����
    string name; // 20λ���ļ���
    int type;	// 1λ����� 0���ļ� 1��Ŀ¼
    int user;	// 1λ���û� 0��root 1��user
    int size;	// 7λ���ļ���С
    int address; // 4λ�������ַ
    string modifyTime; // 14λ���޸�ʱ��
 * */
fcb* fcbs;  // fcb
bool isLogin = false;   // �Ƿ��¼

bool os::update() {
    // ��
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;   //�����
    string ss;
    file.open("disk.txt", ios::in | ios::out);   //ÿ��д����λ���ļ���β�����ᶪʧԭ�������ݣ���out��ᶪʧԭ��������
    if (!file.is_open()) {
        cout << "File open failed!" << endl;
        return false;
    }
    file.seekg(0, ios::beg);//���ļ�ָ�붨λ���ļ���ͷ
    file >> ss;
    // ����Ƿ��޸�
    if (strtod(ss.c_str(), nullptr) == modifedTimes) {
        return true;
    }
    else {
        return false;
    }
    file.close();
    // ����
    fileLock.unlock();
}

// ����λ���洢��������Ҫ��Ϊ 4 λ������ֻ�� 2 λ����ô����ǰ�油 0
string fillFileStrings(string str, int len) {
    int length = str.length();
    if (str.length() < len) {
        for (int i = 0; i < len - length; i++) {
            str += "%";
        }
    }
    return str;
}

// int ת string
string intToString(int num, int len) {
    string str = to_string(num);
    return fillFileStrings(str, len);
}

// string ת int
int stringToInt(const string& str) {
    return strtol(str.c_str(), nullptr, 10);
}

// ��ȡ��ʵ���ַ�����
string getTrueFileStrings(string s) {
    string str = "";
    for (int i = 0; i < s.length(); i++) {
        if (s[i] != '%') {
            str += s[i];
        }
    }
    return str;
}

// ��ʼ��������Ϣ
void os::initTemps() {
    fatBlock = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        fatBlock[i] = 0;
    }
    bitMap = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        bitMap[i] = 0;
    }
    fcbs = new fcb[maxBlockCount];
    user = new class User[maxUserCount];
    message = 0;
    argument = "";
    nowUser = -1;
}

string formatTime(int m) {
    // ��ת��Ϊ string
    string str = to_string(m);
    // ���С�� 2 λ��ǰ�油 0
    if (str.length() < 2) {
        str = "0" + str;
    }
    return str;
}

// ��ȡ��ǰʱ�䣬���Ϊ 202305301454 ����ʽ
string getCurrentTime() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    string year = formatTime(1900 + ltm->tm_year);
    string month = formatTime(1 + ltm->tm_mon);
    string day = formatTime(ltm->tm_mday);
    string hour = formatTime(ltm->tm_hour);
    string min = formatTime(ltm->tm_min);
    return year + month + day + hour + min;
}

// �����û���Ϣ
bool os::saveUserToFile(int u) {
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;
    string ss;
    // file.open("disk.txt", ios::out | ios::in | ios::binary);
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    file.seekg(modifedTimesLength + u * 33, ios::beg);
    file << user[u].isused << endl;
    file << fillFileStrings(user[u].username, 12) << endl;
    file << fillFileStrings(user[u].password, 8) << endl;
    file << fillFileStrings(intToString(user[u].root, 4), 4) << endl;
    file.close();
    cout << "Save user success!" << endl;
    return true;
}

// ���� fatBlock ��Ϣ
bool os::saveFatBlockToFile() {
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    file.seekg(modifedTimesLength + userLength, ios::beg);
    for (int i = 0; i < maxBlockCount; i++) {
        file << intToString(fatBlock[i], 4) << endl;
    }
    file.close();
    // ����
    fileLock.unlock();
    return true;
}

// ���� bitMap ��Ϣ
bool os::saveBitMapToFile() {
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    file.seekg(modifedTimesLength + userLength + fatLength, ios::beg);
    for (int i = 0; i < maxBlockCount; i++) {
        file << bitMap[i] << endl;
    }
    file.close();
    // ����
    fileLock.unlock();
    return true;
}
//����ָ����f��ȥ�ļ��С�

bool os::saveFcbToFile(int f) {
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    file.seekg(modifedTimesLength + userLength + fatLength + bitMapLength + 63 * f, ios::beg);
    file << fcbs[f].isused << endl;
    file << fcbs[f].isLocked << endl;
    file << fillFileStrings(fcbs[f].name, 20) << endl;
    file << fcbs[f].type << endl;
    file << fcbs[f].user << endl;
    // size 7 λ ����
    file << fillFileStrings(intToString(fcbs[f].size, 7), 7) << endl;
    // address 4 λ ����
    file << fillFileStrings(intToString(fcbs[f].address, 4), 4) << endl;
    file << fillFileStrings(fcbs[f].modifyTime, 12) << endl;
    file.close();
    // ����
    fileLock.unlock();
    return true;
}

bool saveModifyTimesToFile() {
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    file.seekg(0, ios::beg);
    file << fillFileStrings(intToString(modifedTimes, 5), 5) << endl;
    file.close();
    // ����
    fileLock.unlock();
    return true;
}

int os::getEmptyFcb() {
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].isused == 0) {
            return i;
        }
    }
    return -1;
}

// ��ȡ���� Block
int os::getEmptyBlock() {
    for (int i = 0; i < maxBlockCount; i++) {
        if (bitMap[i] == 0) {
            bitMap[i] = 1;
            return i;
        }
    }
    return -1;
}

void os::updateData() {
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;
    string ss;
    //��ȡ�ļ�
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return;
    }
    file >> ss;
    modifedTimes = stringToInt(ss);    // �޸Ĵ���
    // �����û���Ϣ
    for (int i = 0; i < 10; i++) {
        file >> ss;
        user[i].isused = stringToInt(ss);
        file >> ss;
        user[i].username = getTrueFileStrings(ss);
        file >> ss;
        user[i].password = getTrueFileStrings(ss);
        file >> ss;
        user[i].root = stringToInt(ss);
    }
    // ���� fat block ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file >> ss;
        fatBlock[i] = strtol(ss.c_str(), nullptr, 10);  // ���������ֱ�Ϊ���ַ�����ָ�룬���ƣ���˼�ǽ��ַ���ת��Ϊ 10 ���Ƶ�����
    }
    // ���� bit map ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file >> ss;
        bitMap[i] = stringToInt(ss);
    }
    // ���� fcb ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file >> ss;
        fcbs[i].isused = stringToInt(ss);
        file >> ss;
        fcbs[i].isLocked = stringToInt(ss);
        file >> ss;
        fcbs[i].name = getTrueFileStrings(ss);
        file >> ss;
        fcbs[i].type = stringToInt(ss);
        file >> ss;
        fcbs[i].user = stringToInt(ss);
        file >> ss;
        fcbs[i].size = stringToInt(ss);
        file >> ss;
        fcbs[i].address = stringToInt(ss);
        file >> ss;
        fcbs[i].modifyTime = getTrueFileStrings(ss);
    }
    file.close();
    // ��ȡ��ǰĿ¼�µ��ļ� fcb
    filesInCatalog.clear();
    filesInCatalog = openDirectory(currentCatalog);
    // ����
    fileLock.unlock();
}

// �������������ڱ����޸Ĺ����ļ�ϵͳ
bool os::saveFileSys(int f, string content) {
    // 1. �����ļ�
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;
    string ss;
    // 2. ���ļ�
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    // 3.��λ�� fcbs ��ʼ��λ��
    file.seekg(modifedTimesLength + userLength + fatLength + bitMapLength + 63 * f, ios::beg);
    file << fcbs[f].isused << endl;
    file << fcbs[f].isLocked << endl;
    file << fillFileStrings(fcbs[f].name, 20) << endl;
    file << fcbs[f].type << endl;
    file << fcbs[f].user << endl;
    // size 7 λ ����
    file << fillFileStrings(intToString(fcbs[f].size, 7), 7) << endl;
    // address 4 λ ����
    file << fillFileStrings(intToString(fcbs[f].address, 4), 4) << endl;
    file << fillFileStrings(fcbs[f].modifyTime, 12) << endl;
    // 4.��λ���û����ݿ�
    file.seekg(userDataAddress + fcbs[f].address * 1026, ios::beg);  // 1024 + 2�����з���
    int blockNum = fcbs[f].size / 1024;   // ���ڼ�¼��ǰ�ļ���Ҫ���ٸ���
    string temp;    // ���ڼ�¼��ǰ�������
    int addressPointer = 0;  // ���ڼ�¼��ǰ��ĵ�ַָ��
    int nowAddress = fcbs[f].address;   // ���ڼ�¼��ǰ��ĵ�ַ
    // 5.��ʼд���ļ�����
    while (true) {
        if (blockNum <= 0) {
            break;
        }
        int size = 0;   // ���ڼ�¼��ǰ��Ĵ�С
        int big = 0;
        int i = addressPointer;   // ���ڼ�¼��ǰ��ĵ�������
        while (true) {
            // һ������� 1024 ���ַ�
            if (content[i] == '\n') {
                big += 2;
            }
            else {
                big += 1;
            }
            i++;
            if (i == addressPointer + 1024) {
                break;
            }
        }
        temp = content.substr(addressPointer, big);
        addressPointer += big;
        blockNum--;
        file << temp << endl;
        // �����ǰ���Ѿ�д��������Ҫ������һ����
        fatBlock[nowAddress] = getEmptyBlock();
        // �����ǰ�������һ���飬���޷�������һ����
        if (fatBlock[nowAddress] == -1) {
            fatBlock[nowAddress] = 0;
            return false;
        }
        nowAddress = fatBlock[nowAddress];
        file.seekg(userDataAddress + nowAddress * 1026, ios::beg);
    }
    temp = content.substr(addressPointer, content.size() - addressPointer);
    file << temp;

    // 6.���հ�
    int size = 0;
    int big = 0;
    int j = addressPointer;
    while (true) {
        if (j == content.size()) {
            break;
        }
        if (content[j] == '\n') {
            big += 2;
        }
        else {
            big += 1;
        }
        j++;

    }
    for (int i = 0; i < 1024 - big; i++) {
        file << "%";
    }
    file << endl;
    file.close();
    // ����
    fileLock.unlock();
    saveFatBlockToFile();
    saveBitMapToFile();
    return true;
}

bool os::saveFileSys(int f, vector<int> content) {
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;
    string ss;
    file.open("disk.txt", ios::out | ios::in);
    if (!file.is_open()) {
        cout << "Error: Can't open file!" << endl;
        return false;
    }
    // ��λ�� fcbs ��ʼ��λ��
    file.seekg(modifedTimesLength + userLength + fatLength + bitMapLength + 63 * f, ios::beg);

    file << fcbs[f].isused << endl;
    file << fcbs[f].isLocked << endl;
    file << fillFileStrings(fcbs[f].name, 20) << endl;
    file << fcbs[f].type << endl;
    file << fcbs[f].user << endl;
    // size 7 λ ����
    file << fillFileStrings(intToString(fcbs[f].size, 7), 7) << endl;
    // address 4 λ ����
    file << fillFileStrings(intToString(fcbs[f].address, 4), 4) << endl;
    file << fillFileStrings(fcbs[f].modifyTime, 12) << endl;

    // ��λ���û����ݿ�
    file.seekg(userDataAddress + fcbs[f].address * 1026, ios::beg);  // 1024 + 2�����з���
    int blockNum = fcbs[f].size / 1020;   // ���ڼ�¼��ǰ�ļ���Ҫ���ٸ���
    string temp;    // ���ڼ�¼��ǰ�������
    int addressPointer = 0;  // ���ڼ�¼��ǰ��ĵ�ַָ��
    int nowAddress = fcbs[f].address;   // ���ڼ�¼��ǰ��ĵ�ַ
    while (true) {
        if (blockNum <= 0) {
            break;
        }
        int max = addressPointer + 204;
        for (int i = addressPointer; i < max; i++) {
            file << intToString(content[i], 4) << "%";
        }
        file << "    " << endl;
        blockNum--;
        fatBlock[nowAddress] = getEmptyBlock();
        if (fatBlock[nowAddress] == -1) {
            fatBlock[nowAddress] = 0;
            return false;
        }
        nowAddress = fatBlock[nowAddress];
        file.seekg(userDataAddress + nowAddress * 1026, ios::beg);
    }
    for (int i = addressPointer; i < content.size(); i++) {
        file << intToString(content[i], 4) << "%";
    }
    for (int i = 0; i < 1024 + (addressPointer - content.size()) * 5; i++) {
        file << "%";
    }
    file << endl;
    file.close();
    // ����
    fileLock.unlock();
    saveFatBlockToFile();
    saveBitMapToFile();

    return true;
}

// ɾ���ļ��е���Ϣ
bool os::deleteFileSystemFile(int f) {
    int n = fcbs[f].address;
    vector<int> changes;
    while (true) {
        changes.push_back(n);
        bitMap[n] = 0;
        n = fatBlock[n];
        if (fatBlock[n] == 0) {
            break;
        }
    }
    for (int i = 0; i < changes.size(); i++) {
        fatBlock[changes[i]] = 0;
    }
    for (int i = 2; i < catalogStack.size(); i++) {
        fcbs[catalogStack[i]].modifyTime = getCurrentTime();
        saveFcbToFile(catalogStack[i]);
    }
    fcbs[f].reset();
    saveFcbToFile(f);
    saveFatBlockToFile();
    saveBitMapToFile();
    return true;
}

// ����Ŀ¼������Ϊ�û����
int os::makeDirectory(int u) {
    // ��ʾ�û��Ǵ����û�Ŀ¼����
    if (u != -1) {

        int voidFcb = getEmptyFcb();
        if (voidFcb == -1) {
            cout << "Error: Can't create directory!" << endl;
            return -1;
        }
        fcbs[voidFcb].isused = 1;
        fcbs[voidFcb].isLocked = 0;
        fcbs[voidFcb].type = 1;
        fcbs[voidFcb].user = u;
        fcbs[voidFcb].size = 0;
        fcbs[voidFcb].address = getEmptyBlock();
        fcbs[voidFcb].modifyTime = getCurrentTime();
        fcbs[voidFcb].name = user[u].username;
        // �û����ݿ鲻��
        if (fcbs[voidFcb].address == -1) {
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            cout << "Error: Can't create directory!" << endl;
            return -1;
        }
        // �û��ĸ�Ŀ¼Ϊ������Ŀ¼
        user[nowUser].root = voidFcb;

        // ���� README.txt
        int voidFcbFile = getEmptyFcb();
        if (voidFcbFile == -1) {
            cout << "Error: Can't create directory!" << endl;
            return -1;
        }
        fcbs[voidFcbFile].isused = 1;
        fcbs[voidFcbFile].isLocked = 0;
        fcbs[voidFcbFile].type = 0;
        fcbs[voidFcbFile].user = u;
        fcbs[voidFcbFile].size = 0;
        fcbs[voidFcbFile].address = getEmptyBlock();
        fcbs[voidFcbFile].modifyTime = getCurrentTime();
        fcbs[voidFcbFile].name = "README.txt";
        // �û����ݿ鲻��
        if (fcbs[voidFcbFile].address == -1) {
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            cout << "Error: Can't create directory!" << endl;
            return -1;
        }

        // Ŀ¼��
        vector<int> stack;
        stack.push_back(voidFcbFile);
        if (!saveFileSys(voidFcb, stack)) {
            cout << "Error: Can't create directory!" << endl;
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            deleteFileSystemFile(voidFcbFile);
            return -1;
        }
        else {
            cout << "Create directory successfully!" << endl;
        }
        string info = "This is temporary README.txt.";

        if (!saveFileSys(voidFcbFile, info)) {
            cout << "Error: Can't create directory!" << endl;
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            deleteFileSystemFile(voidFcbFile);
            return -1;
        }
        else {
            cout << "Create README.txt successfully!" << endl;
        }
        user[u].root = voidFcb;
        return voidFcb;
    }
    else {
        // ��ʾָ�����Ŀ¼����
        cout << "Please input the name of the directory: ";
        //�ж������Ƿ�Ϲ�
        string dirName;
        while (cin >> dirName) {
            if (dirName == "root") {
                cout << "Error: Can't create root directory!" << endl;
                cout << "Please input the name of the directory: ";
                continue;
            }
            if (dirName.size() > 20) {
                cout << "Error: Directory name is too long!" << endl;
                cout << "Please input the name of the directory: ";
                continue;
            }
            bool flag = false;
            for (int i = 0; i < filesInCatalog.size(); i++) {
                if (fcbs[filesInCatalog[i]].name == dirName && fcbs[filesInCatalog[i]].type == 1) {
                    flag = true;
                }
            }
            if (flag) {
                cout << "Error: Directory name is already exist!" << endl;
                cout << "Please input the name of the directory: ";
                continue;
            }
            break;
        }

        int voidFcb = getEmptyFcb();    // ��ȡ�յ�Ŀ¼��
        if (voidFcb == -1) {
            cout << "Error: No space for new directory!" << endl;
            return -1;
        }
        fcbs[voidFcb].isused = 1;
        fcbs[voidFcb].isLocked = 0;
        fcbs[voidFcb].name = dirName;
        fcbs[voidFcb].type = 1;
        fcbs[voidFcb].user = nowUser;
        fcbs[voidFcb].size = 0;
        fcbs[voidFcb].address = getEmptyBlock();
        fcbs[voidFcb].modifyTime = getCurrentTime();
        vector<int> stack;
        //�Ҳ����ڴ�ռ䣺
        if (fcbs[voidFcb].address == -1) {
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            cout << "Error: No space for new directory!" << endl;
            return -1;
        }

        //�����ļ���
        if (!saveFileSys(voidFcb, stack)) {
            cout << "Error: No space for new directory!" << endl;
            // �ͷ��ѷ���Ŀռ�
            deleteFileSystemFile(voidFcb);
            return -1;
        }
        else {
            cout << "Create directory successfully!" << endl;
        }
        // ����Ŀ¼������浽�ļ���
        filesInCatalog.push_back(voidFcb);
        for (int i = 2; i < catalogStack.size(); i++) {
            fcbs[catalogStack[i]].modifyTime = getCurrentTime();
            saveFcbToFile(catalogStack[i]);
        }
        //�����ļ����������
        saveFileSys(currentCatalog, filesInCatalog);
        saveFatBlockToFile();
        saveBitMapToFile();
        return voidFcb;
    }
}

int os::makeFile() {
    // ָ������ļ�����
    cout << "Please input the name of the file: ";
    //�����ļ�����
    string fileName;
    while (cin >> fileName) {
        if (fileName.size() > 20) {
            cout << "Error: File name is too long!" << endl;
            cout << "Please input the name of the file: ";
            continue;
        }
        bool flag = false;
        for (int i = 0; i < filesInCatalog.size(); i++) {
            if (fcbs[filesInCatalog[i]].name == fileName && fcbs[filesInCatalog[i]].type == 0) {
                flag = true;
            }
        }
        if (flag) {
            cout << "Error: File name is already exist!" << endl;
            cout << "Please input the name of the file: ";
            continue;
        }
        break;
    }

    //��ȡ���е�fcb
    int voidFcb = getEmptyFcb();
    if (voidFcb == -1) {
        cout << "Error: No space for new file!" << endl;
        return -1;
    }
    //��ʼ����ص�����
    fcbs[voidFcb].isused = 1;
    fcbs[voidFcb].isLocked = 0;
    fcbs[voidFcb].name = fileName;
    fcbs[voidFcb].type = 0;
    fcbs[voidFcb].user = nowUser;
    fcbs[voidFcb].size = 0;
    fcbs[voidFcb].address = getEmptyBlock();
    fcbs[voidFcb].modifyTime = getCurrentTime();
    vector<int> stack;
    if (fcbs[voidFcb].address == -1) {
        // �ͷ��ѷ���Ŀռ�
        deleteFileSystemFile(voidFcb);
        cout << "Error: No space for new file!" << endl;
        return -1;
    }
    //�����ļ�
    if (!saveFileSys(voidFcb, stack)) {
        cout << "Error: No space for new file!" << endl;
        // �ͷ��ѷ���Ŀռ�
        deleteFileSystemFile(voidFcb);
        return -1;
    }
    else {
        cout << "Create file successfully!" << endl;
    }
    //���µ�ǰfcbĿ¼��
    filesInCatalog.push_back(voidFcb);
    for (int i = 2; i < catalogStack.size(); i++) {
        fcbs[catalogStack[i]].modifyTime = getCurrentTime();
        saveFcbToFile(catalogStack[i]);
    }
    //����Ŀ¼��
    saveFileSys(currentCatalog, filesInCatalog);
    saveFatBlockToFile();
    saveBitMapToFile();
    return voidFcb;
}

int os::makeFile(string name, string content) {
    int n = getEmptyFcb();
    if (n == -1) {
        cout << "Error: No space for new file!" << endl;
        return -1;
    }
    fcbs[n].isused = 1;
    fcbs[n].isLocked = 0;
    fcbs[n].name = name;
    fcbs[n].type = 0;
    fcbs[n].user = nowUser;
    fcbs[n].size = content.size();
    fcbs[n].address = getEmptyBlock();
    fcbs[n].modifyTime = getCurrentTime();

    if (fcbs[n].address == -1) {
        cout << "Error: No space for new file!" << endl;
        return -1;
    }
    if (!saveFileSys(n, content)) {
        cout << "Error: No space for new file!" << endl;
        deleteFileSystemFile(n);
        return -1;
    }
    filesInCatalog.push_back(n);
    for (int i = 2; i < catalogStack.size(); i++) {
        fcbs[catalogStack[i]].modifyTime = getCurrentTime();
        saveFcbToFile(catalogStack[i]);
    }
    saveFileSys(currentCatalog, filesInCatalog);
    saveFatBlockToFile();
    saveBitMapToFile();
    return n;
}

// �ݹ���ҵ�ǰĿ¼�������ļ���Ŀ¼
void os::findAllFiles(vector<int>& files, int fcb) {
    files.push_back(fcb);
    vector<int> temp = openDirectory(fcb);
    for (int i = 0; i < temp.size(); i++) {
        if (fcbs[temp[i]].type == 1) {
            findAllFiles(files, temp[i]);
        }
        else {
            files.push_back(temp[i]);
        }
    }
}

int os::findAllFilesForRemove(vector<int>& files, int fcb) {
    // ���������ļ���������ļ��򷵻� 1 �����򷵻� 0
    files.push_back(fcb);
    vector<int> temp = openDirectory(fcb);
    for (int i = 0; i < temp.size(); i++) {
        if (fcbs[temp[i]].type == 1) {
            findAllFilesForRemove(files, temp[i]);
        }
        else {
            files.push_back(temp[i]);
        }
    }
    return files.size() > 0;
}

// ɾ����Ŀ¼���ݹ�ɾ����Ŀ¼�µ������ļ�����Ŀ¼
int os::removeDirectory(string name) {
    // name �ǿո�������ַ���, ���� "dir1 dir2 dir3" ������ת��Ϊ vector
    vector<string> dirNames;
    string temp;
    for (int i = 0; i < name.size(); i++) {
        if (name[i] == ' ') {
            dirNames.push_back(temp);
            temp = "";
        }
        else {
            temp += name[i];
        }
    }
    dirNames.push_back(temp);
    int n = -1;
    int deleteNumber;
    for (int i = 0; i < dirNames.size(); i++) {
        for (int j = 0; j < filesInCatalog.size(); j++) {
            //Ѱ��Ŀ¼���Ƿ���ڣ�
            if (fcbs[filesInCatalog[j]].name == dirNames[i] && fcbs[filesInCatalog[j]].type == 1) {
                n = filesInCatalog[j];
                deleteNumber = j;
                break;
            }
        }
        if (n == -1) {
            cout << "Error: Directory " << dirNames[i] << " is not exist!" << endl;
            return -1;
        }
        //���Ŀ¼���ڣ����� findAllFiles �����ݹ�ز��Ҹ�Ŀ¼����������Ŀ¼���ļ����������ǵ������洢�� stack �С�
        vector<int> stack;
        findAllFiles(stack, n);
        //���� stack������ deleteFileSystemFile ����ɾ��ÿ���ļ���Ŀ¼��
        for (int j = 0; j < stack.size(); j++) {
            deleteFileSystemFile(stack[j]);
        }
        cout << "Delete directory " << dirNames[i] << " successfully!" << endl;
        // �ͷ��ѷ���Ŀռ䣬erase() �������ص���ɾ��Ԫ�غ�ĵ���������������ص��ļ���
        filesInCatalog.erase(filesInCatalog.begin() + deleteNumber);
        saveFileSys(currentCatalog, filesInCatalog);
    }
    return 1;
}

// ɾ���ļ�
int os::removeFile(string name) {
    // name �ǿո�������ַ���, ���� "dir1 dir2 dir3" ������ת��Ϊ vector
    //��ȡ�ַ�����
    vector<string> fileNames;
    string temp;
    for (int i = 0; i < name.size(); i++) {
        if (name[i] == ' ') {
            fileNames.push_back(temp);
            temp = "";
        }
        else {
            temp += name[i];
        }
    }
    //������Ϊ�˿��Ի�ȡ���Ҫɾ�����ļ���
    fileNames.push_back(temp);
    int n = -1;
    int deleteNumber;
    for (int i = 0; i < fileNames.size(); i++) {
        for (int j = 0; j < filesInCatalog.size(); j++) {
            //Ѱ���Ƿ�������ļ���
            if (fcbs[filesInCatalog[j]].name == fileNames[i] && fcbs[filesInCatalog[j]].type == 0) {
                n = filesInCatalog[j];
                deleteNumber = j;
                break;
            }
        }
        if (fcbs[n].isLocked == 1)
        {
            cout << "The File is Locked,Can not Delete" << endl;
            return -1;
        }
        if (n == -1) {
            cout << "Error: File " << fileNames[i] << " is not exist!" << endl;
            return -1;
        }
        deleteFileSystemFile(n);
        cout << "Delete file " << fileNames[i] << " successfully!" << endl;
        // filesInCatalog.begin() + deleteNumber ��ʾɾ���� deleteNumber ��Ԫ��
        filesInCatalog.erase(filesInCatalog.begin() + deleteNumber);
        saveFileSys(currentCatalog, filesInCatalog);
    }
    return 1;
}

void os::displayFileInfo() {
    // ���û��Ŀ¼���ļ�����ʾ "There is no file or directory."
    if (filesInCatalog.size() == 0) {
        cout << "There is no file or directory." << endl;
        return;
    }
    for (int i = 0; i < filesInCatalog.size(); i++) {
        if (fcbs[filesInCatalog[i]].user == nowUser) {
            // ��ʾ�ļ���Ϣ
            cout << fcbs[filesInCatalog[i]].name << (fcbs[filesInCatalog[i]].type == 1 ? "(dir)" : "") << "\t";
        }
    }
    cout << endl;
}

void os::displayFileInfo(string args) {
    // ʵ�� /** ����,���Կ��� ** Ŀ¼�µ������ļ������û�����Ŀ¼����ʾ "There is no file or directory."������·����
    // ʵ�� /l ����,���Կ����ļ�����ϸ��Ϣ�������ļ������ļ����͡��ļ���С���ļ�����ʱ�䡢�ļ��޸�ʱ�䡢�ļ������û�
    // ʵ�� *.[��׺��] ����,���Կ��������Ե�ǰĿ¼�������� [��׺��] ��β���ļ�
    if (args == "l") {
        for (int i = 0; i < filesInCatalog.size(); i++) {
            if (fcbs[filesInCatalog[i]].user == nowUser) {
                cout << "�ļ� FCB ��" << filesInCatalog[i] << "\t" << "�ļ��� " << fcbs[filesInCatalog[i]].name << "\t" << "�ļ����� "
                    << (fcbs[filesInCatalog[i]].type == 1 ? "Ŀ¼" : "�ļ�") << "\t" << "�ļ���С "
                    << fcbs[filesInCatalog[i]].size << "\t" << "�ļ��޸�ʱ�� " << fcbs[filesInCatalog[i]].modifyTime
                    << "\t" << "�ļ������û� " << fcbs[filesInCatalog[i]].user << endl;
            }
        }
    }
    else if (args[0] == '*') {
        string suffix = args.substr(1, args.size() - 1);
        for (int i = 0; i < filesInCatalog.size(); i++) {
            if (fcbs[filesInCatalog[i]].user == nowUser) {
                if (fcbs[filesInCatalog[i]].name.size() >= suffix.size() &&
                    fcbs[filesInCatalog[i]].name.substr(fcbs[filesInCatalog[i]].name.size() - suffix.size(),
                        suffix.size()) == suffix) {
                    cout << fcbs[filesInCatalog[i]].name << (fcbs[filesInCatalog[i]].type == 1 ? "(dir)" : "") << "\t";
                }
            }
        }
        cout << endl;
    }
    else {
        displayFileInfo();
    }
}

// ���ļ��ж�ȡָ�� FCB ������
vector<int> os::getFcbs(int fcb) {
    fstream file;
    string ss;
    file.open("disk.txt", ios::in | ios::out);
    if (!file.is_open()) {
        cout << "Error: Can't open the disk!" << endl;
        vector<int> error;
        return error;
    }
    string data = "";
    string temp;
    int fcbAddress = fcbs[fcb].address;
    while (true) {
        file.seekg(userDataAddress + fcbAddress * 1024, ios::beg);
        if (fatBlock[fcbAddress] == 0) {
            break;
        }
        file >> temp;
        data += temp;
        fcbAddress = fatBlock[fcbAddress];  // ��һ�����ݿ�
        cout << "!!" << endl;
    }
    file >> temp;
    data += temp;
    istringstream iss(data);
    string s;
    vector<int> res;
    while (getline(iss, s, '%')) {
        if (s != "") {
            res.push_back(strtol(s.c_str(), nullptr, 10));
        }
    }
    return res;
}

// ʵ���û�ע���߼������û���Ϣд���ļ�
void os::userRegister() {
    int u = -1;
    // ���ҿհ׿ռ�
    for (int i = 0; i < 10; i++) {
        if (user[i].isused == 0) {
            u = i;
            break;
        }
    }
    if (u == -1) {
        cout << "Error: No space for new user!" << endl;
        return;
    }
    cout
        << "* Welcome to register!You should input your username and password.The username should be less than 20 characters and only contains letters and numbers.And the password should be less than 8 characters."
        << endl;
    cout << "Please input your username: ";
    string username;
    while (cin >> username) {
        if (username.size() > 20) {
            cout << "Error: Username is too long!" << endl;
            cout << "Please input your username: ";
            continue;
        }
        // �ж��û��Ƿ��Ѿ�����
        for (int i = 0; i < 10; i++) {
            if (user[i].username == username) {
                cout << "Error: Username is already exist!" << endl;
                continue;
            }
        }
        bool flag = true;
        for (int i = 0; i < username.size(); i++) {
            if (username[i] < 48 || (username[i] > 57 && username[i] < 65) || (username[i] > 90 && username[i] < 97) ||
                username[i] > 122) {
                flag = false;
                break;
            }
        }
        if (flag) {
            break;
        }
        else {
            cout << "Error: Username is not valid!" << endl;
            cout << "Please input your username: ";
        }
    }

    user[u].username = username;
    cout << "Please input your password: ";
    string password;
    while (cin >> password) {
        if (password.size() > 8) {
            cout << "Error: Password is too long!" << endl;
            cout << "Please input your password: ";
            continue;
        }
        bool flag = true;
        for (int i = 0; i < password.size(); i++) {
            if (password[i] < 48 || (password[i] > 57 && password[i] < 65) || (password[i] > 90 && password[i] < 97) ||
                password[i] > 122) {
                flag = false;
                break;
            }
        }
        if (flag) {
            break;
        }
        else {
            cout << "Error: Password is not valid!" << endl;
            cout << "Please input your password: ";
        }
    }
    user[u].password = password;
    user[u].isused = 1;
    user[u].root = 0;
    cout << "*** User " << user[u].username << " register successfully!" << endl;
    makeDirectory(u);
    saveUserToFile(u);
}

// ʵ���û���¼�߼�
void os::userLogin() {
    int u = -1;
    cout << "Input \"-r\" to register a new user." << endl;
    cout << "Please input your username: ";
    string username;
    cin >> username;
    if (username == "-r") {
        userRegister();
        return;
    }
    for (int i = 0; i < 10; i++) {
        if (user[i].username == username) {
            u = i;
        }
    }
    if (u == -1) {
        cout << "Error: Username is not exist!" << endl;
        cout << "You can register first!" << endl;
        return;
    }
    cout << "Please input your password: ";
    string password;
    cin >> password;
    if (user[u].password == password) {
        nowUser = u;
        dirStack.push_back(u);
        currentCatalog = user[u].root;
        catalogStack.push_back(currentCatalog);
        filesInCatalog = openDirectory(user[u].root);
        cout << "* Welcome " << user[u].username << "!" << endl;
        displayFileInfo();
        isLogin = true;
    }
    else {
        cout << "Error: Password is not correct!" << endl;
        return;
    }
}

void os::createFileSys() {
    //    unique_lock<mutex> fileLock(fileMutex);
    thread::id id = this_thread::get_id();
    fstream file;   // �ļ���
    // �ļ��� cmake-build-debug/ ��
    file.open("disk.txt", ios::out);    // ���ļ�
    if (!file) {
        cout << "Error: Can't open file!" << endl;
        exit(1);
    }
    cout << "*** Creating file system..." << endl;
    modifedTimes = 0;   // �޸Ĵ���
    file << "*****" << endl; // �޸Ĵ���
    // д���ʼ�����û���Ϣ
    for (int i = 0; i < 10; i++) {
        file << user[i].isused << endl;
        file << fillFileStrings(user[i].username, 12) << endl;
        file << fillFileStrings(user[i].password, 8) << endl;
        file << fillFileStrings(intToString(user[i].root, 4), 4) << endl;
    }
    // д���ʼ���� fat block ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file << intToString(fatBlock[i], 4) << endl;
    }
    // д���ʼ���� bit map ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file << bitMap[i] << endl;
    }
    // д���ʼ���� fcb ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file << fcbs[i].isused << endl;
        file << fcbs[i].isLocked << endl;
        file << fillFileStrings(fcbs[i].name, 20) << endl;
        file << fcbs[i].type << endl;
        file << fcbs[i].user << endl;
        // size 7 λ ����
        file << fillFileStrings(intToString(fcbs[i].size, 7), 7) << endl;
        file << intToString(fcbs[i].address, 4) << endl;
        file << fillFileStrings(fcbs[i].modifyTime, 12) << endl;
    }
    // ��λ���û�����
    file.seekg(userDataAddress, ios::beg);
    file << endl;
    // д���ʼ�����û�����
    for (int i = 0; i < maxBlockCount; i++) {
        // 1024 �� 0
        for (int j = 0; j < 1024; j++) {
            file << "0";
        }
        file << endl;
    }
    file.close();
    // ����
//    fileLock.unlock();
}

// �����ļ���Ϣ
void os::initFileSystem() {
    unique_lock<mutex> fileLock(fileMutex);
    fstream file;
    string ss;
    cout << "*** Reading file system..." << endl;
    cout << ss << endl;
    file.open("disk.txt", ios::in | ios::out);  // ��дģʽ���ļ�
    if (!file.is_open()) {
        createFileSys();
        return;
    }
    file >> ss;
    modifedTimes = stringToInt(ss);    // �޸Ĵ���
    // �����û���Ϣ
    for (int i = 0; i < 10; i++) {
        file >> ss;
        user[i].isused = stringToInt(ss);
        file >> ss;
        user[i].username = getTrueFileStrings(ss);
        file >> ss;
        user[i].password = getTrueFileStrings(ss);
        file >> ss;
        user[i].root = stringToInt(ss);
    }
    // ���� fat block ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file >> ss;
        fatBlock[i] = strtol(ss.c_str(), nullptr, 10);  // ���������ֱ�Ϊ���ַ�����ָ�룬���ƣ���˼�ǽ��ַ���ת��Ϊ 10 ���Ƶ�����
    }
    // ���� bit map ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file >> ss;
        bitMap[i] = stringToInt(ss);
    }
    // ���� fcb ��Ϣ
    for (int i = 0; i < maxBlockCount; i++) {
        file >> ss;
        fcbs[i].isused = stringToInt(ss);
        file >> ss;
        fcbs[i].isLocked = stringToInt(ss);
        file >> ss;
        fcbs[i].name = getTrueFileStrings(ss);
        file >> ss;
        fcbs[i].type = stringToInt(ss);
        file >> ss;
        fcbs[i].user = stringToInt(ss);
        file >> ss;
        fcbs[i].size = stringToInt(ss);
        file >> ss;
        fcbs[i].address = stringToInt(ss);
        file >> ss;
        fcbs[i].modifyTime = getTrueFileStrings(ss);
    }
    file.close();
    // ����
    fileLock.unlock();
}

// ����������kernel �̺߳� run �̹߳���
os::os() : ready(false) {
    cout << "*** Preparing for the system..." << endl;
    fatBlock = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        fatBlock[i] = 0;
    }
    bitMap = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        bitMap[i] = 0;
    }
    fcbs = new fcb[maxBlockCount];
    user = new class User[10];  // class �������� user ���ͺ� user ��
    message = 0;    // ����ʶ�����ĸ�ָ��, 0 ������ָ��
}

void os::reset() {
    fatBlock = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        fatBlock[i] = 0;
    }
    bitMap = new int[maxBlockCount];
    for (int i = 0; i < maxBlockCount; i++) {
        bitMap[i] = 0;
    }
    fcbs = new fcb[maxBlockCount];
    user = new class User[10];
}

os::~os() {
}


// create ��������ļ�
void os::createFile(const string& filename, int type) {
    // �ж��Ƿ��������ļ�
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].name == filename && fcbs[i].isused == 1) {
            cout << "Error: File already exists" << endl;
            return;
        }
    }
    // �ж��Ƿ��п��� fcb
    int fcbIndex = -1;
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].isused == 0) {
            fcbIndex = i;
            break;
        }
    }
    if (fcbIndex == -1) {
        cout << "Error: No more space for new file" << endl;
        return;
    }
    // �ж��Ƿ��п��� block
    int blockIndex = -1;
    for (int i = 0; i < maxBlockCount; i++) {
        if (bitMap[i] == 0) {
            blockIndex = i;
            break;
        }
    }
    if (blockIndex == -1) {
        cout << "Error: No more space for new file" << endl;
        return;
    }
    // �����ļ�
    fcbs[fcbIndex].isused = 1;
    fcbs[fcbIndex].isLocked = 0;
    fcbs[fcbIndex].name = filename;
    fcbs[fcbIndex].type = type;

    fcbs[fcbIndex].user = nowUser;
    fcbs[fcbIndex].size = 0;
    fcbs[fcbIndex].address = blockIndex;
    fcbs[fcbIndex].modifyTime = getCurrentTime();
    bitMap[blockIndex] = 1;

    cout << "File created successfully" << endl;
}

void os::deleteFile(const string& filename) {
    // �ж��Ƿ��и��ļ�
    int fcbIndex = -1;
    for (int i = 0; i < maxBlockCount; i++) {
        if (fcbs[i].name == filename && fcbs[i].isused == 1) {
            fcbIndex = i;
            break;
        }
    }
    if (fcbIndex == -1) {
        cout << "Error: No such file" << endl;
        return;
    }
    // �ж��Ƿ���Ȩ��
    if (fcbs[fcbIndex].user != nowUser && nowUser != 0) {
        cout << "Error: Permission denied" << endl;
        return;
    }
    // ɾ���ļ�
    int blockIndex = fcbs[fcbIndex].address;
    bitMap[blockIndex] = 0;
    fcbs[fcbIndex].isused = 0;
    fcbs[fcbIndex].isLocked = 0;
    fcbs[fcbIndex].name = "";
    fcbs[fcbIndex].type = 0;
    fcbs[fcbIndex].user = 0;
    fcbs[fcbIndex].size = 0;
    fcbs[fcbIndex].address = 0;
    fcbs[fcbIndex].modifyTime = "";


    cout << "File deleted successfully" << endl;
}


// ��Ŀ¼����ȡĿ¼�µ��ļ�
vector<int> os::openDirectory(int f) {
    fstream file;
    string ss;
    file.open("disk.txt", ios::in | ios::out);
    if (!file.is_open()) {
        cout << "Error: Failed to open disk" << endl;
        vector<int> error;
        return error;
    }

    string data = "";
    string temp;
    int address = fcbs[f].address;
    //    cout<<"nowAddress"<<address<<endl;
    while (true) {
        file.seekg(userDataAddress + address * 1026, ios::beg);
        // ����������û�������ˣ����˳�
        if (fatBlock[address] == 0) {
            break;
        }
        // �еĻ��ͽ��Ŷ�
        file >> temp;
        data += temp;
        address = fatBlock[address];
    }
    file >> temp;
    data += temp;
    istringstream is(data);
    string s;
    vector<int> res;
    while (getline(is, s, '%')) {
        if (s != "") {
            res.push_back(::strtol(s.c_str(), nullptr, 10));
        }
    }
    return res;
}

void os::cd(const string& filename) {
    // filename ���ɿո�ָ����ַ�����������Ϊ vector
    vector<string> v;
    istringstream iss(filename);
    while (iss) {
        string sub;
        iss >> sub;
        v.push_back(sub);
    }
    switch (v[0][0]) {
    case '.': {
        if (v[0] == "..") {
            if (catalogStack.size() == 1) {
                cout << "Error: No such directory" << endl;
                return;
            }
            else {
                currentCatalog = catalogStack[catalogStack.size() - 2];
                catalogStack.pop_back();
                filesInCatalog = openDirectory(currentCatalog);
            }
        }
        else if (v[0] == ".") {
            // ���ֵ�ǰĿ¼����
        }
        else {
            cout << "Error: Invalid command" << endl;
            return;
        }
    }
            break;
    default: {
        istringstream is(v[0]);
        string key;
        vector<string> temp;
        while (getline(is, key, '/')) {
            if (key != "") {
                temp.push_back(key);
            }
        }
        // ���� temp �е����ݣ��ҵ���Ӧ��Ŀ¼
        if (temp[0] == "root") {
            // ���Ҫǰ����Ŀ¼�� root�������Ŀ¼ջ
            catalogStack.clear();
            catalogStack.push_back(nowUser);    // ��ǰĿ¼Ϊ��ǰ�û��ĸ�Ŀ¼
            filesInCatalog = openDirectory(user[nowUser].root);   // �򿪵�ǰ�û��ĸ�Ŀ¼
            catalogStack.push_back(user[nowUser].root);   // ����ǰ�û��ĸ�Ŀ¼ѹ��Ŀ¼ջ
            for (int i = 1; i < temp.size(); i++) {
                bool flag = false;
                for (int j = 0; j < filesInCatalog.size(); j++) {
                    if (fcbs[filesInCatalog[j]].name == temp[i] && fcbs[filesInCatalog[j]].type == 1) {
                        flag = true;
                        currentCatalog = filesInCatalog[j]; // ��ǰĿ¼Ϊ�ҵ���Ŀ¼
                        catalogStack.push_back(currentCatalog); // ����ǰĿ¼ѹ��Ŀ¼ջ
                        filesInCatalog = openDirectory(currentCatalog); // �򿪵�ǰĿ¼
                    }
                }
                if (!flag) {
                    cout << "Error: No such directory" << endl;
                }
            }
        }
        else {
            for (int i = 0; i < temp.size(); i++) {
                bool flag = false;
                for (int j = 0; j < filesInCatalog.size(); j++) {
                    if (fcbs[filesInCatalog[j]].name == temp[i] && fcbs[filesInCatalog[j]].type == 1) {
                        flag = true;
                        currentCatalog = filesInCatalog[j]; // ��ǰĿ¼�滻Ϊ�ҵ���Ŀ¼
                        catalogStack.push_back(currentCatalog); // ����ǰĿ¼ѹ��Ŀ¼ջ
                        filesInCatalog = openDirectory(currentCatalog); // �򿪵�ǰĿ¼
                    }
                }
                if (!flag) {
                    cout << "Error: No such directory" << endl;
                    return;
                }
            }
        }
    }
           break;
    }
}

string os::openFile(int n) {
    fstream file;
    string ss;
    file.open("disk.txt", ios::in | ios::out);
    if (!file.is_open()) {
        cout << "Error: Failed to open disk" << endl;
        return "";
    }

    string data = "";
    string temp;
    int address = fcbs[n].address;
    int len = 0;
    int round = 0;

    while (true) {
        file.seekg(userDataAddress + address * 1026, ios::beg);
        if (fatBlock[address] == 0) {
            break;
        }
        len = 0;
        round = 1;
        getline(file, temp);
        data += temp;
        while (true) {
            len += temp.size();
            // ��ǰ��������Ѿ�����
            if (len + 2 * round >= 1026) {
                break;
            }
            getline(file, temp);
            data += temp;
            data += "\n";
            round++;
        }
        address = fatBlock[address];
    }
    len = 0;
    round = 1;
    getline(file, temp);
    len += temp.size();
    for (int i = 0; i < temp.size(); i++) {
        if (temp[i] == '%') {
            temp.erase(temp.begin() + i);
            if (i != 0)
                i--;
        }
    }
    if (temp == "%") {
        temp = "";
    }
    data += temp;
    while (true) {
        if (len + 2 * round >= 1026) {
            break;
        }
        getline(file, temp);
        len += temp.size();
        for (int i = 0; i < temp.size(); i++) {
            if (temp[i] == '%') {
                temp.erase(temp.begin() + i);
                if (i != 0)
                    i--;
            }
        }
        if (temp == "%") {
            temp = "";
        }
        data += '\n';
        data += temp;
        round++;
    }
    file.close();
    return data;
}

bool os::reWrite(int f) {
    cout << "Are you sure to rewrite the file? (y/n)";
    string choice;
    cin >> choice;
    if (choice == "y") {
        cout << "Please input the new content:(Input 'EOF' as end) " << endl;
        string data;
        cin.ignore();

        //�°����룺
        while (true) {
            string line;
            getline(cin, line);
            if (line.find("EOF") != string::npos) {
                data += line.substr(0, line.find("EOF"));
                break;
            }
            data += line + '\n';
        }

        //����������ݣ�
        if (!saveFileSys(f, data)) {
            cout << "Error: The file cannot be rewritten";
            return false;
        }
        return true;
    }
    else if (choice == "n") {
        return false;
    }
    else {
        cout << "Error: The input is wrong";
        return false;
    }
}

bool os::appendWrite(int f) {
    cout << "Are you sure to append the file? (y/n)";
    string choice;
    cin >> choice;
    if (choice == "y") {
        string data = openFile(f);
        cout << "Please input the content you want to append: (Input 'EOF' as end)" << endl;
        cin.ignore();
        //�°����룺
        while (true)
        {
            string line;
            getline(cin, line);
            if (line.find("EOF") != string::npos) {
                data += line.substr(0, line.find("EOF"));
                break;
            }
            data += line + "\n";
        }

        //�����������
        if (!saveFileSys(f, data)) {
            cout << "Error: The file cannot be appended";
            return false;
        }
        return true;
    }
    else if (choice == "n") {
        return false;
    }
    else {
        cout << "Error: The input is wrong";
        return false;
    }
}

// lseek �����������ƶ��ļ�ָ�룬����Ϊ�ļ����������ƶ����ֽ�����ִ�к�������ָ��λ�ý���д�룬д�����б��棬����ƶ�ʧ�ܣ�����-1

//��һ�溯������ʵ�ִӺ���ǰ�ƶ�ָ��Ĳ�����
bool os::lseek(int f, int n) {
    string data = openFile(f);
    if (data == "") {
        cout << "Error: void file" << endl;
        return false;
    }
    cout << "��С�� " << static_cast<int>(data.size()) << endl;
    cout << "�ļ�ָ�룺 " << filePointer << endl;
    cout << "Please input the pointer movement amount�� ";
    int pos;
    cin >> pos;
    if (cin.fail())
    {
        cout << "Error: The position is invalid" << endl;
    }
    cin.ignore();
    if (pos > static_cast<int>(data.size()))
    {
        cout << "Error: The position is out of range or invalid" << endl;
    }
    //Ѱ�ҵ���ǰλ�ã�����һ��ȫ�ֱ���
    int currentPos = pos + filePointer;
    if (currentPos < 0)
    {
        //��һ��Ϊ�˱���˵��0��-1֮���ܹ��ƶ����ַ�����ĩβ��������ĩβ��ǰһ����
        currentPos = static_cast<int>(data.size()) + currentPos + 1;
    }
    //����޸��ļ���ָ���ʱ����Ҫע�⵽һ�����⣬�����޸�һ��֮��ָ�����ļ�ĩβ�������������һ��ָ�룬��΢��һЩ�Ļ����������ٻص��ļ���ͷ�ͺ��ˣ�
    if (currentPos > static_cast<int>(data.size())) {

        currentPos = currentPos - static_cast<int>(data.size());
    }

    //�����ļ�ָ�룺
    filePointer = currentPos;
    cout << "The current content is: " << data << endl;
    cout << "Please input the content you want to write (type 'EOF' to finish):" << endl;
    string temp;
    string line;
    //�޸����£�
    while (true) {
        getline(cin, line);
        if (line.find("EOF") != string::npos) {
            temp += line.substr(0, line.find("EOF"));
            break;
        }
        temp += line + "\n";
    }
    data.insert(currentPos, temp);
    cout << "Updated content: " << endl << data << endl;
    if (!saveFileSys(f, data)) {
        cout << "Error: The file cannot be written" << endl;
        return false;
    }
    return true;
}

//һ��ʼ�İ汾����û�а취��ǰ�ƶ�ָ�룡

int os::importFileFromOut(string arg) {
    // arg ���ɿո�ָ����ַ�����������Ϊ vector
    vector<string> v;
    istringstream iss(arg);
    while (iss) {
        string sub;
        iss >> sub;
        v.push_back(sub);
    }
    string filePath = v[0];
    string name;
    cout << "Please input the new new for the file in your system: ";
    while (cin >> name) {
        if (name.size() > 20) {
            cout << "Error: The name of the file is too long" << endl;
            cout << "Please input the new new for the file in your system: ";
            continue;
        }
        bool flag = false;
        for (int i = 0; i < filesInCatalog.size(); i++) {
            if (fcbs[filesInCatalog[i]].name == name && fcbs[filesInCatalog[i]].type == 0) {
                flag = true;
            }
        }
        if (flag) {
            cout << "Error: The file already exists" << endl;
            cout << "Please input the name of the file you want to import: ";
            continue;
        }
        break;
    }
    fstream file;
    file.open(filePath, ios::in);
    if (!file) {
        cout << "Error: The file does not exist" << endl;
        return 0;
    }
    string data;
    string temp;
    try {
        while (!file.eof()) {
            getline(file, temp);
            data += temp;
            data += "\n";
        }
        //ȥ�������һ�����з�
        if (!data.empty() && data.back() == '\n') {
            data.pop_back();
        }
        makeFile(name, data);
    }
    catch (const std::exception& e) {
        cout << e.what() << endl;
        return 0;
    }
    cout << "Import successfully!" << endl;
    return 1;
}

/*
 * exportFileToOut ���������ڽ��ļ���������棬����Ϊ�ļ���������ִ�к��ļ���������棬�������ʧ�ܣ�����-1
 * ���� arg Ϊ�ļ���
 * ʵ�ֹ��̣�
 * 1. ���� filesInCatalog���ҵ��ļ���Ϊ arg ���ļ�
 * 2. ���ļ�����д�뵽 arg+'.txt' �ļ���
 * 3. �����ɹ������� 1
 * */
int os::exportFileToOut(string arg) {
    // arg ���ɿո�ָ����ַ�����������Ϊ vector
    //��ȡ�ַ�����
    vector<string> v;
    istringstream iss(arg);
    while (iss) {
        string sub;
        iss >> sub;
        v.push_back(sub);
    }
    if ((v.size() == 3) && (v[2] == ""))
    {
        v.pop_back();
    }

    if (v.size() > 2)
    {
        cout << "Error Invaild Input!" << endl;
        return 0;
    }
    //��ȡ�ļ�·����
    string filePath;
    if (v.size() == 2)
        filePath = v[1];
    else
        filePath = "";
    int n = -1;
    for (int i = 0; i < filesInCatalog.size(); i++) {
        if (fcbs[filesInCatalog[i]].name == v[0]) {
            n = filesInCatalog[i];
        }
    }
    if (n == -1) {
        cout << "Error: The file does not exist" << endl;
        return 0;
    }
    fstream file;
    if (v.size() == 2) {
        v.push_back("");
    }
    // v[2]+fcbs[n].name+'.txt' ƴ��Ϊ�ַ���
    string fileName = filePath + "exported.txt";
    // �ж��ļ��Ƿ���ڣ�û�оʹ���
    file.open(fileName, ios::in | ios::out | ios::trunc);
    if (!file.is_open()) {
        cout << "Error: The file cannot be exported" << endl;
        return 0;
    }
    //��ָ���ĵط���ȡ�ļ���
    string data = openFile(n);
    //�����ļ���
    file << data;
    cout << "Export successfully!" << endl;
    return 1;
}

// rename test.txt test2.txt
bool os::rename(string arg) {
    //    arg ���ɿո�ָ����ַ�����������Ϊ vector
    vector<string> v;
    istringstream iss(arg);
    while (iss) {
        string sub;
        iss >> sub;
        v.push_back(sub);
    }
    // ��� v.size() == 3 ����ɾ�����һ��Ԫ��
    if (v.size() == 3) {
        v.pop_back();
    }
    // v[0] ��ԭ�ļ�����v[1] �����ļ���
    // �����������,��ڶ�������Ϊ�ո���
    if (v.size() != 2 || v[1].size() == 0) {
        cout << "Error: The number of parameters is wrong" << endl;
        return false;
    }
    int n = -1;
    for (int i = 0; i < filesInCatalog.size(); i++) {
        if (fcbs[filesInCatalog[i]].name == v[0]) {
            n = filesInCatalog[i];
        }
    }
    if (n == -1) {
        cout << "Error: The file does not exist" << endl;
        return false;
    }
    if (v[1].size() > 20) {
        cout << "Error: The name of the file is too long" << endl;
        return false;
    }
    for (int i = 0; i < filesInCatalog.size(); i++) {
        if (fcbs[filesInCatalog[i]].name == v[1]) {
            cout << "Error: The file already exists" << endl;
            return false;
        }
    }
    fcbs[n].name = v[1];
    fcbs[n].modifyTime = getCurrentTime();
    saveFcbToFile(n);
    cout << "Rename successfully!" << endl;
    return true;
}

bool os::openFileMode(string arg) {
    //    arg ���ɿո�ָ����ַ�����������Ϊ vector
    vector<string> v;
    istringstream iss(arg);
    while (iss) {
        string sub;
        iss >> sub;
        v.push_back(sub);
    }

    int openingFile = -1;  // ��ǰ�򿪵��ļ�
    int flag = false;
    for (int i = 0; i < filesInCatalog.size(); i++) {
        if (fcbs[filesInCatalog[i]].name == v[0]) {
            if (fcbs[filesInCatalog[i]].type == 1) {
                cout << "Error: The file is a directory" << endl;
                return false;
            }
            if (fcbs[filesInCatalog[i]].isLocked == 1)
            {
                cout << "The File is Locked!" << endl;
                return false;
            }
            openingFile = filesInCatalog[i];
            flag = true;
            cout << "Open successfully!" << endl;
            // ����ѭ��
            break;
        }
    }

    if (!flag) {
        cout << "Error: The file does not exist" << endl;
        return false;
    }

    cout << "You can put in the following commands:" << endl;
    cout << "* read: output the content of the file" << endl;
    cout << "* write -r/-a: rewrite/append the file" << endl;
    cout << "* close: close the file" << endl;
    cout << "* lseek: change the pointer of the file and edit from the pointer" << endl;
    cout << "* exit: exit file mode" << endl;
    cout << "* head -num: display the first num lines of the file" << endl;
    cout << "* tail -num: display the last num lines of the file" << endl;

    vector<string> choice;
    cout << "Please input the command:";
    while (true) {
        // ���� choice
        string temp;
        getline(cin, temp);
        choice.clear();
        istringstream iss(temp);
        while (iss) {
            string sub;
            iss >> sub;
            choice.push_back(sub);
        }
        switch (choice[0][0]) {
        case 'r':
            if (choice[0] == "read") {
                cout << openFile(openingFile) << endl;
                cout << "Please input the next command:";
            }
            else {
                cout << "Error: Wrong command" << endl;
            }
            break;
        case 'w': {
            // �� choice ���Ϊ vector
            vector<string> v;
            v = choice;
            // ��� v[1] Ϊ�ո���
            if (v.size() == 1 || v[1].empty()) {
                cout << "Error: The number of parameters is wrong" << endl;
                cout << "Please input the next command:";
                break;
            }
            if (v[0] == "write") {
                if (v[1] == "-r") {
                    reWrite(openingFile);
                    //                        cin.ignore();
                    cout << "Write successfully!" << endl;
                }
                else if (v[1] == "-a") {
                    appendWrite(openingFile);
                    //                        cin.ignore();
                    cout << "Append successfully!" << endl;
                }
                else {
                    cout << "SError: Wrong command" << endl;
                }
                cout << "Please input the next command:";
            }
            else {
                cout << "?Error: Wrong command" << endl;
                cout << "Please input the next command:";
            }
        }
                break;
        case 'c':
            if (choice[0] == "close") {
                openingFile = -1;
                cout << "Close successfully!" << endl;
                return true;
            }
            else {
                cout << "Error: Wrong command" << endl;
                cout << "Please input the next command:";
            }
            break;
        case 'e':
            if (choice[0] == "exit") {
                cout << "Exit successfully!" << endl;
                return true;
            }
            else {
                cout << "Error: Wrong command��Do you main \"exit\"?" << endl;
                cout << "Please input the next command:";
            }
            break;
        case 'l':
            // lseek
            if (choice[0] == "lseek") {
                if (choice.size() != 2) {
                    cout << "Error: The number of parameters is wrong" << endl;
                    cout << "Please input the next command:";
                    break;
                }
                int offset = strtol(choice[1].c_str(), nullptr, 10);
                lseek(openingFile, offset);
                cout << "Please input the next command:";
            }
            else {
                cout << "Error: Wrong command" << endl;
                cout << "Please input the next command:";
            }
            break;

            //�������ܣ�
        case 'h':
            //** head 
            if (choice[0] == "head") {
                /*  for (int i = 0; i < choice.size(); i++)
                      cout << "choice " << i << " :" << choice[i] << endl;*/
                      //��Ϊ�ڶ����ʱ�򣬻���ȡ��һ���ո��ˣ�����size��3��
                if (choice.size() > 3) {
                    cout << "Error: The number of parameters is wrong" << endl;
                    cout << "Please input the next command:";
                    break;
                }
                int numLines = strtol(choice[1].c_str(), nullptr, 10);
                head(openingFile, numLines);
                cout << "Please input the next command:";
            }
            else {
                cout << "Error: Wrong command" << endl;
                cout << "Please input the next command:";
            }
            break;
        case 't':
            //** tail
            if (choice[0] == "tail") {
                if (choice.size() > 3) {
                    cout << "Error: The number of parameters is wrong" << endl;
                    cout << "Please input the next command:";
                    break;
                }
                int numLines = strtol(choice[1].c_str(), nullptr, 10);
                tail(openingFile, numLines);
                cout << "Please input the next command:";
            }
            else {
                cout << "Error: Wrong command" << endl;
                cout << "Please input the next command:";
            }
            break;
        default:
            cout << "QError: Wrong command" << endl;
            cout << "Please input the next command:";
            break;
        }
    }
}

void os::showTime() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    cout << "Current time: " << 1900 + ltm->tm_year << "-" << 1 + ltm->tm_mon << "-" << ltm->tm_mday << " "
        << ltm->tm_hour << ":" << ltm->tm_min << ":" << ltm->tm_sec << endl;
}

void os::showVersion() {
    /*  cout << endl;
      cout << "MiniOS " << VERSION << endl;
      cout << "Made by ChengZihan,BJFU" << endl;
      cout << "* June 1st, 2023" << endl;
      cout << "GitHub:" << " https://github.com/inannan423 " << endl;
      cout << endl;*/
}

//�ļ��������ܣ�
bool os::flock(string argument)
{
    vector<string> args;
    istringstream iss(argument);
    string temp;
    bool isLock;
    //���ָ���ַ�����
    while (iss >> temp)
    {
        args.push_back(temp);
    }
    if (args.size() != 2)
    {
        cout << "Error: Invalid argument" << endl;
    }

    else
    {
        isLock = (args[0] == "lock");
    }
    //  cout << "isLock: " << isLock << endl;
    string name = args[1];
    //����Ѱ��fcb��
    for (int i = 0; i < filesInCatalog.size(); i++)
    {
        if ((fcbs[filesInCatalog[i]].name == name) && (fcbs[filesInCatalog[i]].isused == 1))
        {
            if ((fcbs[filesInCatalog[i]].isLocked == 1) && (isLock == true))
            {
                cout << "Error: File is already locked" << endl;
                return false;
            }
            else if ((fcbs[filesInCatalog[i]].isLocked == 0) && (isLock == false))
            {
                cout << "Error: File is already unlocked" << endl;
                return false;
            }
            else
            {
                if (isLock == 1)
                {
                    fcbs[filesInCatalog[i]].isLocked = 1;
                    cout << "File locked successfully" << endl;
                }
                else
                {
                    fcbs[filesInCatalog[i]].isLocked = 0;
                    cout << "File unlocked successfully" << endl;
                }
                /*  cout << "filesInCatalog[i]: " << filesInCatalog[i] << endl;*/
                saveFcbToFile(filesInCatalog[i]);
                return true;
            }
        }
    }
    cout << "Error: File not found" << endl;
    return false;
}


//*ʵ��head����
void os::head(int fileFcb, int numLines) {
    string data = openFile(fileFcb);
    istringstream iss(data);
    vector<string> lines;
    string line;
    while (getline(iss, line))
    {
        lines.push_back(line);
    }
    if (lines.size() < numLines)
    {
        cout << "���ļ���û����ô���У������������룡" << endl;
        return;
    }

    for (int i = 0; i < numLines; i++) {
        cout << lines[i] << endl;
    }
}

//*ʵ��tail����
void os::tail(int fileFcb, int numLines) {
    string data = openFile(fileFcb);
    istringstream iss(data);
    vector<string> lines;
    string line;
    while (getline(iss, line)) {
        lines.push_back(line);
    }

    if (lines.size() < numLines)
    {
        cout << "���ļ���û����ô���У������������룡" << endl;
        return;
    }
    int start = max(0, (int)lines.size() - numLines);
    for (int i = start; i < lines.size(); i++) {
        cout << lines[i] << endl;
    }
}

bool os::moveFile(const string& sourcePath, const string& destPath) {
    // Find the source file
    int sourceFcb = -1;
    for (int i = 0; i < filesInCatalog.size(); i++) {
        if (fcbs[filesInCatalog[i]].name == sourcePath && fcbs[filesInCatalog[i]].type == 0) {
            sourceFcb = filesInCatalog[i];
            break;
        }
    }
    if (sourceFcb == -1) {
        cout << "Error: The source file does not exist" << endl;
        return false;
    }

    // Parse destination path
    int destFcb = currentCatalog;
    istringstream iss(destPath);
    string dir;
    while (getline(iss, dir, '/')) {
        bool found = false;
        vector<int> dirContents = openDirectory(destFcb);
        for (int i = 0; i < dirContents.size(); i++) {
            if (fcbs[dirContents[i]].name == dir && fcbs[dirContents[i]].type == 1) {
                destFcb = dirContents[i];
                found = true;
                break;
            }
        }
        if (!found) {
            cout << "Error: The destination directory does not exist" << endl;
            return false;
        }
    }

    // Extract source file name without path
    string srcFileName = sourcePath;
    size_t lastSlashPos = sourcePath.find_last_of("/\\");
    if (lastSlashPos != string::npos) {
        srcFileName = sourcePath.substr(lastSlashPos + 1);
    }

    // Check for name conflict and resolve it by renaming if necessary
    vector<int> destFiles = openDirectory(destFcb);
    string newFileName = srcFileName;
    int copyCount = 1;
    bool nameExists;
    do {
        nameExists = false;
        for (int i = 0; i < destFiles.size(); i++) {
            if (fcbs[destFiles[i]].name == newFileName && fcbs[destFiles[i]].isused == 1) {
                nameExists = true;
                // Split base name and extension
                string baseName = newFileName;
                string extension = "";
                size_t dotPos = newFileName.find_last_of('.');
                if (dotPos != string::npos) {
                    baseName = newFileName.substr(0, dotPos);
                    extension = newFileName.substr(dotPos);
                }
                newFileName = baseName + "(" + to_string(copyCount++) + ")" + extension;
                break;
            }
        }
    } while (nameExists);

    // Update the file name in the FCB
    fcbs[sourceFcb].name = newFileName;

    // Remove the source file from its current directory
    filesInCatalog.erase(std::remove(filesInCatalog.begin(), filesInCatalog.end(), sourceFcb), filesInCatalog.end());
    saveFileSys(currentCatalog, filesInCatalog);

    // Move the source file to the destination directory
    destFiles.push_back(sourceFcb);
    saveFileSys(destFcb, destFiles);

    // Update the modify time of the directories involved
    fcbs[sourceFcb].modifyTime = getCurrentTime();
    saveFcbToFile(sourceFcb);

    cout << "File moved successfully!" << endl;
    return true;
}


// cmd �̣߳����ڽ����û�����
void os::run() {
    // �߳� 1���������߳�
    cmd = this_thread::get_id();
    cout << "* Welcome to LHY Document Management System" << endl << endl << endl;
    std::cout << "\n"
        "   �������������[ ���������������[���������������[�����[  �����[\n"
        "  �����X�T�T�T�����[�����X�T�T�T�T�a�����X�T�T�T�T�a�����U  �����U\n"
        "  �����U   �����U���������������[���������������[���������������U\n"
        "  �����U   �����U�^�T�T�T�T�����U�^�T�T�T�T�����U�����X�T�T�����U\n"
        "  �^�������������X�a���������������U���������������U�����U  �����U\n"
        "   �^�T�T�T�T�T�a �^�T�T�T�T�T�T�a�^�T�T�T�T�T�T�a�^�T�a  �^�T�a\n"
        "                                    \n"
        "  ���������������[�����[  �����[ �������������[ �������[   �����[\n"
        "  �����X�T�T�T�T�a�����U  �����U�����X�T�T�T�����[���������[  �����U\n"
        "  ���������������[���������������U�����U   �����U�����X�����[ �����U\n"
        "  �^�T�T�T�T�����U�����X�T�T�����U�����U   �����U�����U�^�����[�����U\n"
        "  ���������������U�����U  �����U�^�������������X�a�����U �^���������U\n"
        "  �^�T�T�T�T�T�T�a�^�T�a  �^�T�a �^�T�T�T�T�T�a �^�T�a  �^�T�T�T�a\n"
        "                                     " << std::endl;

    string command;
    while (true) {
        // ����¼
        if (nowUser == -1) {
            userLogin();
        }
        else {
            // ��ʾǰ׺ [�û���@������ /��Ŀ¼/../../��ǰĿ¼]$
            cout << "" << user[nowUser].username << "@MiniOS " << user[nowUser].username << "/";
            for (int i = 1; i < catalogStack.size(); i++) {
                if (fcbs[catalogStack[i]].name != user[nowUser].username) {
                    cout << fcbs[catalogStack[i]].name << "/";
                }
            }
            cout << ":$ ";

            cin >> command;

            if (command == "help") {
                cout << "* help: ��ȡ����" << endl
                    << "* print [arg]: ��ӡ arg ����" << endl
                    << "* create: �����ļ�" << endl
                    << "* delete��[..args]: ɾ���ļ�" << endl
                    // << "* dir [arg]: ��ʾ��ǰĿ¼�µ��ļ���Ŀ¼��arg: �� | -l | *.[��׺]���ֱ��ʾĬ�ϡ���ϸ����׺" << endl
                    << "* dir: ��ʾ��ǰĿ¼�µ��ļ���Ŀ¼" << endl
                    << "* cd: [arg]: ���� arg Ŀ¼��arg: .. | Ŀ¼�� | root" << endl
                    << "* open: [arg]: �� arg �ļ�" << endl
                    << "* read��write��close��lseek,head,tail: �Դ򿪵��ļ����ж�д�������� open ��ʹ��" << endl
                    << "* mkdir: ����Ŀ¼" << endl
                    << "* rmdir: [..args]: ɾ��Ŀ¼" << endl
                    << "* import: [arg] [path]: �����ļ�" << endl
                    << "* export: [arg] [path]: �����ļ�" << endl
                    << "* exit: �˳�ϵͳ" << endl;
            }
            else if (command == "exit") {
                cout << "Bye!" << endl;
                system("pause");
                exit(0);
            }
            else if (command == "print") {
                // ��ȡ hello ����Ĳ��������ܴ��ո�
                string arg;
                getline(cin, arg);
                // ���ݲ����� cmd_handler �߳�
                argument = arg;
                unique_lock<mutex> lock(m); // ��������ֹ����߳�ͬʱ����
                message = 1;
                ready = true;
                cv.notify_all();    // ���������߳�
                cv.wait(lock, [this] { return !ready; });   // �ȴ� ready ��Ϊ false
            }
            else if (command == "create") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 2;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            else if (command == "delete") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 16;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            else if (command == "dir") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 4;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            else if (command == "cd") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 5;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            else if (command == "mkdir") {
                string arg;
                getline(cin, arg);
                argument = arg;
                // ���Я������������ʾ���Ϸ�
                if (argument.find_first_not_of(" ") != string::npos) {
                    cout << "Error: Invalid argument" << endl;
                    continue;
                }
                unique_lock<mutex> lock(m);
                message = 6;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            else if (command == "register") {
                string arg;
                getline(cin, arg);
                argument = arg;
                // ���Я������������ʾ���Ϸ�
                if (arg.find_first_not_of(" ") != string::npos) {
                    cout << "Error: Invalid argument" << endl;
                    continue;
                }
                unique_lock<mutex> lock(m);
                message = 7;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            // rmdir
            else if (command == "rmdir") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 9;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            // time ��ʾϵͳʱ��
            else if (command == "time") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 10;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            // ver ��ʾϵͳ�汾
            else if (command == "ver") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 11;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            // import
            else if (command == "import") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 12;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            // export
            else if (command == "export") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 13;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            // rename
            else if (command == "rename") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 14;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            // open
            else if (command == "open") {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 15;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            // remove file
           /* else if (command == "rmfile"){
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 16;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }*/
            //flock��
            else if (command == "flock")
            {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 17;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            else if (command == "move")
            {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 18;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            else if (command == "copy")
            {
                string arg;
                getline(cin, arg);
                argument = arg;
                unique_lock<mutex> lock(m);
                message = 19;
                ready = true;
                cv.notify_all();
                cv.wait(lock, [this] { return !ready; });
            }
            else {
                cout << command << ": command not found" << endl;
            }
        }
    }
}
bool os::copyFile(string arg) {
    // ������������ȡԴ�ļ���Ŀ��·��
    vector<string> args;
    istringstream iss(arg);
    string sub;
    while (iss >> sub) {
        args.push_back(sub);
    }

    if (args.size() < 1) {
        cout << "Error: Missing arguments" << endl;
        return false;
    }

    string srcFileName = args[0];
    string destPath = (args.size() > 1) ? args[1] : "";
    cout << destPath << endl;
    // ����Դ�ļ�
    int srcFcbIndex = -1;
    for (int i = 0; i < filesInCatalog.size(); i++) {
        if (fcbs[filesInCatalog[i]].name == srcFileName && fcbs[filesInCatalog[i]].isused == 1) {
            srcFcbIndex = filesInCatalog[i];
            break;
        }
    }

    if (srcFcbIndex == -1) {
        cout << "Error: Source file not found" << endl;
        return false;
    }

    // ��ȡԴ�ļ�����
    string fileContent = openFile(srcFcbIndex);

    // ��ȡԴ�ļ�����ȥ��·����
    string srcFileNameWithoutPath;
    size_t lastSlashPos = srcFileName.find_last_of("/\\");
    if (lastSlashPos == string::npos) {
        srcFileNameWithoutPath = srcFileName;
    }
    else {
        srcFileNameWithoutPath = srcFileName.substr(lastSlashPos + 1);
    }

    // �����ļ�������չ��
    string baseName = srcFileNameWithoutPath;
    string extension = "";
    size_t dotPos = srcFileNameWithoutPath.find_last_of('.');
    if (dotPos != string::npos) {
        baseName = srcFileNameWithoutPath.substr(0, dotPos);
        extension = srcFileNameWithoutPath.substr(dotPos);
    }

    // ����Ŀ��·��
    int destFcbIndex = currentCatalog;
    if (!destPath.empty()) {
        vector<string> pathComponents;
        istringstream pathStream(destPath);
        while (getline(pathStream, sub, '/')) {
            pathComponents.push_back(sub);
        }

        for (const string& component : pathComponents) {
            bool found = false;
            vector<int> currentDir = openDirectory(destFcbIndex);
            for (int i = 0; i < currentDir.size(); i++) {
                if (fcbs[currentDir[i]].name == component && fcbs[currentDir[i]].type == 1) {
                    destFcbIndex = currentDir[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                cout << "Error: Destination path not found" << endl;
                return false;
            }
        }
    }

    // ���Ŀ��·���Ƿ���������ļ���������
    vector<int> destCatalog = openDirectory(destFcbIndex);
    string newFileName = baseName + extension;
    int copyCount = 1;
    bool nameExists;
    do {
        nameExists = false;
        for (int i = 0; i < destCatalog.size(); i++) {
            if (fcbs[destCatalog[i]].name == newFileName && fcbs[destCatalog[i]].isused == 1) {
                nameExists = true;
                newFileName = baseName + "(" + to_string(copyCount++) + ")" + extension;
                break;
            }
        }
    } while (nameExists);

    // �������ļ���д������
    int newFcbIndex = getEmptyFcb();
    if (newFcbIndex == -1) {
        cout << "Error: No space for new file" << endl;
        return false;
    }

    fcbs[newFcbIndex].isused = 1;
    fcbs[newFcbIndex].isLocked = 0;
    fcbs[newFcbIndex].name = newFileName;
    fcbs[newFcbIndex].type = 0;
    fcbs[newFcbIndex].user = nowUser;
    fcbs[newFcbIndex].size = fileContent.size();
    fcbs[newFcbIndex].address = getEmptyBlock();
    fcbs[newFcbIndex].modifyTime = getCurrentTime();

    if (fcbs[newFcbIndex].address == -1) {
        cout << "Error: No space for new file" << endl;
        return false;
    }
    //cout << "newFcbIndex: " << newFcbIndex << "fileContent: " << fileContent << endl;
    if (!saveFileSys(newFcbIndex, fileContent)) {
        cout << "Error: Failed to save new file" << endl;
        deleteFileSystemFile(newFcbIndex);
        return false;
    }

    // ����Ŀ��Ŀ¼��Ϣ
    destCatalog.push_back(newFcbIndex);
    saveFileSys(destFcbIndex, destCatalog);
    saveFatBlockToFile();
    saveBitMapToFile();
    cout << "File copied successfully!" << endl;
    return true;
}


// kernel���ں˴������
[[noreturn]] void os::kernel() {
    kernels = this_thread::get_id();
    // ѭ��������� message �仯
    while (true) {
        unique_lock<mutex> lock(m); // ��������ֹ����߳�ͬʱ����
        cv.wait(lock, [this] { return ready; });    // �ȴ� ready ��Ϊ true
        //if (update()) {		//�ж����޸��£������޸Ĵ����������ж�
        updateData();	//��ȡ�µ���Ϣ�����洢��������
        /*     }*/
             // *1 print ����
        if (message == 1) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                cout << argument << endl;
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        // *2 create ����
        else if (message == 2) {
            makeFile();
            saveModifyTimesToFile();
            modifedTimes++;
            message = 0;
            argument = "";
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        // *3 delete ����
        else if (message == 3) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // ɾ���ļ�
                deleteFile(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        // *4 dir ����
        else if (message == 4) {
            // ��ʾ�ļ��б�
            if (argument.empty()) {
                displayFileInfo();
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // ��ʾ�ļ��б�
                displayFileInfo(argument);
            }
            message = 0;
            argument = "";
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        //cd���
        else if (message == 5) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // �л�Ŀ¼
                cd(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        // *6 mkdir ����
        else if (message == 6) {
            makeDirectory(-1);
            saveModifyTimesToFile();
            modifedTimes++;
            message = 0;
            argument = "";
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        // *7 register ����
        else if (message == 7) {
            // ע���û�
            userRegister();
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ����
        }
        // *9 rmdir ����
        else if (message == 9) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // ɾ���ļ���
                removeDirectory(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        // *10 ��ʾϵͳʱ��
        else if (message == 10) {
            // ��ʾϵͳʱ��
            showTime();
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ����
        }
        // *11 ��ʾϵͳ�汾
        else if (message == 11) {
            // ��ʾϵͳ�汾
            showVersion();
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ����
        }
        // *import �����ⲿ�ļ�
        else if (message == 12) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // �����ļ�
                importFileFromOut(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        // *export �����ļ�
        else if (message == 13) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // �����ļ�
                exportFileToOut(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        // *rename �������ļ�
        else if (message == 14) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // �������ļ�
                rename(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�

        }
        // *open ��
        else if (message == 15) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                // ���ļ�
                openFileMode(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ����
        }
        // *rmfile
        else if (message == 16) {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                //                cout<<"argument:"<<argument<<endl;
                                // ɾ���ļ�
                removeFile(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ����
        }
        //flock �ļ�������
        else if (message == 17)
        {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                //                cout<<"argument:"<<argument<<endl;
                // ɾ���ļ�
                flock(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ����
        }
        //move�ļ��ƶ���
        else if (message == 18)
        {
            vector<string> args;
            istringstream iss(argument);
            string temp;
            while (iss >> temp) {
                args.push_back(temp);
            }
            if (args.size() != 2) {
                cout << "Error: Invalid argument" << endl;
            }
            else {
                moveFile(args[0], args[1]);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ���������߳�
        }
        //copy�ļ�����
        else if (message == 19)
        {
            if (argument.empty()) {
                cout << "argument is empty!" << endl;
            }
            else {
                // ȥ���׸��ո�
                argument.erase(0, 1);
                copyFile(argument);
            }
            argument = "";
            message = 0;
            ready = false;  // ready ��Ϊ false
            cv.notify_all();    // ����
        }
    }
}

